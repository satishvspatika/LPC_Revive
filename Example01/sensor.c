#include <stdio.h>
#include <string.h>
#include "sensor.h"
#include "globals.h"
#include "ui.h"
#include "lpc17xx_uart.h"
#include "lpc17xx_pinsel.h"
#include "lpc17xx_libcfg_default.h"
#include "FreeRTOS.h"
#include "task.h"

char sensor_rx_data[RBUF_SIZE];
char sensor_tx_data[TBUF_SIZE];

//char sensor_loc_buf[RBUF_SIZE];

char *sensor_xmit_ptr;


int sensor_uart_in_count;
int sensor_recv_state;

char sensor_loc_buf[100];

void define_sensor_uart_pins(LPC_UART_TypeDef* sensor_port);
void init_sensor_uart(LPC_UART_TypeDef* sensor_port);

enum {WAITING_FOR_STX, RECEIVING_CMD, CMD_RECVD};


void UART3_IRQHandler(void)
{
	UART3_StdIntHandler();
}


void define_sensor_uart_pins(LPC_UART_TypeDef* sensor_port)
{
	PINSEL_CFG_Type sensor_config;

	sensor_config.OpenDrain = 0;
	sensor_config.Pinmode = 0;
	sensor_config.Funcnum = 3;
	sensor_config.Portnum = 4;
	sensor_config.Pinnum = SENSOR_TX_PIN;
	PINSEL_ConfigPin(&sensor_config);
	sensor_config.Pinnum = SENSOR_RX_PIN;
	PINSEL_ConfigPin(&sensor_config);

}

void init_sensor_uart(LPC_UART_TypeDef* sensor_port)
{
	UART_CFG_Type UART_InitStruct;
	UART_FIFO_CFG_Type UART_FIFOInitStruct;
	UART_ConfigStructInit(&UART_InitStruct);
	UART_InitStruct.Baud_rate = 9600;
	UART_Init(sensor_port, &UART_InitStruct);
	UART_SetupCbs(sensor_port, 0, &sensor_recv_isr);
	UART_SetupCbs(sensor_port, 1, &sensor_xmit_isr);
	UART_FIFOConfigStructInit(&UART_FIFOInitStruct);
	UART_FIFOConfig(sensor_port, &UART_FIFOInitStruct);
	UART_IntConfig(sensor_port, UART_INTCFG_RBR, ENABLE);
	UART_IntConfig(sensor_port, UART_INTCFG_THRE, ENABLE);
	UART_TxCmd(sensor_port,ENABLE);
	NVIC_EnableIRQ(UART3_IRQn);
}



void sensor_xmit_isr(void)
{
	if(*sensor_xmit_ptr){
		UART_SendData((LPC_UART_TypeDef*) SENSOR_PORT, *sensor_xmit_ptr);
		sensor_xmit_ptr++;
	}
}


void sensor_send_cmd_direct(const char *cmd)
{
	if (!sensor_task_started)
		return;

	/* Disable THRE interrupt temporarily so ISR does not collide */
	LPC_UART3->IER &= ~(1 << 1);

	while (*cmd) {
		int timeout = 10000;
		while (!(LPC_UART3->LSR & (1 << 5)) && --timeout > 0); /* Wait for THRE with safety timeout */
		LPC_UART3->THR = (uint8_t)*cmd++;
	}
	int timeout = 10000;
	while (!(LPC_UART3->LSR & (1 << 6)) && --timeout > 0);     /* Wait for TEMT with safety timeout */

	/* Re-enable THRE interrupt */
	LPC_UART3->IER |= (1 << 1);
}

void sensor_recv_isr(void)
{
	char c;
	c = UART_ReceiveData((LPC_UART_TypeDef*) SENSOR_PORT);

	switch(sensor_recv_state){
	case WAITING_FOR_STX:
		if(c == '$'){
			sensor_rx_data[0] = '$';
			sensor_uart_in_count = 1;
			sensor_recv_state = RECEIVING_CMD;
		}
		break;

	case RECEIVING_CMD:
		if(c == '$')
			sensor_uart_in_count = 1;
		else{
			sensor_rx_data[sensor_uart_in_count++] = c;
		}
		if(c == ';') {
			sensor_rx_data[sensor_uart_in_count] = '\0';
			sensor_recv_state = CMD_RECVD;
		}
		if(sensor_uart_in_count >= RBUF_SIZE)
			sensor_recv_state = WAITING_FOR_STX;
		break;

	case CMD_RECVD:
		break;

	default:
		break;

	}
}



void check_last_rec_date_time(void)
{
	int n_dd, n_mm, n_yy;
	int sector;

	n_dd = latest_date;
	n_mm = latest_month;
	n_yy = latest_year;

	Next_Date(&n_dd, &n_mm, &n_yy);


	if((rf_close_date == latest_date) && (rf_close_month == latest_month) &&
			(rf_close_year == latest_year)){
		gi_recv_pkt_ready = 1;				//proceed to memory task
	}

	else if((rf_close_date == n_dd) && (rf_close_month == n_mm) &&
			(rf_close_year == n_yy)){
		gi_recv_pkt_ready = 1;			//proceed to memory task
	}

	else{
		gi_recv_pkt_ready = 0;

		if(auto_time_set == 0){
			get_gps_time = 1;
			get_gprs_time = 1;			//get date and time using gps or gprs and update this time to LPC845 RTC
		}
	}

}


void find_latest_date_from_data_sectors(void){

	int sector, count;
	int latest_date_val, oldest_date_val, comparing_date_val;


	count = 0;
	latest_date = 1;
	latest_month = 1;
	latest_year = 2021;

	oldest_date = 1;
	oldest_month = 1;
	oldest_year = 2099;

	latest_date_val = latest_year*256*256 + latest_month*256 + latest_date;
	oldest_date_val = oldest_year*256*256 + oldest_month*256 + oldest_date;

	//scanning all data sectors
	for(sector=2;sector<4; sector++){

		if(days_sector[sector-2]->year != 0xFFFF){
			comparing_date_val = days_sector[sector-2]->year*256*256 + days_sector[sector-2]->month*256 + days_sector[sector-2]->date;

			if(comparing_date_val > latest_date_val){
				latest_date = days_sector[sector-2]->date;
				latest_month = days_sector[sector-2]->month;
				latest_year = days_sector[sector-2]->year;

				latest_date_val = latest_year*256*256 + latest_month*256 + latest_date;
				latest_Sector = sector;
				// hr*4 + min/15;
				latest_sample_num = days_sector[sector-2]->last_rec_hr * 4 + days_sector[sector-2]->last_rec_min /15;
				latest_sample_num = (latest_sample_num + 61) % 96 ;

			}

			if(comparing_date_val < oldest_date_val){
				oldest_date =  days_sector[sector-2]->date;
				oldest_month = days_sector[sector-2]->month;
				oldest_year = days_sector[sector-2]->year;

				oldest_date_val = latest_year*256*256 + latest_month*256 + latest_date;
				oldest_Sector = sector;
			}
		}
		else
			count++;

	}
	//if(count >= 2 )
	if(count >= 2 )
		data_sectors_not_allocated = 1;
}


void decode_command(void)
{
	int chr1,chr2;
	int date, month, year, hour, minute;
	int gi_rec_1min_rf;
	int gi_rec_15min_rf,gi_rec_bat1_adc,gi_rec_bat2_adc;
	xDateTime_def pkt_time;
	char local_rx_buf[256];

	char data_rec;

	if (sensor_recv_state == CMD_RECVD){
		taskENTER_CRITICAL();
		strncpy(local_rx_buf, sensor_rx_data, sizeof(local_rx_buf) - 1);
		local_rx_buf[sizeof(local_rx_buf) - 1] = '\0';
		sensor_recv_state = WAITING_FOR_STX;
		taskEXIT_CRITICAL();

		chr1 = local_rx_buf[1];
		chr2 = local_rx_buf[2];

		switch(chr1){

		case 'D':
			{
				int t_rf = 0, t_bat = 0, t_ltemp = 0, t_lhum = 0, t_lws = 0, t_lwd = 0, t_iws = 0, t_lws_sec = 0;
				int t_ptemp = 0, t_phum = 0, t_pws = 0, t_pwd = 0, t_pws_sec = 0;
				int t_sc_bat1 = 0, t_sc_bat2 = 0;
				char c_solar = 'N';

				/* Full 31-field decoding matching LPC845 $D telemetry transmission:
				 * $D<14-digit present time>,<14-digit slot time>,<rf>,<bat1>,<ltemp>,<lhum>,<lws>,<lwd>,<iws>,<lws_sec>,<ptemp>,<phum>,<pws>,<pwd>,<pws_sec>,<gprs_pwr>,<usb_pwr>,<ver>,<sc_bat1>,<sc_bat2>,<solar_status>; */
				int n_fields = sscanf(local_rx_buf, "$D%04d%02d%02d%02d%02d%02d,%04d%02d%02d%02d%02d%02d,%04d,%04d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%c,%c,%c,%04d,%04d,%c;",
						&pkt_time.year,
						&pkt_time.month,
						&pkt_time.date,
						&pkt_time.hour,
						&pkt_time.minute,
						&pkt_time.second,
						&xLast_Rec_15min_Date_Time.year,
						&xLast_Rec_15min_Date_Time.month,
						&xLast_Rec_15min_Date_Time.date,
						&xLast_Rec_15min_Date_Time.hour,
						&xLast_Rec_15min_Date_Time.minute,
						&xLast_Rec_15min_Date_Time.second,
						&t_rf,
						&t_bat,
						&t_ltemp,
						&t_lhum,
						&t_lws,
						&t_lwd,
						&t_iws,
						&t_lws_sec,
						&t_ptemp,
						&t_phum,
						&t_pws,
						&t_pwd,
						&t_pws_sec,
						&gprs_pwr,
						&usb_lcd_pwr,
						&lpc845_ver,
						&t_sc_bat1,
						&t_sc_bat2,
						&c_solar);

				if (n_fields >= 26) {
					last_rec_15min_rf     = (uint16_t)t_rf;
					temp_bat_adc          = (uint16_t)t_bat;
					last_rec_15min_bat1   = (uint16_t)t_bat;
					last_rec_temperature  = (uint16_t)t_ltemp;
					last_rec_humid        = (uint16_t)t_lhum;
					last_rec_ws           = (uint16_t)t_lws;
					last_rec_wd           = (uint16_t)t_lwd;
					inst_ws               = (uint16_t)t_iws;
					last_rec_ws_sec       = (uint16_t)t_lws_sec;
					present_temperature   = (uint16_t)t_ptemp;
					present_humid         = (uint16_t)t_phum;
					present_ws            = (uint16_t)t_pws;
					present_wd            = (uint16_t)t_pwd;
					present_ws_sec_cnt    = (uint16_t)t_pws_sec;
				}
				if (n_fields >= 29 && t_sc_bat1 > 0) {
					last_rec_15min_bat1   = (uint16_t)t_sc_bat1;
				}
				if (n_fields >= 30 && t_sc_bat2 > 0) {
					last_rec_15min_bat2   = (uint16_t)t_sc_bat2;
				}
				if (n_fields >= 31) {
					solar_status          = c_solar;
				}
			}

			/* Normalize 2-digit years (e.g. 26 -> 2026) */
			if (pkt_time.year < 100 && pkt_time.year > 0) {
				pkt_time.year += 2000;
			}
			if (xLast_Rec_15min_Date_Time.year < 100 && xLast_Rec_15min_Date_Time.year > 0) {
				xLast_Rec_15min_Date_Time.year += 2000;
			}

			/* Clock management:
			 * If LPC845 RTC is actively advancing, sync to it.
			 * If LPC845 RTC is frozen (sending the same static second/minute repeatedly),
			 * do not overwrite LPC1768's advancing clock.
			 */
			static int last_lpc845_sec = -1;
			static int last_lpc845_min = -1;
			if (pkt_time.year >= 2020 && pkt_time.year <= 2100 &&
			    pkt_time.month >= 1 && pkt_time.month <= 12 &&
			    pkt_time.date >= 1 && pkt_time.date <= 31) {

				if (xPresent_Date_Time.year < 2020) {
					xPresent_Date_Time = pkt_time;
					last_lpc845_sec = pkt_time.second;
					last_lpc845_min = pkt_time.minute;
				} else if (pkt_time.second != last_lpc845_sec || pkt_time.minute != last_lpc845_min) {
					xPresent_Date_Time = pkt_time;
					last_lpc845_sec = pkt_time.second;
					last_lpc845_min = pkt_time.minute;
				}
			}

			gf_bat_volt1 = ((((float)(last_rec_15min_bat1) * 3.3/4095)/0.128));
			gf_bat_volt2 = ((((float)(last_rec_15min_bat2) * 3.3/4095)/0.128));

			if((gf_bat_volt1 < 2) && (gf_bat_volt2 < 2)){
				gf_bat_volt1 = ((((float)(temp_bat_adc) * 3.3/4095)/0.138));
				last_rec_15min_bat1 = temp_bat_adc;
			}

			Global_WS_Data.temperature = ((float)present_temperature/65536) * 165 - 40;
			Global_WS_Data.humidity = ((float)present_humid/65534) * 100;
			Global_WS_Data.wind_dir = ((int)(present_wd * 0.088)) % 360;		// 360/4095 = 0.088
			if (present_ws_sec_cnt > 0) {
				Global_WS_Data.avg_ws = ((float)(present_ws / 4)) / present_ws_sec_cnt;
			} else {
				Global_WS_Data.avg_ws = 0.0f;
			}

			Global_WS_Data.avg_ws = (Global_WS_Data.avg_ws * ws_calib_factor ) + ws_offset;
			Global_WS_Data.wind_dir = ((int)Global_WS_Data.wind_dir * wd_calib_factor) + wd_offset;
			Global_WS_Data.temperature = (Global_WS_Data.temperature * temp_calib_factor) + temp_offset;
			Global_WS_Data.humidity = (Global_WS_Data.humidity * humid_calib_factor) + humid_offset;
			Global_WS_Data.inst_ws = ((float)inst_ws) / 4;

			/* Only queue 15-minute record storage to memory task if previous packet was processed */
			if(gi_recv_pkt_ready == 0){
				if((xPresent_Date_Time.year >= 2020) && (xPresent_Date_Time.year <= 2100) &&
						(xLast_Rec_15min_Date_Time.year >= 2020) && (xLast_Rec_15min_Date_Time.year <= 2100) &&
						(xPresent_Date_Time.month > 0) && (xPresent_Date_Time.month <= 12) &&
						(xLast_Rec_15min_Date_Time.month > 0) && (xLast_Rec_15min_Date_Time.month <= 12) &&
						(xPresent_Date_Time.date > 0) && (xPresent_Date_Time.date <= 31) &&
						(xLast_Rec_15min_Date_Time.date > 0) && (xLast_Rec_15min_Date_Time.date <= 31)){

					sampleNo = xLast_Rec_15min_Date_Time.hour * 4 +  xLast_Rec_15min_Date_Time.minute /15;
					sampleNo = (sampleNo + 61) % 96 ;

					rf_close_date = xLast_Rec_15min_Date_Time.date;
					rf_close_month = xLast_Rec_15min_Date_Time.month;
					rf_close_year = xLast_Rec_15min_Date_Time.year;

					if(sampleNo <= 60){
						Next_Date(&rf_close_date, &rf_close_month, &rf_close_year);
					}

					gi_recv_pkt_ready = 1;
				}
			}
			vTaskDelay(1 / portTICK_RATE_MS);
			break;


		case 'C':
			{
				int c_rf = 0, c_b1 = 0, c_b2 = 0;
				sscanf(sensor_rx_data, "$C%04d%02d%02d%02d%02d%02d,%04d,%04d,%04d;",
						&xPresent_Date_Time.year,
						&xPresent_Date_Time.month,
						&xPresent_Date_Time.date,
						&xPresent_Date_Time.hour,
						&xPresent_Date_Time.minute,
						&xPresent_Date_Time.second,
						&c_rf,
						&c_b1,
						&c_b2);
				calib_rf = c_rf;
				last_rec_15min_bat1 = (uint16_t)c_b1;
				last_rec_15min_bat2 = (uint16_t)c_b2;
			}
			break;


		case 'G':



			if(chr2 == 'Y'){
				sscanf(sensor_rx_data, "$G%c,%04d%02d%02d%02d%02d%02d,%04d%02d%02d%02d%02d%02d,%04d,%04d,%04d,%04d;",
						&data_rec,
						&xPresent_Date_Time.year,
						&xPresent_Date_Time.month,
						&xPresent_Date_Time.date,
						&xPresent_Date_Time.hour,
						&xPresent_Date_Time.minute,
						&xPresent_Date_Time.second,
						&xGet_15min_Date_Time.year,
						&xGet_15min_Date_Time.month,
						&xGet_15min_Date_Time.date,
						&xGet_15min_Date_Time.hour,
						&xGet_15min_Date_Time.minute,
						&xGet_15min_Date_Time.second,
						&gi_rec_1min_rf,
						&gi_rec_15min_rf,
						&gi_rec_bat1_adc,
						&gi_rec_bat2_adc);
				vTaskDelay(10 / portTICK_RATE_MS);

			}

			else if(chr2 == 'N'){
				sscanf(sensor_rx_data, "$G%c,%04d%02d%02d%02d%02d%02d,%04d%02d%02d%02d%02d%02d;",
						&data_rec,
						&xPresent_Date_Time.year,
						&xPresent_Date_Time.month,
						&xPresent_Date_Time.date,
						&xPresent_Date_Time.hour,
						&xPresent_Date_Time.minute,
						&xPresent_Date_Time.second,
						&xGet_15min_Date_Time.year,
						&xGet_15min_Date_Time.month,
						&xGet_15min_Date_Time.date,
						&xGet_15min_Date_Time.hour,
						&xGet_15min_Date_Time.minute,
						&xGet_15min_Date_Time.second);
				vTaskDelay(10 / portTICK_RATE_MS);

			}

			break;
		default:
			break;
		}
		sensor_recv_state = WAITING_FOR_STX;
	}
}

void vSensor_Task(void *pvParameters)
{
	char *match_ptr;
	int last_rec_min;

	int get_rec;

	log_interval_ovf = 0;
	report_interval_ovf = 0;
	last_rec_min = -1;
	gi_recv_pkt_ready = 0;

	shutdown_pwr = 0;
	set_time_to_rtc = 0;
	calib_on = 0;
	get_rec = 0;

	get_gprs_time = 0;
	get_gps_time = 0;
	data_sectors_not_allocated = 0;
	auto_time_set = 0;

	// startup delay
	vTaskDelay(100 / portTICK_RATE_MS);
	// first pins of gps-uart
	define_sensor_uart_pins((LPC_UART_TypeDef*) SENSOR_PORT);
	vTaskDelay(50 / portTICK_RATE_MS);
	// next registers of gps-uart
	init_sensor_uart((LPC_UART_TypeDef*) SENSOR_PORT);
	sensor_task_started = 1;
	vTaskDelay(50 / portTICK_RATE_MS);

	sensor_recv_state = WAITING_FOR_STX;
	sensor_uart_in_count = 0;

	find_latest_date_from_data_sectors();
	vTaskDelay(50 / portTICK_RATE_MS);

	last_rec_15min_bat1 = 0;
	last_rec_15min_bat2 = 0;
	solar_status = 'N';

	new_sect_no = 0;
	get_gps_time = 0;
	get_gprs_time = 0;
	//get_rec = 1;

	for(;;){

		taskENTER_CRITICAL();
		gi_wdt_task_ok_stat |= WDT_SENSOR_OK;
		taskEXIT_CRITICAL();

		if(sensor_recv_state == CMD_RECVD){
			decode_command();
		}

		vTaskDelay(100 / portTICK_RATE_MS);


		// shutdown power takes highest priority
		if(shutdown_pwr == 1){
			sensor_send_cmd_direct("$f7;\012");
			vTaskDelay(500 / portTICK_RATE_MS);
			usb_lcd_pwr = 'u';
		}
		else if (usb_lcd_pwr != 'U') {
			/* Ensure 5V rail is active for sensors and HD44780 controller */
			sensor_send_cmd_direct("$n4;\012");
			vTaskDelay(200 / portTICK_RATE_MS);
			usb_lcd_pwr = 'U';
		}

		if(get_rec == 1){
			sprintf(sensor_loc_buf, "$G20210204030000;");
			sensor_xmit_ptr = sensor_loc_buf;
			UART_SendData((LPC_UART_TypeDef*) SENSOR_PORT, *sensor_xmit_ptr++);
			vTaskDelay(500 / portTICK_RATE_MS);

			vTaskDelay(3000 / portTICK_RATE_MS);
			get_rec = 0;
		}


		//Set time to LPC845 RTC
		if(set_time_to_rtc == 1){
			sprintf(sensor_loc_buf, "$S%04d%02d%02d%02d%02d%02d;",
					xSet_Date_Time.year,
					xSet_Date_Time.month,
					xSet_Date_Time.date,
					xSet_Date_Time.hour,
					xSet_Date_Time.minute,
					xSet_Date_Time.second);

			sensor_xmit_ptr = sensor_loc_buf;
			UART_SendData((LPC_UART_TypeDef*) SENSOR_PORT, *sensor_xmit_ptr++);
			vTaskDelay(500 / portTICK_RATE_MS);

			set_time_to_rtc = 0;

			vTaskDelay(3000 / portTICK_RATE_MS);

			data_sectors_not_allocated = 1;

		}

		//Set Calibration
		if(calib_on == 1){
			sprintf(sensor_loc_buf, "$C;");

			sensor_xmit_ptr = sensor_loc_buf;
			UART_SendData((LPC_UART_TypeDef*) SENSOR_PORT, *sensor_xmit_ptr++);
			vTaskDelay(500 / portTICK_RATE_MS);
			calib_on = 0;
		}

		//Clear Calibration
		if(calib_on == 2){
			sprintf(sensor_loc_buf, "$c;");

			sensor_xmit_ptr = sensor_loc_buf;
			UART_SendData((LPC_UART_TypeDef*) SENSOR_PORT, *sensor_xmit_ptr++);
			vTaskDelay(500 / portTICK_RATE_MS);

			calib_on = 0;

		}
	}
}
