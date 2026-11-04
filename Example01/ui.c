#include "globals.h"
#include "FreeRTOS.h"
#include "task.h"
#include "kbd.h"
#include "lcd.h"
#include "ui.h"
#include "gprs.h"
#include "sensor.h"
#include "memory.h"
#include <stdio.h>
#include <string.h>
#include "ff.h"

/* Global communication structs required by other modules */
XITC_Ui_Gprs_def XITC_Ui_Gprs;
XITC_Ui_Disk_def XITC_Ui_Disk;
XITC_Ui_Lcd_def XITC_Ui_Lcd;
char ui_lcd_disp_buf[50], ui_lcd_disp_buf2[32];
int ui_date, ui_month, ui_year, ui_hour, ui_minute;
int ui_rain;
xDateTime_def xUi_Date_Time;
int iUi_Param_No = 0;
int gUi_Active = 1;

/* Current state tracking */
static int current_screen = SCR_STATION;
static int current_mode = UI_MODE_BROWSE;
static int lcd_active = 1;
static uint32_t inactivity_ticks = 0;

/* Station ID Edit State */
static char stn_edit_buf[5] = "0000";
static int stn_edit_pos = 0;

/* Date & Time Edit State */
static int dt_day = 1, dt_month = 1, dt_year = 2026, dt_hour = 0, dt_min = 0;
static int dt_field = 0; /* 0: DD, 1: MM, 2: YY, 3: HH, 4: MI */

/* Heartbeat indicator state */
static int heartbeat_tick = 0;

/* Firmware Confirmation State */
static int fw_confirm_target = FW_TARGET_NONE;
static int fw_confirm_source = FW_SRC_NONE;

/* Forward declarations */
static void wait_for_lcd_idle(void);
static void ui_send_lcd(const char *line1, const char *line2);
static void render_screen(int scr_id, char *line1, char *line2);
static void handle_stn_edit_mode(int key);
static void handle_datetime_edit_mode(int key);
static void start_fw_update_flow(int target);
static void handle_fw_confirm_mode(int key);
static void handle_wipe_confirm_mode(int key);
static void handle_sleep_confirm_mode(int key);

/*-------------------------------------------------------------------------------------------------------------------*
* 	FUNCTION NAME : wait_for_lcd_idle
* 	DESCRIPTION : Waits for vLcd_Task to complete commands with timeout to guarantee zero deadlock.
*---------------------------------------------------------------------------------------------------------------------*/
static void wait_for_lcd_idle(void)
{
	int timeout = 200; /* 200ms safety timeout */
	while (XITC_Ui_Lcd.sig && timeout > 0) {
		vTaskDelay(1 / portTICK_RATE_MS);
		timeout--;
	}
	XITC_Ui_Lcd.sig = 0; /* Guarantee clear if timed out */
}

/*-------------------------------------------------------------------------------------------------------------------*
* 	FUNCTION NAME : ui_set_cursor_addr & ui_enable_cursor
* 	DESCRIPTION : Sends cursor positioning and blink state commands to vLcd_Task.
*---------------------------------------------------------------------------------------------------------------------*/
static void ui_set_cursor_addr(uint8_t addr)
{
	wait_for_lcd_idle();
	XITC_Ui_Lcd.lcd_posn = addr;
	XITC_Ui_Lcd.lcd_cmd = eLcd_Set_Addr;
	XITC_Ui_Lcd.sig = 1;
	wait_for_lcd_idle();
}

static void ui_enable_cursor(int enable)
{
	wait_for_lcd_idle();
	XITC_Ui_Lcd.lcd_cmd = enable ? eLcd_Cur_On : eLcd_Cur_Off;
	XITC_Ui_Lcd.sig = 1;
	wait_for_lcd_idle();
}

/*-------------------------------------------------------------------------------------------------------------------*
* 	FUNCTION NAME : ui_send_lcd
* 	DESCRIPTION : Safely delivers two 16-character lines to vLcd_Task via XITC_Ui_Lcd.
*---------------------------------------------------------------------------------------------------------------------*/
static void ui_send_lcd(const char *line1, const char *line2)
{
	if (!lcd_active) {
		return;
	}

	wait_for_lcd_idle();

	/* Top row at 0x80 */
	XITC_Ui_Lcd.lcd_posn = 0x80;
	XITC_Ui_Lcd.lcd_cmd = eLcd_Display;
	snprintf(XITC_Ui_Lcd.disp_str, sizeof(XITC_Ui_Lcd.disp_str), "%-16.16s", line1);
	XITC_Ui_Lcd.sig = 1;

	wait_for_lcd_idle();

	/* Bottom row at 0xC0 */
	XITC_Ui_Lcd.lcd_posn = 0xC0;
	XITC_Ui_Lcd.lcd_cmd = eLcd_Display;
	snprintf(XITC_Ui_Lcd.disp_str, sizeof(XITC_Ui_Lcd.disp_str), "%-16.16s", line2);
	XITC_Ui_Lcd.sig = 1;

	wait_for_lcd_idle();
}

/*-------------------------------------------------------------------------------------------------------------------*
* 	FUNCTION NAME : is_screen_supported
* 	DESCRIPTION : Returns 1 if screen is active for current hardware profile, 0 to skip.
*---------------------------------------------------------------------------------------------------------------------*/
static int is_screen_supported(int scr_id)
{
#if (UNIT_CFG == CFG_KSNDMC_TWS)
	/* TWS: No rainfall sensor, has Temp/Hum, Wind Speed, Wind Direction */
	if (scr_id == SCR_RAIN) {
		return 0;
	}
	return 1;
#else
	/* TRG (KSNDMC_TRG, BIHAR_TRG, etc.): Rain gauge only, no Temp/Hum or Wind sensors */
	if (scr_id == SCR_TEMP_HUM || scr_id == SCR_WIND || scr_id == SCR_WIND_DIR) {
		return 0;
	}
	return 1;
#endif
}

/*-------------------------------------------------------------------------------------------------------------------*
* 	FUNCTION NAME : render_screen
* 	DESCRIPTION : Populates two 16-character buffers with live data according to screen ID.
*---------------------------------------------------------------------------------------------------------------------*/
static void render_screen(int scr_id, char *line1, char *line2)
{
	switch (scr_id) {
	/* Group 1: Live Telemetry & Sensors */
	case SCR_STATION: {
		const char *sid = Global_Data_Params.gc_station_id;
		if (sid[0] == '0' && sid[1] == '0') {
			sid += 2;
		}
		if (*sid == '\0') sid = "0000";

#if (UNIT_CFG == CFG_BIHAR_TRG)
		snprintf(line1, 17, "  BIHAR - TRG   ");
#elif (UNIT_CFG == CFG_KSNDMC_TWS)
		snprintf(line1, 17, "  KSNDMC - TWS  ");
#else
		snprintf(line1, 17, "  KSNDMC - TRG  ");
#endif
		snprintf(line2, 17, "ID:%-4.4s K:%d C:%d", sid, dbg_last_key, dbg_raw_cols);
		break;
	}

	case SCR_DATE_TIME: {
		snprintf(line1, 17, "  Date & Time   ");
		int disp_yr = xPresent_Date_Time.year;
		if (disp_yr < 100 && disp_yr > 0) disp_yr += 2000;
		if (disp_yr < 2020 || xPresent_Date_Time.month == 0 || xPresent_Date_Time.date == 0) {
			snprintf(line2, 17, "  Acquiring...  ");
		} else {
			snprintf(line2, 17, "%02d-%02d-%04d %02d:%02d",
				xPresent_Date_Time.date, xPresent_Date_Time.month,
				disp_yr,
				xPresent_Date_Time.hour, xPresent_Date_Time.minute);
		}
		break;
	}

	case SCR_RAIN:
		snprintf(line1, 17, " Rainfall Today ");
		snprintf(line2, 17, "    %3d.%1d mm    ", u_rainfall / 2, ((u_rainfall % 2) ? 5 : 0));
		break;

	case SCR_TEMP_HUM:
		snprintf(line1, 17, " Temp & Humidity");
		if (Global_WS_Data.temperature > -50 && Global_WS_Data.temperature < 80) {
			snprintf(line2, 17, " %4.1fC   %4.1f%% ", Global_WS_Data.temperature, Global_WS_Data.humidity);
		} else {
			snprintf(line2, 17, "  Not Available ");
		}
		break;

	case SCR_WIND:
		snprintf(line1, 17, " Wind Speed km/h");
		snprintf(line2, 17, "Inst:%-4.1f Avg:%-4.1f", Global_WS_Data.inst_ws, Global_WS_Data.avg_ws);
		break;

	case SCR_WIND_DIR:
		snprintf(line1, 17, " Wind Direction ");
		snprintf(line2, 17, "    %-3d Deg     ", Global_WS_Data.wind_dir);
		break;

	case SCR_BATTERY:
		snprintf(line1, 17, " Battery Status ");
		snprintf(line2, 17, "B1:%4.1fV B2:%4.1fV", gf_bat_volt1, gf_bat_volt2);
		break;

	case SCR_SOLAR:
		snprintf(line1, 17, "  Solar Status  ");
		snprintf(line2, 17, "%-16.16s", solar_status ? "STATUS: CHARGING" : "STATUS: STANDBY ");
		break;

	/* Group 2: System Diagnostics & GPS */
	case SCR_SIM_SIGNAL: {
		int sim_no = (Global_Data_Params.gi_sim_select == 1) ? 2 : 1;
		char clean_op[12];
		memset(clean_op, 0, sizeof(clean_op));

		if (gc_operator_name[0] != '\0') {
			strncpy(clean_op, gc_operator_name, 11);
			for (int i = 0; i < 11 && clean_op[i] != '\0'; i++) {
				if (clean_op[i] < 32 || clean_op[i] > 126) clean_op[i] = ' ';
			}
		} else {
			strcpy(clean_op, "Searching..");
		}

		snprintf(line1, 17, "SIM%d: %-10.10s", sim_no, clean_op);

		if (gi_signal_level < 0) {
			int csq = (gi_signal_level + 113) / 2;
			if (csq < 0) csq = 0;
			if (csq > 31) csq = 31;
			snprintf(line2, 17, "%ddBm (CSQ:%02d) ", gi_signal_level, csq);
		} else if (!gi_sim_state) {
			snprintf(line1, 17, "SIM%d: NO SIM CARD", sim_no);
			snprintf(line2, 17, "SIGNAL: NO SVC  ");
		} else if (!gi_sim_reg) {
			snprintf(line2, 17, "SEARCHING NET...");
		} else {
			snprintf(line2, 17, "SIGNAL: WEAK    ");
		}
		break;
	}

	case SCR_LAST_LOG:
		snprintf(line1, 17, " Last Log Sent  ");
		if (gi_log_year > 2000) {
			snprintf(line2, 17, "%02d/%02d %02d:%02d OK ", gi_log_date, gi_log_month, gi_log_hour, gi_log_minute);
		} else {
			snprintf(line2, 17, " NO RECENT LOG  ");
		}
		break;

	case SCR_ERRORS:
		snprintf(line1, 17, "PACKET / NET ERR");
		snprintf(line2, 17, " PKT:%-3d NET:%-3d ", gi_present_error, gi_net_error);
		break;

	case SCR_SDC_STATUS:
		snprintf(line1, 17, " MicroSD Storage");
		if (gi_disk_ok) {
			snprintf(line2, 17, "  STATUS: OK    ");
		} else {
			snprintf(line2, 17, "  STATUS: FAIL  ");
		}
		break;

	case SCR_GPS_LOC:
		snprintf(line1, 17, "  GPS LOCATION  ");
		{
			float disp_lat = (GPS.latitude > 0.0f) ? GPS.latitude : Global_Data_Params.gps_latitude;
			float disp_lon = (GPS.longitude > 0.0f) ? GPS.longitude : Global_Data_Params.gps_longitude;
			char disp_lat_dir = (GPS.lat_dir == 'N' || GPS.lat_dir == 'S') ? GPS.lat_dir :
			                    ((Global_Data_Params.gps_lat_dir == 'N' || Global_Data_Params.gps_lat_dir == 'S') ? Global_Data_Params.gps_lat_dir : 'N');
			char disp_lon_dir = (GPS.lon_dir == 'E' || GPS.lon_dir == 'W') ? GPS.lon_dir :
			                    ((Global_Data_Params.gps_lon_dir == 'E' || Global_Data_Params.gps_lon_dir == 'W') ? Global_Data_Params.gps_lon_dir : 'E');

			if (disp_lat > 0.0f && disp_lon > 0.0f) {
				int lat_i = (int)disp_lat;
				int lat_f = (int)((disp_lat - (float)lat_i) * 10000.0f + 0.5f);
				if (lat_f < 0) lat_f = -lat_f;
				if (lat_f >= 10000) { lat_i++; lat_f = 0; }

				int lon_i = (int)disp_lon;
				int lon_f = (int)((disp_lon - (float)lon_i) * 10000.0f + 0.5f);
				if (lon_f < 0) lon_f = -lon_f;
				if (lon_f >= 10000) { lon_i++; lon_f = 0; }

				if (lon_i >= 100) {
					snprintf(line2, 17, "%02d.%04d,%03d.%03d", lat_i, lat_f, lon_i, lon_f / 10);
				} else {
					snprintf(line2, 17, "%02d.%04d,%02d.%04d", lat_i, lat_f, lon_i, lon_f);
				}
			} else {
				snprintf(line2, 17, "[EN] Acquire Fix");
			}
		}
		break;

	case SCR_VERSION:
		snprintf(line1, 17, "Firmware Version");
		snprintf(line2, 17, "%-16.16s", Global_Data_Params.gc_version);
		break;

	/* Group 4: Operator Actions */
	case SCR_ACT_SEND_HTTP:
		snprintf(line1, 17, " TRANSMIT HTTP  ");
		snprintf(line2, 17, "[EN]SEND BACKLOG");
		break;

	case SCR_ACT_SEND_SMS:
		snprintf(line1, 17, "Send Status SMS?");
		snprintf(line2, 17, "[EN] Confirm    ");
		break;

	case SCR_ACT_USB_COPY:
		snprintf(line1, 17, "  Export Logs?  ");
		snprintf(line2, 17, "[EN] Confirm    ");
		break;

	case SCR_ACT_MAIN_FW_UPDATE:
		snprintf(line1, 17, "Update Main FW? ");
		snprintf(line2, 17, "[EN] Confirm    ");
		break;

	case SCR_ACT_SENS_FW_UPDATE:
		snprintf(line1, 17, "Update Sens FW? ");
		snprintf(line2, 17, "[EN] Confirm    ");
		break;

	case SCR_ACT_WIPE_DATA:
		snprintf(line1, 17, " Wipe All Data? ");
		snprintf(line2, 17, "[EN] Confirm    ");
		break;

	case SCR_ACT_SLEEP:
		snprintf(line1, 17, "  Enter Sleep?  ");
		snprintf(line2, 17, "[EN] Select     ");
		break;

	default:
		snprintf(line1, 17, "   VARSHA-TWS   ");
		snprintf(line2, 17, "                ");
		break;
	}
}

/*-------------------------------------------------------------------------------------------------------------------*
* 	FUNCTION NAME : handle_action
* 	DESCRIPTION : Executes an operator action when ENTER is pressed on an action screen.
*---------------------------------------------------------------------------------------------------------------------*/
static void handle_action(int scr_id)
{
	int wait_cnt = 0;

	if (scr_id == SCR_ACT_SEND_SMS) {
		ui_send_lcd("Send Status SMS ", "SENDING...      ");
		XITC_Ui_Gprs.result = 0;
		XITC_Ui_Gprs.command = eUi_Gprs_Send_Status;
		XITC_Ui_Gprs.signal = 1;

		while (XITC_Ui_Gprs.signal && wait_cnt < 250) {
			vTaskDelay(100 / portTICK_RATE_MS);
			wait_cnt++;
			taskENTER_CRITICAL();
			gi_wdt_task_ok_stat |= WDT_UI_OK;
			taskEXIT_CRITICAL();
			if ((wait_cnt % 20) == 0) {
				char progress[17];
				int dots = (wait_cnt / 20) % 4;
				snprintf(progress, sizeof(progress), "PLEASE WAIT%.*s ", dots + 1, "....");
				ui_send_lcd("Send Status SMS ", progress);
			}
		}

		if (XITC_Ui_Gprs.result == eUi_Gprs_Send_Status_Done) {
			ui_send_lcd("Send Status SMS ", "    SENT OK     ");
		} else {
			ui_send_lcd("Send Status SMS ", "  SEND FAILED   ");
		}
		vTaskDelay(2000 / portTICK_RATE_MS);
		current_screen = SCR_ACT_SEND_SMS;
	}
	else if (scr_id == SCR_GPS_LOC) {
		ui_send_lcd("  GPS LOCATION  ", "ACQUIRING FIX...");
		XITC_Ui_Gprs.result = 0;
		XITC_Ui_Gprs.command = eUI_Gps_Info;
		XITC_Ui_Gprs.signal = 1;
		wait_cnt = 0;

		while (XITC_Ui_Gprs.signal && wait_cnt < 650) {
			vTaskDelay(100 / portTICK_RATE_MS);
			wait_cnt++;
			taskENTER_CRITICAL();
			gi_wdt_task_ok_stat |= WDT_UI_OK;
			taskEXIT_CRITICAL();
			if ((wait_cnt % 20) == 0) {
				char progress[17];
				snprintf(progress, sizeof(progress), "SEARCH SKY: %02ds ", wait_cnt / 10);
				ui_send_lcd("  GPS LOCATION  ", progress);
			}
		}

		if (XITC_Ui_Gprs.result == eUI_Gps_Info_A || (gps_found && GPS.latitude > 0.0f)) {
			ui_send_lcd("  GPS LOCATION  ", " FIX ACQUIRED!  ");
			vTaskDelay(2000 / portTICK_RATE_MS);
		} else {
			float has_lat = (GPS.latitude > 0.0f) ? GPS.latitude : Global_Data_Params.gps_latitude;
			if (has_lat > 0.0f) {
				ui_send_lcd("  GPS LOCATION  ", "KEPT STORED FIX ");
				vTaskDelay(2000 / portTICK_RATE_MS);
			} else {
				ui_send_lcd("  GPS LOCATION  ", "NO FIX: GO OUTDO");
				vTaskDelay(2500 / portTICK_RATE_MS);
			}
		}
		current_screen = SCR_GPS_LOC;
	}
	else if (scr_id == SCR_ACT_USB_COPY) {
		Ui_Usb_Copy();
		current_screen = SCR_ACT_USB_COPY;
	}
	else if (scr_id == SCR_ACT_MAIN_FW_UPDATE) {
		start_fw_update_flow(FW_TARGET_MAIN);
	}
	else if (scr_id == SCR_ACT_SENS_FW_UPDATE) {
		start_fw_update_flow(FW_TARGET_SENSOR);
	}
}

/*-------------------------------------------------------------------------------------------------------------------*
* 	FUNCTION NAME : start_fw_update_flow
* 	DESCRIPTION : Checks USB first (copies to SD), then falls back to SD card.
* 	              Displays interactive confirmation screen once firmware is found.
*---------------------------------------------------------------------------------------------------------------------*/
static void start_fw_update_flow(int target)
{
	int wait_cnt = 0;
	ui_send_lcd("Scanning USB/SD ", "PLEASE WAIT...  ");

	XITC_Ui_Disk.result = eFw_Not_Found;
	if (target == FW_TARGET_MAIN) {
		XITC_Ui_Disk.command = eITC_Ui_Copy_Firmware;
	} else {
		XITC_Ui_Disk.command = eITC_Ui_Copy_Firmware1;
	}

	while (XITC_Ui_Disk.command != eITC_Ui_Disk_Nop && wait_cnt < 250) {
		vTaskDelay(100 / portTICK_RATE_MS);
		wait_cnt++;
		taskENTER_CRITICAL();
		gi_wdt_task_ok_stat |= WDT_UI_OK;
		taskEXIT_CRITICAL();
	}

	if (XITC_Ui_Disk.result == eFw_Found_USB || XITC_Ui_Disk.result == eFw_Found_SD) {
		fw_confirm_target = target;
		fw_confirm_source = (XITC_Ui_Disk.result == eFw_Found_USB) ? FW_SRC_USB : FW_SRC_SD;
		current_mode = UI_MODE_FW_CONFIRM;

		char l1[17], l2[17];
		if (target == FW_TARGET_MAIN) {
			snprintf(l1, 17, (fw_confirm_source == FW_SRC_USB) ? "Main FW [USB]   " : "Main FW [SD]    ");
		} else {
			snprintf(l1, 17, (fw_confirm_source == FW_SRC_USB) ? "Sens FW [USB]   " : "Sens FW [SD]    ");
		}
		snprintf(l2, 17, "Confirm: [EN]   ");
		ui_send_lcd(l1, l2);
	} else {
		ui_send_lcd("File Not Found! ", "Insert USB / SD ");
		vTaskDelay(2500 / portTICK_RATE_MS);
		current_mode = UI_MODE_BROWSE;
		fw_confirm_target = FW_TARGET_NONE;
		fw_confirm_source = FW_SRC_NONE;
	}
}

/*-------------------------------------------------------------------------------------------------------------------*
* 	FUNCTION NAME : handle_fw_confirm_mode
* 	DESCRIPTION : Handles key events on the interactive firmware flash confirmation screen.
*---------------------------------------------------------------------------------------------------------------------*/
static void handle_fw_confirm_mode(int key)
{
	if (key == eSet_Key) {
		/* Operator pressed ENTER to confirm flashing */
		if (fw_confirm_target == FW_TARGET_MAIN) {
			ui_send_lcd("FLASHING MAIN.. ", "REBOOTING MCU...");
			vTaskDelay(1200 / portTICK_RATE_MS);
			NVIC_SystemReset();
		} else if (fw_confirm_target == FW_TARGET_SENSOR) {
			ui_send_lcd("FLASHING SENSOR ", "PLEASE WAIT...  ");
			vTaskDelay(2000 / portTICK_RATE_MS);
			firmware_copied = 1;
			ui_send_lcd(" SENSOR UPDATED!", "FIRMWARE READY  ");
			vTaskDelay(2500 / portTICK_RATE_MS);
			current_mode = UI_MODE_BROWSE;
			fw_confirm_target = FW_TARGET_NONE;
			fw_confirm_source = FW_SRC_NONE;
		}
	} else if (key != eNo_Key) {
		/* Any other key cancels (eBack_Key, Up, Down, Menu, etc.) */
		ui_send_lcd("FLASH CANCELLED ", "                ");
		vTaskDelay(1500 / portTICK_RATE_MS);
		current_mode = UI_MODE_BROWSE;
		fw_confirm_target = FW_TARGET_NONE;
		fw_confirm_source = FW_SRC_NONE;
	}
}

/*-------------------------------------------------------------------------------------------------------------------*
* 	FUNCTION NAME : handle_wipe_confirm_mode
* 	DESCRIPTION : Handles key events on the Wipe All Data confirmation screen.
*---------------------------------------------------------------------------------------------------------------------*/
static void handle_wipe_confirm_mode(int key)
{
	if (key == eSet_Key) {
		/* Operator confirmed factory data wipe */
		ui_send_lcd("WIPING MEMORY.. ", "PLEASE WAIT...  ");
		vTaskDelay(500 / portTICK_RATE_MS);

		taskENTER_CRITICAL();
		init_entire_space(); /* Erases Flash data sectors 2 through 8 */
		taskEXIT_CRITICAL();

		present_sample_no = 0;
		present_sector_no = 2;
		gi_present_error = 0;
		gi_net_error = 0;
		c_rainfall = 0;
		u_rainfall = 0;
		last_rec_15min_rf = 0;

		f_unlink("err.txt");

		ui_send_lcd("DATA WIPED CLEAN", "FRESH RESTART OK");
		vTaskDelay(2500 / portTICK_RATE_MS);

		current_screen = SCR_STATION;
		current_mode = UI_MODE_BROWSE;
	} else if (key != eNo_Key) {
		/* Any other key cancels wipe */
		ui_send_lcd("WIPE CANCELLED  ", "                ");
		vTaskDelay(1500 / portTICK_RATE_MS);
		current_mode = UI_MODE_BROWSE;
	}
}

/*-------------------------------------------------------------------------------------------------------------------*
* 	FUNCTION NAME : handle_sleep_confirm_mode
* 	DESCRIPTION : Handles key events on the Enter Sleep confirmation screen.
*---------------------------------------------------------------------------------------------------------------------*/
static void handle_sleep_confirm_mode(int key)
{
	if (key == eSet_Key) {
		/* Operator confirmed enter sleep */
		system_enter_sleep();
	} else if (key != eNo_Key) {
		/* Any other key cancels sleep (Back, Up, Down, Menu, etc.) */
		ui_send_lcd("SLEEP CANCELLED ", "                ");
		vTaskDelay(1500 / portTICK_RATE_MS);
		current_mode = UI_MODE_BROWSE;
	}
}

/*-------------------------------------------------------------------------------------------------------------------*
* 	FUNCTION NAME : handle_calib_mode
* 	DESCRIPTION : Live Tipping Bucket Calibration screen and logic.
*---------------------------------------------------------------------------------------------------------------------*/
static void handle_calib_mode(int key)
{
	char line1[17], line2[17];

	if (key == eBack_Key) {
		/* Cancel calibration */
		calib_on = 0;
		current_mode = UI_MODE_BROWSE;
		ui_send_lcd("Calibration     ", "CANCELLED       ");
		vTaskDelay(1500 / portTICK_RATE_MS);
		return;
	}

	if (key == eSet_Key) {
		/* Confirm and save calibration */
		calib_on = 0;
		current_mode = UI_MODE_BROWSE;
		Cailb_Status_Details.date = xPresent_Date_Time.date;
		Cailb_Status_Details.month = xPresent_Date_Time.month;
		Cailb_Status_Details.year = xPresent_Date_Time.year;
		Cailb_Status_Details.status = (calib_rf >= 10) ? 1 : 0;

		XITC_Ui_Disk.command = eITC_Ui_Disk_Save_Params;

		if (Cailb_Status_Details.status) {
			ui_send_lcd("CALIB RESULT:   ", "PASS (10 tips)  ");
		} else {
			ui_send_lcd("CALIB RESULT:   ", "FAIL (<10 tips) ");
		}
		vTaskDelay(3000 / portTICK_RATE_MS);
		return;
	}

	/* Render live calibration data */
	snprintf(line1, 17, "CALIB: TIPS   RF");
	snprintf(line2, 17, "Count:%-3d %3d.%1dmm", calib_rf, calib_rf / 2, ((calib_rf % 2) ? 5 : 0));
	ui_send_lcd(line1, line2);
}

/*-------------------------------------------------------------------------------------------------------------------*
* 	FUNCTION NAME : handle_stn_edit_mode
* 	DESCRIPTION : Interactive Station ID digit editor.
*---------------------------------------------------------------------------------------------------------------------*/
static void handle_stn_edit_mode(int key)
{
	char line1[17], line2[17];

	if (key == eBack_Key) {
		/* Cancel edit without saving */
		ui_enable_cursor(0);
		current_mode = UI_MODE_BROWSE;
		ui_send_lcd(" SET STATION ID ", "   CANCELLED    ");
		vTaskDelay(800 / portTICK_RATE_MS);
		return;
	}

	if (key == eSet_Key) {
		/* Save edited station ID */
		ui_enable_cursor(0);
		snprintf(Global_Data_Params.gc_station_id, sizeof(Global_Data_Params.gc_station_id), "00%-4.4s", stn_edit_buf);
		strncpy(gc_station_id1, Global_Data_Params.gc_station_id, sizeof(gc_station_id1));

		ui_send_lcd(" SET STATION ID ", "    SAVING...   ");
		write_param_sector();
		XITC_Ui_Disk.command = eITC_Ui_Disk_Save_Params;

		ui_send_lcd(" SET STATION ID ", "    SAVED OK    ");
		vTaskDelay(1200 / portTICK_RATE_MS);
		current_mode = UI_MODE_BROWSE;
		return;
	}

	if (key == eLeft_Key) {
		if (stn_edit_pos > 0) stn_edit_pos--;
	} else if (key == eRight_Key) {
		if (stn_edit_pos < 3) stn_edit_pos++;
	} else if (key == eUp_Key) {
		if (stn_edit_buf[stn_edit_pos] >= '9') stn_edit_buf[stn_edit_pos] = '0';
		else stn_edit_buf[stn_edit_pos]++;
	} else if (key == eDown_Key) {
		if (stn_edit_buf[stn_edit_pos] <= '0') stn_edit_buf[stn_edit_pos] = '9';
		else stn_edit_buf[stn_edit_pos]--;
	}

	/* Render edit display */
	snprintf(line1, 17, " SET STATION ID ");
	snprintf(line2, 17, "   ID: [ %-4.4s ]  ", stn_edit_buf);
	ui_send_lcd(line1, line2);

	/* Position blinking cursor right on the active digit */
	/* Index 0=' ', 1=' ', 2=' ', 3='I', 4='D', 5=':', 6=' ', 7='[', 8=' ', 9=digit 0 */
	ui_set_cursor_addr(0xC0 + 9 + stn_edit_pos);
	ui_enable_cursor(1);
}

/*-------------------------------------------------------------------------------------------------------------------*
* 	FUNCTION NAME : handle_datetime_edit_mode
* 	DESCRIPTION : Interactive RTC Date & Time editor.
*---------------------------------------------------------------------------------------------------------------------*/
static void handle_datetime_edit_mode(int key)
{
	char line1[17], line2[17];
	static const int col_map[5] = {1, 4, 7, 10, 13};

	if (key == eBack_Key) {
		/* Cancel edit without saving */
		ui_enable_cursor(0);
		current_mode = UI_MODE_BROWSE;
		ui_send_lcd("  Date & Time   ", "   CANCELLED    ");
		vTaskDelay(800 / portTICK_RATE_MS);
		return;
	}

	if (key == eSet_Key) {
		/* Save new RTC time to external RTC via LPC845 and internal RTC */
		ui_enable_cursor(0);
		xPresent_Date_Time.date = dt_day;
		xPresent_Date_Time.month = dt_month;
		xPresent_Date_Time.year = dt_year;
		xPresent_Date_Time.hour = dt_hour;
		xPresent_Date_Time.minute = dt_min;
		xPresent_Date_Time.second = 0;

		xSet_Date_Time.date = dt_day;
		xSet_Date_Time.month = dt_month;
		xSet_Date_Time.year = dt_year;
		xSet_Date_Time.hour = dt_hour;
		xSet_Date_Time.minute = dt_min;
		xSet_Date_Time.second = 0;
		set_time_to_rtc = 1;

		LPC_RTC->SEC   = 0;
		LPC_RTC->MIN   = dt_min;
		LPC_RTC->HOUR  = dt_hour;
		LPC_RTC->DOM   = dt_day;
		LPC_RTC->MONTH = dt_month;
		LPC_RTC->YEAR  = dt_year;

		ui_send_lcd("  Date & Time   ", "    SAVED OK    ");
		vTaskDelay(1200 / portTICK_RATE_MS);
		current_mode = UI_MODE_BROWSE;
		return;
	}

	if (key == eLeft_Key) {
		if (dt_field > 0) dt_field--;
	} else if (key == eRight_Key) {
		if (dt_field < 4) dt_field++;
	} else if (key == eUp_Key) {
		if (dt_field == 0) dt_day = (dt_day >= 31) ? 1 : dt_day + 1;
		else if (dt_field == 1) dt_month = (dt_month >= 12) ? 1 : dt_month + 1;
		else if (dt_field == 2) dt_year = (dt_year >= 2099) ? 2024 : dt_year + 1;
		else if (dt_field == 3) dt_hour = (dt_hour >= 23) ? 0 : dt_hour + 1;
		else if (dt_field == 4) dt_min = (dt_min >= 59) ? 0 : dt_min + 1;
	} else if (key == eDown_Key) {
		if (dt_field == 0) dt_day = (dt_day <= 1) ? 31 : dt_day - 1;
		else if (dt_field == 1) dt_month = (dt_month <= 1) ? 12 : dt_month - 1;
		else if (dt_field == 2) dt_year = (dt_year <= 2024) ? 2099 : dt_year - 1;
		else if (dt_field == 3) dt_hour = (dt_hour <= 0) ? 23 : dt_hour - 1;
		else if (dt_field == 4) dt_min = (dt_min <= 0) ? 59 : dt_min - 1;
	}

	/* Render edit display */
	snprintf(line1, 17, " SET DATE & TIME");
	snprintf(line2, 17, "%02d-%02d-%02d %02d:%02d ", dt_day, dt_month, dt_year % 100, dt_hour, dt_min);
	ui_send_lcd(line1, line2);

	/* Position blinking cursor right on the active field */
	ui_set_cursor_addr(0xC0 + col_map[dt_field]);
	ui_enable_cursor(1);
}

/*-------------------------------------------------------------------------------------------------------------------*
* 	FUNCTION NAME : wake_display / sleep_display / toggle_display
* 	DESCRIPTION : Manages HD44780 4-bit re-initialization and LCD power rail via LPC845.
*---------------------------------------------------------------------------------------------------------------------*/
static void wake_display(void)
{
	gUi_Active = 1;
	lcd_active = 1;

	/* 1. Turn on LCD display pixels */
	wait_for_lcd_idle();
	XITC_Ui_Lcd.lcd_cmd = eLcd_On;
	XITC_Ui_Lcd.sig = 1;
	wait_for_lcd_idle();

	/* 2. Directly activate backlight hardware pin */
	lcd_backlight_on();
}

static void sleep_display(void)
{
	lcd_active = 0;
	gUi_Active = 0;

	/* 1. Directly deactivate backlight hardware pin */
	lcd_backlight_off();

	/* 2. Turn off LCD display pixels (HD44780 controller remains powered at 5V, 4-bit mode preserved) */
	wait_for_lcd_idle();
	XITC_Ui_Lcd.lcd_cmd = eLcd_Off;
	XITC_Ui_Lcd.sig = 1;
	wait_for_lcd_idle();
}

static void toggle_display(void)
{
	if (!lcd_active) {
		wake_display();
	} else {
		sleep_display();
	}
}

/*-------------------------------------------------------------------------------------------------------------------*
* 	FUNCTION NAME : system_enter_sleep
* 	DESCRIPTION : Cleanly shuts down display, baseband RF, keypad pins, and cuts power via LPC845.
*---------------------------------------------------------------------------------------------------------------------*/
void system_enter_sleep(void)
{
	ui_send_lcd("POWERING DOWN...", "DSP KEY TO WAKE ");
	vTaskDelay(1200 / portTICK_RATE_MS);
	ui_send_lcd("                ", "                ");
	vTaskDelay(100 / portTICK_RATE_MS);
	lcd_active = 0;
	gUi_Active = 0;
	shutdown_pwr = 1;

	/* Flush key events so key release cannot cancel sleep */
	XITC_Kbd_Log.sig_key = 0;
	XITC_Kbd_Log.key_value = 0;

	/* 1. Shut down SIM7600 RF & baseband to conserve power and turn off LEDs */
	gprs_send_cmd_direct("AT+CPOF\r\n");
	vTaskDelay(500 / portTICK_RATE_MS);

	/* 2. Release keyboard pins to High-Z inputs so LPC845 UI_COL_PIN (P0.15) sees open circuit */
	kbd_release_pins_for_sleep();
	vTaskDelay(100 / portTICK_RATE_MS);

	/* 3. Send $f7;\012 directly to LPC845 to cut power rails */
	sensor_send_cmd_direct("$f7;\012");
	vTaskDelay(200 / portTICK_RATE_MS);

	/* Clear cached power states so bench power does not trigger immediate reset */
	gprs_pwr = 'g';
	usb_lcd_pwr = 'u';
	shutdown_pwr = 1;

	/* 4. Power will now be cut by LPC845 in normal operation.
	 * If powered by CMSIS-DAP debugger probe on bench, wait here until power cut
	 * or until LPC845 wakes up rails ('G' or 'U') on DSP key or 15-min slot.
	 */
	vTaskDelay(3000 / portTICK_RATE_MS); /* Ensure user has released enter key and stale packets flush */
	gprs_pwr = 'g';
	usb_lcd_pwr = 'u';

	while (shutdown_pwr == 1) {
		XITC_Kbd_Log.sig_key = 0;
		taskENTER_CRITICAL();
		gi_wdt_task_ok_stat |= (WDT_UI_OK | WDT_MEMORY_OK | WDT_SENSOR_OK | WDT_GPRS_OK | WDT_DISK_OK);
		taskEXIT_CRITICAL();

		/* Only wake up if LPC845 has restored the power rails ('G' or 'U') */
		if (gprs_pwr == 'G' || usb_lcd_pwr == 'U') {
			shutdown_pwr = 0;
			NVIC_SystemReset();
		}

		vTaskDelay(200 / portTICK_RATE_MS);
	}
}

/*-------------------------------------------------------------------------------------------------------------------*
* 	FUNCTION NAME : vUi_Task
* 	DESCRIPTION : Main FreeRTOS UI Engine task for LPC1768.
*---------------------------------------------------------------------------------------------------------------------*/
void vUi_Task(void *pvParameters)
{
	char line1[17], line2[17];
	uint32_t render_counter = 0;
	int need_render = 1;
	static uint32_t inactivity_seconds = 0;

	/* Startup synchronization: ensure hardware drivers & sensor UART are ready */
	XITC_Ui_Lcd.sig = 0;
	while (!lcd_task_started) {
		vTaskDelay(20 / portTICK_RATE_MS);
	}
	while (!kbd_task_started) {
		vTaskDelay(20 / portTICK_RATE_MS);
	}
	while (!memory_task_started) {
		vTaskDelay(20 / portTICK_RATE_MS);
	}
	while (!sensor_task_started) {
		vTaskDelay(20 / portTICK_RATE_MS);
	}

	XITC_Ui_Disk.command = eITC_Ui_Disk_Nop;

	/* Turn on display and show splash screen */
	lcd_active = 1;
	gUi_Active = 1;
	lcd_backlight_on();
	ui_send_lcd(WELCOME_STR1, " INITIALIZING.. ");
	vTaskDelay(600 / portTICK_RATE_MS);

	/* Transition cleanly from splash to Station ID home screen */
	current_screen = SCR_STATION;
	render_screen(current_screen, line1, line2);
	ui_send_lcd(line1, line2);
	need_render = 0;

	for (;;) {
		/* Watchdog heartbeat */
		taskENTER_CRITICAL();
		gi_wdt_task_ok_stat |= WDT_UI_OK;
		taskEXIT_CRITICAL();

		/* Check for key input */
		if (XITC_Kbd_Log.sig_key) {
			int key = XITC_Kbd_Log.key_value;
			XITC_Kbd_Log.sig_key = 0;
			inactivity_ticks = 0;
			inactivity_seconds = 0;
			gUi_Active = 1;

			/* Wake up screen on any key if asleep */
			if (!lcd_active) {
				wake_display();
				need_render = 1;
				if (!gprs_modem_active) {
					select_sim_0();
					gprs_modem_active = 1;
				}
				/* If key was DSP key, wake is already achieved; do not toggle back off */
				if (key == eDisp_Key) {
					continue;
				}
			}

			if (current_mode == UI_MODE_CALIB) {
				handle_calib_mode(key);
				need_render = 1;
			} else if (current_mode == UI_MODE_EDIT_STN) {
				handle_stn_edit_mode(key);
			} else if (current_mode == UI_MODE_EDIT_DATETIME) {
				handle_datetime_edit_mode(key);
			} else if (current_mode == UI_MODE_FW_CONFIRM) {
				handle_fw_confirm_mode(key);
				need_render = 1;
			} else if (current_mode == UI_MODE_WIPE_CONFIRM) {
				handle_wipe_confirm_mode(key);
				need_render = 1;
			} else if (current_mode == UI_MODE_SLEEP_CONFIRM) {
				handle_sleep_confirm_mode(key);
				need_render = 1;
			} else {
				/* Browse Mode Navigation */
				switch (key) {
				case eUp_Key:
				case eLeft_Key:
					/* Previous screen in carousel (skipping unsupported profile screens) */
					do {
						current_screen = (current_screen + SCR_TOTAL_SCREENS - 1) % SCR_TOTAL_SCREENS;
					} while (!is_screen_supported(current_screen));
					need_render = 1;
					break;

				case eDown_Key:
				case eRight_Key:
					/* Next screen in carousel (skipping unsupported profile screens) */
					do {
						current_screen = (current_screen + 1) % SCR_TOTAL_SCREENS;
					} while (!is_screen_supported(current_screen));
					need_render = 1;
					break;

				case eMenu_Key:
					/* Quick jump between Station/Sensors (0), Power & Storage (6), and Diagnostics (14) */
					if (current_screen < SCR_BATTERY) {
						current_screen = SCR_BATTERY;
					} else if (current_screen < SCR_ERRORS) {
						current_screen = SCR_ERRORS;
					} else {
						current_screen = SCR_STATION;
					}
					while (!is_screen_supported(current_screen)) {
						current_screen = (current_screen + 1) % SCR_TOTAL_SCREENS;
					}
					need_render = 1;
					break;

				case eCalib_Key:
#if (UNIT_CFG == CFG_KSNDMC_TWS)
					ui_send_lcd("  VARSHA - TWS  ", " No Rain Gauge  ");
					vTaskDelay(1500 / portTICK_RATE_MS);
#else
					/* Instant hotkey into Live Rain Gauge Calibration */
					current_mode = UI_MODE_CALIB;
					calib_on = 1;
					calib_rf = 0;
#endif
					need_render = 1;
					break;

				case eBack_Key:
					/* Immediate Home shortcut (Station ID screen) */
					current_screen = SCR_STATION;
					need_render = 1;
					break;

				case eSet_Key:
					/* ENTER key: triggers Edit Mode or Action depending on screen */
					if (current_screen == SCR_STATION) {
						/* Enter Station ID edit mode */
						current_mode = UI_MODE_EDIT_STN;
						const char *src = Global_Data_Params.gc_station_id;
						if (src[0] == '0' && src[1] == '0') src += 2;
						snprintf(stn_edit_buf, sizeof(stn_edit_buf), "%-4.4s", src);
						for (int i = 0; i < 4; i++) {
							if (stn_edit_buf[i] < '0' || stn_edit_buf[i] > '9') {
								stn_edit_buf[i] = '0';
							}
						}
						stn_edit_pos = 0;
						handle_stn_edit_mode(eNo_Key);
						need_render = 0;
						break;
					} else if (current_screen == SCR_DATE_TIME) {
						/* Enter Date/Time edit mode */
						current_mode = UI_MODE_EDIT_DATETIME;
						dt_day = (xPresent_Date_Time.date >= 1 && xPresent_Date_Time.date <= 31) ? xPresent_Date_Time.date : 1;
						dt_month = (xPresent_Date_Time.month >= 1 && xPresent_Date_Time.month <= 12) ? xPresent_Date_Time.month : 1;
						dt_year = (xPresent_Date_Time.year >= 2020 && xPresent_Date_Time.year <= 2099) ? xPresent_Date_Time.year : 2026;
						dt_hour = (xPresent_Date_Time.hour >= 0 && xPresent_Date_Time.hour <= 23) ? xPresent_Date_Time.hour : 0;
						dt_min = (xPresent_Date_Time.minute >= 0 && xPresent_Date_Time.minute <= 59) ? xPresent_Date_Time.minute : 0;
						dt_field = 0;
						handle_datetime_edit_mode(eNo_Key);
						need_render = 0;
						break;
					} else if (current_screen == SCR_LAST_LOG || current_screen == SCR_ACT_SEND_HTTP) {
						/* Immediate on-demand HTTP transmission (Live record + Flash backlogs) */
						ui_send_lcd("TRANSMITTING...", "LIVE + BACKLOGS ");
						gi_manual_http_trigger = 1;
						gi_data_ready_for_report = 1;
						vTaskDelay(1500 / portTICK_RATE_MS);
					} else if (current_screen == SCR_GPS_LOC) {
						/* Refresh / Acquire live GPS fix on-demand */
						handle_action(SCR_GPS_LOC);
					} else if (current_screen == SCR_ERRORS) {
						/* Reset error counters */
						gi_present_error = 0;
						gi_net_error = 0;
						ui_send_lcd("PACKET / NET ERR", "  ERRORS RESET  ");
						vTaskDelay(1000 / portTICK_RATE_MS);
					} else if (current_screen == SCR_ACT_SEND_SMS) {
						/* Execute Send Status SMS action */
						handle_action(SCR_ACT_SEND_SMS);
					} else if (current_screen == SCR_ACT_USB_COPY) {
						/* Execute Copy to USB action */
						handle_action(SCR_ACT_USB_COPY);
					} else if (current_screen == SCR_ACT_MAIN_FW_UPDATE) {
						start_fw_update_flow(FW_TARGET_MAIN);
					} else if (current_screen == SCR_ACT_SENS_FW_UPDATE) {
						start_fw_update_flow(FW_TARGET_SENSOR);
					} else if (current_screen == SCR_ACT_WIPE_DATA) {
						current_mode = UI_MODE_WIPE_CONFIRM;
						ui_send_lcd("WIPE ALL DATA?  ", "Confirm: [EN]   ");
					} else if (current_screen == SCR_ACT_SLEEP) {
						current_mode = UI_MODE_SLEEP_CONFIRM;
						ui_send_lcd("  ENTER SLEEP?  ", "Confirm: [EN]   ");
					}
					need_render = 1;
					break;

				case eDisp_Key:
					/* Screen is active; pressing DSP puts it to sleep */
					sleep_display();
					break;

				default:
					break;
				}
			}
		}

		/* Inactivity tracking: 60s timeout for unattended field operation */
		inactivity_ticks++;

		/* Hardware SysTick-Locked Precision System Clock (20ppm accuracy from 12MHz CPU crystal) */
		static portTickType last_sys_tick = 0;
		if (last_sys_tick == 0) {
			last_sys_tick = xTaskGetTickCount();
		}
		portTickType now_tick = xTaskGetTickCount();
		if ((now_tick - last_sys_tick) >= configTICK_RATE_HZ) {
			uint32_t secs_to_advance = (now_tick - last_sys_tick) / configTICK_RATE_HZ;
			last_sys_tick += (secs_to_advance * configTICK_RATE_HZ);

			inactivity_seconds += secs_to_advance;
			if (inactivity_seconds >= 60 && gUi_Active) {
				/* Operator inactivity timeout: turn off LCD backlight to save power without cutting MCU or locking keypad */
				inactivity_seconds = 0;
				gUi_Active = 0;
				if (lcd_active) {
					sleep_display();
				}
			}

			if (xPresent_Date_Time.year < 100 && xPresent_Date_Time.year > 0) {
				xPresent_Date_Time.year += 2000;
			}

			if (xPresent_Date_Time.year >= 2020) {
				xPresent_Date_Time.second += secs_to_advance;
				while (xPresent_Date_Time.second >= 60) {
					xPresent_Date_Time.second -= 60;
					xPresent_Date_Time.minute++;
					need_render = 1;
					if (xPresent_Date_Time.minute >= 60) {
						xPresent_Date_Time.minute = 0;
						xPresent_Date_Time.hour++;
						if (xPresent_Date_Time.hour >= 24) {
							xPresent_Date_Time.hour = 0;
							Next_Date(&xPresent_Date_Time.date, &xPresent_Date_Time.month, &xPresent_Date_Time.year);
						}
					}
				}
			}
		}

		/* Periodic Screen Refresh (every 500ms or when navigation occurs) */
		render_counter++;
		if (lcd_active && (need_render || render_counter >= (500 / 20))) {
			render_counter = 0;
			need_render = 0;

			if (current_mode == UI_MODE_CALIB) {
				handle_calib_mode(eNo_Key);
			} else if (current_mode == UI_MODE_EDIT_STN) {
				handle_stn_edit_mode(eNo_Key);
			} else if (current_mode == UI_MODE_EDIT_DATETIME) {
				handle_datetime_edit_mode(eNo_Key);
			} else if (current_mode == UI_MODE_FW_CONFIRM) {
				char l1[17], l2[17];
				if (fw_confirm_target == FW_TARGET_MAIN) {
					snprintf(l1, 17, (fw_confirm_source == FW_SRC_USB) ? "Main FW [USB]   " : "Main FW [SD]    ");
				} else {
					snprintf(l1, 17, (fw_confirm_source == FW_SRC_USB) ? "Sens FW [USB]   " : "Sens FW [SD]    ");
				}
				snprintf(l2, 17, "Confirm: [EN]   ");
				ui_send_lcd(l1, l2);
			} else if (current_mode == UI_MODE_WIPE_CONFIRM) {
				ui_send_lcd("WIPE ALL DATA?  ", "Confirm: [EN]   ");
			} else if (current_mode == UI_MODE_SLEEP_CONFIRM) {
				ui_send_lcd("  ENTER SLEEP?  ", "Confirm: [EN]   ");
			} else {
				render_screen(current_screen, line1, line2);

				/* Heartbeat dot on corner of line1 */
				heartbeat_tick = !heartbeat_tick;
				if (heartbeat_tick && line1[15] == ' ') {
					line1[15] = '.';
				}

				ui_send_lcd(line1, line2);
			}
		}

		vTaskDelay(20 / portTICK_RATE_MS);
	}
}

/*-------------------------------------------------------------------------------------------------------------------*
* 	FUNCTION NAME : Ui_Usb_Copy
* 	DESCRIPTION : Dumps logs to USB drive with interactive LCD progress.
*---------------------------------------------------------------------------------------------------------------------*/
void Ui_Usb_Copy(void)
{
	int time_out, copying_over;
	time_out = 3000;
	ui_date  = xPresent_Date_Time.date;
	ui_month = xPresent_Date_Time.month;
	ui_year  = xPresent_Date_Time.year;

	ui_send_lcd("Insert USB Disk ", "Please Wait...  ");
	vTaskDelay(1000 / portTICK_RATE_MS);

	copying_over = 0;
	while (!copying_over) {
		snprintf(ui_lcd_disp_buf, sizeof(ui_lcd_disp_buf), "   %02d-%02d-%04d   ", ui_date, ui_month, ui_year);
		ui_send_lcd(ui_lcd_disp_buf, "Copying...      ");

		XITC_Ui_Disk.date = ui_date;
		XITC_Ui_Disk.month = ui_month;
		XITC_Ui_Disk.year = ui_year;
		XITC_Ui_Disk.command = eITC_Ui_Disk_Copy;

		while (XITC_Ui_Disk.command != eITC_Ui_Disk_Nop) {
			vTaskDelay(10 / portTICK_RATE_MS);
			time_out--;
			if (time_out <= 0) break;
		}

		if (XITC_Ui_Disk.result == -1) {
			ui_send_lcd(ui_lcd_disp_buf, "Copying Failed  ");
		} else {
			ui_send_lcd(ui_lcd_disp_buf, "Copying OK      ");
		}

		vTaskDelay(300 / portTICK_RATE_MS);

		if ((ui_date == xPresent_Date_Time.date) &&
		    (ui_month == xPresent_Date_Time.month) &&
		    (ui_year == xPresent_Date_Time.year)) {
			copying_over = 1;
		}

		if (copying_over) break;
		Next_Date(&ui_date, &ui_month, &ui_year);
	}

	ui_send_lcd("USB Copy Done   ", "Power OFF & Pull");
	vTaskDelay(3000 / portTICK_RATE_MS);
}

/*-------------------------------------------------------------------------------------------------------------------*
* 	CALENDAR HELPER FUNCTIONS (Preserved for sensor.c, gprs.c and memory.c)
*---------------------------------------------------------------------------------------------------------------------*/
void Next_Date(int *Nd, int *Nm, int *Ny)
{
	int d = *Nd, m = *Nm, y = *Ny;
	int daysInMonth[] = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

	if ((y % 4 == 0 && y % 100 != 0) || (y % 400 == 0)) {
		daysInMonth[2] = 29;
	}

	d++;
	if (d > daysInMonth[m]) {
		d = 1;
		m++;
		if (m > 12) {
			m = 1;
			y++;
		}
	}

	*Nd = d;
	*Nm = m;
	*Ny = y;
}

void Previous_Date(int *Cd, int *Cm, int *Cy)
{
	int d = *Cd, m = *Cm, y = *Cy;
	int daysInMonth[] = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

	if ((y % 4 == 0 && y % 100 != 0) || (y % 400 == 0)) {
		daysInMonth[2] = 29;
	}

	d--;
	if (d < 1) {
		m--;
		if (m < 1) {
			m = 12;
			y--;
		}
		d = daysInMonth[m];
	}

	*Cd = d;
	*Cm = m;
	*Cy = y;
}
