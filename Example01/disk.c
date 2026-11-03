/* Includes */
#include <stdio.h>
#include <string.h>
#include "globals.h"
#include "disk.h"
#include "FreeRTOS.h"
#include "diskio.h"
#include "ff.h"
#include "task.h"
#include "gprs.h"
#include "memory.h"
#include "ui.h"

/* local variables */
FATFS Fatfs[_DRIVES];
FIL  File_Sd, File_Usb, File_Sd_Dota;
BYTE Buff[8*512];
UINT BytesWritten, BytesRead;
char l1;
FRESULT fresult;
char *fname_Sd; // ="Test.txt";
char *fname_Usb; // ="1:/21052012.txt";
//char loc_buff[150];
FRESULT fret,fresult;
char src_filename[24];
char dest_filename[24];
char read_buf[150];
//char get_last_rec[150];
int iTempCnt;
char loc_buff[150];

char *fgets_ret_val, *matching_ptr;


/*
 * -------------------------------------------------------------------------------------------------
 * Function getLineNoforFTP
 * This will read the file ftpline.txt and read the record to get the present line number value
 * Argument: NIL
 * Return: present line number as integer
 * if the file does not exist, it creates the file and set the present line number as 1
 * -------------------------------------------------------------------------------------------------
 */
int getLineNoforFTP()
{
	int pr_line_no;
	char *matching_ptr;
	char *fgets_ret_val;

	pr_line_no = 1;						// Let us assume this to be 1
	fret = f_mount(0,&Fatfs[0]);
	fresult=f_open(&File_Sd, "ftpline.txt", FA_OPEN_EXISTING | FA_READ);
	if (fresult != FR_OK){
		fresult=f_open(&File_Sd, "ftpline.txt", FA_CREATE_ALWAYS | FA_WRITE);
		if(fresult == FR_OK){
			sprintf(loc_buff, "PRESENT LINE: %d\r\n", pr_line_no);
			f_write(&File_Sd, loc_buff, strlen(loc_buff), &BytesWritten);
			f_close(&File_Sd);
		}
		fresult=f_open(&File_Sd, "ftpline.txt", FA_OPEN_EXISTING | FA_READ);
	}
	while((fgets_ret_val = f_gets(read_buf, sizeof(read_buf), &File_Sd))!= (char *)-1){
		if ((matching_ptr=look_for_str(read_buf, "PRESENT LINE: "))!=0){
			// retrieve station name and number here
			sscanf(matching_ptr, "PRESENT LINE: %d", &pr_line_no);
		}
		if(f_eof(&File_Sd)){
			break;
		}
	}
	f_close(&File_Sd);
	return(pr_line_no);
}



void setLineNoforFTP(int pr_line_no)
{
	fret = f_mount(0,&Fatfs[0]);
	fresult=f_open(&File_Sd, "ftpline.txt", FA_OPEN_EXISTING | FA_WRITE);
	if (fresult == FR_OK){
		sprintf(loc_buff, "PRESENT LINE: %d\r\n", pr_line_no);
		f_write(&File_Sd, loc_buff, strlen(loc_buff), &BytesWritten);
		f_close(&File_Sd);
	}
}




void check_sdc_status(void){
	//create a file name with spatika.txt and written a string Welcome
	char cmp_str[16], *match_ptr;

	sprintf(cmp_str, "Welcome\r\n");

	//writing
	fret = f_mount(0,&Fatfs[0]);
	fresult=f_open(&File_Sd, "spatika.txt", FA_OPEN_ALWAYS | FA_WRITE);
	if(fresult == FR_OK){
		f_write(&File_Sd, "Welcome\r\n", 10,&BytesWritten);
		f_close(&File_Sd);
		gi_disk_ok = 1;
	}
	else
		gi_disk_ok = 0;


	//reading
	fret = f_mount(0,&Fatfs[0]);
	fresult=f_open(&File_Sd, "spatika.txt", FA_OPEN_EXISTING | FA_READ);
	if (fresult == FR_OK){
		while((fgets_ret_val = f_gets(read_buf,sizeof(read_buf),&File_Sd))!= (char *)-1){
			matching_ptr = look_for_str(read_buf, "Welcome\r\n");
			if(matching_ptr != 0){
				gi_disk_ok = 1;
				break;
			}
			if(f_eof(&File_Sd)){
				break;
			}
		}
		f_close(&File_Sd);
	}
	else
		gi_disk_ok = 0;
}

void get_params(void){
	fret = f_mount(0,&Fatfs[0]), *matching_ptr;
	fresult=f_open(&File_Sd, "params.txt", FA_OPEN_EXISTING | FA_READ);
	if (fresult != FR_OK){
		XITC_Memory_Disk.result = eNo_File;

	}

	else if (fresult == FR_OK){
		while((fgets_ret_val = f_gets(read_buf,sizeof(read_buf),&File_Sd))!= (char *)-1){
			if ((matching_ptr=look_for_str(read_buf, "STATION:"))!=0){
				// retrieve station name and number here
				sscanf(matching_ptr, "STATION: Name=%[^,],Number=%s", &Global_Data_Params.gc_station_name, &gc_station_id1);
			}
			else if((matching_ptr=look_for_str(read_buf, "VERSION:"))!=0){
				sscanf(matching_ptr, "VERSION: %s\r\n", &gc_version_sdc);
			}

			else if ((matching_ptr=look_for_str(read_buf, "ERROR:"))!=0) {
				sscanf(matching_ptr, "ERROR: P=%d,C=%d", &gi_present_error, &gi_net_error);
			}


			else if((matching_ptr=look_for_str(read_buf, "CALIB:"))!=0){
				sscanf(matching_ptr, "CALIB: Status=%d, On=%02d-%02d-%04d\r\n",
						&Cailb_Status_Details.status, &Cailb_Status_Details.date,&Cailb_Status_Details.month,&Cailb_Status_Details.year);
			}

			else if((matching_ptr=look_for_str(read_buf, "DOTA:"))!=0){
				sscanf(matching_ptr, "DOTA: Flag=%d, Flag1=%d, File=%s\r\n", &gi_start_dota, &gi_start_dota_lpc824, &gc_dota_file_name);
			}

			else if((matching_ptr=look_for_str(read_buf, "LDT:"))!=0){
				sscanf(matching_ptr, "LDT: Date=%02d-%02d-%04d, Time=%02d:%02d",
						&gi_log_date, &gi_log_month, &gi_log_year, &gi_log_hour, &gi_log_minute);
			}

			else if((matching_ptr=look_for_str(read_buf, "ACTUAL_WS1="))!=0){
				sscanf(matching_ptr, "ACTUAL_WS1=%f, MEASURED_WS1=%f, ACTUAL_WS2=%f, MEASURED_WS2=%f", &ws_actual1, &ws_measued1, &ws_actual2, &ws_measued2);
			}

			else if((matching_ptr=look_for_str(read_buf, "ACTUAL_TEMPERATURE1="))!=0){
				sscanf(matching_ptr, "ACTUAL_TEMPERATURE1=%f, MEASURED_TEMPERATURE1=%f, ACTUAL_TEMPERATURE2=%f, MEASURED_TEMPERATURE2=%f", &temp_actual1, &temp_measued1, &temp_actual2, &temp_measued2);
			}

			else if((matching_ptr=look_for_str(read_buf, "ACTUAL_HUMIDITY1="))!=0){
				sscanf(matching_ptr, "ACTUAL_HUMIDITY1=%f, MEASURED_HUMIDITY1=%f, ACTUAL_HUMIDITY2=%f, MEASURED_HUMIDITY2=%f", &humid_actual1, &humid_measued1, &humid_actual2, &humid_measued2);
			}

			else if((matching_ptr=look_for_str(read_buf, "ACTUAL_WD1="))!=0){
				sscanf(matching_ptr, "ACTUAL_WD1=%f, MEASURED_WD1=%f, ACTUAL_WD2=%f, MEASURED_WD2=%f", &wd_actual1, &wd_measued1, &wd_actual2, &wd_measued2);
			}

			if(f_eof(&File_Sd)){
				//f_close(&File_Sd);
				break;
			}
		}
		f_close(&File_Sd);
	}
}


void prepare_write_params(void)
{
	fret = f_mount(0,&Fatfs[0]);
	fresult=f_open(&File_Sd, "params.txt", FA_CREATE_ALWAYS | FA_WRITE);
	if(fresult == FR_OK){
		sprintf(Buff, "STATION: Name=%s,Number=%s\r\n", Global_Data_Params.gc_station_name, gc_station_id1);
		sprintf(Buff, "%sVERSION: %s\r\n",Buff, gc_version_sdc);
		sprintf(Buff, "%sERROR: P=%d,C=%d\r\n", Buff, gi_present_error, gi_net_error);
		sprintf(Buff, "%sCALIB: Status=%d, On=%02d-%02d-%04d\r\n", Buff, Cailb_Status_Details.status,
				Cailb_Status_Details.date,Cailb_Status_Details.month,Cailb_Status_Details.year);
		sprintf(Buff, "%sDOTA: Flag=%d, Flag1=%d,File=%s\r\n",Buff, gi_start_dota, gi_start_dota_lpc824, gc_dota_file_name);
		sprintf(Buff, "%sLDT: Date=%02d-%02d-%04d, Time=%02d:%02d\r\n",Buff,gi_log_date, gi_log_month, gi_log_year, gi_log_hour, gi_log_minute);
		sprintf(Buff, "%sACTUAL_WS1=%.2f, MEASURED_WS1=%.2f, ACTUAL_WS2=%.2f, MEASURED_WS2=%.2f\r\n", Buff, ws_actual1, ws_measued1, ws_actual2, ws_measued2);
		sprintf(Buff, "%sACTUAL_TEMPERATURE1=%.2f, MEASURED_TEMPERATURE1=%.2f, ACTUAL_TEMPERATURE2=%.2f, MEASURED_TEMPERATURE2=%.2f\r\n", Buff, temp_actual1, temp_measued1, temp_actual2, temp_measued2);
		sprintf(Buff, "%sACTUAL_HUMIDITY1=%.2f, MEASURED_HUMIDITY1=%.2f, ACTUAL_HUMIDITY2=%.2f, MEASURED_HUMIDITY2=%.2f \r\n", Buff, humid_actual1, humid_measued1, humid_actual2, humid_measued2);
		sprintf(Buff, "%sACTUAL_WD1=%.2f, MEASURED_WD1=%.2f, ACTUAL_WD2=%.2f, MEASURED_WD2=%.2f\r\n", Buff, wd_actual1, wd_measued1, wd_actual2, wd_measued2);

		f_write(&File_Sd, Buff, strlen(Buff),&BytesWritten);
		f_close(&File_Sd);
		f_write(&File_Sd, Buff, strlen(Buff),&BytesWritten);
		f_close(&File_Sd);
	}

}


void prepare_auto_dump(int date, int month, int year)
{
	/*
	  KSNDMC,TWS-RF,RF-DATA
	  STATION=999999
	  DATE=31-12-99
	  REC=Y
	  D0830=999.9
	  D0430=999.9
	  D0030=999.9
	  D2030=999.9
	  D1630=999.9
	  D1230=999.9

	  ////
		10:30 = rf_val[0]
		12:30 = rf_val[1]
		14:30 = rf_val[2]
		16:30 = rf_val[3]
		18:30 = rf_val[4]
		20:30 = rf_val[5]
		22:30 = rf_val[6]
		00:30 = rf_val[7]
		02:30 = rf_val[8]
		04:30 = rf_val[9]
		06:30 = rf_val[10]
		08:30 = rf_val[11]

	 */

	char file_name[16];
	char *matching_ptr;
	char loc_stn[8];
	char *fgets_ret_val;
	int i,record=0;
	int loc_date, loc_month, loc_year, loc_hour, loc_min, loc_rain1, loc_rain2;
	float loc_rain;
	int cur_date, cur_month, cur_year;
	//	float rf_val[6] = {-1, -1, -1, -1, -1, -1};
	float rf_val[12] = {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1};
	// current file
	cur_date = date;
	cur_month = month;
	cur_year = year;
	sprintf(file_name, "%02d%02d%04d.txt", date, month, year);
	fret = f_mount(0,&Fatfs[0]);
	fresult=f_open(&File_Sd, file_name, FA_OPEN_EXISTING | FA_READ);
	if(fresult == FR_OK){
		while((fgets_ret_val = f_gets(read_buf,sizeof(read_buf),&File_Sd))!= (char *)-1){
			// stn_no=SP1060,rec_time=2013-02-02,23:45:00,rainfall=001.5,signal=-78,bat_vol=00.00
			if((matching_ptr=look_for_str(read_buf, "rainfall"))!=0){
				if((matching_ptr=look_for_str(read_buf, ","))!=0)
					sscanf(matching_ptr, ",rec_time=%04d-%02d-%02d,%02d:%02d,rainfall=%f",
							&loc_year, &loc_month, &loc_date, &loc_hour, &loc_min, &loc_rain);
			}

			if((loc_hour == 8) && (loc_min == 30))
				rf_val[11] = loc_rain;

			if((loc_hour == 6) && (loc_min == 30))
				rf_val[10] = loc_rain;

			if((loc_hour == 4) && (loc_min == 30))
				rf_val[9] = loc_rain;

			if((loc_hour == 2) && (loc_min == 30))
				rf_val[8] = loc_rain;

			if((loc_hour == 0) && (loc_min == 30))
				rf_val[7] = loc_rain;
			if(f_eof(&File_Sd))
				break;

		}
		record = 1;
		f_close(&File_Sd);
	}
	// previous file
	Previous_Date(&date, &month, &year);

	sprintf(file_name, "%02d%02d%04d.txt", date, month, year);
	fret = f_mount(0,&Fatfs[0]);
	fresult=f_open(&File_Sd, file_name, FA_OPEN_EXISTING | FA_READ);
	if(fresult == FR_OK){
		while((fgets_ret_val = f_gets(read_buf,sizeof(read_buf),&File_Sd))!= (char *)-1){
			// stn_no=SP1060,rec_time=2013-02-02,23:45:00,rainfall=001.5,signal=-78,bat_vol=00.00
			if((matching_ptr=look_for_str(read_buf, "rainfall"))!=0){
				if((matching_ptr=look_for_str(read_buf, ","))!=0)
					sscanf(matching_ptr, ",rec_time=%04d-%02d-%02d,%02d:%02d,rainfall=%f",
							&loc_year, &loc_month, &loc_date, &loc_hour, &loc_min, &loc_rain);
			}
			if((loc_hour == 10) && (loc_min == 30))
				rf_val[0] = loc_rain;

			if((loc_hour == 12) && (loc_min == 30))
				rf_val[1] = loc_rain;

			if((loc_hour == 14) && (loc_min == 30))
				rf_val[2] = loc_rain;

			if((loc_hour == 16) && (loc_min == 30))
				rf_val[3] = loc_rain;

			if((loc_hour == 18) && (loc_min == 30))
				rf_val[4] = loc_rain;

			if((loc_hour == 20) && (loc_min == 30))
				rf_val[5] = loc_rain;

			if((loc_hour == 22) && (loc_min == 30))
				rf_val[6] = loc_rain;

			if(f_eof(&File_Sd))
				break;

		}
		record = 1;
		f_close(&File_Sd);
	}

	sprintf(dump_buf,"%s,%s,RF-DATA,%s,%04d-%02d-%02d,",
			Global_Data_Params.gc_network_name, Global_Data_Params.gc_station_type, Global_Data_Params.gc_station_id, cur_year, cur_month, cur_date);
	if(record == 1)
		sprintf(dump_buf, "%sY,",dump_buf);
	else
		sprintf(dump_buf, "%sN,",dump_buf);

	if(rf_val[0] == -1)
		sprintf(dump_buf, "%sNIL,", dump_buf);
	else
		sprintf(dump_buf, "%s%.1f,", dump_buf, rf_val[0]);

	if(rf_val[1] == -1)
		sprintf(dump_buf, "%sNIL,", dump_buf);
	else
		sprintf(dump_buf, "%s%.1f,", dump_buf, rf_val[1]);

	if(rf_val[2] == -1)
		sprintf(dump_buf, "%sNIL,", dump_buf);
	else
		sprintf(dump_buf, "%s%.1f,", dump_buf, rf_val[2]);

	if(rf_val[3] == -1)
		sprintf(dump_buf, "%sNIL,", dump_buf);
	else
		sprintf(dump_buf, "%s%.1f,", dump_buf, rf_val[3]);

	if(rf_val[4] == -1)
		sprintf(dump_buf, "%sNIL,", dump_buf);
	else
		sprintf(dump_buf, "%s%.1f,", dump_buf, rf_val[4]);

	if(rf_val[5] == -1)
		sprintf(dump_buf, "%sNIL,", dump_buf);
	else
		sprintf(dump_buf, "%s%.1f,", dump_buf, rf_val[5]);

	if(rf_val[6] == -1)
		sprintf(dump_buf, "%sNIL,", dump_buf);
	else
		sprintf(dump_buf, "%s%.1f,", dump_buf, rf_val[6]);

	if(rf_val[7] == -1)
		sprintf(dump_buf, "%sNIL,", dump_buf);
	else
		sprintf(dump_buf, "%s%.1f,", dump_buf, rf_val[7]);

	if(rf_val[8] == -1)
		sprintf(dump_buf, "%sNIL,", dump_buf);
	else
		sprintf(dump_buf, "%s%.1f,", dump_buf, rf_val[8]);

	if(rf_val[9] == -1)
		sprintf(dump_buf, "%sNIL,", dump_buf);
	else
		sprintf(dump_buf, "%s%.1f,", dump_buf, rf_val[9]);

	if(rf_val[10] == -1)
		sprintf(dump_buf, "%sNIL,", dump_buf);
	else
		sprintf(dump_buf, "%s%.1f,", dump_buf, rf_val[10]);

	if(rf_val[11] == -1)
		sprintf(dump_buf, "%sNIL,%04d-%02d-%02dT%02d:%02d,%d\r\n\032", dump_buf,gi_log_year,gi_log_month,gi_log_date,gi_log_hour,gi_log_minute,gi_present_error);
	else
		sprintf(dump_buf, "%s%.1f,%04d-%02d-%02dT%02d:%02d,%d\r\n\032", dump_buf, rf_val[11],gi_log_year,gi_log_month,gi_log_date,gi_log_hour,gi_log_minute, gi_present_error);

}


void vDisk_Task(void *pvParameters)
{

	int file_opened, file_created;
	int valid_dota_file;

	vTaskDelay(500 / portTICK_RATE_MS);

	gi_disk_ok = 0;
	valid_dota_file = 0;
	file_created = 0;
	file_opened = 0;

	check_sdc_status();
	vTaskDelay(100 / portTICK_RATE_MS);


	vTaskDelay(5000 / portTICK_RATE_MS);

	for(;;){

		taskENTER_CRITICAL();
		gi_wdt_task_ok_stat |= WDT_DISK_OK;
		taskEXIT_CRITICAL();
		vTaskDelay(1000 / portTICK_RATE_MS);

		/* Periodic retry to auto-detect SD Card if not yet mounted or hot-plugged */
		static int sdc_retry_cnt = 0;
		if (!gi_disk_ok) {
			if (++sdc_retry_cnt >= 10) {
				sdc_retry_cnt = 0;
				check_sdc_status();
			}
		}

		vTaskDelay(100 / portTICK_RATE_MS);


		if(XITC_Memory_Disk.signal == 1){
			// Logging to disk starts here..
			if(XITC_Memory_Disk.command == eLog){
				int log_yr = XITC_Memory_Disk.year;
				if (log_yr < 100 && log_yr > 0) log_yr += 2000;
				if (log_yr < 2020) log_yr = 2026;
				int log_mo = (XITC_Memory_Disk.month >= 1 && XITC_Memory_Disk.month <= 12) ? XITC_Memory_Disk.month : 1;
				int log_dt = (XITC_Memory_Disk.date >= 1 && XITC_Memory_Disk.date <= 31) ? XITC_Memory_Disk.date : 1;

				sprintf(src_filename, "%02d%02d%04d.txt", log_dt, log_mo, log_yr);

				fret = f_mount(0,&Fatfs[0]);

				fresult=f_open(&File_Sd, src_filename, FA_WRITE|FA_OPEN_ALWAYS);
				if(fresult == FR_OK){
					f_lseek(&File_Sd, File_Sd.fsize);
					f_write(&File_Sd, XITC_Memory_Disk.record, strlen(XITC_Memory_Disk.record),&BytesWritten);
					f_close(&File_Sd);
					gi_disk_ok = 1;
				} else {
					gi_disk_ok = 0;
				}
			}

			else if(XITC_Memory_Disk.command == eStore_Ftp_Data){
				fret = f_mount(0,&Fatfs[0]);
				fresult=f_open(&File_Sd, "ftpdata.txt", FA_OPEN_ALWAYS | FA_WRITE);
				if(fresult == FR_OK){
					f_lseek(&File_Sd, File_Sd.fsize);
					f_write(&File_Sd, XITC_Memory_Disk.record, strlen(XITC_Memory_Disk.record),&BytesWritten);
					f_close(&File_Sd);
				}
			}

			// Create Params starts here.......
			else if(XITC_Memory_Disk.command == eCreate_Params)
				prepare_write_params();




			// Retrieving Parameters from file... Starts here....
			else if((XITC_Memory_Disk.command == eGet_Params)){
				fret = f_mount(0,&Fatfs[0]);
				fresult=f_open(&File_Sd, "params.txt", FA_OPEN_EXISTING | FA_READ);
				if (fresult != FR_OK){
					XITC_Memory_Disk.result = eNo_File;

				}
				else if (fresult == FR_OK){
					while((fgets_ret_val = f_gets(read_buf,sizeof(read_buf),&File_Sd))!= (char *)-1){
						if ((matching_ptr=look_for_str(read_buf, "STATION:"))!=0){
							// retrieve station name and number here
							sscanf(matching_ptr, "STATION: Name=%[^,],Number=%s", &Global_Data_Params.gc_station_name, &gc_station_id1);
						}
						else if((matching_ptr=look_for_str(read_buf, "VERSION:"))!=0){
							sscanf(matching_ptr, "VERSION: %s\r\n", &gc_version_sdc);
						}

						else if ((matching_ptr=look_for_str(read_buf, "ERROR:"))!=0) {
							sscanf(matching_ptr, "ERROR: P=%d,C=%d", &gi_present_error, &gi_net_error);
						}


						else if((matching_ptr=look_for_str(read_buf, "CALIB:"))!=0){
							sscanf(matching_ptr, "CALIB: Status=%d, On=%02d-%02d-%04d\r\n",
									&Cailb_Status_Details.status, &Cailb_Status_Details.date,&Cailb_Status_Details.month,&Cailb_Status_Details.year);
						}

						else if((matching_ptr=look_for_str(read_buf, "DOTA:"))!=0){
							sscanf(matching_ptr, "DOTA: Flag=%d, Flag1=%d, File=%s\r\n", &gi_start_dota, &gi_start_dota_lpc824, &gc_dota_file_name);
						}

						else if((matching_ptr=look_for_str(read_buf, "LDT:"))!=0){
							sscanf(matching_ptr, "LDT: Date=%02d-%02d-%04d, Time=%02d:%02d",
									&gi_log_date, &gi_log_month, &gi_log_year, &gi_log_hour, &gi_log_minute);
						}

						else if((matching_ptr=look_for_str(read_buf, "ACTUAL_WS1="))!=0){
							sscanf(matching_ptr, "ACTUAL_WS1=%f, MEASURED_WS1=%f, ACTUAL_WS2=%f, MEASURED_WS2=%f", &ws_actual1, &ws_measued1, &ws_actual2, &ws_measued2);
						}

						else if((matching_ptr=look_for_str(read_buf, "ACTUAL_TEMPERATURE1="))!=0){
							sscanf(matching_ptr, "ACTUAL_TEMPERATURE1=%f, MEASURED_TEMPERATURE1=%f, ACTUAL_TEMPERATURE2=%f, MEASURED_TEMPERATURE2=%f", &temp_actual1, &temp_measued1, &temp_actual2, &temp_measued2);
						}

						else if((matching_ptr=look_for_str(read_buf, "ACTUAL_HUMIDITY1="))!=0){
							sscanf(matching_ptr, "ACTUAL_HUMIDITY1=%f, MEASURED_HUMIDITY1=%f, ACTUAL_HUMIDITY2=%f, MEASURED_HUMIDITY2=%f", &humid_actual1, &humid_measued1, &humid_actual2, &humid_measued2);
						}

						else if((matching_ptr=look_for_str(read_buf, "ACTUAL_WD1="))!=0){
							sscanf(matching_ptr, "ACTUAL_WD1=%f, MEASURED_WD1=%f, ACTUAL_WD2=%f, MEASURED_WD2=%f", &wd_actual1, &wd_measued1, &wd_actual2, &wd_measued2);
						}

						if(f_eof(&File_Sd)){
							//f_close(&File_Sd);
							break;
						}
					}
					f_close(&File_Sd);
				}
			}



			XITC_Memory_Disk.signal = 0;
		}





		if (XITC_Ui_Disk.command == eITC_Ui_Disk_Save_Params){
			prepare_write_params();
			XITC_Ui_Disk.command = eITC_Ui_Disk_Nop;
		}


		// Copy command starts here
		if(XITC_Ui_Disk.command == eITC_Ui_Disk_Copy){
			sprintf(src_filename, "%02d%02d%04d.txt",
					XITC_Ui_Disk.date, XITC_Ui_Disk.month, XITC_Ui_Disk.year);
			sprintf(dest_filename, "1:/%s_%02d%02d%04d.txt",
					Global_Data_Params.gc_station_id, XITC_Ui_Disk.date, XITC_Ui_Disk.month, XITC_Ui_Disk.year);

			// mount the volumes
			fret = f_mount(0,&Fatfs[0]);
			fret = f_mount(1,&Fatfs[1]);

			//Open the source file first

			fresult=f_open(&File_Sd, src_filename, FA_OPEN_EXISTING | FA_READ);
			if(fresult == FR_OK){
				// now open the destination file
				fret=f_open(&File_Usb, dest_filename, FA_CREATE_ALWAYS | FA_WRITE);

				// copy from source to destination
				for (;;) {
					fresult = f_read(&File_Sd, Buff, sizeof(Buff), &BytesRead);
					if (fresult || (BytesRead == 0)) break;   // error or eof
					fresult = f_write(&File_Usb, Buff, BytesRead, &BytesWritten);
					if (fresult || (BytesWritten < BytesRead)) break;   //error or disk full
				}

				// close the files
				f_close(&File_Sd);
				f_close(&File_Usb);
				XITC_Ui_Disk.result = 1;
			}
			else
				XITC_Ui_Disk.result = -1;		// means that the source file is not there

			XITC_Ui_Disk.command = eITC_Ui_Disk_Nop;

		}
		if(XITC_Ui_Disk.command == eITC_Ui_Load_Params){

			char loc_station[7];
			strcpy(loc_station, Global_Data_Params.gc_station_id);
			sprintf(src_filename, "1:/params.txt");
			sprintf(dest_filename, "params.txt");

			// mount the volumes
			fret = f_mount(0,&Fatfs[0]);
			fret = f_mount(1,&Fatfs[1]);

			//Open the source file first

			fresult=f_open(&File_Usb, src_filename, FA_OPEN_EXISTING | FA_READ);
			if(fresult == FR_OK){
				// now open the destination file
				fret=f_open(&File_Sd, dest_filename, FA_CREATE_ALWAYS | FA_WRITE);

				// copy from source to destination
				for (;;) {
					fresult = f_read(&File_Usb, Buff, sizeof(Buff), &BytesRead);
					if (fresult || (BytesRead == 0)) break;   // error or eof
					fresult = f_write(&File_Sd, Buff, BytesRead, &BytesWritten);
					if (fresult || (BytesWritten < BytesRead)) break;   //error or disk full

				}
				gi_load_params = 1;
				// close the files
				f_close(&File_Usb);
				f_close(&File_Sd);
			}

			if(gi_load_params == 1){
				get_params();
				vTaskDelay(100 / portTICK_RATE_MS);
				strcpy(Global_Data_Params.gc_station_id, loc_station);
				vTaskDelay(100 / portTICK_RATE_MS);
				prepare_write_params();

			}
			XITC_Ui_Disk.command = eITC_Ui_Disk_Nop;

		}

		if(XITC_Ui_Disk.command == eITC_Ui_Copy_Firmware){

			sprintf(src_filename, "1:/fw.hex");
			sprintf(dest_filename, "fw.hex");
			// mount the volumes
			fret = f_mount(0,&Fatfs[0]);
			fret = f_mount(1,&Fatfs[1]);

			//Open the source file first
			fresult=f_open(&File_Usb, src_filename, FA_OPEN_EXISTING | FA_READ);
			if(fresult == FR_OK){
				// now open the destination file
				fret=f_open(&File_Sd, dest_filename, FA_CREATE_ALWAYS | FA_WRITE);

				// copy from source to destination
				for (;;) {
					fresult = f_read(&File_Usb, Buff, sizeof(Buff), &BytesRead);
					if (fresult || (BytesRead == 0)) break;   // error or eof
					fresult = f_write(&File_Sd, Buff, BytesRead, &BytesWritten);
					if (fresult || (BytesWritten < BytesRead)) break;   //error or disk full
				}
				gi_update_firmware = 1;
				// close the files
				f_close(&File_Usb);
				f_close(&File_Sd);
			}
			XITC_Ui_Disk.command = eITC_Ui_Disk_Nop;
		}

		if(XITC_Ui_Disk.command == eITC_Ui_Copy_Firmware1){

			sprintf(src_filename, "1:/Lpc824.hex");
			sprintf(dest_filename, "Lpc824.hex");
			// mount the volumes
			fret = f_mount(0,&Fatfs[0]);
			fret = f_mount(1,&Fatfs[1]);

			//Open the source file first
			fresult=f_open(&File_Usb, src_filename, FA_OPEN_EXISTING | FA_READ);
			if(fresult == FR_OK){
				// now open the destination file
				fret=f_open(&File_Sd, dest_filename, FA_CREATE_ALWAYS | FA_WRITE);

				// copy from source to destination
				for (;;) {
					fresult = f_read(&File_Usb, Buff, sizeof(Buff), &BytesRead);
					if (fresult || (BytesRead == 0)) break;   // error or eof
					fresult = f_write(&File_Sd, Buff, BytesRead, &BytesWritten);
					if (fresult || (BytesWritten < BytesRead)) break;   //error or disk full
				}
				gi_update_firmware1 = 1;
				// close the files
				f_close(&File_Usb);
				f_close(&File_Sd);
			}
			XITC_Ui_Disk.command = eITC_Ui_Disk_Nop;
		}



		if(del_fw_file == 1){
			f_unlink("Lpc824.hex");
			del_fw_file = 0;
			//f_rename
		}

		// get command starts here
		if(XITC_Ui_Disk.command == eITC_Ui_Disk_Get){
			char cmp_str[10], *match_ptr;
			XITC_Ui_Disk.read_buf[0] = 0;
			sprintf(cmp_str, "%02d:%02d", XITC_Ui_Disk.hour, XITC_Ui_Disk.minute);
			sprintf(src_filename, "%02d%02d%04d.txt",
					XITC_Ui_Disk.date, XITC_Ui_Disk.month, XITC_Ui_Disk.year);

			fret = f_mount(0,&Fatfs[0]);

			fresult=f_open(&File_Sd, src_filename, FA_OPEN_EXISTING | FA_READ);
			if (fresult != FR_OK){
				XITC_Ui_Disk.command = eITC_Ui_Disk_Nop;
				XITC_Ui_Disk.read_buf[0] = 0;
			}
			else{
				while((fgets_ret_val = f_gets(read_buf,sizeof(read_buf),&File_Sd))!= (char *)-1){
					XITC_Ui_Disk.read_buf[0] = 0;
					match_ptr = look_for_str(read_buf, cmp_str);
					if(match_ptr != 0){
						sprintf(XITC_Ui_Disk.read_buf, read_buf);
						XITC_Ui_Disk.command = eITC_Ui_Disk_Nop;
						break;
					}
					if(f_eof(&File_Sd)){
						XITC_Ui_Disk.command = eITC_Ui_Disk_Nop;
						break;
					}
				}
				f_close(&File_Sd);

				XITC_Ui_Disk.command = eITC_Ui_Disk_Nop;
			}
		}

		// first opening a file
		if (XITC_Gprs_Disk.signal){
			if(XITC_Gprs_Disk.command == eFile_Open){
				// open the file
				fret = f_mount(0,&Fatfs[0]);
				fresult=f_open(&File_Sd, XITC_Gprs_Disk.filename, FA_OPEN_EXISTING | FA_READ);
				if(fresult == FR_OK){
					file_opened = 1;
					XITC_Gprs_Disk.result = eFile_Opened;
				}

				else {
					file_opened = 0;
					XITC_Gprs_Disk.result = eFile_Not_Found;
				}
				XITC_Gprs_Disk.signal = 0;
			}
			// request from gprs to save the parameter file
			else if(XITC_Gprs_Disk.command == eSave_Params){
				prepare_write_params();
				XITC_Gprs_Disk.signal = 0;
			}
		}

		// getting records as well as closing the file...
		while (file_opened){
			while (!XITC_Gprs_Disk.signal)
				vTaskDelay(10 / portTICK_RATE_MS);
			if(XITC_Gprs_Disk.command == eFile_Close){
				// close the file
				f_close(&File_Sd);
				XITC_Gprs_Disk.result = eFile_Closed;
				file_opened = 0;
				XITC_Gprs_Disk.signal = 0;
			}

			else if (XITC_Gprs_Disk.command == eFile_Get_Line){
				// read one line and send
				// V1.6 Change
				if((fgets_ret_val = f_gets(XITC_Gprs_Disk.data, sizeof(XITC_Gprs_Disk.data)-10, &File_Sd))
						!= (char *)-1){

					XITC_Gprs_Disk.result = eLine_Read;
					if(f_eof(&File_Sd)){
						//strcpy(XITC_Gprs_Disk.data,XITC_Gprs_Disk.data);
						XITC_Gprs_Disk.result = eFile_End;
					}
				}
				else
				{
					// V1.6 Change #1
					//file_opened = 0;
					XITC_Gprs_Disk.result = eFile_End;
				}
				XITC_Gprs_Disk.signal = 0;
			}
			else if (XITC_Gprs_Disk.command == eSkipLines){
				XITC_Gprs_Disk.result = eLine_Read;
				if(XITC_Gprs_Disk.line_no > 1){
					for(iTempCnt=1; iTempCnt<XITC_Gprs_Disk.line_no; iTempCnt++ ){
						if((fgets_ret_val = f_gets(XITC_Gprs_Disk.data, sizeof(XITC_Gprs_Disk.data)-10, &File_Sd))
								!= (char *)-1){
							XITC_Gprs_Disk.result = eLine_Read;
							if(f_eof(&File_Sd)){
								//strcpy(XITC_Gprs_Disk.data,XITC_Gprs_Disk.data);
								XITC_Gprs_Disk.result = eFile_End;
								break;		// for breaking the for loop
							}
						}
					}
				}
				XITC_Gprs_Disk.signal = 0;
			}
		}


		if (XITC_Gprs_Disk_Fw.signal){
			if(XITC_Gprs_Disk_Fw.command == eFTP_File_Create){
				// open or create the file for ftp
				fret = f_mount(0,&Fatfs[0]);
				fresult=f_open(&File_Sd_Dota, "fw.hex", FA_WRITE|FA_OPEN_ALWAYS);
				if(fresult == FR_OK){
					file_created = 1;
					XITC_Gprs_Disk_Fw.result = eFile_Opened;
				}

				else {
					file_created = 0;
					XITC_Gprs_Disk_Fw.result = eFile_Not_Found;
				}
				XITC_Gprs_Disk_Fw.signal = 0;
			}
		}

		// getting records as well as closing the file...
		while (file_created){
			while (!XITC_Gprs_Disk_Fw.signal)
				vTaskDelay(1 / portTICK_RATE_MS);
			if(XITC_Gprs_Disk_Fw.command == eFile_Close){
				// close the file
				f_close(&File_Sd_Dota);
				XITC_Gprs_Disk_Fw.result = eFile_Closed;
				file_created = 0;
				XITC_Gprs_Disk_Fw.signal = 0;
			}

			else if (XITC_Gprs_Disk_Fw.command == eFTP_Write_Buf){
				// read one line and send
				f_write(&File_Sd_Dota, XITC_Gprs_Disk_Fw.buf_ptr, XITC_Gprs_Disk_Fw.count, &BytesWritten);
				XITC_Gprs_Disk_Fw.result = eBuf_Written;
				XITC_Gprs_Disk_Fw.signal = 0;
			}
		}


		// get command starts here
		if (XITC_Gprs_Disk.signal) {
			if(XITC_Gprs_Disk.command == eGet_Rec){
				char cmp_str[10], *match_ptr;
				XITC_Gprs_Disk.data[0]=0;
				sprintf(cmp_str, "%02d:%02d", XITC_Gprs_Disk.hour, XITC_Gprs_Disk.minute);
				sprintf(src_filename, "%02d%02d%04d.txt",
						XITC_Gprs_Disk.date, XITC_Gprs_Disk.month, XITC_Gprs_Disk.year);

				fret = f_mount(0,&Fatfs[0]);

				fresult=f_open(&File_Sd, src_filename, FA_OPEN_EXISTING | FA_READ);
				if (fresult != FR_OK){
					XITC_Gprs_Disk.result = eFile_Not_Found;
					XITC_Gprs_Disk.data[0] = 0;
					XITC_Gprs_Disk.signal = 0;
				}
				else{
					XITC_Gprs_Disk.result = eLine_Read_In_Progress;
					while((fgets_ret_val = f_gets(read_buf,sizeof(read_buf),&File_Sd))!= (char *)-1){
						XITC_Gprs_Disk.data[0] = 0;
						match_ptr = look_for_str(read_buf, cmp_str);
						if(match_ptr != 0){
							sprintf(XITC_Gprs_Disk.data, read_buf);
							XITC_Gprs_Disk.result = eLine_Read;
							break;
						}
						if(f_eof(&File_Sd)){
							//f_close(&File_Sd);
							//XITC_Gprs_Disk.signal = 0;
							break;
						}
					}
					f_close(&File_Sd);

					XITC_Gprs_Disk.signal = 0;
				}
			}// get command ends here
		}


		if(XITC_Gprs_Disk.signal == 1){
			if(XITC_Gprs_Disk.command == eSave_Params){
				prepare_write_params();
				XITC_Gprs_Disk.signal = 0;
			}
			else if(XITC_Gprs_Disk.command == eDelete_Dota_File){
				f_unlink("fw.hex");
				XITC_Gprs_Disk.signal = 0;
			}
			else if(XITC_Gprs_Disk.command == eDelete_Dota_File1){
				f_unlink("Lpc824.hex");
				XITC_Gprs_Disk.signal = 0;
			}

			else if(XITC_Gprs_Disk.command == ePrepare_Auto_Dump){
				prepare_auto_dump(auto_dump_date, auto_dump_month, auto_dump_year);
				XITC_Gprs_Disk.signal = 0;
			}

			else if(XITC_Gprs_Disk.command == eDel_Ftp_File){
				f_unlink("ftpdata.txt");
				XITC_Gprs_Disk.signal = 0;
			}

			else if(XITC_Gprs_Disk.command == eDel_Param_File){
				f_unlink("params.txt");
				XITC_Gprs_Disk.signal = 0;
			}

			else if(XITC_Gprs_Disk.command == ePrepare_Auto_Dump){
				//prepare_auto_dump(auto_dump_date, auto_dump_month, auto_dump_year);
				XITC_Gprs_Disk.signal = 0;
			}

			else if(XITC_Gprs_Disk.command == eGetLineNo){
				XITC_Gprs_Disk.line_no = getLineNoforFTP();
				XITC_Gprs_Disk.signal = 0;
			}

			else if(XITC_Gprs_Disk.command == eSetLineNo){
				setLineNoforFTP(XITC_Gprs_Disk.line_no);
				XITC_Gprs_Disk.signal = 0;
			}

			else if(XITC_Gprs_Disk.command == eCheck_Dota_File){

				fret = f_mount(0,&Fatfs[0]);
				fresult=f_open(&File_Sd, "fw.hex", FA_OPEN_EXISTING | FA_READ);
				if (fresult != FR_OK){
					XITC_Gprs_Disk.result = eNo_File;

				}
				else if (fresult == FR_OK){
					//f_lseek(&File_Sd, File_Sd.fsize / 2);
					while((fgets_ret_val = f_gets(read_buf,sizeof(read_buf),&File_Sd))!= (char *)-1){
						if ((matching_ptr=look_for_str(read_buf, ":00000001FF"))!=0){
							valid_dota_file = 1;
						}
						if(f_eof(&File_Sd)){
							if(valid_dota_file != 1){
								f_unlink("fw.hex");
								XITC_Gprs_Disk.result = eNo_File;
							}
							break;
						}
					}
					f_close(&File_Sd);
				}

				XITC_Gprs_Disk.signal = 0;
			}

			if(valid_dota_file == 1)
				XITC_Gprs_Disk.result = eFile_Close;
			XITC_Gprs_Disk.signal = 0;
		}



	}
}

/*
 * -------------------------------------------------------------------------------------------------
 * Function sd_log_http
 * Appends live debug messages directly to httplog.txt on the SD Card
 * -------------------------------------------------------------------------------------------------
 */
void sd_log_http(const char *msg)
{
	static FIL File_Http_Log;
	UINT bw;
	char time_str[36];
	int yr;

	if (!msg || *msg == '\0') return;

	if (gi_disk_ok != 1) {
		check_sdc_status();
		if (gi_disk_ok != 1) {
			return;
		}
	}

	fret = f_mount(0, &Fatfs[0]);

	yr = xPresent_Date_Time.year;
	if (yr < 100 && yr > 0) yr += 2000;
	if (yr < 2020) yr = 2026;

	sprintf(time_str, "[%04d-%02d-%02d %02d:%02d:%02d] ",
		yr, xPresent_Date_Time.month, xPresent_Date_Time.date,
		xPresent_Date_Time.hour, xPresent_Date_Time.minute, xPresent_Date_Time.second);

	if (f_open(&File_Http_Log, "httplog.txt", FA_OPEN_ALWAYS | FA_WRITE) == FR_OK) {
		f_lseek(&File_Http_Log, File_Http_Log.fsize);
		f_write(&File_Http_Log, time_str, strlen(time_str), &bw);
		f_write(&File_Http_Log, msg, strlen(msg), &bw);
		f_write(&File_Http_Log, "\r\n", 2, &bw);
		f_close(&File_Http_Log);
	}
}
