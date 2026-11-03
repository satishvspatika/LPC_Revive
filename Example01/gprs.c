
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "globals.h"
#include "gprs.h"
#include "lpc17xx_gpio.h"
#include "lpc17xx_uart.h"
#include "lpc17xx_pinsel.h"
#include "lpc17xx_libcfg_default.h"
#include "FreeRTOS.h"
#include "task.h"
#include "ui.h"
#include <cr_section_macros.h>
#include "memory.h"
#include "disk.h"


#define MAX_NO_OF_MSGS 25
//#define RECVBUF_SIZE  16384
#define RECVBUF_SIZE  12288

char *gprs_recv_ptr;
char *gprs_xmit_ptr;
char *matching_ptr;
char *ptr_RAM2;

int packet_recved;
//char recv_str[700];
int char_count;

int gprs_recv_in_cnt, gprs_recv_out_cnt;
int end_of_text_recd;
int gprs_timeout_occurred;
int process_msgs,gprs_status;

char gprs_xmit_buf[300];
char gprs_response[300];
char status_response[256];
char message_recd[MAX_NO_OF_MSGS];
int response_no;

int gprs_new_date, gprs_new_month, gprs_new_year, gprs_new_hour, gprs_new_minute;
int gprs_server_date_Ok;

int ftp_mode;

char msg_rcvd_number[16];


enum {eMatch_E, eMatch_R1, eMatch_R2, eMatch_O, eMatch_R3};


void define_gprs_uart_pins(LPC_UART_TypeDef* gprs_port);
void init_gprs_uart(LPC_UART_TypeDef* gprs_port);


const char SIMXFERRES[]=">";
int ftp_start_line_no;
int ftp_line_no;

///////////////////////////////////////////////////////////////////
void power_on_sim_module(void){

	GPIO_ClearValue(PWR_KEY_PORT, PWR_KEY_MASK);
	vTaskDelay(500 / portTICK_RATE_MS);
	GPIO_SetValue(PWR_KEY_PORT, PWR_KEY_MASK);
	vTaskDelay(500 / portTICK_RATE_MS);

}
///////////////////////////////////////////////////////////////////



///////////////////////////////////////////////////////////////////
void define_gprs_uart_pins(LPC_UART_TypeDef* gprs_port)
{

	PINSEL_CFG_Type gprs_config;
	// tx and rx pins
	gprs_config.OpenDrain = 0;
	gprs_config.Pinmode = 0;
	gprs_config.Funcnum = 1;
	gprs_config.Portnum = GPRS_PORT_NO;
	gprs_config.Pinnum = GPRS_TX_PIN;
	PINSEL_ConfigPin(&gprs_config);
	gprs_config.Portnum = GPRS_PORT_NO;
	gprs_config.Pinnum = GPRS_RX_PIN;
	PINSEL_ConfigPin(&gprs_config);

	gprs_config.Funcnum = 0;
	gprs_config.Portnum = PWR_KEY_PORT;
	gprs_config.Pinnum = PWR_KEY_PIN;
	gprs_config.Pinmode = PINSEL_PINMODE_PULLUP;
	PINSEL_ConfigPin(&gprs_config);

	GPIO_SetDir(PWR_KEY_PORT, PWR_KEY_MASK, DIR_OUT);
	vTaskDelay(100 / portTICK_RATE_MS);

	power_on_sim_module();
	vTaskDelay(100 / portTICK_RATE_MS);

	gprs_config.Funcnum = 0;
	gprs_config.Portnum = STATUS_KEY_PORT;
	gprs_config.Pinnum = STATUS_KEY_PIN;
	gprs_config.Pinmode = PINSEL_PINMODE_PULLUP;
	PINSEL_ConfigPin(&gprs_config);

	GPIO_SetDir(STATUS_KEY_PORT, STATUS_KEY_MASK, DIR_OUT);
	vTaskDelay(100 / portTICK_RATE_MS);

	//	GPIO_ClearValue(STATUS_KEY_PORT, STATUS_KEY_MASK);
	vTaskDelay(100 / portTICK_RATE_MS);

}
///////////////////////////////////////////////////////////////////


void prepare_and_send_status(char *gsm_no){
	//SPATIKA,TRG,STATUS-C,000511,2017-11-25T12:15,TRG4-3.0.1-1.1,115,-110,12.56,SDC-FAIL,CLB-FAIL,2017-11-25,99999,2017-11-28T13:00
	sprintf(status_response,"%s,%s,STATUS-C,%s,%04d-%02d-%02dT%02d:%02d,%s-%c,%d,%04d,%.2f,%.2f,%c,%s,%04d-%02d-%02dT%02d:%02d,%d\r\n\032",
			Global_Data_Params.gc_network_name,
			Global_Data_Params.gc_station_type,
			Global_Data_Params.gc_station_id,
			xPresent_Date_Time.year,
			xPresent_Date_Time.month,
			xPresent_Date_Time.date,
			xPresent_Date_Time.hour,
			xPresent_Date_Time.minute,
			Global_Data_Params.gc_version,
			lpc845_ver,
			Global_Data_Params.gi_report_interval,
			gi_signal_level,
			gf_bat_volt1,
			gf_bat_volt2,
			solar_status,
			(gi_disk_ok ? "SDC-OK" : "SDC-FAIL"),
			gi_log_year,
			gi_log_month,
			gi_log_date,
			gi_log_hour,
			gi_log_minute,
			gi_present_error
	);
	send_message(msg_rcvd_number, status_response);
}
///////////////////////////////////////////////////////////////////


void save_err_det(void)
{
	XITC_Gprs_Disk.command = eSave_Params;
	XITC_Gprs_Disk.signal = 1;
	while (XITC_Gprs_Disk.signal)
		vTaskDelay(100 / portTICK_RATE_MS);
}

///////////////////////////////////////////////////////////////////


void process_msg(int msg_no)
{
	int msg_char_count, timeout, msg_recd, response_no;
	int m_date, m_month, m_year, m_hour, m_minute;
	char msg_cmd[16], msg_data[256], *matching_ptr;
	char msg_stat[16];
	xDateTime_def xDateTime_Gprs_Sms;
	int itemp1, itemp2, itemp3, itemp4;
	float ftemp, ftemp1, ftemp2, ftemp3, ftemp4, ftemp5;
	char ctemp1[30], ctemp2[30], ctemp3[30], ctemp4[30];
	char msg_response[256];
	char temp[20],temp1[20];
	char send_cmd[32];
	int response_numb, iCount;
	float mbat_volt1;
	int msect_no,mfound,mrec_no;
	mfound = 0;

	response_no	=  Send_And_Check_Gprs("\r\nAT+CMGF=1\r\n", "OK", "ERROR", 5000);
	vTaskDelay(5 / portTICK_RATE_MS);

	// first let us read
	sprintf(msg_cmd, "AT+CMGR=%d\r\n", msg_no);
	gprs_xmit_ptr = msg_cmd;
	UART_SendData((LPC_UART_TypeDef*) GPRS_UART, *gprs_xmit_ptr++);
	vTaskDelay(5 / portTICK_RATE_MS);	// wait a while for transmission to take place
	msg_char_count = 0;
	timeout = 30000/10;
	msg_recd = 0;
	msg_data[0] = 0;

	while((!msg_recd) && (timeout)){
		while( gprs_recv_in_cnt != gprs_recv_out_cnt){
			//			msg_data[msg_char_count] = gprs_recv_buf[gprs_recv_out_cnt];
			msg_data[msg_char_count] = *((char *)(0x2007c000 + gprs_recv_out_cnt));
			msg_data[msg_char_count+1] = 0;
			gprs_recv_out_cnt = (gprs_recv_out_cnt+1)%(RECVBUF_SIZE); //(sizeof(gprs_recv_buf)
			msg_char_count++;
			if(msg_char_count > 248)
				break;
		}
		if(timeout){
			timeout--;
			vTaskDelay(10 / portTICK_RATE_MS);
		}
		if((matching_ptr = look_for_str(msg_data, "OK")) != 0 )
			msg_recd = 1;
		if((matching_ptr = look_for_str(msg_data, "ERROR")) != 0 )
			msg_recd = -1;
	}

	if((!timeout) || (msg_recd == -1))
		return;
	if(msg_recd == 1){
		if ( ((matching_ptr = look_for_str(msg_data, "REC READ")) == 0)
				&& ((matching_ptr = look_for_str(msg_data, "REC UNREAD")) != 0)
				&& ((matching_ptr = look_for_str(msg_data, "VARSHA")) != 0)
				&& (((matching_ptr = look_for_str(msg_data, Global_Data_Params.gc_master_gsm)) != 0)
						|| ((matching_ptr = look_for_str(msg_data, Global_Data_Params.gc_universal_gsm)) != 0)
						|| ((matching_ptr = look_for_str(msg_data, Global_Data_Params.gc_secondary_gsm)) != 0)
						|| ((matching_ptr = look_for_str(msg_data, Global_Data_Params.gc_secondary_gsm1)) != 0)
						|| ((matching_ptr = look_for_str(msg_data, Global_Data_Params.gc_secondary_gsm2)) != 0)
						|| ((matching_ptr = look_for_str(msg_data, Global_Data_Params.gc_secondary_gsm3)) != 0))){

			//sscanf(msg_data, "%[^:]: \"%[^\"]\",\"%[^\"]\"" , &temp, &temp1, &msg_rcvd_number);
			sscanf(msg_data, "%[^:]: \"%[^\"]\",\"+91%[^\"]\"" , &temp, &temp1, &msg_rcvd_number);
			// let us decode the command now....
			// setting date and time....
			if( (matching_ptr = look_for_str(msg_data, "SET_DATE")) != 0){
				sscanf(matching_ptr, "SET_DATE\rDATE=%d-%d-%d\rTIME=%d:%d",
						&xSet_Date_Time.date,
						&xSet_Date_Time.month,
						&xSet_Date_Time.year,
						&xSet_Date_Time.hour,
						&xSet_Date_Time.minute);
				xSet_Date_Time.second = 10;
				set_time_to_rtc = 1;
			}
			// ............setting date and time....ends here

			if( (matching_ptr = look_for_str(msg_data, "DEL_FTP_FILE")) != 0){
				XITC_Gprs_Disk.command = eDel_Ftp_File;
				XITC_Gprs_Disk.signal = 1;
				while(XITC_Gprs_Disk.signal)
					vTaskDelay(100 / portTICK_RATE_MS);
			}

			if( (matching_ptr = look_for_str(msg_data, "DELETE_PARAM_FILE")) != 0){
				XITC_Gprs_Disk.command = eDel_Param_File;
				XITC_Gprs_Disk.signal = 1;
				while(XITC_Gprs_Disk.signal)
					vTaskDelay(100 / portTICK_RATE_MS);
			}

			if ((matching_ptr = look_for_str(msg_data, "MODULE_CMD")) != 0){
				if((matching_ptr = look_for_str(msg_data, "AT")) != 0){
					//int date, month, year;
					sscanf(matching_ptr, "AT%s",&ctemp4);

					sprintf(send_cmd, "AT%s\r\n", ctemp4);
					gprs_xmit_ptr = send_cmd;

					for(iCount=0; iCount<5; iCount++){
						response_numb = Send_And_Check_Gprs(gprs_xmit_ptr, "\r\n","ERROR", 5000);
						vTaskDelay(100 / portTICK_RATE_MS);
						//	if(response_numb == eAck){
						if((strlen(gprs_response) > 4) && (response_numb != eNack)){
							sprintf(status_response, "%s,%s,\"%s\",%s,%04d-%02d-%02dT%02d:%02d,%s\r\n\032",
									Global_Data_Params.gc_network_name,
									Global_Data_Params.gc_station_type,
									send_cmd,
									Global_Data_Params.gc_station_id,
									xPresent_Date_Time.year,
									xPresent_Date_Time.month,
									xPresent_Date_Time.date,
									xPresent_Date_Time.hour,
									xPresent_Date_Time.minute,
									gprs_response);
							send_message(msg_rcvd_number, status_response);
							break;
						}
					}
				}
			}


			// Reset dota cmd
			if( (matching_ptr = look_for_str(msg_data, "RESET_DOTA")) != 0){
				gi_start_dota = 0;
			}

			if( (matching_ptr = look_for_str(msg_data, "ERASE_DATA_SECTORS")) != 0){
				erase_data_sector = 1;
			}

			if( (matching_ptr = look_for_str(msg_data, "RESET_LDOTA")) != 0){
				gi_start_dota_lpc824 = 0;
			}


			if( (matching_ptr = look_for_str(msg_data, "RESTART")) != 0)
				gi_restart_flag = 1;

			if( (matching_ptr = look_for_str(msg_data, "SEND_SMS")) != 0){
				sscanf(matching_ptr, "SEND_SMS\rNUMBER=%s\rTEXT=%[^;]",&ctemp1,&ctemp2);
				strcat(ctemp2,"\032");
				send_message(ctemp1, ctemp2);
			}

			// .............send status message ...................

			// all set type commands ........
			else if ((matching_ptr = look_for_str(msg_data, "SET_")) != 0){
				// setting station name and id ....
				if ((matching_ptr = look_for_str(msg_data, "STATION")) != 0){
					if((matching_ptr = look_for_str(msg_data, "NUMBER")) != 0){
						sscanf(matching_ptr, "NUMBER=%s", &Global_Data_Params.gc_station_id);
						strcpy(gc_station_id1, Global_Data_Params.gc_station_id);
					}
					if((matching_ptr = look_for_str(msg_data, "NAME")) != 0)
						sscanf(matching_ptr, "NAME=%s",&Global_Data_Params.gc_station_name);
					//save params in flash
				}
				// ........// setting station name and id ....ends here

				// setting http parameter1.....starts here..
				else if ((matching_ptr = look_for_str(msg_data, "HTTP_PARAMS1")) != 0) {
					if((matching_ptr = look_for_str(msg_data, "SERVER")) != 0)
						sscanf(matching_ptr,"SERVER=%s", &Global_Data_Params.gc_http_server_name1);
					if((matching_ptr = look_for_str(msg_data, "PATH")) != 0)
						sscanf(matching_ptr,"PATH=%s", &Global_Data_Params.gc_http_link1);
					if((matching_ptr = look_for_str(msg_data, "PORT")) != 0)
						sscanf(matching_ptr,"PORT=%d", &Global_Data_Params.gi_http_server_port1);
					if((matching_ptr = look_for_str(msg_data, "KEY")) != 0)
						sscanf(matching_ptr,"KEY=%s", &Global_Data_Params.gc_http_key1);
				}
				// setting http parameter1.....ends here...

				// setting http parameter2.....starts here
				else if ((matching_ptr = look_for_str(msg_data, "HTTP_PARAMS2")) != 0) {
					if((matching_ptr = look_for_str(msg_data, "SERVER")) != 0)
						sscanf(matching_ptr,"SERVER=%s", &Global_Data_Params.gc_http_server_name2);
					if((matching_ptr = look_for_str(msg_data, "PATH")) != 0)
						sscanf(matching_ptr,"PATH=%s", &Global_Data_Params.gc_http_link2);
					if((matching_ptr = look_for_str(msg_data, "PORT")) != 0)
						sscanf(matching_ptr,"PORT=%d", &Global_Data_Params.gi_http_server_port2);
					if((matching_ptr = look_for_str(msg_data, "KEY")) != 0)
						sscanf(matching_ptr,"KEY=%s", &Global_Data_Params.gc_http_key2);
				}
				// setting http parameter2.....ends here

				// setting ftp parameter1.....starts here
				else if ((matching_ptr = look_for_str(msg_data, "FTP_PARAMS1")) != 0) {
					if((matching_ptr = look_for_str(msg_data, "SERVER")) != 0)
						sscanf(matching_ptr, "SERVER=%s", &Global_Data_Params.gc_ftp_server_name1);
					if((matching_ptr = look_for_str(msg_data, "USER")) != 0)
						sscanf(matching_ptr, "USER=%s", &Global_Data_Params.gc_ftp_user1);
					if((matching_ptr = look_for_str(msg_data, "PWD")) != 0)
						sscanf(matching_ptr, "PWD=%s", &Global_Data_Params.gc_ftp_pwd1);
				}
				// setting fpt parameter1.....ends here

				// setting ftp parameter2.....starts here
				else if ((matching_ptr = look_for_str(msg_data, "FTP_PARAMS2")) != 0) {
					if((matching_ptr = look_for_str(msg_data, "SERVER")) != 0)
						sscanf(matching_ptr, "SERVER=%s", &Global_Data_Params.gc_ftp_server_name2);
					if((matching_ptr = look_for_str(msg_data, "USER")) != 0)
						sscanf(matching_ptr, "USER=%s", &Global_Data_Params.gc_ftp_user2);
					if((matching_ptr = look_for_str(msg_data, "PWD")) != 0)
						sscanf(matching_ptr, "PWD=%s", &Global_Data_Params.gc_ftp_pwd2);
				}
				// setting fpt parameter2.....ends here

				// setting GSM parameters.....starts here
				else if ((matching_ptr = look_for_str(msg_data, "M_GSM_NUMBER")) != 0) {
					sscanf(matching_ptr,"M_GSM_NUMBER=%s", &Global_Data_Params.gc_master_gsm);
				}

				else if ((matching_ptr = look_for_str(msg_data, "U_GSM_NUMBER")) != 0) {
					sscanf(matching_ptr, "U_GSM_NUMBER=%s", &Global_Data_Params.gc_universal_gsm);
				}

				else if ((matching_ptr = look_for_str(msg_data, "S_GSM_NUMBER")) != 0) {
					sscanf(matching_ptr, "S_GSM_NUMBER=%s", &Global_Data_Params.gc_secondary_gsm);
				}

				// setting GSM parameters.....ends here

				// setting ISP0 parameters.....starts here
				else if ((matching_ptr = look_for_str(msg_data, "ISP_PARAMS0")) != 0) {
					if((matching_ptr = look_for_str(msg_data, "SVC")) != 0)
						sscanf(matching_ptr,"SVC=%s", &Global_Data_Params.ISP_APN_PARAMS[0].svc);
					if((matching_ptr = look_for_str(msg_data, "APN")) != 0)
						sscanf(matching_ptr,"APN=%s", &Global_Data_Params.ISP_APN_PARAMS[0].apn);
					if((matching_ptr = look_for_str(msg_data, "APN_USER")) != 0)
						sscanf(matching_ptr,"APN_USER=%s", &Global_Data_Params.ISP_APN_PARAMS[0].apn_user);
					if((matching_ptr = look_for_str(msg_data, "APN_PWD")) != 0)
						sscanf(matching_ptr,"APN_PWD=%s", &Global_Data_Params.ISP_APN_PARAMS[0].apn_pwd);
				}
				// setting ISP0 parameters.....ends here

				// setting ISP1 parameters.....starts here
				else if ((matching_ptr = look_for_str(msg_data, "ISP_PARAMS1")) != 0) {
					if((matching_ptr = look_for_str(msg_data, "SVC")) != 0)
						sscanf(matching_ptr,"SVC=%s", &Global_Data_Params.ISP_APN_PARAMS[1].svc);
					if((matching_ptr = look_for_str(msg_data, "APN")) != 0)
						sscanf(matching_ptr,"APN=%s", &Global_Data_Params.ISP_APN_PARAMS[1].apn);
					if((matching_ptr = look_for_str(msg_data, "APN_USER")) != 0)
						sscanf(matching_ptr,"APN_USER=%s", &Global_Data_Params.ISP_APN_PARAMS[1].apn_user);
					if((matching_ptr = look_for_str(msg_data, "APN_PWD")) != 0)
						sscanf(matching_ptr,"APN_PWD=%s", &Global_Data_Params.ISP_APN_PARAMS[1].apn_pwd);
				}
				// setting ISP1 parameters.....ends here

				// setting ISP2 parameters.....starts here
				else if ((matching_ptr = look_for_str(msg_data, "ISP_PARAMS2")) != 0) {
					if((matching_ptr = look_for_str(msg_data, "SVC")) != 0)
						sscanf(matching_ptr,"SVC=%s", &Global_Data_Params.ISP_APN_PARAMS[2].svc);
					if((matching_ptr = look_for_str(msg_data, "APN")) != 0)
						sscanf(matching_ptr,"APN=%s", &Global_Data_Params.ISP_APN_PARAMS[2].apn);
					if((matching_ptr = look_for_str(msg_data, "APN_USER")) != 0)
						sscanf(matching_ptr,"APN_USER=%s", &Global_Data_Params.ISP_APN_PARAMS[2].apn_user);
					if((matching_ptr = look_for_str(msg_data, "APN_PWD")) != 0)
						sscanf(matching_ptr,"APN_PWD=%s", &Global_Data_Params.ISP_APN_PARAMS[2].apn_pwd);
				}
				// setting ISP2 parameters.....ends here

				// setting ISP3 parameters.....starts here
				else if ((matching_ptr = look_for_str(msg_data, "ISP_PARAMS3")) != 0) {
					if((matching_ptr = look_for_str(msg_data, "SVC")) != 0)
						sscanf(matching_ptr,"SVC=%s", &Global_Data_Params.ISP_APN_PARAMS[3].svc);
					if((matching_ptr = look_for_str(msg_data, "APN")) != 0)
						sscanf(matching_ptr,"APN=%s", &Global_Data_Params.ISP_APN_PARAMS[3].apn);
					if((matching_ptr = look_for_str(msg_data, "APN_USER")) != 0)
						sscanf(matching_ptr,"APN_USER=%s", &Global_Data_Params.ISP_APN_PARAMS[3].apn_user);
					if((matching_ptr = look_for_str(msg_data, "APN_PWD")) != 0)
						sscanf(matching_ptr,"APN_PWD=%s", &Global_Data_Params.ISP_APN_PARAMS[3].apn_pwd);
				}
				// setting ISP3 parameters.....ends here

				// setting ISP4 parameters.....starts here
				else if ((matching_ptr = look_for_str(msg_data, "ISP_PARAMS4")) != 0) {
					if((matching_ptr = look_for_str(msg_data, "SVC")) != 0)
						sscanf(matching_ptr,"SVC=%s", &Global_Data_Params.ISP_APN_PARAMS[4].svc);
					if((matching_ptr = look_for_str(msg_data, "APN")) != 0)
						sscanf(matching_ptr,"APN=%s", &Global_Data_Params.ISP_APN_PARAMS[4].apn);
					if((matching_ptr = look_for_str(msg_data, "APN_USER")) != 0)
						sscanf(matching_ptr,"APN_USER=%s", &Global_Data_Params.ISP_APN_PARAMS[4].apn_user);
					if((matching_ptr = look_for_str(msg_data, "APN_PWD")) != 0)
						sscanf(matching_ptr,"APN_PWD=%s", &Global_Data_Params.ISP_APN_PARAMS[4].apn_pwd);
				}
				// setting ISP4 parameters.....ends here

				// setting log interval.....starts here
				else if((matching_ptr = look_for_str(msg_data, "INTERVAL")) != 0){
					if((matching_ptr = look_for_str(msg_data, "LOG_INTERVAL")) != 0)
						sscanf(matching_ptr,"LOG_INTERVAL=%d", &Global_Data_Params.gi_log_interval);
					if((matching_ptr = look_for_str(msg_data, "rREP_INTERVAL")) != 0)
						sscanf(matching_ptr,"REP_INTERVAL=%d", &Global_Data_Params.gi_report_interval);

				}
				// setting log interval.....ends here


				/*	else if((matching_ptr = look_for_str(msg_data, "CLOSING")) != 0){
					if((matching_ptr = look_for_str(msg_data, "CLOSING_HOUR")) != 0)
						sscanf(matching_ptr,"CLOSING_HOUR=%d", &Global_Data_Params.);
					if((matching_ptr = look_for_str(msg_data, "CLOSING_MIN")) != 0)
						sscanf(matching_ptr,"CLOSING_MIN=%d", &gi_closing_minute);
				}*/

				else if((matching_ptr = look_for_str(msg_data, "WS_CF")) != 0){
					if((matching_ptr = look_for_str(msg_data, "ACTUAL_WS1")) != 0)
						sscanf(matching_ptr,"ACTUAL_WS1=%f", &ws_actual1);
					if((matching_ptr = look_for_str(msg_data, "MEASURED_WS1")) != 0)
						sscanf(matching_ptr,"MEASURED_WS1=%f", &ws_measued1);
					if((matching_ptr = look_for_str(msg_data, "ACTUAL_WS2")) != 0)
						sscanf(matching_ptr,"ACTUAL_WS2=%f", &ws_actual2);
					if((matching_ptr = look_for_str(msg_data, "MEASURED_WS2")) != 0)
						sscanf(matching_ptr,"MEASURED_WS2=%f", &ws_measued2);

					ws_calib_factor    = ((ws_actual2-ws_actual1) / (ws_measued2 - ws_measued1));
					//ws_offset = (((ws_actual2 * (ws_measued2 - ws_measued1)) -  (ws_measued2 * (ws_actual2-ws_actual1))) / (ws_measued2 - ws_measued1));
					ws_offset    = ws_actual2 - (ws_calib_factor * ws_measued2);
					vTaskDelay(10 / portTICK_RATE_MS);

				}


				else if((matching_ptr = look_for_str(msg_data, "TEMP_CF")) != 0){
					if((matching_ptr = look_for_str(msg_data, "ACTUAL_TEMP1")) != 0)
						sscanf(matching_ptr,"ACTUAL_TEMP1=%f", &temp_actual1);
					if((matching_ptr = look_for_str(msg_data, "MEASURED_TEMP1")) != 0)
						sscanf(matching_ptr,"MEASURED_TEMP1=%f", &temp_measued1);
					if((matching_ptr = look_for_str(msg_data, "ACTUAL_TEMP2")) != 0)
						sscanf(matching_ptr,"ACTUAL_TEMP2=%f", &temp_actual2);
					if((matching_ptr = look_for_str(msg_data, "MEASURED_TEMP2")) != 0)
						sscanf(matching_ptr,"MEASURED_TEMP2=%f", &temp_measued2);

					temp_calib_factor  = ((temp_actual2-temp_actual1) / (temp_measued2 - temp_measued1));
					//temp_offset = (((temp_actual2 * (temp_measued2 - temp_measued1)) -  (temp_measued2 * (temp_actual2 - temp_actual1))) / (temp_measued2 - temp_measued1));
					temp_offset  = temp_actual2 - (temp_calib_factor * temp_measued2);
					vTaskDelay(10 / portTICK_RATE_MS);

				}


				else if((matching_ptr = look_for_str(msg_data, "HUMI_CF")) != 0){
					if((matching_ptr = look_for_str(msg_data, "ACTUAL_HUMIDITY1")) != 0)
						sscanf(matching_ptr,"ACTUAL_HUMIDITY1=%f", &humid_actual1);
					if((matching_ptr = look_for_str(msg_data, "MEASURED_HUMIDITY1")) != 0)
						sscanf(matching_ptr,"MEASURED_HUMIDITY1=%f", &humid_measued1);
					if((matching_ptr = look_for_str(msg_data, "ACTUAL_HUMIDITY2")) != 0)
						sscanf(matching_ptr,"ACTUAL_HUMIDITY2=%f", &humid_actual2);
					if((matching_ptr = look_for_str(msg_data, "MEASURED_HUMIDITY2")) != 0)
						sscanf(matching_ptr,"MEASURED_HUMIDITY2=%f", &humid_measued2);
					humid_calib_factor = ((humid_actual2-humid_actual1) / (humid_measued2 - humid_measued1));
					//humid_offset = (((humid_actual2 * (humid_measued2 - humid_measued1)) -  (humid_measued2 * (humid_actual2 - humid_actual1))) / (humid_measued2 - humid_measued1));
					humid_offset = humid_actual2 - (humid_calib_factor * humid_measued2);
					vTaskDelay(10 / portTICK_RATE_MS);

				}


				else if((matching_ptr = look_for_str(msg_data, "WD_CF")) != 0){
					if((matching_ptr = look_for_str(msg_data, "ACTUAL_WD1")) != 0)
						sscanf(matching_ptr,"ACTUAL_WD1=%f", &wd_actual1);
					if((matching_ptr = look_for_str(msg_data, "MEASURED_WD1")) != 0)
						sscanf(matching_ptr,"MEASURED_WD1=%f", &wd_measued1);
					if((matching_ptr = look_for_str(msg_data, "ACTUAL_WD2")) != 0)
						sscanf(matching_ptr,"ACTUAL_WD2=%f", &wd_actual2);
					if((matching_ptr = look_for_str(msg_data, "MEASURED_WD2")) != 0)
						sscanf(matching_ptr,"MEASURED_WD2=%f", &wd_measued2);

					wd_calib_factor    = ((wd_actual2-wd_actual1) / (wd_measued2 - wd_measued1));
					//wd_offset = (((wd_actual2 * (wd_measued2 - wd_measued1)) -  (wd_measued2 * (wd_actual2-wd_actual1))) / (wd_measued2 - wd_measued1));
					wd_offset    = wd_actual2 - (wd_calib_factor * wd_measued2);
					vTaskDelay(10 / portTICK_RATE_MS);

				}

				// setting cmd for firmware updation.....starts here
				else if((matching_ptr = look_for_str(msg_data, "FW_FOR_LPC")) != 0){
					if((matching_ptr = look_for_str(msg_data, "FILE")) != 0){
						sscanf(matching_ptr,"FILE=%s", &gc_dota_file_name);
						gi_start_dota = 1;
					}
				}

				// setting cmd for firmware updation.....ends here


				// setting cmd for firmware updation for LPC824.....starts here
				else if((matching_ptr = look_for_str(msg_data, "FIRMWARE_824")) != 0){
					if((matching_ptr = look_for_str(msg_data, "FILE")) != 0){
						sscanf(matching_ptr,"FILE=%s", &gc_dota_file_name);
						gi_start_dota_lpc824 = 1;
					}
				}


				else if((matching_ptr = look_for_str(msg_data, "SELECT_SIM")) != 0){
					if((matching_ptr = look_for_str(msg_data, "SELECT_SIM")) != 0){
						sscanf(matching_ptr,"SELECT_SIM=%d", &sim_select);
						Global_Data_Params.gi_sim_select = sim_select;
					}
				}

				else if((matching_ptr = look_for_str(msg_data, "DATA_SMS")) != 0){
					if((matching_ptr = look_for_str(msg_data, "DATA_SMS")) != 0){
						sscanf(matching_ptr,"DATA_SMS=%d", &Global_Data_Params.gi_send_data_sms);
						//Global_Data_Params.gi_sim_select = sim_select;
					}
				}

				// Communication module updation ..... ends here

				// save the params in the disk or flash
				XITC_Gprs_Disk.command = eSave_Params;
				XITC_Gprs_Disk.signal = 1;
				while(XITC_Gprs_Disk.signal)
					vTaskDelay(100 / portTICK_RATE_MS);

				gi_save_params_in_flash = 1;

				// insert responses here..
			}
			// .......... all set type commands ........end here....

			// Now Get type commands start .........
			// V 1.6 changes
			else if ((matching_ptr = look_for_str(msg_data, "$STATUS")) != 0){
				prepare_and_send_status(msg_rcvd_number);
			}
			else if ((matching_ptr = look_for_str(msg_data, "GET_")) != 0){
				if ((matching_ptr = look_for_str(msg_data, "STATUS")) != 0){
					prepare_and_send_status(msg_rcvd_number);
				}

				else if ((matching_ptr = look_for_str(msg_data, "GPS")) != 0){
					get_gps = 1;
					get_gps_info();
				}

				else if ((matching_ptr = look_for_str(msg_data, "DATA")) != 0){
					sscanf(matching_ptr, "DATA\rDATE=%d-%d-%d\rTIME=%d:%d",
							&XITC_Gprs_Disk.date, &XITC_Gprs_Disk.month, &XITC_Gprs_Disk.year,
							&XITC_Gprs_Disk.hour, &XITC_Gprs_Disk.minute);
					XITC_Gprs_Disk.command = eGet_Rec;
					XITC_Gprs_Disk.signal = 1;
					while(XITC_Gprs_Disk.signal)
						vTaskDelay(100 / portTICK_RATE_MS);
					status_response[0] = 0;
					//			sprintf(record, "stn_no=%s,rec_time=%04d-%02d-%02d,%02d:%02d:00,rainfall=%03d.%1d,signal=%d,bat_vol=%02d.%02d\r\n",
					//stn_no=%s,rec_time=%04d-%02d-%02d,%02d:%02d,rainfall=%03d.%01d,signal=%d,bat_volt1=%02d.%02d,bat_volt2=%02d.%02d\r\n"
					//stn_no=001920,rec_time=2021-01-20,00:00,rainfall=015.5,signal=114,bat_volt1=13.09,bat_volt2=13.09\r\n
					/*	sscanf(XITC_Gprs_Disk.data,"stn_no=%[^,],rec_time=%d-%d-%d,%d:%d,rainfall=%f,signal=%[^,],bat_volt1=%f",
							&ctemp1, &XITC_Gprs_Disk.year, &XITC_Gprs_Disk.month, &XITC_Gprs_Disk.date,
							&XITC_Gprs_Disk.hour, &XITC_Gprs_Disk.minute, &ftemp1, &ctemp2, &ftemp3);*/

					sscanf(XITC_Gprs_Disk.data,"stn_no=%[^,],rec_time=%d-%d-%d,%d:%d,temp=%f,humi=%f,ws=%f,wd=%d,signal=%[^,],bat_vol=%f,bat_vol2=%f",
							&ctemp1, &XITC_Gprs_Disk.year, &XITC_Gprs_Disk.month, &XITC_Gprs_Disk.date,
							&XITC_Gprs_Disk.hour, &XITC_Gprs_Disk.minute, &ftemp1, &ftemp2,&ftemp3,&itemp3, &ctemp2, &ftemp4, &ftemp5);


					if(XITC_Gprs_Disk.result == eLine_Read){
						//sprintf(status_response, "\r\nVARSHA-TRG\r\nSTN_NO=%s\r\n\032", XITC_Gprs_Disk.data);
						/*	sprintf(status_response, "\r\nSPATIKA\r\nTRGDAT-R\r\nSTN=%s\r\nDAT=%02d-%02d-%04d\r\nTIM=%02d:%02d\r\nREC=Y\r\nRFV=%03d.%d\r\nSIG=%s\r\n\032",
								ctemp1,XITC_Gprs_Disk.date, XITC_Gprs_Disk.month, XITC_Gprs_Disk.year,
								XITC_Gprs_Disk.hour,XITC_Gprs_Disk.minute, itemp1 ,itemp2, ctemp2);*/

						sprintf(status_response, "%s,%s,WS-DATA,%s,%04d-%02d-%02dT%02d:%02d,Y,%c%.1f,%.1f,%.1f,%d,%s,%.2f,%.2f,%04d-%02d-%02dT%02d:%02d,%d\r\n\032",

								Global_Data_Params.gc_network_name,
								Global_Data_Params.gc_station_type,
								Global_Data_Params.gc_station_id,
								XITC_Gprs_Disk.year,
								XITC_Gprs_Disk.month,
								XITC_Gprs_Disk.date,
								XITC_Gprs_Disk.hour,
								XITC_Gprs_Disk.minute,
								plus_minus_sym,
								ftemp1,
								ftemp2,
								ftemp3,
								itemp3,
								ctemp2,
								ftemp4,
								ftemp5,
								gi_log_year,
								gi_log_month,
								gi_log_date,
								gi_log_hour,
								gi_log_minute,
								gi_present_error
						);

						send_message(msg_rcvd_number, status_response);



					}
					else{
						/*sprintf(status_response, "\r\nSPATIKA\r\nTRGDAT-R\r\nSTN=%s\r\nDAT=%02d-%02d-%04d\r\nTIM=%02d:%02d\r\nREC=N\r\n\032",
								station_id,XITC_Gprs_Disk.date, XITC_Gprs_Disk.month, XITC_Gprs_Disk.year,
								XITC_Gprs_Disk.hour,XITC_Gprs_Disk.minute);
						send_message(msg_rcvd_number, status_response);*/
						sprintf(status_response, "%s,%s,WS-DATA,%s,%04d-%02d-%02dT%02d:%02d,N\r\n\032",
								Global_Data_Params.gc_network_name,
								Global_Data_Params.gc_station_type,
								Global_Data_Params.gc_station_id,
								XITC_Gprs_Disk.year,
								XITC_Gprs_Disk.month,
								XITC_Gprs_Disk.date,
								XITC_Gprs_Disk.hour,
								XITC_Gprs_Disk.minute);
						send_message(msg_rcvd_number, status_response);

					}
				}

				else if ((matching_ptr = look_for_str(msg_data, "MEMORY")) != 0){
					sscanf(matching_ptr, "MEMORY\rDATE=%d-%d-%d\rTIME=%d:%d", &m_date, &m_month, &m_year,&m_hour, &m_minute);

					mrec_no = m_hour*4 + m_minute/15;
					mrec_no = (mrec_no + 61) % 96 ;

					if(mrec_no <= 60){
						Next_Date(&m_date, &m_month, &m_year);

					}

					for(msect_no=2, mfound=0; ((msect_no<4) && !mfound);){
						if((days_sector[msect_no-2]->date == m_date) && (days_sector[msect_no-2]->month == m_month) && (days_sector[msect_no-2]->year == m_year)){
							mfound =1;
						}
						else
							msect_no++;
					}

					if(mfound == 1){

						//cumulative_rf(xLast_Rec_15min_Date_Time.date, xLast_Rec_15min_Date_Time.month, xLast_Rec_15min_Date_Time.year, xLast_Rec_15min_Date_Time.hour, xLast_Rec_15min_Date_Time.minute);
						vTaskDelay(1 / portTICK_RATE_MS);

						mbat_volt1 = ((((float)(days_sector[msect_no-2]->samples[mrec_no].battery1) * 3.3/4095)/0.138));

						sprintf(status_response, "%s,%s,WS-MDATA,%s,%04d-%02d-%02dT%02d:%02d,Y,%d,%d,%.2f,%04d-%02d-%02dT%02d:%02d,%d\r\n\032",
								Global_Data_Params.gc_network_name,
								Global_Data_Params.gc_station_type,
								Global_Data_Params.gc_station_id,
								m_year,
								m_month,
								m_date,
								m_hour,
								m_minute,
								days_sector[msect_no-2]->samples[mrec_no].rf,
								days_sector[msect_no-2]->samples[mrec_no].signal,
								mbat_volt1,
								gi_log_year,
								gi_log_month,
								gi_log_date,
								gi_log_hour,
								gi_log_minute,
								gi_present_error);
						send_message(msg_rcvd_number, status_response);
						vTaskDelay(100 / portTICK_RATE_MS);


					}
					else{
						sprintf(status_response, "%s,%s,WS-MDATA,%s,%04d-%02d-%02dT%02d:%02d,N\r\n\032",
								Global_Data_Params.gc_network_name,
								Global_Data_Params.gc_station_type,
								Global_Data_Params.gc_station_id,
								m_year,
								m_month,
								m_date,
								m_hour,
								m_minute);
						send_message(msg_rcvd_number, status_response);
						vTaskDelay(100 / portTICK_RATE_MS);


					}


				}

				else if ((matching_ptr = look_for_str(msg_data, "AUTO_DUMP")) != 0){
					//int date, month, year;
					sscanf(matching_ptr, "AUTO_DUMP\rDATE=%d-%d-%d",&auto_dump_date, &auto_dump_month, &auto_dump_year);

					XITC_Gprs_Disk.command = ePrepare_Auto_Dump;
					XITC_Gprs_Disk.signal = 1;
					while(XITC_Gprs_Disk.signal)
						vTaskDelay(100 / portTICK_RATE_MS);
					sprintf(dump_buf, "%s\032",dump_buf);
					response_no =  send_message(msg_rcvd_number, dump_buf);
					XITC_Gprs_Disk.signal = 0;
				}
			}
			// .......Get type commands end here
		}
		// if there is a response, send the response to both master and universal gsms
		// delete all read and sent messages
		response_no = Send_And_Check_Gprs("AT+CMGD=1,2\r\n", "OK", "ERROR", 5000);

		//	sprintf(msg_cmd, "AT+CMGD=%d\r\n", msg_no);
		//	response_no = Send_And_Check_Gprs(msg_cmd, "OK", "ERROR", 5000);
		vTaskDelay(100 / portTICK_RATE_MS);
	}
}



void init_gprs_uart(LPC_UART_TypeDef* gprs_port)
{
	UART_CFG_Type UART_InitStruct;
	UART_FIFO_CFG_Type UART_FIFOInitStruct;
	UART_ConfigStructInit(&UART_InitStruct);
	UART_InitStruct.Baud_rate = 115200;
	//	UART_InitStruct.Baud_rate = 9600;
	UART_Init(gprs_port, &UART_InitStruct);
	UART_SetupCbs(gprs_port, 0, &gprs_recv_isr);
	UART_SetupCbs(gprs_port, 1, &gprs_xmit_isr);
	UART_FIFOConfigStructInit(&UART_FIFOInitStruct);
	UART_FIFOConfig(gprs_port, &UART_FIFOInitStruct);
	UART_IntConfig(gprs_port, UART_INTCFG_RBR, ENABLE);
	UART_IntConfig(gprs_port, UART_INTCFG_THRE, ENABLE);
	UART_TxCmd(gprs_port,ENABLE);
	NVIC_EnableIRQ(UART2_IRQn);
}

void UART2_IRQHandler(void)
{
	UART2_StdIntHandler();
}


void gprs_send_cmd_direct(const char *cmd)
{
	/* Disable THRE interrupt temporarily so ISR does not collide */
	((LPC_UART_TypeDef*)GPRS_UART)->IER &= ~(1 << 1);

	while (*cmd) {
		while (!(((LPC_UART_TypeDef*)GPRS_UART)->LSR & (1 << 5)));
		((LPC_UART_TypeDef*)GPRS_UART)->THR = (uint8_t)*cmd++;
	}
	while (!(((LPC_UART_TypeDef*)GPRS_UART)->LSR & (1 << 6)));

	/* Re-enable THRE interrupt */
	((LPC_UART_TypeDef*)GPRS_UART)->IER |= (1 << 1);
}

void gprs_xmit_isr(void)
{
	if(*gprs_xmit_ptr){
		UART_SendData((LPC_UART_TypeDef*) GPRS_UART, *gprs_xmit_ptr);
		gprs_xmit_ptr++;
	}
}

void gprs_recv_isr(void)
{
	char c;

	*((char *)(0x2007c000 + gprs_recv_in_cnt) )= UART_ReceiveData((LPC_UART_TypeDef*) GPRS_UART);
	c = *((char *)(0x2007c000 + gprs_recv_in_cnt)) ;
	if(*((char *)(0x2007c000 + gprs_recv_in_cnt) ) == 0x03)
		end_of_text_recd = 1;
	gprs_recv_in_cnt = (gprs_recv_in_cnt+1)%(RECVBUF_SIZE);  //(sizeof(gprs_recv_buf)

	gi_dota_state %= 5;
	switch(gi_dota_state){
	case eMatch_E:
		if(c=='E')
			gi_dota_state= eMatch_R1;
		else
			gi_dota_state = eMatch_E;
		break;
	case eMatch_R1:
		if(c=='R')
			gi_dota_state= eMatch_R2;
		else
			gi_dota_state = eMatch_E;
		break;
	case eMatch_R2:
		if(c=='R')
			gi_dota_state= eMatch_O;
		else
			gi_dota_state = eMatch_E;
		break;
	case eMatch_O:
		if(c=='O')
			gi_dota_state= eMatch_R3;
		else
			gi_dota_state = eMatch_E;
		break;

	case eMatch_R3:
		if((c=='R')&&(gi_dota_error==0))
			gi_dota_error = 1;
		else
			gi_dota_state = eMatch_E;
		break;
	default:
		break;

	}
}


int Send_And_Check_Gprs(char* xmit_str, char *ack, char *nack, int timeout)
{

	char *matching_ptr;
	int lf_recd, response_in_count, msg_no;
	lf_recd = response_in_count = 0;
	// get ready for transmitting
	gprs_xmit_ptr = xmit_str;
	UART_SendData((LPC_UART_TypeDef*) GPRS_UART, *gprs_xmit_ptr++);
	vTaskDelay(25 / portTICK_RATE_MS);	// wait a while for transmission to take place
	timeout /= 10;						// take care of 10 ms wait in the loop

	while(timeout > 0){
		taskENTER_CRITICAL();
		gi_wdt_task_ok_stat |= WDT_GPRS_OK;
		taskEXIT_CRITICAL();

		lf_recd = response_in_count = 0;
		gprs_response[0] =0;
		while(( (lf_recd = getline((char *)0x2007c000, gprs_response, &gprs_recv_in_cnt, &gprs_recv_out_cnt,
				&response_in_count, RECVBUF_SIZE, LINE_FEED)) != 1) &&(timeout)) {
			vTaskDelay(10 / portTICK_RATE_MS); //sizeof(gprs_recv_buf)
			timeout--;
			if ((timeout % 50) == 0) {
				taskENTER_CRITICAL();
				gi_wdt_task_ok_stat |= WDT_GPRS_OK;
				taskEXIT_CRITICAL();
			}
		}
		if(lf_recd){
			if(0 != (matching_ptr = look_for_str(gprs_response, ack)))
				return(1);
			else if (0 != (matching_ptr = look_for_str(gprs_response,nack)))
				return(2);
			else if (0 != (matching_ptr = look_for_str(gprs_response, "+CMTI:"))){
				sscanf(matching_ptr, "+CMTI: \"SM\",%d", &msg_no);
				if(msg_no <= MAX_NO_OF_MSGS)
					message_recd[msg_no] = 1;
			}
			else {
				vTaskDelay(10 / portTICK_RATE_MS);
				timeout--;
			}
		}
	}
	if ((timeout == 0) && (lf_recd != 1)){
		gprs_timeout_occurred = 1;
		return(eTimeout);
	}
}


void check_basic_commands(void)
{

	int i,j;

	response_no=Send_And_Check_Gprs("\r\nAT\r\n", "OK", "ERROR", 1000);
	response_no=Send_And_Check_Gprs("\r\nAT\r\n", "OK", "ERROR", 1000);
	response_no=Send_And_Check_Gprs("\r\nATE0V1\r\n", "OK", "ERROR", 1000);	// disable echo enable verbose
	Send_And_Check_Gprs("AT+CGEREP=0\r\n", "OK", "ERROR", 2000);		// silence GPRS event URCs to protect UART buffers
	response_no=Send_And_Check_Gprs("\r\nAT+CMGF=1\r\n", "OK", "ERROR", 1000);	// switch to Text mode for sms
	response_no=Send_And_Check_Gprs("\r\nAT&W\r\n", "OK", "ERROR", 3000);

	if(response_no != eAck){
		response_no=Send_And_Check_Gprs("\r\nAT+CFUN=1,1\r\n", "OK", "ERROR", 3000);
		vTaskDelay(6000 / portTICK_RATE_MS);
	}

	vTaskDelay(500 / portTICK_RATE_MS);

}





void check_sim_cpin(void){
	int iCountTemp;

	response_no=eNack;

	for(iCountTemp=0; iCountTemp<10; iCountTemp++){
		response_no	=	Send_And_Check_Gprs("AT+CPIN?\r\n", "+CPIN: READY","ERROR", 5000);
		if(response_no == eAck){
			gi_sim_state = 1;
			process_msgs = 1;
			break;
		}
		else
			vTaskDelay(1000 / portTICK_RATE_MS);
	}
	if(response_no != eAck){
		//need to reset module and check once again
		response_no=Send_And_Check_Gprs("\r\nAT+CFUN=1,1\r\n", "OK", "ERROR", 3000);
		vTaskDelay(2000 / portTICK_RATE_MS);
		gi_sim_state = 0;

	}

}

void check_sim_registration(void){
	int iCountTemp, temp, stat;

	response_no = eNack;
	gi_sim_reg = 0;

	// Ensure automatic 2G/3G/4G mode (SIM7600 auto RAT selection)
	Send_And_Check_Gprs("AT+CNMP=2\r\n", "OK", "ERROR", 2000);

	for(iCountTemp = 0; iCountTemp < 60; iCountTemp++){
		// 1. Check Circuit-Switched 2G/3G registration
		response_no = Send_And_Check_Gprs("AT+CREG?\r\n", "+CREG: ", "ERROR", 2000);
		if(response_no == eAck){
			stat = 0;
			sscanf(gprs_response, "+CREG: %d,%d", &temp, &stat);
			if(stat == 1 || stat == 5) {
				gi_sim_reg = 1;
				break;
			}
		}

		// 2. Check Packet Data 2G/2.5G GPRS registration (critical for BSNL data SIMs)
		response_no = Send_And_Check_Gprs("AT+CGREG?\r\n", "+CGREG: ", "ERROR", 2000);
		if(response_no == eAck){
			stat = 0;
			sscanf(gprs_response, "+CGREG: %d,%d", &temp, &stat);
			if(stat == 1 || stat == 5) {
				gi_sim_reg = 1;
				break;
			}
		}

		// 3. Check EPS / 4G LTE registration (critical for Airtel 4G mode)
		response_no = Send_And_Check_Gprs("AT+CEREG?\r\n", "+CEREG: ", "ERROR", 2000);
		if(response_no == eAck){
			stat = 0;
			sscanf(gprs_response, "+CEREG: %d,%d", &temp, &stat);
			if(stat == 1 || stat == 5) {
				gi_sim_reg = 1;
				break;
			}
		}

		vTaskDelay(500 / portTICK_RATE_MS);
	}
	if(gi_sim_reg != 1)
		vTaskDelay(10 / portTICK_RATE_MS);

}

void get_signal_level(void){
	int iCountTemp, sig_lvl;

	response_no=eNack;
	if(gi_sim_state == 1){
		for(iCountTemp=0; iCountTemp<10; iCountTemp++){
			response_no	=	Send_And_Check_Gprs("AT+CSQ\r\n", "+CSQ:", "ERROR", 5000);
			if(response_no == eAck){
				sscanf(gprs_response, "+CSQ: %d", &sig_lvl);
				gi_signal_level = sig_lvl*2 -1 -113;
				if(gi_signal_level < 0)
					break;
			}
			else
				vTaskDelay(500 / portTICK_RATE_MS);
		}
	}
	else
		gi_signal_level = 0;
}


void get_apn(void){
	/*
	 Auto-APN Detection:
	 1. Query IMSI (AT+CIMI) for SIM card hardware operator identification
	 2. Query Registered Operator (AT+COPS?) to match Airtel, BSNL, Jio, Vodafone
	 */
	int iCountTemp;
	response_no=eNack;

	// Step 1: Query IMSI directly from SIM
	if (Send_And_Check_Gprs("AT+CIMI\r\n", "40", "ERROR", 3000) == eAck) {
		if (look_for_str(gprs_response, "40445") || look_for_str(gprs_response, "40410") ||
		    look_for_str(gprs_response, "40470") || look_for_str(gprs_response, "40431") ||
		    look_for_str(gprs_response, "40406") || look_for_str(gprs_response, "40492") ||
		    look_for_str(gprs_response, "40493") || look_for_str(gprs_response, "40494") ||
		    look_for_str(gprs_response, "40495") || look_for_str(gprs_response, "40496") ||
		    look_for_str(gprs_response, "40497") || look_for_str(gprs_response, "40498")) {
			strcpy(gc_operator_name, "Airtel");
			strcpy(gc_apn_str, "airtelgprs.com");
		} else if (look_for_str(gprs_response, "40438") || look_for_str(gprs_response, "40471") ||
		           look_for_str(gprs_response, "40472") || look_for_str(gprs_response, "40481") ||
		           look_for_str(gprs_response, "40462") || look_for_str(gprs_response, "40454") ||
		           look_for_str(gprs_response, "40451") || look_for_str(gprs_response, "40453") ||
		           look_for_str(gprs_response, "40455") || look_for_str(gprs_response, "40457") ||
		           look_for_str(gprs_response, "40458") || look_for_str(gprs_response, "40459") ||
		           look_for_str(gprs_response, "40464") || look_for_str(gprs_response, "40473") ||
		           look_for_str(gprs_response, "40474") || look_for_str(gprs_response, "40475") ||
		           look_for_str(gprs_response, "40476") || look_for_str(gprs_response, "40477") ||
		           look_for_str(gprs_response, "40480")) {
			strcpy(gc_operator_name, "BSNL");
			strcpy(gc_apn_str, "bsnlnet");
		} else if (look_for_str(gprs_response, "4058")) {
			strcpy(gc_operator_name, "JIO");
			strcpy(gc_apn_str, "jionet");
		}
	}

	/* Check ICCID for Airtel IoT / M2M dedicated APN (airteliot.com) */
	if (Send_And_Check_Gprs("AT+ICCID\r\n", "8991", "ERROR", 2000) == eAck ||
	    Send_And_Check_Gprs("AT+CCID\r\n", "8991", "ERROR", 2000) == eAck) {
		if (look_for_str(gprs_response, "899116") || look_for_str(gprs_response, "899110") || look_for_str(gprs_response, "899145")) {
			strcpy(gc_operator_name, "Airtel");
			strcpy(gc_apn_str, "airteliot.com");
		}
	}

	// Step 2: Ensure alphanumeric long format on SIM7600 & query COPS
	Send_And_Check_Gprs("AT+COPS=3,0\r\n", "OK", "ERROR", 2000);

	if( (response_no = Send_And_Check_Gprs("AT+COPS?\r\n", "+COPS: ", "ERROR", 15000)) == eAck){
		if (look_for_str(gprs_response, "airtel") || look_for_str(gprs_response, "Airtel")) {
			strcpy(gc_operator_name, "Airtel");
			if (strcmp(gc_apn_str, "airteliot.com") != 0) {
				strcpy(gc_apn_str, "airtelgprs.com");
			}
		} else if (look_for_str(gprs_response, "BSNL") || look_for_str(gprs_response, "bsnl") || look_for_str(gprs_response, "CellOne")) {
			strcpy(gc_operator_name, "BSNL");
			strcpy(gc_apn_str, "bsnlnet");
		} else if (look_for_str(gprs_response, "Vodafone") || look_for_str(gprs_response, "vodafone") || look_for_str(gprs_response, "Vi ")) {
			strcpy(gc_operator_name, "Vodafone");
			strcpy(gc_apn_str, "www");
		} else if (look_for_str(gprs_response, "Idea") || look_for_str(gprs_response, "idea")) {
			strcpy(gc_operator_name, "Idea");
			strcpy(gc_apn_str, "www");
		} else if (look_for_str(gprs_response, "JIO") || look_for_str(gprs_response, "jio") || look_for_str(gprs_response, "Jio")) {
			strcpy(gc_operator_name, "JIO");
			strcpy(gc_apn_str, "jionet");
		} else {
			char *p1 = strchr(gprs_response, '"');
			if (p1) {
				char *p2 = strchr(p1 + 1, '"');
				if (p2) {
					int len = p2 - (p1 + 1);
					if (len > 12) len = 12;
					strncpy(gc_operator_name, p1 + 1, len);
					gc_operator_name[len] = '\0';
				}
			}
		}

		for(iCountTemp=0; iCountTemp<5; iCountTemp++){
			if((matching_ptr = look_for_str(gprs_response, Global_Data_Params.ISP_APN_PARAMS[iCountTemp].mobile_network)) != 0){
				strcpy(gc_apn_str, Global_Data_Params.ISP_APN_PARAMS[iCountTemp].apn);
				strcpy(gc_apn_user_name, Global_Data_Params.ISP_APN_PARAMS[iCountTemp].apn_user);
				strcpy(gc_apn_user_pwd, Global_Data_Params.ISP_APN_PARAMS[iCountTemp].apn_pwd);
				break;
			}
		}
	}

	if (strlen(gc_apn_str) == 0) {
		strcpy(gc_apn_str, "airtelgprs.com");//set default apn
	}
}



/*-------------------------------------------------------------------------------------------------------------------*
* 	FUNCTION NAME : parse_nmea_coord
* 	DESCRIPTION : Parses NMEA / SIM7600 ddmm.mmmm (lat) or dddmm.mmmm (lon) into decimal degrees
* 	              without relying on newlib float sscanf (which causes RAM overflow in newlib-nano).
*---------------------------------------------------------------------------------------------------------------------*/
static float parse_nmea_coord(const char *str)
{
	if (!str || *str < '0' || *str > '9')
		return 0.0f;

	const char *dot = strchr(str, '.');
	if (!dot || (dot - str < 2))
		return 0.0f;

	/* Minutes integer part is the 2 digits immediately preceding '.' */
	int min_int = (dot[-2] - '0') * 10 + (dot[-1] - '0');

	/* Degrees are all digits preceding (dot - 2) */
	int deg = 0;
	for (const char *p = str; p < dot - 2; p++) {
		if (*p >= '0' && *p <= '9') {
			deg = deg * 10 + (*p - '0');
		}
	}

	/* Fractional minutes after '.' */
	float min_frac = 0.0f;
	float div = 10.0f;
	for (const char *p = dot + 1; *p >= '0' && *p <= '9'; p++) {
		min_frac += (float)(*p - '0') / div;
		div *= 10.0f;
	}

	float total_min = (float)min_int + min_frac;
	return (float)deg + (total_min / 60.0f);
}

void get_gps_info(void){
	int iCountTemp;
	char sim_holder[8];
	char msg_type;

	gps_found = 0;

	// Ensure SIM7600 active GNSS antenna bias is powered (3.05V)
	Send_And_Check_Gprs("AT+CVAUXV=3050\r\n", "OK", "ERROR", 1000);
	vTaskDelay(50 / portTICK_RATE_MS);
	Send_And_Check_Gprs("AT+CVAUXS=1\r\n", "OK", "ERROR", 1000);
	vTaskDelay(50 / portTICK_RATE_MS);
	Send_And_Check_Gprs("AT+CGPSAUTO=1\r\n", "OK", "ERROR", 1000);
	vTaskDelay(50 / portTICK_RATE_MS);

	// Check if GPS is already active on SIM7600
	response_no = Send_And_Check_Gprs("AT+CGPS?\r\n", "+CGPS:", "ERROR", 2000);
	if (!(response_no == 1 && look_for_str(gprs_response, "+CGPS: 1"))) {
		response_no = Send_And_Check_Gprs("AT+CGPS=1\r\n", "OK", "ERROR", 3000);
		if (response_no != 1) {
			Send_And_Check_Gprs("AT+CGPS=1,1\r\n", "OK", "ERROR", 3000);
		}
		vTaskDelay(2000 / portTICK_RATE_MS);
	}

	if(Global_Data_Params.gi_sim_select == 0)
		strcpy(sim_holder, "SIM_1");
	else
		strcpy(sim_holder, "SIM_2");

	if(get_gps == 1)
		msg_type = 'C';
	else
		msg_type = 'F';

	// Poll AT+CGPSINFO up to 25 times (~50 seconds max)
	for(iCountTemp = 0; iCountTemp < 25; iCountTemp++){
		taskENTER_CRITICAL();
		gi_wdt_task_ok_stat |= WDT_GPRS_OK;
		taskEXIT_CRITICAL();

		response_no = Send_And_Check_Gprs("AT+CGPSINFO\r\n", "+CGPSINFO:", "ERROR", 4000);
		char *p_info = look_for_str(gprs_response, "+CGPSINFO:");
		if ((response_no == 1) && (p_info != 0)) {
			char *p = p_info + 10;
			while (*p == ' ') p++;

			// If not empty (SIM7600 returns "+CGPSINFO: ,,,,,,,," when searching)
			if (*p != ',' && *p != '\0') {
				char lat_buf[16] = {0};
				char lon_buf[16] = {0};
				char lat_c = 'N', lon_c = 'E';

				int idx = 0;
				while (*p != ',' && *p != '\0' && idx < 15) lat_buf[idx++] = *p++;
				if (*p == ',') p++;
				if (*p != ',' && *p != '\0') lat_c = *p++;
				if (*p == ',') p++;
				idx = 0;
				while (*p != ',' && *p != '\0' && idx < 15) lon_buf[idx++] = *p++;
				if (*p == ',') p++;
				if (*p != ',' && *p != '\0') lon_c = *p++;

				float parsed_lat = parse_nmea_coord(lat_buf);
				float parsed_lon = parse_nmea_coord(lon_buf);

				if (parsed_lat > 0.0f && parsed_lon > 0.0f) {
					GPS.latitude = parsed_lat;
					GPS.lat_dir = (lat_c == 'S') ? 'S' : 'N';
					GPS.longitude = parsed_lon;
					GPS.lon_dir = (lon_c == 'W') ? 'W' : 'E';
					gps_found = 1;

					// Persist acquired GPS fix into Flash parameter sector
					Global_Data_Params.gps_latitude = GPS.latitude;
					Global_Data_Params.gps_lat_dir = GPS.lat_dir;
					Global_Data_Params.gps_longitude = GPS.longitude;
					Global_Data_Params.gps_lon_dir = GPS.lon_dir;
					write_param_sector();

					// Parse GPS Date and UTC Time (format: ddmmyy,hhmmss.s)
					if (*p == ',') p++;
					char date_buf[8] = {0}, time_buf[12] = {0};
					idx = 0;
					while (*p != ',' && *p != '\0' && idx < 7) date_buf[idx++] = *p++;
					if (*p == ',') p++;
					idx = 0;
					while (*p != ',' && *p != '\0' && idx < 11) time_buf[idx++] = *p++;

					int g_dd = 0, g_mm = 0, g_yy = 0;
					int g_hh = 0, g_min = 0, g_sec = 0;
					if (sscanf(date_buf, "%02d%02d%02d", &g_dd, &g_mm, &g_yy) == 3 &&
					    sscanf(time_buf, "%02d%02d%02d", &g_hh, &g_min, &g_sec) >= 2) {
						g_min += 30;
						if (g_min >= 60) { g_min -= 60; g_hh++; }
						g_hh += 5;
						if (g_hh >= 24) { g_hh -= 24; g_dd++; }
						g_yy += 2000;
						if (g_dd >= 1 && g_dd <= 31 && g_mm >= 1 && g_mm <= 12 && g_yy >= 2024 && g_yy <= 2079) {
							xSet_Date_Time.date   = g_dd;
							xSet_Date_Time.month  = g_mm;
							xSet_Date_Time.year   = g_yy;
							xSet_Date_Time.hour   = g_hh;
							xSet_Date_Time.minute = g_min;
							xSet_Date_Time.second = g_sec;
							set_time_to_rtc = 1;
						}
					}
					break;
				}
			}
		}
		vTaskDelay(2000 / portTICK_RATE_MS);
	}

	if(get_gps == 1){
		if(gps_found == 1){
			sprintf(status_response,"KSNDMC,TWS,STAT_AD-%c,%s,%04d-%02d-%02dT%02d:%02d,%s,%04d,%.6f%c,%.6f%c,%.1f,%04d-%02d-%02dT%02d:%02d:%02d\r\n\032",
					msg_type,
					Global_Data_Params.gc_station_id,
					xPresent_Date_Time.year,
					xPresent_Date_Time.month,
					xPresent_Date_Time.date,
					xPresent_Date_Time.hour,
					xPresent_Date_Time.minute,
					sim_holder,
					gi_signal_level,
					GPS.latitude,
					GPS.lat_dir,
					GPS.longitude,
					GPS.lon_dir,
					GPS.altitude,
					GPS.year,
					GPS.month,
					GPS.date,
					GPS.hour,
					GPS.min,
					GPS.sec);
		} else {
			sprintf(status_response,"KSNDMC,TWS,STAT_AD-%c,%s,%04d-%02d-%02dT%02d:%02d,%s,%04d,NA,NA,NA,NA\r\n\032",
					msg_type,
					Global_Data_Params.gc_station_id,
					xPresent_Date_Time.year,
					xPresent_Date_Time.month,
					xPresent_Date_Time.date,
					xPresent_Date_Time.hour,
					xPresent_Date_Time.minute,
					sim_holder,
					gi_signal_level);
		}
		response_no = send_message(Global_Data_Params.gc_universal_gsm, status_response);
		vTaskDelay(100 / portTICK_RATE_MS);
		get_gps = 0;
	}

	// Note: Keep GPS active in background so SIM7600 continuously maintains satellite tracking
	vTaskDelay(100 / portTICK_RATE_MS);
}


int connect_ftp2(){

	/*	sprintf(gprs_xmit_buf,"AT+CFTPSSTOP\r\n");
	response_no =  Send_And_Check_Gprs(gprs_xmit_buf, "OK", "ERROR", 5000);
	vTaskDelay(10 / portTICK_RATE_MS);
	if(response_no != eAck){
		return(-1);
	}

	vTaskDelay(1000 / portTICK_RATE_MS);*/


	sprintf(gprs_xmit_buf,"AT+CGSOCKCONT=1,\"IP\",\"%s\"\r\n",gc_apn_str);
	response_no =  Send_And_Check_Gprs(gprs_xmit_buf, "OK", "ERROR", 5000);
	vTaskDelay(10 / portTICK_RATE_MS);
	if(response_no != eAck){
		return(-1);
	}

	sprintf(gprs_xmit_buf,"AT+CGPADDR\r\n");
	response_no =  Send_And_Check_Gprs(gprs_xmit_buf, "+CGPADDR:", "ERROR", 5000);
	vTaskDelay(10 / portTICK_RATE_MS);
	if(response_no != eAck){
		return(-1);
	}


	sprintf(gprs_xmit_buf,"AT+CFTPSSTART\r\n");
	response_no =  Send_And_Check_Gprs(gprs_xmit_buf, "+CFTPSSTART: 0", "ERROR", 15000);
	vTaskDelay(10 / portTICK_RATE_MS);
	if(response_no != eAck){
		return(-1);
	}

	sprintf(gprs_xmit_buf,"AT+CFTPSSINGLEIP=1\r\n");
	response_no =  Send_And_Check_Gprs(gprs_xmit_buf, "OK", "ERROR", 5000);
	vTaskDelay(10 / portTICK_RATE_MS);
	if(response_no != eAck){
		return(-1);
	}

	sprintf(gprs_xmit_buf,"AT+CFTPSLOGIN=\"%s\",%d,\"%s\",\"%s\",0\r\n",Global_Data_Params.gc_ftp_server_name2, Global_Data_Params.gi_ftpserver_port2, Global_Data_Params.gc_ftp_user2, Global_Data_Params.gc_ftp_pwd2);
	response_no =  Send_And_Check_Gprs(gprs_xmit_buf, "+CFTPSLOGIN: 0", "ERROR", 25000);
	vTaskDelay(10 / portTICK_RATE_MS);
	if(response_no == eAck){
		return(1);
	}
	else
		return(-1);


}



int  ftp_dota(void)
{
	int xfr_res, bytes_recevd, stat, reply, code_start,i ;
	//	int char_count;
	char *matching_ptr;
	int lf_recd, timeout;
	char temp_file_name[32];
	int response_out_cnt;
	char recv_str[700];
	char recv_decode_str[700];

	//	int packet_recved;
	int start_byte;
	int err_cnt;

	int p;
	char *rptr;
	char match_str[32];

	int temp_cnt;
	temp_cnt = 0;

	//	char recv_str[600];

	char_count = 0;
	bytes_recevd = 0;
	err_cnt = 0;
	response_out_cnt = 0;

	start_byte = 0;
	xfr_res = eXfer_Fail;
	gi_dota_error = 0;

	//	strcpy(gc_dota_file_name, "trg5.hex");

	//AT+FSDEL=*.*
	sprintf(gprs_xmit_buf,"AT+FSDEL=*.*\r\n");
	response_no =  Send_And_Check_Gprs(gprs_xmit_buf, "OK", "ERROR", 15000);
	vTaskDelay(10 / portTICK_RATE_MS);

	vTaskDelay(1000 / portTICK_RATE_MS);

	sprintf(gprs_xmit_buf,"AT+CFTPSGETFILE=\"%s\",3\r\n",gc_dota_file_name);

	response_no = eNack;
	response_no =  Send_And_Check_Gprs(gprs_xmit_buf, "CFTPSGETFILE: 0", "ERROR", 180000);

	if(response_no == eAck){
		sprintf(gprs_xmit_buf,"AT+FSCD=E:/\r\n");
		response_no =  Send_And_Check_Gprs(gprs_xmit_buf, "+FSCD: E:/", "ERROR", 15000);
		vTaskDelay(10 / portTICK_RATE_MS);

		/*	sprintf(gprs_xmit_buf,"AT+FSLS=0\r\n");
		response_no =  Send_And_Check_Gprs(gprs_xmit_buf, gc_dota_file_name, "ERROR", 15000);
		vTaskDelay(10 / portTICK_RATE_MS);*/

		sprintf(temp_file_name,"E:/%s", gc_dota_file_name);
		vTaskDelay(1 / portTICK_RATE_MS);

		// here first open the file fw.hex and keep it opened
		XITC_Gprs_Disk_Fw.command = eFTP_File_Create;
		XITC_Gprs_Disk_Fw.signal = 1;
		while(XITC_Gprs_Disk_Fw.signal == 1)
			vTaskDelay(5 / portTICK_RATE_MS);

		vTaskDelay(1000 / portTICK_RATE_MS);

		while(1){
			temp_cnt++;
			char_count = 0;
			timeout = 500;
			packet_recved = 0;

			for(i=0;i<650;i++){
				recv_str[i] = 0xff;
				recv_decode_str[i] = 0xff;
				//	*(ptr_RAM2+i) = 0xff;
			}

			gprs_recv_in_cnt = 0;
			gprs_recv_out_cnt = 0;
			response_out_cnt = 0;

			//	printf("cycle %d starts stage 1\r\n", temp_cnt);
			//printf("cycle %d starts stage 1, start byte is %d\r\n", temp_cnt,start_byte);

			sprintf(gprs_xmit_buf,"AT+CFTRANTX=\"%s\",%d,512\r\n", temp_file_name,start_byte);
			gprs_xmit_ptr = gprs_xmit_buf;
			UART_SendData((LPC_UART_TypeDef*) GPRS_UART, *gprs_xmit_ptr++);
			vTaskDelay(50 / portTICK_RATE_MS);	// wait a while for transmission to take place

			vTaskDelay(200 / portTICK_RATE_MS);

			while((!packet_recved) && (timeout)){
				if(timeout){
					timeout--;
					vTaskDelay(1 / portTICK_RATE_MS);
				}

				while( gprs_recv_in_cnt != response_out_cnt){
					recv_str[char_count] = *((char *)(0x2007c000 + response_out_cnt));
					recv_str[char_count+1] = 0;
					response_out_cnt++;

					char_count++;
					if(gprs_recv_in_cnt > 600)
						break;
				}

				if(char_count > 32){
					/*	if((matching_ptr = look_for_str(recv_str, "+CFTPSNOTIFY:")) != 0 ){

						XITC_Gprs_Disk_Fw.command = eFile_Close;
						XITC_Gprs_Disk_Fw.signal = 1;
						while(XITC_Gprs_Disk_Fw.signal == 1)
							vTaskDelay(5 / portTICK_RATE_MS);
						gi_dota_error = 1;
						break;
					}*/

					if((matching_ptr = look_for_str(recv_str, "OK")) != 0 )
						packet_recved = 1;
					else
						packet_recved = 0;
				}
			}

			//	printf("cycle %d starts stage 2\r\n", temp_cnt);

			if((timeout == 0) || (packet_recved == 0)){
				err_cnt++;
				timeout = 500;

				vTaskDelay(1 / portTICK_RATE_MS);

				if(err_cnt > 10){

					XITC_Gprs_Disk.command = eDelete_Dota_File;
					XITC_Gprs_Disk.signal = 1;
					while(XITC_Gprs_Disk.signal)
						vTaskDelay(100 / portTICK_RATE_MS);

					XITC_Gprs_Disk_Fw.command = eFile_Close;
					XITC_Gprs_Disk_Fw.signal = 1;
					while(XITC_Gprs_Disk_Fw.signal == 1)
						vTaskDelay(5 / portTICK_RATE_MS);
					break;
				}
			}

			if(char_count > 32){
				matching_ptr = mv2endof_str(recv_str, "DATA,", 150 );
				if (matching_ptr != 0)
					sscanf(matching_ptr, "%d\r\n", &bytes_recevd);
			}
			//	printf("cycle %d starts stage 3\r\n", temp_cnt);
			if(((bytes_recevd != 512) || (packet_recved == 1)) && (char_count > 32)){
				sprintf(match_str,"DATA,%d\r\n", bytes_recevd);

				matching_ptr = mv2endof_str(recv_str, match_str, 150 );
				//printf("cycle %d starts stage 4\r\n", temp_cnt);

				if (matching_ptr != 0)
				{
					for(i=0;i<bytes_recevd;i++){
						recv_decode_str[i] = *matching_ptr;
						*matching_ptr++;
					}

					//printf("cycle %d starts stage 5\r\n", temp_cnt);
					XITC_Gprs_Disk_Fw.buf_ptr = (char *)(recv_decode_str);
					XITC_Gprs_Disk_Fw.count = bytes_recevd;
					XITC_Gprs_Disk_Fw.command = eFTP_Write_Buf;
					XITC_Gprs_Disk_Fw.signal = 1;
					while(XITC_Gprs_Disk_Fw.signal)
						vTaskDelay(1 / portTICK_RATE_MS);

				}
				if(bytes_recevd<512){
					//	printf("cycle %d ends\r\n", temp_cnt);

					file_saved = 1;
					vTaskDelay(1 / portTICK_RATE_MS);
					XITC_Gprs_Disk_Fw.command = eFile_Close;
					XITC_Gprs_Disk_Fw.signal = 1;
					while(XITC_Gprs_Disk_Fw.signal == 1)
						vTaskDelay(5 / portTICK_RATE_MS);
					break;
				}
				start_byte = start_byte + bytes_recevd;
			}

			else{
				vTaskDelay(1 / portTICK_RATE_MS);
			}

			bytes_recevd = 0;

		}
	}
}









int connect_ftp1(){

	sprintf(gprs_xmit_buf,"AT+CGSOCKCONT=1,\"IP\",\"%s\"\r\n",gc_apn_str);
	response_no =  Send_And_Check_Gprs(gprs_xmit_buf, "OK", "ERROR", 8000);
	vTaskDelay(10 / portTICK_RATE_MS);

	if(response_no != eAck){
		strcpy(gc_apn_str, "airtelgprs.com");//set default apn
		sprintf(gprs_xmit_buf,"AT+CGSOCKCONT=1,\"IP\",\"%s\"\r\n",gc_apn_str);
		response_no =  Send_And_Check_Gprs(gprs_xmit_buf, "OK", "ERROR", 8000);
	}

	vTaskDelay(10 / portTICK_RATE_MS);
	if(response_no != eAck){
		return(-1);
	}


	sprintf(gprs_xmit_buf,"AT+CGPADDR\r\n");
	response_no =  Send_And_Check_Gprs(gprs_xmit_buf, "+CGPADDR:", "ERROR", 5000);
	vTaskDelay(10 / portTICK_RATE_MS);
	if(response_no != eAck){
		return(-1);
	}


	sprintf(gprs_xmit_buf,"AT+CFTPSSTART\r\n");
	response_no =  Send_And_Check_Gprs(gprs_xmit_buf, "+CFTPSSTART: 0", "ERROR", 70000);
	vTaskDelay(10 / portTICK_RATE_MS);
	if(response_no != eAck){
		return(-1);
	}

	sprintf(gprs_xmit_buf,"AT+CFTPSSINGLEIP=1\r\n");
	response_no =  Send_And_Check_Gprs(gprs_xmit_buf, "OK", "ERROR", 5000);
	vTaskDelay(10 / portTICK_RATE_MS);
	if(response_no != eAck){
		return(-1);
	}

	//AT+CFTPSLOGIN="ftp1.ksndmc.net",21,"trg_spatika_v2@ksndmc.net","trgsp#123",0

	sprintf(gprs_xmit_buf,"AT+CFTPSLOGIN=\"%s\",%d,\"%s\",\"%s\",0\r\n",Global_Data_Params.gc_ftp_server_name1, Global_Data_Params.gi_ftpserver_port1, Global_Data_Params.gc_ftp_user1, Global_Data_Params.gc_ftp_pwd1);
	response_no =  Send_And_Check_Gprs(gprs_xmit_buf, "+CFTPSLOGIN: 0", "ERROR", 45000);
	vTaskDelay(10 / portTICK_RATE_MS);
	if(response_no == eAck){
		return(1);
	}
	else
		return(-1);

}


int ftp_send_data(){
	int dest_in_count, timeout, lf_recd;
	char msg_response[80];
	int temp_cnt;

	sprintf(gc_ftp_put_file, "./TWS_%s_%02d%02d%02d_%02d%02d00.kwd",
			Global_Data_Params.gc_station_id,
			(xPresent_Date_Time.year%2000),
			xPresent_Date_Time.month,
			xPresent_Date_Time.date,
			xPresent_Date_Time.hour,
			(xPresent_Date_Time.minute/Global_Data_Params.gi_log_interval)*Global_Data_Params.gi_log_interval);

	sprintf(gprs_xmit_buf,"AT+CFTPSPUT=\"%s\"\r\n",gc_ftp_put_file);

	gprs_xmit_ptr = gprs_xmit_buf;
	UART_SendData((LPC_UART_TypeDef*) GPRS_UART, *gprs_xmit_ptr++);
	vTaskDelay(25 / portTICK_RATE_MS);	// wait a while for transmission to take place
	vTaskDelay(5 / portTICK_RATE_MS);

	dest_in_count = 0;
	msg_response[0] = 0;
	timeout = 15000/10;
	while(( (lf_recd = getline((char *)0x2007c000, msg_response, &gprs_recv_in_cnt, &gprs_recv_out_cnt,
			&dest_in_count, RECVBUF_SIZE, (int) *(SIMXFERRES))) != 1) &&(timeout)) {
		vTaskDelay(10 / portTICK_RATE_MS);
		timeout--;
	}

	vTaskDelay(2 / portTICK_RATE_MS);

	if(lf_recd){
		strcat(ftp_buff, "\032");	// CTRL+Z
		vTaskDelay(2 / portTICK_RATE_MS);

		//	temp_cnt = strlen(ftp_buff);
		response_no=Send_And_Check_Gprs(ftp_buff, "+CFTPSPUT: 0", "ERROR", 40000);
		if(response_no == eAck)
			ftp_status = 1;
		else
			ftp_status = 0;


		/*		sprintf(gprs_xmit_buf,"AT+CFTPSLOGOUT\r\n");
		response_no =  Send_And_Check_Gprs(gprs_xmit_buf, "OK", "ERROR", 5000);
		vTaskDelay(50 / portTICK_RATE_MS);

		sprintf(gprs_xmit_buf,"AT+CFTPSSTOP\r\n");
		response_no =  Send_And_Check_Gprs(gprs_xmit_buf, "OK", "ERROR", 5000);
		vTaskDelay(50 / portTICK_RATE_MS);*/

	}
	else
		ftp_status = 0;

	ftp_buff[0] = 0;
}


int connect_http1(){
	char log_tmp[128];
	sprintf(log_tmp, "NET: Connecting APN=%s...", gc_apn_str);
	sd_log_http(log_tmp);

	// 1. Configure 3GPP PDP context (profile 1)
	sprintf(gprs_xmit_buf,"AT+CGDCONT=1,\"IP\",\"%s\"\r\n",gc_apn_str);
	response_no =  Send_And_Check_Gprs(gprs_xmit_buf, "OK", "ERROR", 5000);
	vTaskDelay(20 / portTICK_RATE_MS);

	// 2. Configure Socket PDP context (profile 1) on SIM7600
	sprintf(gprs_xmit_buf,"AT+CGSOCKCONT=1,\"IP\",\"%s\"\r\n",gc_apn_str);
	response_no =  Send_And_Check_Gprs(gprs_xmit_buf, "OK", "ERROR", 5000);
	vTaskDelay(20 / portTICK_RATE_MS);

	// 3. Select profile 1 for socket stack
	sprintf(gprs_xmit_buf,"AT+CSOCKSETPN=1\r\n");
	Send_And_Check_Gprs(gprs_xmit_buf, "OK", "ERROR", 5000);
	vTaskDelay(20 / portTICK_RATE_MS);

	// 4. Non-transparent buffer mode
	sprintf(gprs_xmit_buf,"AT+CIPMODE=0\r\n");
	Send_And_Check_Gprs(gprs_xmit_buf, "OK", "ERROR", 5000);
	vTaskDelay(20 / portTICK_RATE_MS);

	/* 5. Check if network bearer is already opened */
	if (Send_And_Check_Gprs("AT+NETOPEN?\r\n", "+NETOPEN: 1", "ERROR", 3000) != eAck) {
		sprintf(gprs_xmit_buf, "AT+NETOPEN\r\n");
		// SIM7600 responds OK first, then asynchronously +NETOPEN: 0 on success.
		// NACK must be "ERROR", never "OK".
		response_no = Send_And_Check_Gprs(gprs_xmit_buf, "+NETOPEN: 0", "ERROR", 30000);
		if (response_no != eAck && (look_for_str(gprs_response, "already opened") != 0 || look_for_str(gprs_response, "Network is already") != 0)) {
			response_no = eAck;
		} else if (response_no != eAck) {
			/* Fallback to alternate Airtel APN if on Airtel */
			if (look_for_str(gc_operator_name, "Airtel") != 0 || look_for_str(gc_apn_str, "airtel") != 0) {
				const char *alt_apn = (strcmp(gc_apn_str, "airtelgprs.com") == 0) ? "airteliot.com" : "airtelgprs.com";
				strcpy(gc_apn_str, alt_apn);
				sprintf(log_tmp, "NET: Retrying with alternate APN %s", gc_apn_str);
				sd_log_http(log_tmp);
				sprintf(gprs_xmit_buf, "AT+CGDCONT=1,\"IP\",\"%s\"\r\n", gc_apn_str);
				Send_And_Check_Gprs(gprs_xmit_buf, "OK", "ERROR", 3000);
				sprintf(gprs_xmit_buf, "AT+CGSOCKCONT=1,\"IP\",\"%s\"\r\n", gc_apn_str);
				Send_And_Check_Gprs(gprs_xmit_buf, "OK", "ERROR", 3000);
				response_no = Send_And_Check_Gprs("AT+NETOPEN\r\n", "+NETOPEN: 0", "ERROR", 30000);
				if (response_no != eAck && (look_for_str(gprs_response, "already opened") != 0 || look_for_str(gprs_response, "Network is already") != 0)) {
					response_no = eAck;
				}
			}
		}
		vTaskDelay(50 / portTICK_RATE_MS);
	} else {
		response_no = eAck;
	}

	if (response_no == eAck) {
		sd_log_http("NET: Bearer opened (NETOPEN OK)");
	} else {
		sprintf(log_tmp, "NET: NETOPEN failed! %s", gprs_response);
		sd_log_http(log_tmp);
	}

	sprintf(gprs_xmit_buf,"AT+IPADDR\r\n");
	response_no =  Send_And_Check_Gprs(gprs_xmit_buf, "+IPADDR:", "ERROR", 8000);
	vTaskDelay(10 / portTICK_RATE_MS);
	if(response_no == eAck){
		sprintf(log_tmp, "NET: IP OK -> %s", gprs_response);
		sd_log_http(log_tmp);
		return(1);
	}
	else {
		sd_log_http("NET: IP address query failed");
		Send_And_Check_Gprs("AT+NETCLOSE\r\n", "OK", "ERROR", 2000);
		return(-1);
	}
}



void reset_radio_stack(void) {
	Send_And_Check_Gprs("AT+NETCLOSE\r\n", "OK", "ERROR", 2000);
	Send_And_Check_Gprs("AT+HTTPTERM\r\n", "OK", "ERROR", 2000);
	Send_And_Check_Gprs("AT+CFUN=0\r\n", "OK", "ERROR", 3000);
	vTaskDelay(1000 / portTICK_RATE_MS);
	Send_And_Check_Gprs("AT+CFUN=1\r\n", "OK", "ERROR", 3000);
	vTaskDelay(3000 / portTICK_RATE_MS);
}

int http_open_tcp(char *server, int port) {
	char log_tmp[160];
	int timeout;

	/* 1. Ensure socket 0 is closed before opening to prevent 'socket exists' / ERROR */
	Send_And_Check_Gprs("AT+CIPCLOSE=0\r\n", "OK", "ERROR", 1000);
	vTaskDelay(100 / portTICK_RATE_MS);

	/* Flush circular buffer so no residual 'ERROR', 'OK', or stale URCs from CIPCLOSE remain */
	gprs_recv_out_cnt = gprs_recv_in_cnt;

	sprintf(log_tmp, "HTTP: Opening TCP %s:%d", server, port);
	sd_log_http(log_tmp);

	sprintf(gprs_xmit_buf, "AT+CIPOPEN=0,\"TCP\",\"%s\",%d\r\n", server, port);
	gprs_xmit_ptr = gprs_xmit_buf;
	UART_SendData((LPC_UART_TypeDef*) GPRS_UART, *gprs_xmit_ptr++);

	/* 2. Wait up to 25 seconds for +CIPOPEN: 0, response */
	timeout = 25000 / 10;
	while (timeout > 0) {
		int lf_recd = 0;
		int response_in_count = 0;
		gprs_response[0] = 0;
		while (((lf_recd = getline((char *)0x2007c000, gprs_response, &gprs_recv_in_cnt, &gprs_recv_out_cnt,
				&response_in_count, RECVBUF_SIZE, LINE_FEED)) != 1) && timeout) {
			vTaskDelay(10 / portTICK_RATE_MS);
			timeout--;
			if ((timeout % 100) == 0) {
				taskENTER_CRITICAL();
				gi_wdt_task_ok_stat |= WDT_GPRS_OK;
				taskEXIT_CRITICAL();
			}
		}
		if (lf_recd) {
			char *p_open = look_for_str(gprs_response, "+CIPOPEN: 0,");
			if (p_open != 0) {
				int err_code = -1;
				sscanf(p_open, "+CIPOPEN: 0,%d", &err_code);
				if (err_code == 0) {
					sd_log_http("HTTP: TCP connected (+CIPOPEN: 0,0 OK)!");
					vTaskDelay(150 / portTICK_RATE_MS); /* Allow SIM7600 socket stack to settle before CIPSEND */
					return 1; /* Success */
				} else {
					const char *err_desc = "Unknown error";
					if (err_code == 1) err_desc = "Network error / bearer not ready";
					else if (err_code == 2) err_desc = "TCP connect rejected / server unreachable";
					else if (err_code == 3) err_desc = "DNS resolution failed";
					else if (err_code == 4) err_desc = "Invalid parameter";
					else if (err_code == 5) err_desc = "Socket in use";
					else if (err_code == 6) err_desc = "Operation not allowed";
					else if (err_code == 7) err_desc = "Out of memory";
					sprintf(log_tmp, "HTTP: CIPOPEN rejected: code %d (%s)", err_code, err_desc);
					sd_log_http(log_tmp);
					Send_And_Check_Gprs("AT+CIPCLOSE=0\r\n", "OK", "ERROR", 1500);
					vTaskDelay(50 / portTICK_RATE_MS);
					gprs_recv_out_cnt = gprs_recv_in_cnt;
					return -1;
				}
			}
			if (look_for_str(gprs_response, "ERROR") != 0) {
				sd_log_http("HTTP: CIPOPEN returned ERROR");
				Send_And_Check_Gprs("AT+CIPCLOSE=0\r\n", "OK", "ERROR", 1500);
				vTaskDelay(50 / portTICK_RATE_MS);
				gprs_recv_out_cnt = gprs_recv_in_cnt;
				return -1;
			}
		}
	}
	sd_log_http("HTTP: CIPOPEN timed out (25s)!");
	Send_And_Check_Gprs("AT+CIPCLOSE=0\r\n", "OK", "ERROR", 1500);
	vTaskDelay(50 / portTICK_RATE_MS);
	gprs_recv_out_cnt = gprs_recv_in_cnt;
	return -1;
}

int http_send_data_single(char *server, int port, char *path) {
	/* Dedicated LPC1768 AHB SRAM Bank 1 (0x20080000, 16KB unreserved hardware SRAM).
	 * Mapping http_data here eliminates 1024 bytes from RamLoc32 .bss, preventing linker overflow
	 * and keeping the FreeRTOS task stack unaffected. */
	char *http_data = (char *)0x20080000;
	http_data[0] = '\0';
	char http_response[300];
	char log_tmp[320];
	char formatted_path[128];
	int i, lf_recd, timeout, dest_in_count, http_xfer_result;
	char *matching_ptr;

	char *gprs_months[12] = {"Jan", "Feb", "Mar", "Apr",
			"May", "Jun", "Jul", "Aug",
			"Sep", "Oct", "Nov", "Dec" };
	char gprs_new_mon[4], gprs_new_day[5];
	int gprs_month_no;

	/* 1. Format HTTP request upfront so exact byte length is known for AT+CIPSEND */
	if (path != NULL && path[0] == '/') {
		strncpy(formatted_path, path, sizeof(formatted_path) - 1);
	} else if (path != NULL) {
		snprintf(formatted_path, sizeof(formatted_path), "/%s", path);
	} else {
		strcpy(formatted_path, "/");
	}
	formatted_path[sizeof(formatted_path) - 1] = 0;

	/* Determine correct Host header: match IP to respective domain for virtual hosting */
	const char *host_name = server;
	if (strcmp(server, "117.216.42.181") == 0) {
		host_name = Global_Data_Params.gc_http_server_name1;
	} else if (strcmp(server, "144.91.104.105") == 0) {
		host_name = Global_Data_Params.gc_http_server_name2;
	}

	int offset = snprintf(http_data, 1024, "POST %s HTTP/1.1\r\n", formatted_path);
	if (port == 80) {
		offset += snprintf(http_data + offset, 1024 - offset, "Host: %s\r\n", host_name);
	} else {
		offset += snprintf(http_data + offset, 1024 - offset, "Host: %s:%d\r\n", host_name, port);
	}
	offset += snprintf(http_data + offset, 1024 - offset, "Connection: close\r\n");
#if (UNIT_CFG == CFG_BIHAR_TRG)
	offset += snprintf(http_data + offset, 1024 - offset, "Content-Type: application/json\r\n");
#else
	offset += snprintf(http_data + offset, 1024 - offset, "Content-Type: application/x-www-form-urlencoded\r\n");
#endif
	offset += snprintf(http_data + offset, 1024 - offset, "Content-Length: %d\r\n\r\n", (int)strlen(gc_http_data));
	offset += snprintf(http_data + offset, 1024 - offset, "%s", gc_http_data);

	int rawLen = strlen(http_data);

	snprintf(log_tmp, sizeof(log_tmp), "HTTP: Target -> http://%s:%d%s (Total: %d B, Body: %d B)", 
		server, port, formatted_path, rawLen, (int)strlen(gc_http_data));
	sd_log_http(log_tmp);

	/* 2. Open TCP connection to target server */
	if (http_open_tcp(server, port) != 1) {
		return eXfer_Fail;
	}

	/* 3. Flush any pending incoming bytes in circular buffer before sending command */
	gprs_recv_out_cnt = gprs_recv_in_cnt;

	/* 4. SIM7600 Direct Socket CIPSEND requires exact byte length: AT+CIPSEND=0,<rawLen> */
	snprintf(log_tmp, sizeof(log_tmp), "HTTP: Sending AT+CIPSEND=0,%d ...", rawLen);
	sd_log_http(log_tmp);

	sprintf(gprs_xmit_buf, "AT+CIPSEND=0,%d\r\n", rawLen);
	gprs_xmit_ptr = gprs_xmit_buf;
	UART_SendData((LPC_UART_TypeDef*) GPRS_UART, *gprs_xmit_ptr++);
	vTaskDelay(20 / portTICK_RATE_MS);

	/* 5. Search circular buffer for '>' prompt (avoids false-exit on \r\n or echo) */
	char prompt_rx[100] = {0};
	int rx_idx = 0;
	int prompt_found = 0;
	timeout = 5000 / 10;
	int prompt_wait_ms = 0;
	while (timeout > 0 && !prompt_found) {
		while (gprs_recv_in_cnt != gprs_recv_out_cnt) {
			char ch = *((char *)(0x2007c000 + gprs_recv_out_cnt));
			gprs_recv_out_cnt = (gprs_recv_out_cnt + 1) % RECVBUF_SIZE;
			if (rx_idx < sizeof(prompt_rx) - 1 && ch >= 32 && ch <= 126) {
				prompt_rx[rx_idx++] = ch;
			}
			if (ch == '>') {
				prompt_found = 1;
				break;
			}
		}
		if (!prompt_found) {
			vTaskDelay(10 / portTICK_RATE_MS);
			prompt_wait_ms += 10;
			timeout--;
		}
	}

	if (!prompt_found) {
		snprintf(log_tmp, sizeof(log_tmp), "HTTP: ERROR: No > prompt from modem! (waited %d ms, modem rx: [%s])", prompt_wait_ms, prompt_rx);
		sd_log_http(log_tmp);
		Send_And_Check_Gprs("AT+CIPCLOSE=0\r\n", "OK", "ERROR", 1500);
		return eXfer_Fail;
	}
	snprintf(log_tmp, sizeof(log_tmp), "HTTP: > prompt OK (%d ms). Streaming %d bytes over UART...", prompt_wait_ms, rawLen);
	sd_log_http(log_tmp);
	vTaskDelay(10 / portTICK_RATE_MS);

	/* 6. Send raw HTTP request payload (exact rawLen bytes, strictly NO Ctrl-Z / 0x1A) */
	for (i = 0; i < sizeof(http_response); i++)
		http_response[i] = 0;
	gprs_xmit_ptr = http_data;
	UART_SendData((LPC_UART_TypeDef*) GPRS_UART, *gprs_xmit_ptr++);

	/* Wait until UART ISR finishes transmitting entire payload */
	int tx_wait = 400;
	int tx_elapsed = 0;
	while (*gprs_xmit_ptr && tx_wait > 0) {
		vTaskDelay(5 / portTICK_RATE_MS);
		tx_elapsed += 5;
		tx_wait--;
	}
	vTaskDelay(20 / portTICK_RATE_MS);
	snprintf(log_tmp, sizeof(log_tmp), "HTTP: UART transmission complete (%d ms). Awaiting server response...", tx_elapsed + 20);
	sd_log_http(log_tmp);

	dest_in_count = 0;
	http_xfer_result = eXfer_In_Progress;
	http_response[dest_in_count] = 0;
	int got_200_ok = 0;
	int lines_received = 0;

	for(timeout=3000; (timeout>0) && (http_xfer_result == eXfer_In_Progress); timeout--){
		while(((lf_recd = getline((char *)0x2007c000, http_response, &gprs_recv_in_cnt, &gprs_recv_out_cnt,
				&dest_in_count, RECVBUF_SIZE, LINE_FEED)) != 1) && (timeout)) {
			vTaskDelay(10 / portTICK_RATE_MS);
			timeout--;
			if ((timeout % 100) == 0) {
				taskENTER_CRITICAL();
				gi_wdt_task_ok_stat |= WDT_GPRS_OK;
				taskEXIT_CRITICAL();
			}
		}

		if(lf_recd){
			lf_recd = 0;
			lines_received++;

			// Trim trailing whitespace, \r, \n
			int rlen = strlen(http_response);
			while (rlen > 0 && (http_response[rlen - 1] == '\r' || http_response[rlen - 1] == '\n' || http_response[rlen - 1] == ' ')) {
				http_response[--rlen] = '\0';
			}

			if (rlen > 0) {
				snprintf(log_tmp, sizeof(log_tmp), "HTTP: Rcvd <- %s", http_response);
				sd_log_http(log_tmp);
			}

			if(look_for_str(http_response, "200 OK") != 0){
				got_200_ok = 1;
				sd_log_http("HTTP: Status 200 OK confirmed!");
			}
			else if(look_for_str(http_response, "400") != 0 && look_for_str(http_response, "Bad") != 0){
				sd_log_http("HTTP: Server returned 400 Bad Request!");
			}
			else if(look_for_str(http_response, "403") != 0 && look_for_str(http_response, "Forbidden") != 0){
				sd_log_http("HTTP: Server returned 403 Forbidden!");
			}
			else if(look_for_str(http_response, "404") != 0 && look_for_str(http_response, "Not Found") != 0){
				sd_log_http("HTTP: Server returned 404 Not Found!");
			}
			else if(look_for_str(http_response, "500") != 0 && look_for_str(http_response, "Internal") != 0){
				sd_log_http("HTTP: Server returned 500 Internal Server Error!");
			}

			if((matching_ptr = look_for_str(http_response, "Date:")) != 0 ){
				sscanf(matching_ptr, "Date: %[^,], %d %s %d %d:%d", &gprs_new_day,
						&gprs_new_date, &gprs_new_mon, &gprs_new_year, &gprs_new_hour, &gprs_new_minute);
				for(gprs_month_no = 0; gprs_month_no < 12; gprs_month_no++)
					if(strcmp(gprs_months[gprs_month_no], gprs_new_mon) == 0)
						break;
				if (gprs_month_no < 12){
					gprs_new_month = gprs_month_no+1;
					gprs_new_minute += 30;
					if(gprs_new_minute >= 60){
						gprs_new_minute %= 60;
						gprs_new_hour += 6;
					}
					else
						gprs_new_hour += 5;
					if(gprs_new_hour >= 24){
						gprs_new_hour %= 24;
						Next_Date(&gprs_new_date, &gprs_new_month, &gprs_new_year);
					}
					gprs_server_date_Ok = 1;
					snprintf(log_tmp, sizeof(log_tmp), "HTTP: Server Clock Sync -> %04d-%02d-%02d %02d:%02d IST",
						gprs_new_year, gprs_new_month, gprs_new_date, gprs_new_hour, gprs_new_minute);
					sd_log_http(log_tmp);
				}
			}
			if((matching_ptr = look_for_str(http_response, "Success")) != 0 || 
			   (matching_ptr = look_for_str(http_response, "SUCCESS")) != 0 ||
			   (matching_ptr = look_for_str(http_response, "success")) != 0){
				http_xfer_result = eXfer_Success;
				http_status  = 1;
				gi_http_error = 0;
				sd_log_http("HTTP: Response body validated -> SUCCESS confirmed!");
			}
			else if ((matching_ptr = look_for_str(http_response, "Rejected")) != 0 ){
				http_xfer_result = eXfer_Rejected;
				gi_http_error = 1;
				sd_log_http("HTTP: Response body validated -> REJECTED by server!");
			}
			else if ((matching_ptr = look_for_str(http_response, "Invalid")) != 0 ){
				http_xfer_result = eXfer_Fail;
				gi_http_error = 1;
				sd_log_http("HTTP: Response body validated -> INVALID rejected by server!");
			}
			else if (look_for_str(http_response, "+CIPCLOSE: 0") != 0 || 
			         look_for_str(http_response, "CLOSED") != 0) {
				sd_log_http("HTTP: Remote server closed connection (+CIPCLOSE: 0)");
				if (got_200_ok) {
					http_xfer_result = eXfer_Success;
					http_status  = 1;
					gi_http_error = 0;
					sd_log_http("HTTP: Status 200 OK was received -> Treating as SUCCESS");
				} else {
					http_xfer_result = eXfer_Fail;
					gi_http_error = 1;
					sd_log_http("HTTP: Server closed without 200 OK -> FAILED");
				}
			}
			else
				vTaskDelay(10 / portTICK_RATE_MS);

			/* Reset line buffer and index for the next line */
			dest_in_count = 0;
			http_response[0] = 0;
		}
	}

	Send_And_Check_Gprs("AT+CIPCLOSE=0\r\n", "OK", "ERROR", 2000);

	if (timeout) {
		if (http_xfer_result == eXfer_Success) {
			sd_log_http("HTTP: Overall Result -> SUCCESS");
		} else if (http_xfer_result == eXfer_Rejected) {
			sd_log_http("HTTP: Overall Result -> REJECTED by server");
		} else {
			sd_log_http("HTTP: Overall Result -> FAILED / INVALID");
		}
		return(http_xfer_result);
	}
	else{
		snprintf(log_tmp, sizeof(log_tmp), "HTTP: Overall Result -> TIMEOUT awaiting server response (lines rx: %d)", lines_received);
		sd_log_http(log_tmp);
		gprs_timeout_occurred = 1;
		gi_http_error = 1;
		return(eXfer_Tmo);
	}
}

int http_send_data(){
	int res;

	// Attempt 1: Configured Primary Server (Domain name)
	sd_log_http("HTTP: [Attempt 1/3] Primary Server (Domain Name)...");
	res = http_send_data_single(Global_Data_Params.gc_http_server_name1, Global_Data_Params.gi_http_server_port1, Global_Data_Params.gc_http_link1);
	if (res == eXfer_Success) {
		return eXfer_Success;
	}

	// Attempt 2: Direct IP Fallback (KSNDMC direct server 117.216.42.181:80 or Spatika IP)
	vTaskDelay(1000 / portTICK_RATE_MS);
	if (look_for_str(Global_Data_Params.gc_http_server_name1, "ksndmc") != 0) {
		sd_log_http("HTTP: [Attempt 2/3] KSNDMC Direct IP Fallback (117.216.42.181:80)...");
		res = http_send_data_single("117.216.42.181", 80, Global_Data_Params.gc_http_link1);
	} else {
		sd_log_http("HTTP: [Attempt 2/3] Direct IP Fallback (144.91.104.105:80)...");
		res = http_send_data_single("144.91.104.105", 80, Global_Data_Params.gc_http_link1);
	}
	if (res == eXfer_Success) {
		return eXfer_Success;
	}

	// Attempt 3: Secondary Server Fallback (without destroying cellular attach)
	vTaskDelay(1000 / portTICK_RATE_MS);
	sd_log_http("HTTP: [Attempt 3/3] Secondary Server Backup...");
	res = http_send_data_single(Global_Data_Params.gc_http_server_name2, Global_Data_Params.gi_http_server_port2, Global_Data_Params.gc_http_link2);
	return res;
}




int send_message(char *gsm_no, char *msg_text)
{
	char send_msg_cmd[40];
	char dest_no[24] = {0};
	char msg_response[128];
	int timeout, lf_recd, dest_in_count;
	char clean_text[256];
	int k = 0;

	if (!gsm_no || strlen(gsm_no) < 5)
		return eNack;

	// Ensure destination number includes country code (+91) for Indian LTE networks
	if (gsm_no[0] == '+') {
		strncpy(dest_no, gsm_no, sizeof(dest_no) - 1);
	} else if (strlen(gsm_no) == 10) {
		snprintf(dest_no, sizeof(dest_no), "+91%s", gsm_no);
	} else {
		strncpy(dest_no, gsm_no, sizeof(dest_no) - 1);
	}

	taskENTER_CRITICAL();
	gi_wdt_task_ok_stat |= WDT_GPRS_OK;
	taskEXIT_CRITICAL();

	// 1. Ensure SMS text mode (CMGF=1)
	response_no = Send_And_Check_Gprs("AT+CMGF=1\r\n", "OK", "ERROR", 4000);
	if (response_no != 1) {
		return eNack;
	}
	vTaskDelay(100 / portTICK_RATE_MS);

	// 2. Set Character Set to standard GSM
	Send_And_Check_Gprs("AT+CSCS=\"GSM\"\r\n", "OK", "ERROR", 2000);
	vTaskDelay(100 / portTICK_RATE_MS);

	// 3. Initiate SMS send: AT+CMGS="<number>"\r
	snprintf(send_msg_cmd, sizeof(send_msg_cmd), "AT+CMGS=\"%s\"\r", dest_no);
	gprs_xmit_ptr = send_msg_cmd;
	UART_SendData((LPC_UART_TypeDef*) GPRS_UART, *gprs_xmit_ptr++);
	vTaskDelay(50 / portTICK_RATE_MS);

	// 4. Wait for '>' prompt from modem (up to 5 seconds)
	dest_in_count = 0;
	msg_response[0] = 0;
	timeout = 500; // 500 * 10ms = 5000ms
	int got_prompt = 0;
	while (timeout > 0) {
		lf_recd = getline((char *)0x2007c000, msg_response, &gprs_recv_in_cnt, &gprs_recv_out_cnt,
				&dest_in_count, RECVBUF_SIZE, '>');
		if (look_for_str(msg_response, ">") != 0) {
			got_prompt = 1;
			break;
		}
		vTaskDelay(10 / portTICK_RATE_MS);
		timeout--;
	}

	if (!got_prompt) {
		// Cancel CMGS if prompt never came (ESC 0x1B)
		Send_And_Check_Gprs("\x1B", "OK", "ERROR", 1000);
		return eNack;
	}

	vTaskDelay(50 / portTICK_RATE_MS);

	// 5. Build clean message payload terminated by single Ctrl+Z (0x1A)
	for (const char *p = msg_text; *p != '\0' && *p != '\032' && k < (sizeof(clean_text) - 2); p++) {
		clean_text[k++] = *p;
	}
	clean_text[k++] = 0x1A; // Ctrl+Z
	clean_text[k] = '\0';

	taskENTER_CRITICAL();
	gi_wdt_task_ok_stat |= WDT_GPRS_OK;
	taskEXIT_CRITICAL();

	// 6. Transmit payload and wait up to 15 seconds for network confirmation
	response_no = Send_And_Check_Gprs(clean_text, "+CMGS:", "ERROR", 15000);
	vTaskDelay(200 / portTICK_RATE_MS);

	return response_no;
}



void disconnect_http1(void){

	sprintf(gprs_xmit_buf,"AT+CIPCLOSE=0\r\n");
	response_no =  Send_And_Check_Gprs(gprs_xmit_buf, "+CIPCLOSE: 0,0", "ERROR", 5000);
	vTaskDelay(10 / portTICK_RATE_MS);

	sprintf(gprs_xmit_buf,"AT+NETCLOSE\r\n");
	response_no =  Send_And_Check_Gprs(gprs_xmit_buf, "+NETCLOSE: 0", "ERROR", 8000);
	vTaskDelay(10 / portTICK_RATE_MS);


}


int ftp_dota1(void)
{
	int xfr_res, bytes_recd, stat, reply, code_start;
	int store_in_sdc;
	char *matching_ptr;

	//AT+CFTPSGET="tws_sms.hex",0

	//char xmit_str[] = "AT+CFTPSGET=2,1024\r\n";	// to get 1024 bytes everytime
	char temp_buff[60];

	int lf_recd, response_in_count, send_cmd, timeout;

	send_cmd = 1;
	xfr_res = eXfer_Fail;
	gi_dota_error = 0;
	store_in_sdc = 0;

	sprintf(temp_buff,"AT+CFTPSGET=\"%s\",0\r\n",gc_dota_file_name);

	//sprintf(gprs_xmit_buf,"AT+CGDCONT=1,\"IP\",\"%s\"\r\n",gc_apn_str);


	gprs_xmit_ptr = temp_buff;
	UART_SendData((LPC_UART_TypeDef*) GPRS_UART, *gprs_xmit_ptr++);
	vTaskDelay(20 / portTICK_RATE_MS);	// wait a while for transmission to take place


	while(1){
		lf_recd = 0;
		response_in_count = 0;
		bytes_recd = 0;
		//	while(send_cmd == 0){
		gprs_response[0] = 0;
		timeout = 15000;
		while(( (lf_recd = getline((char *)0x2007c000, gprs_response, &gprs_recv_in_cnt, &gprs_recv_out_cnt,
				&response_in_count, RECVBUF_SIZE, ',')) != 1) &&(timeout)) {
			//	&response_in_count, RECVBUF_SIZE, LINE_FEED)) != 1) &&(timeout)) {
			vTaskDelay(10 / portTICK_RATE_MS); //sizeof(gprs_recv_buf)
			timeout--;
		}

		/*	while((0 != (matching_ptr = look_for_str(gprs_response, "+CFTPSGET: DATA,"))) && (timeout)){
			lf_recd =1;
			vTaskDelay(10 / portTICK_RATE_MS); //sizeof(gprs_recv_buf)
			timeout--;
		}*/

		if((!timeout)||(gi_dota_error)){
			xfr_res = eXfer_Fail;
			return xfr_res;
		}

		if(lf_recd){
			//+CFTPSGET: DATA,1024
			if(0 != (matching_ptr = look_for_str(gprs_response, "+CFTPSGET: DATA,"))){
				sscanf(gprs_response, "+CFTPSGET: DATA,%d", &bytes_recd);
				if(bytes_recd>0){
					code_start = gprs_recv_out_cnt;
					store_in_sdc = 0;


					if((code_start+bytes_recd)<=RECVBUF_SIZE){
						// we are within the boundary. just write
						XITC_Gprs_Disk_Fw.buf_ptr = (char *)(0x2007c000+code_start);
						XITC_Gprs_Disk_Fw.count = bytes_recd;
						XITC_Gprs_Disk_Fw.command = eFTP_Write_Buf;
						XITC_Gprs_Disk_Fw.signal = 1;
						while(XITC_Gprs_Disk_Fw.signal)
							vTaskDelay(1 / portTICK_RATE_MS);
					}
					else{
						// take care of boundary. first write a fewer bytes till the boundary is reached;
						// them from the top, the balance bytes
						XITC_Gprs_Disk_Fw.buf_ptr = (char *)(0x2007c000+code_start);
						XITC_Gprs_Disk_Fw.count = (RECVBUF_SIZE-code_start);
						XITC_Gprs_Disk_Fw.command = eFTP_Write_Buf;
						XITC_Gprs_Disk_Fw.signal = 1;
						while(XITC_Gprs_Disk_Fw.signal)
							vTaskDelay(1 / portTICK_RATE_MS);



						// write bytes_recd-(RECV_BUF_SIZE-gprs_recv_out_count) no of bytes from the top i.e. from (0x2007c000)
						XITC_Gprs_Disk_Fw.buf_ptr = (char *)(0x2007c000);
						XITC_Gprs_Disk_Fw.count = (bytes_recd - (RECVBUF_SIZE-code_start));
						XITC_Gprs_Disk_Fw.command = eFTP_Write_Buf;
						XITC_Gprs_Disk_Fw.signal = 1;
						while(XITC_Gprs_Disk_Fw.signal)
							vTaskDelay(1 / portTICK_RATE_MS);

					}
					bytes_recd = 0;

				}
			}

			else if(0 != (matching_ptr = look_for_str(gprs_response, "+CFTPSGET: 0"))){
				xfr_res = eXfer_Success;
				return xfr_res;
			}

			/*		else if (0 != (matching_ptr = look_for_str(gprs_response,"FTPGET: 1,"))){
					sscanf(gprs_response, "+FTPGET: 1,%d", &stat);
					if(stat==0){
						xfr_res = eXfer_Success;
						return xfr_res;
					}
					else if(64==stat){
						xfr_res = eXfer_Fail;
						return xfr_res;
					}
					else if(1==stat){
						send_cmd = 1;
						break;
					}
				}*/


			//else if (0 != (matching_ptr = look_for_str(gprs_response,"OK"))){
			if(store_in_sdc == 1){
				store_in_sdc = 0;
				if(bytes_recd>0){
					// here you set up command for writing onto disk

					if((code_start+bytes_recd)<=RECVBUF_SIZE){
						// we are within the boundary. just write
						XITC_Gprs_Disk_Fw.buf_ptr = (char *)(0x2007c000+code_start);
						XITC_Gprs_Disk_Fw.count = bytes_recd;
						XITC_Gprs_Disk_Fw.command = eFTP_Write_Buf;
						XITC_Gprs_Disk_Fw.signal = 1;
						while(XITC_Gprs_Disk_Fw.signal)
							vTaskDelay(1 / portTICK_RATE_MS);
					}
					else{
						// take care of boundary. first write a fewer bytes till the boundary is reached;
						// them from the top, the balance bytes
						XITC_Gprs_Disk_Fw.buf_ptr = (char *)(0x2007c000+code_start);
						XITC_Gprs_Disk_Fw.count = (RECVBUF_SIZE-code_start);
						XITC_Gprs_Disk_Fw.command = eFTP_Write_Buf;
						XITC_Gprs_Disk_Fw.signal = 1;
						while(XITC_Gprs_Disk_Fw.signal)
							vTaskDelay(1 / portTICK_RATE_MS);



						// write bytes_recd-(RECV_BUF_SIZE-gprs_recv_out_count) no of bytes from the top i.e. from (0x2007c000)
						XITC_Gprs_Disk_Fw.buf_ptr = (char *)(0x2007c000);
						XITC_Gprs_Disk_Fw.count = (bytes_recd - (RECVBUF_SIZE-code_start));
						XITC_Gprs_Disk_Fw.command = eFTP_Write_Buf;
						XITC_Gprs_Disk_Fw.signal = 1;
						while(XITC_Gprs_Disk_Fw.signal)
							vTaskDelay(1 / portTICK_RATE_MS);

					}
					bytes_recd = 0;
				}
				gprs_recv_out_cnt = gprs_recv_in_cnt;
			}
		}
		//	}
	}
}


void select_sim_0(void){
	GPIO_ClearValue(STATUS_KEY_PORT, STATUS_KEY_MASK);
	vTaskDelay(2000 / portTICK_RATE_MS);

}

void select_sim_1(void){
	GPIO_SetValue(STATUS_KEY_PORT, STATUS_KEY_MASK);
	vTaskDelay(2000 / portTICK_RATE_MS);

}

int prepare_http_record(int sector_idx, int sample_idx) {
	if (sector_idx < 0 || sector_idx >= 2 || sample_idx < 0 || sample_idx >= 96)
		return 0;

	Days_Data_def *sec = days_sector[sector_idx];
	Samples_def *s = &sec->samples[sample_idx];

	if (s->rf == 0xFFFF || s->year < 2020)
		return 0;

	char clean_stn_id[16];
	if (strlen(Global_Data_Params.gc_station_id) == 4 && (Global_Data_Params.gc_station_id[0] >= '0' && Global_Data_Params.gc_station_id[0] <= '9')) {
		sprintf(clean_stn_id, "00%s", Global_Data_Params.gc_station_id);
	} else {
		strncpy(clean_stn_id, Global_Data_Params.gc_station_id, sizeof(clean_stn_id) - 1);
		clean_stn_id[sizeof(clean_stn_id) - 1] = '\0';
	}

	float bat_volt1 = ((((float)(s->battery1) * 3.3f / 4095.0f) / 0.128f));
	float bat_volt2 = ((((float)(s->battery2) * 3.3f / 4095.0f) / 0.128f));

	float f_cur_rf = (s->rf != 0xFFFF) ? ((float)s->rf * 0.5f) : 0.0f;

	cumulative_rf_new(sec->date, sec->month, sec->year, s->hour, s->minute);
	float f_cuml_rf = (float)c_rainfall * 0.5f;

	int rain_dp_x10 = (int)(f_cur_rf * 10);
	int rain_cuml_x10 = (int)(f_cuml_rf * 10);
	int rain_cuml_x100 = (int)(f_cuml_rf * 100);
	int bat1_x10 = (int)(bat_volt1 * 10);
	int bat2_x10 = (int)(bat_volt2 * 10);

	int sig = (s->signal != 0) ? s->signal : gi_signal_level;

#if (UNIT_CFG == CFG_BIHAR_TRG)
	snprintf(gc_http_data, sizeof(gc_http_data),
			"{\"StnNo\":\"%s\",\"DeviceTime\":\"%04d-%02d-%02d %02d:%02d:00\",\"RainDP\":\"%03d.%1d\",\"RainCuml\":\"%03d.%1d\",\"BatVolt\":\"%02d.%1d\",\"SigStr\":\"%04d\",\"ApiKey\":\"%s\"}",
			clean_stn_id,
			s->year, s->month, s->date, s->hour, s->minute,
			rain_dp_x10 / 10, rain_dp_x10 % 10,
			rain_cuml_x10 / 10, rain_cuml_x10 % 10,
			bat1_x10 / 10, bat1_x10 % 10,
			sig,
			Global_Data_Params.gc_http_key1);
#elif (UNIT_CFG == CFG_KSNDMC_TRG)
	snprintf(gc_http_data, sizeof(gc_http_data),
			"stn_id=%s&rec_time=%04d-%02d-%02d,%02d:%02d&rainfall=%03d.%02d&signal=%04d&key=%s&bat_volt=%02d.%1d&bat_volt1=%02d.%1d",
			clean_stn_id,
			s->year, s->month, s->date, s->hour, s->minute,
			rain_cuml_x100 / 100, rain_cuml_x100 % 100,
			sig,
			Global_Data_Params.gc_http_key1,
			bat1_x10 / 10, bat1_x10 % 10,
			bat2_x10 / 10, bat2_x10 % 10);
#else
	Global_WS_Data_def pkg;
	pkg.temperature = ((float)s->temperature / 65536.0f) * 165.0f - 40.0f;
	pkg.humidity = ((float)s->humidity / 65534.0f) * 100.0f;
	pkg.wind_dir = ((int)(s->wind_direction * 0.088f)) % 360;
	pkg.avg_ws = (s->ws_sec_cnt > 0) ? (((float)(s->wind_speed / 4.0f)) / s->ws_sec_cnt) : 0.0f;
	pkg.avg_ws = (pkg.avg_ws * ws_calib_factor) + ws_offset;
	pkg.wind_dir = ((int)pkg.wind_dir * wd_calib_factor) + wd_offset;
	pkg.temperature = (pkg.temperature * temp_calib_factor) + temp_offset;
	pkg.humidity = (pkg.humidity * humid_calib_factor) + humid_offset;
	if (pkg.temperature < 2 || pkg.temperature > 50) pkg.temperature = 0;
	if (pkg.humidity < 0 || pkg.humidity > 100) pkg.humidity = 100;
	if (pkg.avg_ws < 0) pkg.avg_ws = 0;

	char s_temp[10], s_hum[10], s_ws[10], s_wd[8], s_bat[10];
	if (pkg.temperature < 0.0f) {
		snprintf(s_temp, sizeof(s_temp), "-%04.1f", -pkg.temperature);
	} else {
		snprintf(s_temp, sizeof(s_temp), "%05.1f", pkg.temperature);
	}
	snprintf(s_hum, sizeof(s_hum), "%05.1f", pkg.humidity);
	snprintf(s_ws, sizeof(s_ws), "%04.1f", pkg.avg_ws);
	snprintf(s_wd, sizeof(s_wd), "%03d", pkg.wind_dir);
	snprintf(s_bat, sizeof(s_bat), "%05.2f", bat_volt1);

	snprintf(gc_http_data, sizeof(gc_http_data),
			"stn_no=%s&rec_time=%04d-%02d-%02d,%02d:%02d&temp=%s&humid=%s&w_speed=%s&w_dir=%s&signal=%d&bat_volt=%s&key=%s",
			clean_stn_id,
			s->year, s->month, s->date, s->hour, s->minute,
			s_temp, s_hum, s_ws, s_wd,
			sig, s_bat,
			Global_Data_Params.gc_http_key1);
#endif
	return 1;
}

#define MAX_HTTP_BACKLOGS_PER_SLOT 8

void process_http_backlogs(void) {
	int backlogs_sent = 0;
	int sec_order[2];

	/* Determine chronological sector order (oldest day first) */
	uint32_t d0 = (uint32_t)days_sector[0]->year * 10000 + (uint32_t)days_sector[0]->month * 100 + days_sector[0]->date;
	uint32_t d1 = (uint32_t)days_sector[1]->year * 10000 + (uint32_t)days_sector[1]->month * 100 + days_sector[1]->date;

	if (days_sector[0]->year >= 2020 && days_sector[1]->year >= 2020) {
		if (d0 <= d1) {
			sec_order[0] = 0; sec_order[1] = 1;
		} else {
			sec_order[0] = 1; sec_order[1] = 0;
		}
	} else if (days_sector[0]->year >= 2020) {
		sec_order[0] = 0; sec_order[1] = -1;
	} else if (days_sector[1]->year >= 2020) {
		sec_order[0] = 1; sec_order[1] = -1;
	} else {
		return; /* No valid sectors */
	}

	for (int o = 0; o < 2 && sec_order[o] >= 0; o++) {
		int sec_idx = sec_order[o];
		Days_Data_def *sec = days_sector[sec_idx];

		for (int s_idx = 0; s_idx < 96; s_idx++) {
			/* Skip current record (already processed as present slot) */
			if ((sec_idx == (present_sector_no - 2)) && (s_idx == present_sample_no))
				continue;

			/* Check if this is an unsent valid backlog */
			if (sec->samples[s_idx].rf != 0xFFFF &&
			    sec->samples[s_idx].year >= 2020 &&
			    sec->samples[s_idx].http_status != 1) {

				/* Format HTTP payload for this backlogged slot */
				if (!prepare_http_record(sec_idx, s_idx))
					continue;

				char bk_msg[128];
				sprintf(bk_msg, ">>> Sending Backlog #%d: %04d-%02d-%02d %02d:%02d",
					backlogs_sent + 1,
					sec->samples[s_idx].year, sec->samples[s_idx].month, sec->samples[s_idx].date,
					sec->samples[s_idx].hour, sec->samples[s_idx].minute);
				sd_log_http(bk_msg);

				/* Watchdog heartbeat */
				taskENTER_CRITICAL();
				gi_wdt_task_ok_stat |= WDT_GPRS_OK;
				taskEXIT_CRITICAL();

				/* Send HTTP packet */
				int res = http_send_data();
				vTaskDelay(100 / portTICK_RATE_MS);

				if (res == eXfer_Success) {
					/* Immediately mark this backlog as sent in Flash so it is never resent */
					mark_http_sample_sent_in_flash(sec_idx, s_idx);
					backlogs_sent++;
					sprintf(bk_msg, ">>> Backlog #%d SUCCESS! Marked in Flash.", backlogs_sent);
					sd_log_http(bk_msg);

					if (backlogs_sent >= MAX_HTTP_BACKLOGS_PER_SLOT) {
						/* Reached quota for this 15-minute slot */
						sd_log_http(">>> Backlog quota reached for this slot.");
						return;
					}
					/* Carrier breather delay (Rule 60) to allow tower TCP stack reset */
					vTaskDelay(2500 / portTICK_RATE_MS);
				} else {
					/* Transmission failed / network lost:
					 * Leave this backlog marked as 0 so next 15-minute slot starts right here.
					 * Abort further backlog sends this cycle. */
					sd_log_http(">>> Backlog send FAILED! Aborting backlogs.");
					return;
				}
			}
		}
	}

	if (backlogs_sent == 0) {
		sd_log_http(">>> No unsent HTTP backlogs found in Flash.");
	}
}

void prepare_present_rec_for_http_sms(void){
	if(gi_signal_level > 0)
		get_signal_level();

	int sec_idx = present_sector_no - 2;
	int s_idx = present_sample_no;

	if (sec_idx < 0 || sec_idx >= 2 || s_idx < 0 || s_idx >= 96) {
		if (latest_Sector >= 2 && latest_Sector < 4 && latest_sample_num >= 0 && latest_sample_num < 96) {
			sec_idx = latest_Sector - 2;
			s_idx = latest_sample_num;
		} else {
			sec_idx = 0;
			s_idx = 0;
		}
	}

	/* Try to prepare from flash sample */
	if (!prepare_http_record(sec_idx, s_idx)) {
		/* Flash sample empty or invalid (e.g. freshly booted / on-demand trigger):
		 * build live HTTP packet directly from current live sensor data */
		char clean_stn_id[16];
		if (strlen(Global_Data_Params.gc_station_id) == 4 && (Global_Data_Params.gc_station_id[0] >= '0' && Global_Data_Params.gc_station_id[0] <= '9')) {
			sprintf(clean_stn_id, "00%s", Global_Data_Params.gc_station_id);
		} else {
			strncpy(clean_stn_id, Global_Data_Params.gc_station_id, sizeof(clean_stn_id) - 1);
			clean_stn_id[sizeof(clean_stn_id) - 1] = '\0';
		}

		float bat_volt = (last_rec_15min_bat1 > 0) ? ((((float)last_rec_15min_bat1 * 3.3f / 4095.0f) / 0.128f)) : 12.5f;
		int yr = (xPresent_Date_Time.year >= 2000) ? xPresent_Date_Time.year : (xPresent_Date_Time.year > 0 ? (2000 + xPresent_Date_Time.year) : 2026);
		char s_temp[10], s_hum[10], s_ws[10], s_wd[8], s_bat[10];
		if (Packet_Data.temperature < 0.0f) {
			snprintf(s_temp, sizeof(s_temp), "-%04.1f", -Packet_Data.temperature);
		} else {
			snprintf(s_temp, sizeof(s_temp), "%05.1f", Packet_Data.temperature);
		}
		snprintf(s_hum, sizeof(s_hum), "%05.1f", Packet_Data.humidity);
		snprintf(s_ws, sizeof(s_ws), "%04.1f", Packet_Data.avg_ws);
		snprintf(s_wd, sizeof(s_wd), "%03d", Packet_Data.wind_dir);
		snprintf(s_bat, sizeof(s_bat), "%05.2f", bat_volt);

		snprintf(gc_http_data, sizeof(gc_http_data),
				"stn_no=%s&rec_time=%04d-%02d-%02d,%02d:%02d&temp=%s&humid=%s&w_speed=%s&w_dir=%s&signal=%d&bat_volt=%s&key=%s",
				clean_stn_id,
				yr, xPresent_Date_Time.month ? xPresent_Date_Time.month : 1, xPresent_Date_Time.date ? xPresent_Date_Time.date : 1,
				xPresent_Date_Time.hour, xPresent_Date_Time.minute,
				s_temp, s_hum, s_ws, s_wd,
				gi_signal_level, s_bat,
				Global_Data_Params.gc_http_key1);
	}

	sd_log_http("HTTP: Prepared Payload ->");
	sd_log_http(gc_http_data);

	float b1 = (sec_idx >= 0 && sec_idx < 2 && s_idx >= 0 && s_idx < 96) ?
		((((float)(days_sector[sec_idx]->samples[s_idx].battery1) * 3.3f / 4095.0f) / 0.128f)) : 12.5f;
	float b2 = (sec_idx >= 0 && sec_idx < 2 && s_idx >= 0 && s_idx < 96) ?
		((((float)(days_sector[sec_idx]->samples[s_idx].battery2) * 3.3f / 4095.0f) / 0.128f)) : 12.5f;

	sprintf(gc_sms_data, "%s,%s,WS-DATA,%s,%04d-%02d-%02dT%02d:%02d,Y,%c%.1f,%.1f,%.1f,%d,%d,%.2f,%.2f",
			Global_Data_Params.gc_network_name,
			Global_Data_Params.gc_station_type,
			Global_Data_Params.gc_station_id,
			xPresent_Date_Time.year,
			xPresent_Date_Time.month,
			xPresent_Date_Time.date,
			xPresent_Date_Time.hour,
			xPresent_Date_Time.minute,
			plus_minus_sym,
			Packet_Data.temperature,
			Packet_Data.humidity,
			Packet_Data.avg_ws,
			Packet_Data.wind_dir,
			gi_signal_level,
			b1,
			b2
	);

	vTaskDelay(1 / portTICK_RATE_MS);
}



char *mv2endof_str(char* src, char* match, int len)
{

	char *s, *m;

	s = src;
	m = match;
	int found = 0;

	while(*s && len){
		if(*s == *m){
			s++;
			m++;
			if(*m == 0){
				found = 1;
				break;
			}
		}
		else{
			s++;
			m = match;
		}
		len--;
	}

	if (!found)
		s = 0;
	return (s);
}




void vGprs_Task( void *pvParameters){

	char c, gprs_recv_loc_buf[300];
	int  gprs_loc_buf_in_cnt;
	int gprs_msg_processed_date;
	int lf_recd;
	int response_in_count;

	int msg_no;
	int iCountTemp;
	int xfer_result;
	int http_server_connected;
	int ftp_server_connected;

	int year1, month1, date1, hour1, minute1, second1;

	int dd, mm,yy, sector_no;
	int found;

	xfer_result = 0;
	gi_sig_timeout = 0;
	gprs_server_date_Ok = 0;
	gps_found = 0;
	file_saved = 0;

	vTaskDelay(100 / portTICK_RATE_MS);
	ptr_RAM2 = (char *)0x2007c000;
	for(iCountTemp=0; iCountTemp<1024*8; iCountTemp++){
		*(ptr_RAM2+iCountTemp) = 0xFF;
	}
	for(;iCountTemp<1024*12; iCountTemp++){
		*(ptr_RAM2+iCountTemp) = iCountTemp%256;
	}


	*((char *)0x2007c000) = 0;
	*((char *)0x20080000) = 0; /* Clear AHB Bank 1 HTTP buffer */
	*((char *)0x20080800) = 0; /* Clear AHB Bank 1 FTP buffer */
	gprs_recv_loc_buf[0] = 0;
	gprs_loc_buf_in_cnt = 0;

	gprs_timeout_occurred = 0;
	gi_sim_state = 0;
	gi_sim_reg = 0;
	gi_dota_error = 0;
	gi_gps_error = 0;
	gi_http_error = 0;
	gprs_msg_processed_date = 0;

	gprs_server_date_Ok = 0;
	ftp_mode = 0;
	http_server_connected = 0;
	ftp_server_connected = 0;
	http_status = 0;
	ftp_status = 0;
	ftp_line_cnt = 0;
	ftp_rec_no = 0;
	get_gps = 0;

	// init pins and then UART meant for GPRS
	define_gprs_uart_pins((LPC_UART_TypeDef*) GPRS_UART);
	vTaskDelay(300 / portTICK_RATE_MS);
	init_gprs_uart((LPC_UART_TypeDef*) GPRS_UART);
	vTaskDelay(300 / portTICK_RATE_MS);

	for(msg_no = 0; msg_no < MAX_NO_OF_MSGS; msg_no++)
		message_recd[msg_no] = 0;

	while(!memory_task_started)
		vTaskDelay(100 / portTICK_RATE_MS);

	select_sim_0();  //select default sim for simcard holder 1
	vTaskDelay(2000 / portTICK_RATE_MS);

	//initial delay for module
	for (int d = 0; d < 15; d++) {
		taskENTER_CRITICAL();
		gi_wdt_task_ok_stat |= WDT_GPRS_OK;
		taskEXIT_CRITICAL();
		vTaskDelay(1000 / portTICK_RATE_MS);
	}

	check_basic_commands();
	vTaskDelay(200 / portTICK_RATE_MS);

	check_sim_cpin();
	vTaskDelay(200 / portTICK_RATE_MS);

	get_signal_level();
	vTaskDelay(200 / portTICK_RATE_MS);

	if(gi_sim_state != 1){
		select_sim_1();  //select secondary  sim for simcard holder 2
		vTaskDelay(2000 / portTICK_RATE_MS);

		for (int d = 0; d < 10; d++) {
			taskENTER_CRITICAL();
			gi_wdt_task_ok_stat |= WDT_GPRS_OK;
			taskEXIT_CRITICAL();
			vTaskDelay(1000 / portTICK_RATE_MS);
		}

		check_basic_commands();
		vTaskDelay(200 / portTICK_RATE_MS);

		check_sim_cpin();
		vTaskDelay(200 / portTICK_RATE_MS);

		get_signal_level();
		vTaskDelay(200 / portTICK_RATE_MS);

		vTaskDelay(2000 / portTICK_RATE_MS);
		if(gi_sim_state == 1)
			Global_Data_Params.gi_sim_select = 1;
	}
	else
		Global_Data_Params.gi_sim_select = 0;


	/* Enable Network Time Zone Update (NITZ) before registration */
	Send_And_Check_Gprs("AT+CTZU=1\r\n", "OK", "ERROR", 1000);
	Send_And_Check_Gprs("AT+CTZR=1\r\n", "OK", "ERROR", 1000);
	vTaskDelay(50 / portTICK_RATE_MS);

	check_sim_registration();
	vTaskDelay(200 / portTICK_RATE_MS);

	get_apn();
	vTaskDelay(200 / portTICK_RATE_MS);

	/* Trigger cellular network time sync on startup */
	if (gi_sim_state == 1) {
		get_gprs_time = 1;
	}

	// Configure active antenna power (3.05V) and start SIM7600 GNSS engine early in background
	Send_And_Check_Gprs("AT+CVAUXV=3050\r\n", "OK", "ERROR", 1000);
	vTaskDelay(50 / portTICK_RATE_MS);
	Send_And_Check_Gprs("AT+CVAUXS=1\r\n", "OK", "ERROR", 1000);
	vTaskDelay(50 / portTICK_RATE_MS);
	Send_And_Check_Gprs("AT+CGPSAUTO=1\r\n", "OK", "ERROR", 1000);
	vTaskDelay(50 / portTICK_RATE_MS);
	Send_And_Check_Gprs("AT+CGPS=1\r\n", "OK", "ERROR", 2000);
	vTaskDelay(200 / portTICK_RATE_MS);

	int bg_gps_ticks = 0;

	for(;;){

		taskENTER_CRITICAL();
		gi_wdt_task_ok_stat |= WDT_GPRS_OK;
		taskEXIT_CRITICAL();
		vTaskDelay(1000 / portTICK_RATE_MS);

		// Background periodic satellite lock check if coordinates not yet acquired
		if (Global_Data_Params.gps_latitude <= 0.0f) {
			bg_gps_ticks++;
			if (bg_gps_ticks >= 30) {
				bg_gps_ticks = 0;
				response_no = Send_And_Check_Gprs("AT+CGPSINFO\r\n", "+CGPSINFO:", "ERROR", 2000);
				char *p_bg = look_for_str(gprs_response, "+CGPSINFO:");
				if (response_no == 1 && p_bg != 0) {
					char *p = p_bg + 10;
					while (*p == ' ') p++;
					if (*p != ',' && *p != '\0') {
						char lat_buf[16] = {0}, lon_buf[16] = {0};
						char lat_c = 'N', lon_c = 'E';
						int idx = 0;
						while (*p != ',' && *p != '\0' && idx < 15) lat_buf[idx++] = *p++;
						if (*p == ',') p++;
						if (*p != ',' && *p != '\0') lat_c = *p++;
						if (*p == ',') p++;
						idx = 0;
						while (*p != ',' && *p != '\0' && idx < 15) lon_buf[idx++] = *p++;
						if (*p == ',') p++;
						if (*p != ',' && *p != '\0') lon_c = *p++;

						float parsed_lat = parse_nmea_coord(lat_buf);
						float parsed_lon = parse_nmea_coord(lon_buf);

						if (parsed_lat > 0.0f && parsed_lon > 0.0f) {
							GPS.latitude = parsed_lat;
							GPS.lat_dir = (lat_c == 'S') ? 'S' : 'N';
							GPS.longitude = parsed_lon;
							GPS.lon_dir = (lon_c == 'W') ? 'W' : 'E';
							gps_found = 1;
							Global_Data_Params.gps_latitude = GPS.latitude;
							Global_Data_Params.gps_lat_dir = GPS.lat_dir;
							Global_Data_Params.gps_longitude = GPS.longitude;
							Global_Data_Params.gps_lon_dir = GPS.lon_dir;
							write_param_sector();

							// Parse GPS Date and UTC Time (format: ddmmyy,hhmmss.s)
							if (*p == ',') p++;
							char date_buf[8] = {0}, time_buf[12] = {0};
							idx = 0;
							while (*p != ',' && *p != '\0' && idx < 7) date_buf[idx++] = *p++;
							if (*p == ',') p++;
							idx = 0;
							while (*p != ',' && *p != '\0' && idx < 11) time_buf[idx++] = *p++;

							int g_dd = 0, g_mm = 0, g_yy = 0;
							int g_hh = 0, g_min = 0, g_sec = 0;
							if (sscanf(date_buf, "%02d%02d%02d", &g_dd, &g_mm, &g_yy) == 3 &&
							    sscanf(time_buf, "%02d%02d%02d", &g_hh, &g_min, &g_sec) >= 2) {
								g_min += 30;
								if (g_min >= 60) { g_min -= 60; g_hh++; }
								g_hh += 5;
								if (g_hh >= 24) { g_hh -= 24; g_dd++; }
								g_yy += 2000;
								if (g_dd >= 1 && g_dd <= 31 && g_mm >= 1 && g_mm <= 12 && g_yy >= 2024 && g_yy <= 2079) {
									xSet_Date_Time.date   = g_dd;
									xSet_Date_Time.month  = g_mm;
									xSet_Date_Time.year   = g_yy;
									xSet_Date_Time.hour   = g_hh;
									xSet_Date_Time.minute = g_min;
									xSet_Date_Time.second = g_sec;
									set_time_to_rtc = 1;
								}
							}
						}
					}
				}
			}
		}

		if(process_msgs == 1){
			for(msg_no=1; msg_no<=MAX_NO_OF_MSGS; msg_no++){
				GPRS_Done.sms = 0;
				process_msg(msg_no);
				message_recd[msg_no] = 0;
				GPRS_Done.sms = 1;
				process_msgs = 2;
			}
			gprs_msg_processed_date = xPresent_Date_Time.date;
		}


		/*	if(get_gps_time == 1){
			get_gps_info();
			get_gps_time = 0;

		}*/

		//		get_gprs_time = 1;

		/* Periodic retry for cellular time sync until acquired */
		if (auto_time_set == 0 && gi_sim_state == 1) {
			static int time_sync_retry_cnt = 0;
			time_sync_retry_cnt++;
			if (time_sync_retry_cnt >= 15) {
				time_sync_retry_cnt = 0;
				get_gprs_time = 1;
			}
		}

		if(get_gprs_time == 1){
			if(gi_sim_state == 1){

				Send_And_Check_Gprs("\r\nAT+CTZU=1\r\n", "OK", "ERROR", 2000);	// Auto time zone update
				Send_And_Check_Gprs("\r\nAT+CTZR=1\r\n", "OK", "ERROR", 2000);	// Time zone reporting
				vTaskDelay(50 / portTICK_RATE_MS);

				response_no = Send_And_Check_Gprs("\r\nAT+CCLK?\r\n", "+CCLK", "ERROR", 4000);
				vTaskDelay(200 / portTICK_RATE_MS);

				if(response_no == eAck){
					// Example: +CCLK: "26/10/28,09:25:00+22"
					char *cclk_p = look_for_str(gprs_response, "+CCLK");
					if (cclk_p != 0) {
						char *quote = look_for_str(cclk_p, "\"");
						if (quote != 0) {
							int yr = 0, mo = 0, da = 0, hr = 0, mi = 0, se = 0;
							if (sscanf(quote + 1, "%d/%d/%d,%d:%d:%d", &yr, &mo, &da, &hr, &mi, &se) >= 5) {
								if (yr < 100) yr += 2000;
								if (da >= 1 && da <= 31 && mo >= 1 && mo <= 12 && yr >= 2024 && yr <= 2079) {
									xSet_Date_Time.date   = da;
									xSet_Date_Time.month  = mo;
									xSet_Date_Time.year   = yr;
									xSet_Date_Time.hour   = hr;
									xSet_Date_Time.minute = mi;
									xSet_Date_Time.second = (se >= 0 && se <= 59) ? se : 0;

									set_time_to_rtc = 1; /* Transmit $Syyyymmddhhmmss; to LPC845 RTC */
									auto_time_set = 1;
									get_gprs_time = 0;
									xPresent_Date_Time = xSet_Date_Time;
								}
							}
						}
					}
				}
				get_gprs_time = 0;
			}
		}



		if(wait_for_sms_process == 1){
			if((gi_sim_state == 1) && (gi_sim_reg == 1)){
				for(msg_no=1; msg_no<=MAX_NO_OF_MSGS; msg_no++){
					process_msg(msg_no);
					message_recd[msg_no] = 0;
					process_msgs = 0;
				}
			}
			wait_for_sms_process = 2;
		}

		if(gi_data_ready_for_report == 1){
			char log_tmp[160];
			gi_data_ready_for_report = 0;

			sd_log_http("========================================");
			sd_log_http("=== HTTP TELEMETRY TRANSMISSION START ===");
			sprintf(log_tmp, "Station ID: %s, Firmware: %s, Model: %s",
				Global_Data_Params.gc_station_id,
				Global_Data_Params.gc_version,
				Global_Data_Params.gc_model);
			sd_log_http(log_tmp);
			sprintf(log_tmp, "Cellular: Operator=%s, Signal=%d dBm, Reg=%d, APN=%s",
				gc_operator_name,
				gi_signal_level, gi_sim_reg, gc_apn_str);
			sd_log_http(log_tmp);
			sprintf(log_tmp, "Endpoints: Pri=%s:%d%s, Sec=%s:%d%s",
				Global_Data_Params.gc_http_server_name1, Global_Data_Params.gi_http_server_port1, Global_Data_Params.gc_http_link1,
				Global_Data_Params.gc_http_server_name2, Global_Data_Params.gi_http_server_port2, Global_Data_Params.gc_http_link2);
			sd_log_http(log_tmp);

			prepare_present_rec_for_http_sms();
			vTaskDelay(10 / portTICK_RATE_MS);

			/* Ensure SIM is registered before attempting HTTP connection */
			if ((gi_sim_reg != 1) && (gi_sim_state == 1)) {
				sd_log_http("HTTP: Polling cellular registration before connection...");
				check_sim_registration();
				if (gi_sim_reg == 1) {
					get_apn();
				}
			}

			/* Spatika / KSNDMC 62-Second Slot Boundary Guard (Rule 1):
			 * KSNDMC and Spatika aggregate servers reject packets received < 62s
			 * after the 15-min slot boundary. Hold until T+1m02s before connecting. */
			int m_into = xPresent_Date_Time.minute % 15;
			int s_into = xPresent_Date_Time.second;
			int total_elapsed = (m_into * 60) + s_into;
			if (total_elapsed < 62) {
				int wait_sec = 62 - total_elapsed;
				sprintf(log_tmp, "HTTP: Slot elapsed %ds (< 62s). Holding %ds to reach T+1m02s...", total_elapsed, wait_sec);
				sd_log_http(log_tmp);
				for (int w = 0; w < wait_sec; w++) {
					vTaskDelay(1000 / portTICK_RATE_MS);
					taskENTER_CRITICAL();
					gi_wdt_task_ok_stat |= WDT_GPRS_OK;
					taskEXIT_CRITICAL();
				}
			}

			//	if((http_server_connected == 0) && (gi_sim_state == 1)){
			if((http_server_connected == 0) && (gi_sim_state == 1) && (gi_sim_reg == 1)){

				http_server_connected = connect_http1();
				vTaskDelay(100 / portTICK_RATE_MS);
			}

			response_no = eNack;

			if(http_server_connected == 1){
				response_no = http_send_data();
				vTaskDelay(100 / portTICK_RATE_MS);
			}

			/* If first attempt failed, retry connection and transmission once */
			if((response_no != eXfer_Success) && (gi_sim_state == 1) && (gi_sim_reg == 1)){
				sd_log_http("HTTP: First attempt failed, retrying in 3 seconds...");
				vTaskDelay(3000 / portTICK_RATE_MS);
				http_server_connected = connect_http1();
				vTaskDelay(100 / portTICK_RATE_MS);
				if(http_server_connected == 1){
					response_no = http_send_data();
					vTaskDelay(100 / portTICK_RATE_MS);
				}
			}

			if(response_no ==  eXfer_Success){
				sd_log_http("=== HTTP TELEMETRY TRANSMISSION SUCCESS ===");
				sd_log_http("========================================");
				http_status = 1;		//update http_flag in flash

				gi_present_error = 0;  // clear present error
				gi_log_date	  = xLast_Rec_15min_Date_Time.date;
				gi_log_month  = xLast_Rec_15min_Date_Time.month;
				gi_log_year	  = xLast_Rec_15min_Date_Time.year;
				gi_log_hour	  = xLast_Rec_15min_Date_Time.hour;
				gi_log_minute = xLast_Rec_15min_Date_Time.minute;
				// store last logged date time in params file
				XITC_Gprs_Disk.command = eSave_Params;
				XITC_Gprs_Disk.signal = 1;
				while(XITC_Gprs_Disk.signal)
					vTaskDelay(100 / portTICK_RATE_MS);

				// Immediately mark present sample as sent in flash
				mark_http_sample_sent_in_flash(present_sector_no - 2, present_sample_no);
				sprintf(log_tmp, "HTTP: Present sample marked SENT in Flash (Sector %d, Sample %d)", present_sector_no - 2, present_sample_no);
				sd_log_http(log_tmp);
				vTaskDelay(100 / portTICK_RATE_MS);

				// Check and send backlogs one after the other
				process_http_backlogs();
			}
			else{
				sd_log_http("=== HTTP TELEMETRY TRANSMISSION FAILED ===");
				sd_log_http("========================================");
				// increment the net error count and store
				http_status = 0;
				gi_present_error++;
				gi_net_error++;
				save_err_det();	//save error details in SDC
				vTaskDelay(100 / portTICK_RATE_MS);
			}

			//Sync time from HTTP Server Date response
			if(gprs_server_date_Ok){
				int min_diff = (int)gprs_new_minute - (int)xPresent_Date_Time.minute;
				if (min_diff < 0) min_diff = -min_diff;

				if((gprs_new_year != xPresent_Date_Time.year) || (gprs_new_month != xPresent_Date_Time.month) ||
						(gprs_new_date != xPresent_Date_Time.date) || (gprs_new_hour != xPresent_Date_Time.hour) || (min_diff >= 1)){
					xSet_Date_Time.date		= gprs_new_date;
					xSet_Date_Time.month	= gprs_new_month;
					xSet_Date_Time.year		= gprs_new_year;
					xSet_Date_Time.hour		= gprs_new_hour;
					xSet_Date_Time.minute	= gprs_new_minute;
					xSet_Date_Time.second   = 0;

					set_time_to_rtc = 1;
					xPresent_Date_Time = xSet_Date_Time;
					vTaskDelay(100 / portTICK_RATE_MS);
				}
			}


			///////////////////DOTA


			//		gi_start_dota = 1;

			if(gi_start_dota == 1){

				if(gi_disk_ok == 1){
					if((gi_sim_state == 1) && (gi_sim_reg == 1)){
						response_no = connect_ftp2();
						vTaskDelay(200 / portTICK_RATE_MS);

						response_no = ftp_dota();
						//ftp_dota();
						vTaskDelay(200 / portTICK_RATE_MS);

						if(file_saved == 1){
							gi_start_dota = 0;

							XITC_Gprs_Disk.command = eCheck_Dota_File;
							XITC_Gprs_Disk.signal = 1;
							while(XITC_Gprs_Disk.signal)
								vTaskDelay(100 / portTICK_RATE_MS);

							XITC_Gprs_Disk.command = eSave_Params;
							XITC_Gprs_Disk.signal = 1;
							while(XITC_Gprs_Disk.signal)
								vTaskDelay(100 / portTICK_RATE_MS);
						}
						else{
							XITC_Gprs_Disk.command = eDelete_Dota_File;
							XITC_Gprs_Disk.signal = 1;
							while(XITC_Gprs_Disk.signal)
								vTaskDelay(100 / portTICK_RATE_MS);
						}
					}
				}
				else{
					gi_start_dota = 0;
					sprintf(status_response, "SDC-Problem Firmware unable to update\r\n\032");

					response_no = send_message(Global_Data_Params.gc_universal_gsm, status_response);
					vTaskDelay(100 / portTICK_RATE_MS);

					XITC_Gprs_Disk.command = eSave_Params;
					XITC_Gprs_Disk.signal = 1;
					while(XITC_Gprs_Disk.signal)
						vTaskDelay(100 / portTICK_RATE_MS);
				}
			}


			/////////////////DOTA

			//////////Check FTP for present day
			if(http_status == 1)
				ftp_rec_no = present_sample_no - 1;
			else
				ftp_rec_no = present_sample_no;

			if((sampleNo % 5) != 0) {
				while(ftp_rec_no >= 0){
					if((days_sector[present_sector_no-2]->samples[ftp_rec_no].http_status != 1) && (days_sector[present_sector_no-2]->samples[ftp_rec_no].ftp_status != 1) && ((days_sector[present_sector_no-2]->samples[ftp_rec_no].rf != 0xFFFF))){

						//			cumulative_rf1(days_sector[present_sector_no-2]->date, days_sector[present_sector_no-2]->month, days_sector[present_sector_no-2]->year,
						//					days_sector[present_sector_no-2]->samples[ftp_rec_no].hour, days_sector[present_sector_no-2]->samples[ftp_rec_no].minute);

						//			vTaskDelay(1 / portTICK_RATE_MS);

						//			ftp_rf = c_rainfall;
						f_bat_volt1 = ((((float)(days_sector[present_sector_no-2]->samples[ftp_rec_no].battery1) * 3.3/4095)/0.128));
						f_bat_volt2 = ((((float)(days_sector[present_sector_no-2]->samples[ftp_rec_no].battery2) * 3.3/4095)/0.128));

						f_temp = ((float)days_sector[present_sector_no-2]->samples[ftp_rec_no].temperature/65536) * 165 - 40;




						f_humid = ((float)days_sector[present_sector_no-2]->samples[ftp_rec_no].humidity/65534) * 100;

						f_wd = ((int)(days_sector[present_sector_no-2]->samples[ftp_rec_no].wind_direction * 0.088)) % 360;		// 360/4095 = 0.088

						if (days_sector[present_sector_no-2]->samples[ftp_rec_no].ws_sec_cnt > 0) {
							f_ws = ((float)(days_sector[present_sector_no-2]->samples[ftp_rec_no].wind_speed / 4)) / days_sector[present_sector_no-2]->samples[ftp_rec_no].ws_sec_cnt;
						} else {
							f_ws = 0.0f;
						}

						f_ws = (f_ws * ws_calib_factor ) + ws_offset;
						f_wd = ((int)f_wd * wd_calib_factor) + wd_offset;
						f_temp = (f_temp * temp_calib_factor) + temp_offset;
						f_humid = (f_humid * humid_calib_factor) + humid_offset;

						if((f_temp < 2) || (f_temp > 60)){
							f_temp = 0;
						}

						if(f_humid < 6){
							f_humid = 0;
						}

						if(f_humid > 100)
							f_humid = 100;

						if(f_ws < 0)
							f_ws = 0;


						char temp_str[8];
						int t_val = (int)(f_temp * 10);
						if (t_val < 0) {
							int pos_t = -t_val;
							snprintf(temp_str, sizeof(temp_str), "-%02d.%1d", pos_t / 10, pos_t % 10);
						} else {
							snprintf(temp_str, sizeof(temp_str), "%03d.%1d", t_val / 10, t_val % 10);
						}

						char clean_id[8];
						if (strlen(Global_Data_Params.gc_station_id) == 4 && (Global_Data_Params.gc_station_id[0] >= '0' && Global_Data_Params.gc_station_id[0] <= '9')) {
							snprintf(clean_id, sizeof(clean_id), "00%s", Global_Data_Params.gc_station_id);
						} else {
							snprintf(clean_id, sizeof(clean_id), "%06s", Global_Data_Params.gc_station_id);
						}

						int h_val = (int)(f_humid * 10);
						int w_val = (int)(f_ws * 10);
						int b_val = (int)(f_bat_volt1 * 100);

						// Strict 57-byte format: <STN_ID>;YYYY-MM-DD,HH:MM;<TEMP>;<HUMID>;<WS>;<WD>;<SIGNAL>;<BATTERY>\r\n
						snprintf(ftp_rec, sizeof(ftp_rec),
								"%06s;%04d-%02d-%02d,%02d:%02d;%5s;%03d.%1d;%02d.%1d;%03d;%04d;%02d.%02d\r\n",
								clean_id,
								days_sector[present_sector_no-2]->samples[ftp_rec_no].year,
								days_sector[present_sector_no-2]->samples[ftp_rec_no].month,
								days_sector[present_sector_no-2]->samples[ftp_rec_no].date,
								days_sector[present_sector_no-2]->samples[ftp_rec_no].hour,
								days_sector[present_sector_no-2]->samples[ftp_rec_no].minute,
								temp_str,
								h_val / 10, h_val % 10,
								w_val / 10, w_val % 10,
								f_wd,
								days_sector[present_sector_no-2]->samples[ftp_rec_no].signal,
								b_val / 100, b_val % 100);

						if (strlen(ftp_rec) == 57) {
							strcat(ftp_buff, ftp_rec);
							ftp_flag[ftp_line_cnt] = ftp_rec_no;
							ftp_line_cnt++;
						}

					}

					ftp_rec_no--;
					//	if((ftp_line_cnt > 20))
					if((ftp_line_cnt > 16))
						break;
				}

			}

			if((ftp_line_cnt > 0) && (gi_sim_state == 1)){

				if(gi_sim_reg == 1){
					ftp_server_connected = connect_ftp1();
					vTaskDelay(200 / portTICK_RATE_MS);
				}

				//response_no == eNack;

				if(ftp_server_connected == 1)
					ftp_send_data();

				vTaskDelay(200 / portTICK_RATE_MS);

			}

			if(ftp_status == 1){
				gi_present_error = 0;  // clear present error
				gi_log_date	  = xLast_Rec_15min_Date_Time.date;
				gi_log_month  = xLast_Rec_15min_Date_Time.month;
				gi_log_year	  = xLast_Rec_15min_Date_Time.year;
				gi_log_hour	  = xLast_Rec_15min_Date_Time.hour;
				gi_log_minute = xLast_Rec_15min_Date_Time.minute;
				// store last logged date time in params file
				XITC_Gprs_Disk.command = eSave_Params;
				XITC_Gprs_Disk.signal = 1;
				while(XITC_Gprs_Disk.signal)
					vTaskDelay(100 / portTICK_RATE_MS);
			}

			//	http_status = 0;
			//	ftp_status = 0;
			//	Global_Data_Params.gi_send_data_sms = 1;

			if(http_status == 0){
				if((xLast_Rec_15min_Date_Time.hour == 8) && (xLast_Rec_15min_Date_Time.minute == 30)){

					if(gi_disk_ok == 1){
						sprintf(status_response,"%s,%04d-%02d-%02dT%02d:%02d,%d\r\n\032",
								gc_sms_data,
								gi_log_year,
								gi_log_month,
								gi_log_date,
								gi_log_hour,
								gi_log_minute,
								gi_present_error);

						response_no = send_message(Global_Data_Params.gc_master_gsm, status_response);
						vTaskDelay(100 / portTICK_RATE_MS);
					}

					if((gi_disk_ok != 1) || (response_no != 1)){

						sprintf(status_response,"%s,%04d-%02d-%02dT%02d:%02d,%d\r\n\032",
								gc_sms_data,
								gi_log_year,
								gi_log_month,
								gi_log_date,
								gi_log_hour,
								gi_log_minute,
								gi_present_error);

						response_no = send_message(Global_Data_Params.gc_universal_gsm, status_response);
						vTaskDelay(100 / portTICK_RATE_MS);
					}
				}
			}

			if((http_status == 0) && (ftp_status == 0)){

				if(Global_Data_Params.gi_send_data_sms == 1){
					sprintf(status_response,"%s,%04d-%02d-%02dT%02d:%02d,%d\r\n\032",
							gc_sms_data,
							gi_log_year,
							gi_log_month,
							gi_log_date,
							gi_log_hour,
							gi_log_minute,
							gi_present_error);

					response_no = send_message(Global_Data_Params.gc_master_gsm, status_response);
					vTaskDelay(100 / portTICK_RATE_MS);
				}

				/*			if((xLast_Rec_15min_Date_Time.hour == 8) && (xLast_Rec_15min_Date_Time.minute == 30)){

					auto_dump_date  = xLast_Rec_15min_Date_Time.date;
					auto_dump_month = xLast_Rec_15min_Date_Time.month;
					auto_dump_year	= xLast_Rec_15min_Date_Time.year;

					XITC_Gprs_Disk.signal = 1;
					XITC_Gprs_Disk.command = ePrepare_Auto_Dump;
					while(XITC_Gprs_Disk.signal)
						vTaskDelay(100 / portTICK_RATE_MS);
					sprintf(dump_buf, "%s\032",dump_buf);
					send_message(Global_Data_Params.gc_universal_gsm, dump_buf);
				}*/
				//	sleep_mode = 1;

			}


			if(firmware_updated == 1){
				sprintf(status_response,"%s,%s,STATUS-C,%s,%04d-%02d-%02dT%02d:%02d,%s-%c,%d,%04d,%.2f,%.2f,%c,%s,%04d-%02d-%02dT%02d:%02d,%d\r\n\032",
						Global_Data_Params.gc_network_name,
						Global_Data_Params.gc_station_type,
						Global_Data_Params.gc_station_id,
						xPresent_Date_Time.year,
						xPresent_Date_Time.month,
						xPresent_Date_Time.date,
						xPresent_Date_Time.hour,
						xPresent_Date_Time.minute,
						Global_Data_Params.gc_version,
						lpc845_ver,
						Global_Data_Params.gi_report_interval,
						gi_signal_level,
						gf_bat_volt1,
						gf_bat_volt2,
						solar_status,
						(gi_disk_ok ? "SDC-OK" : "SDC-FAIL"),
						gi_log_year,
						gi_log_month,
						gi_log_date,
						gi_log_hour,
						gi_log_minute,
						gi_present_error
				);
				response_no = send_message(Global_Data_Params.gc_universal_gsm, status_response);
				vTaskDelay(100 / portTICK_RATE_MS);
				firmware_updated = 0;
			}

			if((http_status == 1) || (ftp_status == 1)){
				update_present_day_flag = 1;
			}
			else{
				if((xLast_Rec_15min_Date_Time.hour == 8) && (xLast_Rec_15min_Date_Time.minute == 30))
					store_ftp_sdc = 1;
			}


			//////////////////////////////////
			if((gi_disk_ok == 1) && (ftp_status == 0)){
				//ftp_status = 0;
				// read the line number from which FTP is to be done
				XITC_Gprs_Disk.command = eGetLineNo;
				XITC_Gprs_Disk.signal = 1;
				while(XITC_Gprs_Disk.signal)
					vTaskDelay(5 / portTICK_RATE_MS);
				ftp_start_line_no = XITC_Gprs_Disk.line_no;

				// open the local file
				XITC_Gprs_Disk.command = eFile_Open;
				sprintf(XITC_Gprs_Disk.filename, "ftpdata.txt");
				XITC_Gprs_Disk.data[0] = 0;
				XITC_Gprs_Disk.signal = 1;
				while(XITC_Gprs_Disk.signal)
					vTaskDelay(5 / portTICK_RATE_MS);

				if(XITC_Gprs_Disk.result == eFile_Not_Found){
					ftp_in_sdc = 0;
				}



				if(XITC_Gprs_Disk.result == eFile_Opened){

					XITC_Gprs_Disk.line_no = ftp_start_line_no;
					XITC_Gprs_Disk.command = eSkipLines;
					XITC_Gprs_Disk.signal = 1;
					while (XITC_Gprs_Disk.signal)
						vTaskDelay(5 / portTICK_RATE_MS);

					if(XITC_Gprs_Disk.result != eFile_End){

						for(ftp_line_no=0;ftp_line_no<16;ftp_line_no++){
							XITC_Gprs_Disk.command = eFile_Get_Line;
							XITC_Gprs_Disk.data[0] = 0;
							XITC_Gprs_Disk.result = eLine_Read_In_Progress;
							XITC_Gprs_Disk.signal = 1;
							while (XITC_Gprs_Disk.signal)
								vTaskDelay(5 / portTICK_RATE_MS);

							strcat(ftp_buff,XITC_Gprs_Disk.data);

							if(XITC_Gprs_Disk.result==eFile_End)
								break;
						}
					}

					if((ftp_line_no > 0) && (gi_sim_state == 1)){

						/*	if(gi_sim_reg == 1){
							ftp_server_connected = connect_ftp1();
							vTaskDelay(200 / portTICK_RATE_MS);
						}*/

						//response_no == eNack;
						vTaskDelay(20 / portTICK_RATE_MS);

						if(ftp_server_connected == 1){
							ftp_send_data();
							vTaskDelay(10 / portTICK_RATE_MS);

						}
						else{
							if(gi_sim_reg == 1){
								ftp_server_connected = connect_ftp1();
								vTaskDelay(200 / portTICK_RATE_MS);
							}
							if(ftp_server_connected == 1)
								ftp_send_data();

						}

						vTaskDelay(200 / portTICK_RATE_MS);

					}

					XITC_Gprs_Disk.command = eFile_Close;
					XITC_Gprs_Disk.signal = 1;
					while (XITC_Gprs_Disk.signal)
						vTaskDelay(5 / portTICK_RATE_MS);



					if(ftp_status == 1){
						if(ftp_line_no < 15){
							XITC_Gprs_Disk.line_no = 1;

						}
						else
							XITC_Gprs_Disk.line_no += ftp_line_no;

						XITC_Gprs_Disk.command = eSetLineNo;
						XITC_Gprs_Disk.signal = 1;
						while(XITC_Gprs_Disk.signal)
							vTaskDelay(5 / portTICK_RATE_MS);

						if(ftp_line_no < 15){
							XITC_Gprs_Disk.command = eDel_Ftp_File;
							XITC_Gprs_Disk.signal = 1;
							while(XITC_Gprs_Disk.signal)
								vTaskDelay(100 / portTICK_RATE_MS);
						}

						vTaskDelay(200 / portTICK_RATE_MS);

					}
				}
			}

			Send_And_Check_Gprs("AT+CPOF\r\n", "OK", "ERROR", 2000);
			sleep_mode = 1;
		}

		else if (XITC_Ui_Gprs.signal) {
			XITC_Ui_Gprs.result = eUi_Gprs_Cmd_In_Progress;

			// process Ui request
			if(XITC_Ui_Gprs.command == eUi_Gprs_Get_Sglvl){
				//int sig_lvl;

				get_signal_level();
				XITC_Ui_Gprs.result = eUi_Gprs_Sglvl_Done;

			}

			else if (XITC_Ui_Gprs.command == eUi_Gprs_Get_CPIN){
				check_sim_cpin();

				if(gi_sim_state == 1)
					XITC_Ui_Gprs.result = eUi_Gprs_CPIN_Done;
				else
					XITC_Ui_Gprs.result = eUi_Gprs_Cmd_Fail;
			}

			else if (XITC_Ui_Gprs.command == eUi_Gprs_Get_CREG){
				check_sim_registration();

				if(gi_sim_reg == 1)
					XITC_Ui_Gprs.result = eUi_Gprs_CPIN_Done;
				else
					XITC_Ui_Gprs.result = eUi_Gprs_Cmd_Fail;
			}


			else if(XITC_Ui_Gprs.command == eUi_Gprs_Send_Status){
				sprintf(status_response,"%s,%s,STATUS-F,%s,%04d-%02d-%02dT%02d:%02d,%s-%c,%d,%04d,%.2f,%.2f,%c,%s,%04d-%02d-%02dT%02d:%02d,%d\r\n\032",
						Global_Data_Params.gc_network_name,
						Global_Data_Params.gc_station_type,
						Global_Data_Params.gc_station_id,
						xPresent_Date_Time.year,
						xPresent_Date_Time.month,
						xPresent_Date_Time.date,
						xPresent_Date_Time.hour,
						xPresent_Date_Time.minute,
						Global_Data_Params.gc_version,
						lpc845_ver,
						Global_Data_Params.gi_report_interval,
						gi_signal_level,
						gf_bat_volt1,
						gf_bat_volt2,
						solar_status,
						(gi_disk_ok ? "SDC-OK" : "SDC-FAIL"),
						gi_log_year,
						gi_log_month,
						gi_log_date,
						gi_log_hour,
						gi_log_minute,
						gi_present_error
				);
				response_no = send_message(Global_Data_Params.gc_universal_gsm, status_response);
				vTaskDelay(100 / portTICK_RATE_MS);
				if (response_no == 1)
					XITC_Ui_Gprs.result = eUi_Gprs_Send_Status_Done;
				else
					XITC_Ui_Gprs.result = eUi_Gprs_Cmd_Fail;
			}


			else if(XITC_Ui_Gprs.command == eUi_Gprs_Send_Status1){
				sprintf(status_response,"%s,%s,DEV_INFO,%s,%s,%04d-%02d-%02dT%02d:%02d\r\n\032",
						Global_Data_Params.gc_network_name,
						Global_Data_Params.gc_station_type,
						Global_Data_Params.gc_station_id,
						msg_data,
						xPresent_Date_Time.year,
						xPresent_Date_Time.month,
						xPresent_Date_Time.date,
						xPresent_Date_Time.hour,
						xPresent_Date_Time.minute);
				response_no = send_message(Global_Data_Params.gc_universal_gsm, status_response);
				vTaskDelay(100 / portTICK_RATE_MS);

			}


			else if(XITC_Ui_Gprs.command == eUI_Gps_Info){
				get_gps_info();
				if(gps_found == 1)
					XITC_Ui_Gprs.result = eUI_Gps_Info_A;
				else
					XITC_Ui_Gprs.result = eUI_Gps_Info_NA;
			}

			XITC_Ui_Gprs.signal = 0;
		}

		else {
			// no other request... idling... check if we have any new message received
			lf_recd = response_in_count = 0;
			gprs_response[0] =0;
			if(( (lf_recd = getline((char *)0x2007c000, gprs_response, &gprs_recv_in_cnt, &gprs_recv_out_cnt,
					&response_in_count, RECVBUF_SIZE, LINE_FEED)) == 1) ) {    //sizeof(gprs_recv_buf)
				if (0 != (matching_ptr = look_for_str(gprs_response, "+CMTI:"))){
					sscanf(matching_ptr, "+CMTI: \"SM\",%d", &msg_no);
					if(msg_no <= MAX_NO_OF_MSGS)
						message_recd[msg_no] = 1;
				}
				vTaskDelay(1 / portTICK_RATE_MS);
			}
		}

		// Let us process the message
		for(msg_no=1; msg_no<=MAX_NO_OF_MSGS; msg_no++){
			if ( (message_recd[msg_no]) ){
				GPRS_Done.sms = 0;
				process_msg(msg_no);
				message_recd[msg_no] = 0;
				GPRS_Done.sms = 1;
			}
		}
	}
}
