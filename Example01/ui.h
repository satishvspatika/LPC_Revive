#ifndef UI_H_
#define UI_H_

#include "user_config.h"

#if (UNIT_CFG == CFG_KSNDMC_TWS)
#define WELCOME_STR1 "   VARSHA-TWS   "
#elif (UNIT_CFG == CFG_BIHAR_TRG)
#define WELCOME_STR1 "   VARSHA-TRG   "
#else
#define WELCOME_STR1 "   VARSHA-TRG   "
#endif
#define WELCOME_STR2 "                "
#define DATETIME_STR "  Date & Time   "
#define COPY_USB_STR "  Copy To USB ?  "

#define MAX_UI_PARAMS 20

typedef struct {
	int signal;
	int command;
	int result;
	char data[50];
} XITC_Ui_Gprs_def;

typedef struct{
	volatile int sig;
	int lcd_cmd;
	int lcd_posn;
	char disp_str[120];
} XITC_Ui_Lcd_def;

typedef struct {
	int command;		// copy or get command
	int date;
	int month;
	int year;
	int hour;
	int minute;
	int result;
	char read_buf[150];
} XITC_Ui_Disk_def;

struct UI_STEP_DEFN {
	int ikey;
	void *ptr2function;
	unsigned char uclcd_cmd;
	unsigned char uclcd_addr;
	char *ptr2_lcd_display_str;
	int iNext_State;
	int iNext_Step;
	int iTc;
};

enum Ui_Disk_Cmd {
	eITC_Ui_Disk_Nop,
	eITC_Ui_Disk_Get,
	eITC_Ui_Disk_Copy,
	eITC_Ui_Load_Params,
	eITC_Ui_Disk_Save_Params,
	eITC_Ui_Copy_Firmware,
	eITC_Ui_Copy_Firmware1
};

enum Ui_Gprs_Cmd_Stats{
	eUi_Gprs_Get_Sglvl,
	eUi_Gprs_Sglvl_Done,
	eUi_Gprs_Get_CPIN,
	eUi_Gprs_CPIN_Done,
	eUi_Gprs_Cmd_In_Progress,
	eUi_Gprs_Cmd_Fail,
	eUi_Gprs_Send_Status,
	eUi_Gprs_Send_Status1,
	eUI_Gps_Info,
	eUI_Gps_Info_A,
	eUI_Gps_Info_NA,
	eUi_Gprs_Send_Status_Done,
	eUi_Gprs_Get_CREG
};

/* Carousel Screen IDs - Circular Navigation Centered on Station ID */
enum UI_SCREEN_ID {
	/* Central Home Screen */
	SCR_STATION = 0,    /* VARSHA-TWS & Station ID (Landing Screen) */

	/* Forward Branch (Press DOWN / RIGHT from Station ID: Sensors & Telemetry) */
	SCR_DATE_TIME,      /* 1: RTC Date & Time */
	SCR_RAIN,           /* 2: Rainfall Today */
	SCR_TEMP_HUM,       /* 3: Temperature & Humidity */
	SCR_WIND,           /* 4: Wind Speed Inst & Avg */
	SCR_WIND_DIR,       /* 5: Wind Direction */
	SCR_BATTERY,        /* 6: Battery 1 & Battery 2 Voltages */
	SCR_SOLAR,          /* 7: Solar Charging Status */

	/* Deep Middle: Storage & Infrequent Maintenance (Far from Station ID) */
	SCR_SDC_STATUS,         /* 8: MicroSD Storage Card Health */
	SCR_ACT_USB_COPY,       /* 9: Export Logs to USB Flash Drive (Action) */
	SCR_ACT_MAIN_FW_UPDATE, /* 10: Update Main Board Firmware (Action) */
	SCR_ACT_SENS_FW_UPDATE, /* 11: Update Sensor Board Firmware (Action) */
	SCR_ACT_WIPE_DATA,      /* 12: Wipe All Historical Data & Reset (Action) */
	SCR_ACT_SLEEP,          /* 13: Cut Off Power / Sleep Now (Action) */

	/* Backward Branch (Press UP / LEFT from Station ID: Critical Diagnostics & Actions) */
	SCR_ERRORS,         /* 14: Packet & Network Error Counters */
	SCR_VERSION,        /* 15: Firmware Version */
	SCR_SIM_SIGNAL,     /* 16: Combined SIM (Airtel/BSNL) + Signal (dBm & CSQ) & Reg */
	SCR_LAST_LOG,       /* 17: Last Transmitted Log Timestamp */
	SCR_ACT_SEND_HTTP,  /* 18: Action: Transmit Live & Backlog HTTP Telemetry */
	SCR_GPS_LOC,        /* 19: Single Unified GPS Location Screen */
	SCR_ACT_SEND_SMS,   /* 20: Send Test Status SMS (Action) */

	SCR_TOTAL_SCREENS   /* Total carousel count = 21 */
};

/* Special Operating Modes */
enum UI_MODE {
	UI_MODE_BROWSE = 0,
	UI_MODE_CALIB,
	UI_MODE_EDIT_STN,
	UI_MODE_EDIT_DATETIME,
	UI_MODE_ACTION,
	UI_MODE_FW_CONFIRM,
	UI_MODE_WIPE_CONFIRM,
	UI_MODE_SLEEP_CONFIRM
};

enum FW_TARGET {
	FW_TARGET_NONE = 0,
	FW_TARGET_MAIN,
	FW_TARGET_SENSOR
};

enum FW_SOURCE {
	FW_SRC_NONE = 0,
	FW_SRC_USB,
	FW_SRC_SD
};

enum FW_RESULT {
	eFw_Not_Found = 0,
	eFw_Found_USB = 1,
	eFw_Found_SD  = 2
};

enum UI_STATE{
	eUi_Idle,
	eUi_Welcome,
	eUi_Date_Time,
	eUi_SetDate,
	eUi_Params,
	eUi_History,
	eUi_Hi_Set_Date,
	eUi_USB_Copy,
	eUi_USB_SetDate,
	eUi_Show_Params,
	eUi_Send_SMS,
	eUi_Load_Params,
	eUi_Copy_Firmware,
	eUi_Stn_ID,
	eUi_Set_Stn_ID,
	eUi_Get_GPS,
	eUi_Calibrate,
	UI_RET
};

enum {
	eLcd_Normal, eLcd_Shifting, eLcd_Frozen
};

enum Calib_States {
	eCalib_Idle, eCalib_Rain, eCalib_Temp_Hum,
	eCalib_Wind_Speed, eCalib_Wind_Dir, eCalib_Qei,
	eCalib_Depth, eCalib_Done
};

enum UI_F968UART { eReset_Rf_Ui, eAdd_Rf_Calib, eBat_Charge, eBat_Cutoff };

extern XITC_Ui_Gprs_def XITC_Ui_Gprs;
extern XITC_Ui_Disk_def XITC_Ui_Disk;
extern XITC_Ui_Lcd_def XITC_Ui_Lcd;

extern char ui_lcd_disp_buf[50], ui_lcd_disp_buf2[32];

void vUi_Task(void *pvParameters);
void Next_Date(int *Nd, int *Nm, int *Ny);
void Previous_Date(int *Cd, int *Cm, int *Cy);
void Ui_Usb_Copy(void);
void system_enter_sleep(void);

#endif /* UI_H_ */
