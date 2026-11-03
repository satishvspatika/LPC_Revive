#include <stdio.h>
#include <string.h>
#include "globals.h"
#include "FreeRTOS.h"
#include "task.h"
#include "memory.h"
#include "ui.h"
#include "gprs.h"
#include "sensor.h"

typedef void(*IAP)(unsigned long[], unsigned long[]);
IAP iap_entry;

Days_Data_def *days_sector[MAX_NO_OF_DAYS]= {
		(Days_Data_def *) 0x00002000,
		(Days_Data_def *) 0x00003000,
		(Days_Data_def *) 0x00004000,
		(Days_Data_def *) 0x00005000,
		(Days_Data_def *) 0x00006000,
		(Days_Data_def *) 0x00007000,
		(Days_Data_def *) 0x00008000 };

unsigned long command_iap[6];
unsigned long result_iap[5];

int *write_buf_ptr, *sector_data_ptr;
unsigned int dest_addr;
Days_Data_def *days_storage_ptr;

void *data_buf_dest_rec, *recMemAdd;
Samples_def store_rec;

int write_days_data(Samples_def *rec_data, uint8_t curDate, uint8_t curMonth, uint16_t curYear){

	int sect, found, recNo, i;
	int hr, min, rf, recBlk, recSt, write_512b, src, dest, recLen;
	int recBlk_start, recSt_offset, write_count;
	int sect_date, sect_month, sect_year;
	int cur_date_val, recent_date_val;

	for(sect=2, found=0; ((sect<4) && !found);){
		//if(days_sector[sect-2]->date == curDate)
		if((days_sector[sect-2]->year == curYear) && (days_sector[sect-2]->month == curMonth) && (days_sector[sect-2]->date == curDate))
			found =1;
		else
			sect++;
	}

	if(!found){
		next_sect = 2;  //if all data sectors are free directly allot  sector
		if((days_sector[0]->year == 0xFFFF) || (days_sector[0]->month == 0xFF) || (days_sector[0]->date == 0xFF)){
			next_sect = 2;
		}
		else if((days_sector[1]->year == 0xFFFF) || (days_sector[1]->month == 0xFF) || (days_sector[1]->date == 0xFF)){
			next_sect = 3;
		}

		else{
			cur_date_val = curYear*256*256 + curMonth*256 + curDate;
			recent_date_val = latest_year*256*256 + latest_month*256 + latest_date;

			if(cur_date_val < recent_date_val)
				next_sect = latest_Sector;
			else
				next_sect = oldest_Sector;
		}

		vTaskDelay(10 / portTICK_RATE_MS);

		sect = allocate_space(curDate, curMonth, curYear);
		vTaskDelay(10 / portTICK_RATE_MS);
	}

	vTaskDelay(10 / portTICK_RATE_MS);

	present_sector_no = sect;
	present_sample_no = sampleNo;

	write_buf_ptr = (int *)local_write_buf_st;
	sector_data_ptr = (int *)days_sector[sect-2];

	dest_addr = (unsigned int)days_sector[sect-2];
	for(i=0; i<1024; i++)
		*write_buf_ptr++ = *sector_data_ptr++;

	//now set the start address of the days_sector
	days_storage_ptr =  (Days_Data_def *)local_write_buf_st;

	days_storage_ptr->last_rec_hr = rec_data->hour;
	days_storage_ptr->last_rec_min = rec_data->minute;
	days_storage_ptr->samples[present_sample_no].hour = rec_data->hour;
	days_storage_ptr->samples[present_sample_no].minute = rec_data->minute;
	days_storage_ptr->samples[present_sample_no].rf = rec_data->rf;
	days_storage_ptr->samples[present_sample_no].battery1 = rec_data->battery1;
	days_storage_ptr->samples[present_sample_no].battery2 = rec_data->battery2;
	days_storage_ptr->samples[present_sample_no].signal = rec_data->signal;
	days_storage_ptr->samples[present_sample_no].disk_status = rec_data->disk_status;
	days_storage_ptr->samples[present_sample_no].http_status = rec_data->http_status;
	days_storage_ptr->samples[present_sample_no].ftp_status = rec_data->ftp_status;
	days_storage_ptr->samples[present_sample_no].sms_status = rec_data->sms_status;
	days_storage_ptr->samples[present_sample_no].year = rec_data->year;
	days_storage_ptr->samples[present_sample_no].month = rec_data->month;
	days_storage_ptr->samples[present_sample_no].date = rec_data->date;
	days_storage_ptr->samples[present_sample_no].temperature = rec_data->temperature;
	days_storage_ptr->samples[present_sample_no].humidity = rec_data->humidity;
	days_storage_ptr->samples[present_sample_no].wind_direction = rec_data->wind_direction;
	days_storage_ptr->samples[present_sample_no].wind_speed = rec_data->wind_speed;
	days_storage_ptr->samples[present_sample_no].ws_sec_cnt = rec_data->ws_sec_cnt;


	/*
	// prepare for erase
	Prepare_Sector(sect, sect);
	vTaskDelay(2 / portTICK_RATE_MS);
	__disable_irq();
	Erase_Sector(sect, sect);
	vTaskDelay(2 / portTICK_RATE_MS);
	__enable_irq();


	// prpare for write
	Prepare_Sector(sect, sect);
	vTaskDelay(2 / portTICK_RATE_MS);

	// write all 4 K
	Write_Sector(dest_addr, (unsigned int) local_write_buf_st, 4096);

	vTaskDelay(2 / portTICK_RATE_MS);
	 */

	// prepare for erase

	//steps
	//1 prepare and erase sector if erase status is error once again try prepare and erase sector
	//2 If still erase status is error we are not writing in flash
	//3 If erase success then prepare and write sector
	//4 if write status is error once again try for prepare and erase then prepare and write.

	erase_status = 1;
	write_status = 1;

	Prepare_Sector(sect, sect);
	vTaskDelay(2 / portTICK_RATE_MS);
	__disable_irq();
	erase_status = Erase_Sector(sect, sect);
	__enable_irq();


	if(erase_status != 0){
		Prepare_Sector(sect, sect);
		vTaskDelay(2 / portTICK_RATE_MS);
		__disable_irq();
		erase_status = Erase_Sector(sect, sect);
		__enable_irq();
	}

	//	erase_status = Blank_Check_Sector(sect, sect);
	vTaskDelay(2 / portTICK_RATE_MS);

	if(erase_status == 0){
		// prpare for write
		Prepare_Sector(sect, sect);
		vTaskDelay(2 / portTICK_RATE_MS);

		__disable_irq();
		// write all 4 K
		write_status = Write_Sector(dest_addr, (unsigned int) local_write_buf_st, 4096);
		__enable_irq();

		if(write_status != 0){

			Prepare_Sector(sect, sect);
			vTaskDelay(2 / portTICK_RATE_MS);
			__disable_irq();
			erase_status = Erase_Sector(sect, sect);
			if(erase_status == 0){

				// prpare for write
				Prepare_Sector(sect, sect);
				vTaskDelay(2 / portTICK_RATE_MS);

				__disable_irq();
				write_status = Write_Sector(dest_addr, (unsigned int) local_write_buf_st, 4096);
				__enable_irq();
			}
		}
		/*	int cmp_status;
		cmp_status = 1;
		cmp_status = Compare_Sector(dest_addr, (unsigned int) local_write_buf_st, 4096);
		 */
		vTaskDelay(2 / portTICK_RATE_MS);
	}
	vTaskDelay(1 / portTICK_RATE_MS);
}


int allocate_space(uint8_t curDate, uint8_t curMonth, uint16_t curYear)
{
	static char write_buf[512];
	int i;
	erase_n_format_sector(next_sect);
	// prepare the 512 byte write_buf;
	for(i=0;i<512;i++){
		write_buf[i] = 0xFF;
	}
	/*dateVal.date = 9;
	dateVal.month = 10;
	dateVal.year = 2020;*/
	dateVal = (Days_Data_def *)&write_buf[0];

	dateVal->date = curDate;
	dateVal->month = curMonth;
	dateVal->year = curYear;


	vTaskDelay(2 / portTICK_RATE_MS);

	// program this buffer to sector (oldPosn) for 256 bytes
	Prepare_Sector(next_sect, next_sect);
	Write_Sector((unsigned int)days_sector[next_sect-2], (unsigned int)&write_buf[0], 256);

	vTaskDelay(2 / portTICK_RATE_MS);
	return(next_sect);

}

void erase_n_format_sector(int sector){
	int i;

	Prepare_Sector(sector, sector);
	vTaskDelay(2 / portTICK_RATE_MS);
	__disable_irq();
	Erase_Sector(sector, sector);
	__enable_irq();
	vTaskDelay(2 / portTICK_RATE_MS);

}


void init_entire_space(void){
	int i;

	Prepare_Sector(ST_DATA_SECTOR, END_DATA_SECTOR);
	vTaskDelay(10 / portTICK_RATE_MS);
	__disable_irq();
	Erase_Sector(ST_DATA_SECTOR, END_DATA_SECTOR);
	__enable_irq();
	vTaskDelay(20 / portTICK_RATE_MS);

}

short int Erase_Sector(int Start_Sec_No, int End_Sec_No) {
	command_iap[0] = 52;
	command_iap[1] = Start_Sec_No;
	command_iap[2] = End_Sec_No;
	command_iap[3] = XOSC;
	iap_entry = (IAP) IAP_LOCATION;
	iap_entry(command_iap, result_iap);
	return (result_iap[0]);
}


short int Blank_Check_Sector(int Start_Sec_No, int End_Sec_No) {
	command_iap[0] = 53;
	command_iap[1] = Start_Sec_No;
	command_iap[2] = End_Sec_No;
	command_iap[3] = XOSC;
	iap_entry = (IAP) IAP_LOCATION;
	iap_entry(command_iap, result_iap);
	return (result_iap[0]);
}

short int Prepare_Sector(int Start_Sec_No, int End_Sec_No) {
	command_iap[0] = 50;
	command_iap[1] = Start_Sec_No;
	command_iap[2] = End_Sec_No;
	iap_entry = (IAP) IAP_LOCATION;
	iap_entry(command_iap, result_iap);
	return (result_iap[0]);
}

short int Write_Sector(unsigned int Dest_Addr, unsigned int Source_Addr,
		int Bytes) {

	command_iap[0] = 51;
	command_iap[1] = Dest_Addr;
	command_iap[2] = Source_Addr;
	command_iap[3] = Bytes;
	command_iap[4] = XOSC;
	iap_entry = (IAP) IAP_LOCATION;
	iap_entry(command_iap, result_iap);

	return (result_iap[0]);
}

short int Compare_Sector(unsigned int Dest_Addr, unsigned int Source_Addr,
		int Bytes) {

	command_iap[0] = 56;
	command_iap[1] = Dest_Addr;
	command_iap[2] = Source_Addr;
	command_iap[3] = Bytes;
	command_iap[4] = XOSC;
	iap_entry = (IAP) IAP_LOCATION;
	iap_entry(command_iap, result_iap);

	return (result_iap[0]);
}


void params_default(void){
	//	strcpy(Global_Data_Params.gc_station_id, "001920");
#if (UNIT_CFG == CFG_BIHAR_TRG)
	strcpy(Global_Data_Params.gc_station_id, "001920");
	strcpy(Global_Data_Params.gc_station_name, "VARSHA-TRG");
	strcpy(Global_Data_Params.gc_model, "TRG-BIH");

	strcpy(Global_Data_Params.gc_network_name, "DES-BIH");
	strcpy(Global_Data_Params.gc_station_type, "TRG");

	strcpy(Global_Data_Params.gc_universal_gsm, "9741391102");
	strcpy(Global_Data_Params.gc_master_gsm, "9741391102");
	strcpy(Global_Data_Params.gc_secondary_gsm, "9449020611");
	strcpy(Global_Data_Params.gc_secondary_gsm1, "6366469972");
	strcpy(Global_Data_Params.gc_secondary_gsm2, "9482841038");
	strcpy(Global_Data_Params.gc_secondary_gsm3, "9741391102");

	strcpy(Global_Data_Params.gc_version, "TWS6-1.0.12");

	// Primary Server: Bihar Govt / BMSK (JSON endpoint on port 8085)
	strcpy(Global_Data_Params.gc_http_server_name1, "rtdasbmsk.spatika.net");
	strcpy(Global_Data_Params.gc_http_link1, "/Home/UpdateTRGData");
	strcpy(Global_Data_Params.gc_http_key1, "bmsk1234");
	Global_Data_Params.gi_http_server_port1 = 8085;

	// Secondary Server: Bihar Spatika Backup Server (JSON endpoint on port 80)
	strcpy(Global_Data_Params.gc_http_server_name2, "rtdasbih.spatika.net");
	strcpy(Global_Data_Params.gc_http_link2, "/trg_gprs/upload_bih_trg_data_new");
	strcpy(Global_Data_Params.gc_http_key2, "bmsk1234");
	Global_Data_Params.gi_http_server_port2 = 80;

	// FTP Settings
	strcpy(Global_Data_Params.gc_ftp_server_name1, "rtdasbih.spatika.net");
	strcpy(Global_Data_Params.gc_ftp_user1, "bihar_trg");
	strcpy(Global_Data_Params.gc_ftp_pwd1, "bihar#123");
	strcpy(Global_Data_Params.gc_ftp_path1, "/");
	Global_Data_Params.gi_ftpserver_port1 = 21;

	strcpy(Global_Data_Params.gc_ftp_server_name2, "dota.spatika.net");
	strcpy(Global_Data_Params.gc_ftp_user2, "dota_dmc");
	strcpy(Global_Data_Params.gc_ftp_pwd2, "airdata2016");
	strcpy(Global_Data_Params.gc_ftp_path2, "/");
	Global_Data_Params.gi_ftpserver_port2 = 21;

#elif (UNIT_CFG == CFG_KSNDMC_TWS)
	strcpy(Global_Data_Params.gc_station_id, "001922");
	strcpy(Global_Data_Params.gc_station_name, "VARSHA-TWS");
	strcpy(Global_Data_Params.gc_model, "TWS-DMC");

	strcpy(Global_Data_Params.gc_network_name, "KSNDMC");
	strcpy(Global_Data_Params.gc_station_type, "TWS");

	strcpy(Global_Data_Params.gc_universal_gsm, "9980945474");
	strcpy(Global_Data_Params.gc_master_gsm, "9449020611");
	strcpy(Global_Data_Params.gc_secondary_gsm, "9741391102");
	strcpy(Global_Data_Params.gc_secondary_gsm1, "6366469972");
	strcpy(Global_Data_Params.gc_secondary_gsm2, "9482841038");
	strcpy(Global_Data_Params.gc_secondary_gsm3, "9449020611");

	strcpy(Global_Data_Params.gc_version, "TWS6-1.0.12");

	// HTTP Configuration for KSNDMC_TWS (SYSTEM 1 per Spatika / KSNDMC Specification)
	strcpy(Global_Data_Params.gc_http_server_name1, "rtdas.ksndmc.net");
	strcpy(Global_Data_Params.gc_http_link1, "/tws_gprs/update_tws_data_v3");
	strcpy(Global_Data_Params.gc_http_key1, "climate4pTWS");
	Global_Data_Params.gi_http_server_port1 = 80;

	strcpy(Global_Data_Params.gc_http_server_name2, "rtdas.spatika.net");
	strcpy(Global_Data_Params.gc_http_link2, "/tws_gprs/update_tws_data_v2");
	strcpy(Global_Data_Params.gc_http_key2, "climate4pTWS");
	Global_Data_Params.gi_http_server_port2 = 80;

	// Primary Protocol for KSNDMC_TWS: FTP (Strict 57-byte fixed-column .kwd file)
	strcpy(Global_Data_Params.gc_ftp_server_name1, "ftp1.ksndmc.net");
	strcpy(Global_Data_Params.gc_ftp_user1, "tws_spatika_v2");
	strcpy(Global_Data_Params.gc_ftp_pwd1, "twssp#987");
	strcpy(Global_Data_Params.gc_ftp_path1, "/");
	Global_Data_Params.gi_ftpserver_port1 = 21;

	strcpy(Global_Data_Params.gc_ftp_server_name2, "dota.spatika.net");
	strcpy(Global_Data_Params.gc_ftp_user2, "dota_dmc");
	strcpy(Global_Data_Params.gc_ftp_pwd2, "airdata2016");
	strcpy(Global_Data_Params.gc_ftp_path2, "/");
	Global_Data_Params.gi_ftpserver_port2 = 21;

#else /* CFG_KSNDMC_TRG (Default) */
	strcpy(Global_Data_Params.gc_station_id, "001922");
	strcpy(Global_Data_Params.gc_station_name, "VARSHA-TRG");
	strcpy(Global_Data_Params.gc_model, "TRG-DMC");

	strcpy(Global_Data_Params.gc_network_name, "KSNDMC");
	strcpy(Global_Data_Params.gc_station_type, "TRG");

	strcpy(Global_Data_Params.gc_universal_gsm, "9980945474");
	strcpy(Global_Data_Params.gc_master_gsm, "9449020611");
	strcpy(Global_Data_Params.gc_secondary_gsm, "9741391102");
	strcpy(Global_Data_Params.gc_secondary_gsm1, "6366469972");
	strcpy(Global_Data_Params.gc_secondary_gsm2, "9482841038");
	strcpy(Global_Data_Params.gc_secondary_gsm3, "9449020611");

	strcpy(Global_Data_Params.gc_version, "TWS6-1.0.12");

	// Primary Server: KSNDMC (v3 Endpoint, form-encoded)
	strcpy(Global_Data_Params.gc_http_server_name1, "rtdas.ksndmc.net");
	strcpy(Global_Data_Params.gc_http_link1, "/trg_gprs/update_data_sit_v3");
	strcpy(Global_Data_Params.gc_http_key1, "pse2420");
	Global_Data_Params.gi_http_server_port1 = 80;

	// Secondary Server: Spatika Backup Server
	strcpy(Global_Data_Params.gc_http_server_name2, "rtdas.spatika.net");
	strcpy(Global_Data_Params.gc_http_link2, "/trg_gprs/update_data_sit_v3");
	strcpy(Global_Data_Params.gc_http_key2, "pse2420");
	Global_Data_Params.gi_http_server_port2 = 80;

	// FTP Settings
	strcpy(Global_Data_Params.gc_ftp_server_name1, "ftp1.ksndmc.net");
	strcpy(Global_Data_Params.gc_ftp_user1, "trg_spatika_v2@ksndmc.net");
	strcpy(Global_Data_Params.gc_ftp_pwd1, "trgsp#123");
	strcpy(Global_Data_Params.gc_ftp_path1, "/");
	Global_Data_Params.gi_ftpserver_port1 = 21;

	strcpy(Global_Data_Params.gc_ftp_server_name2, "dota.spatika.net");
	strcpy(Global_Data_Params.gc_ftp_user2, "dota_dmc");
	strcpy(Global_Data_Params.gc_ftp_pwd2, "airdata2016");
	strcpy(Global_Data_Params.gc_ftp_path2, "/");
	Global_Data_Params.gi_ftpserver_port2 = 21;
#endif

	Global_Data_Params.gi_log_interval = LOG_INTERVAL;
	Global_Data_Params.gi_report_interval = REPORT_INTERVAL;
	Global_Data_Params.gi_sim_select = sim_select;
	Global_Data_Params.gi_send_data_sms = 0;


	strcpy(Global_Data_Params.ISP_APN_PARAMS[0].svc, "+9198");
	strcpy(Global_Data_Params.ISP_APN_PARAMS[0].apn, "airtelgprs.com");
	strcpy(Global_Data_Params.ISP_APN_PARAMS[0].apn_user, "");
	strcpy(Global_Data_Params.ISP_APN_PARAMS[0].apn_pwd, "");
	strcpy(Global_Data_Params.ISP_APN_PARAMS[0].mobile_network, "airtel");

	strcpy(Global_Data_Params.ISP_APN_PARAMS[1].svc, "+9194");
	strcpy(Global_Data_Params.ISP_APN_PARAMS[1].apn, "bsnl.net");
	strcpy(Global_Data_Params.ISP_APN_PARAMS[1].apn_user, "");
	strcpy(Global_Data_Params.ISP_APN_PARAMS[1].apn_pwd, "");
	strcpy(Global_Data_Params.ISP_APN_PARAMS[1].mobile_network, "BSNL");

	strcpy(Global_Data_Params.ISP_APN_PARAMS[2].svc, "+91984");
	strcpy(Global_Data_Params.ISP_APN_PARAMS[2].apn, "www");
	strcpy(Global_Data_Params.ISP_APN_PARAMS[2].apn_user, "");
	strcpy(Global_Data_Params.ISP_APN_PARAMS[2].apn_pwd, "");
	strcpy(Global_Data_Params.ISP_APN_PARAMS[2].mobile_network, "Vodafone");

	strcpy(Global_Data_Params.ISP_APN_PARAMS[3].svc, "+919804");
	strcpy(Global_Data_Params.ISP_APN_PARAMS[3].apn, "internet");
	strcpy(Global_Data_Params.ISP_APN_PARAMS[3].apn_user, "");
	strcpy(Global_Data_Params.ISP_APN_PARAMS[3].apn_pwd, "");
	strcpy(Global_Data_Params.ISP_APN_PARAMS[3].mobile_network, "Idea");

	strcpy(Global_Data_Params.ISP_APN_PARAMS[4].svc, "+9170");
	strcpy(Global_Data_Params.ISP_APN_PARAMS[4].apn, "jionet");
	strcpy(Global_Data_Params.ISP_APN_PARAMS[4].apn_user, "");
	strcpy(Global_Data_Params.ISP_APN_PARAMS[4].apn_pwd, "");
	strcpy(Global_Data_Params.ISP_APN_PARAMS[4].mobile_network, "JIO");

	Global_Data_Params.gps_latitude = 0.0f;
	Global_Data_Params.gps_lat_dir = 'N';
	Global_Data_Params.gps_longitude = 0.0f;
	Global_Data_Params.gps_lon_dir = 'E';
}



void write_param_sector(void){
	int erase_status, write_status;
	taskENTER_CRITICAL();
	/* Prepare 4KB aligned RAM buffer in AHB SRAM for sector write */
	memset((void *)local_write_buf_st, 0xFF, 4096);
	memcpy((void *)local_write_buf_st, &Global_Data_Params, sizeof(Global_Data_Params));

	Prepare_Sector(PARAM_SECTOR, PARAM_SECTOR);
	__disable_irq();
	erase_status = Erase_Sector(PARAM_SECTOR, PARAM_SECTOR);
	__enable_irq();

	if (erase_status != 0) {
		Prepare_Sector(PARAM_SECTOR, PARAM_SECTOR);
		__disable_irq();
		erase_status = Erase_Sector(PARAM_SECTOR, PARAM_SECTOR);
		__enable_irq();
	}

	Prepare_Sector(PARAM_SECTOR, PARAM_SECTOR);
	__disable_irq();
	write_status = Write_Sector((unsigned int)PARAM_START_ADDR, (unsigned int)local_write_buf_st, 4096);
	__enable_irq();

	if (write_status != 0) {
		Prepare_Sector(PARAM_SECTOR, PARAM_SECTOR);
		__disable_irq();
		Erase_Sector(PARAM_SECTOR, PARAM_SECTOR);
		__enable_irq();

		Prepare_Sector(PARAM_SECTOR, PARAM_SECTOR);
		__disable_irq();
		write_status = Write_Sector((unsigned int)PARAM_START_ADDR, (unsigned int)local_write_buf_st, 4096);
		__enable_irq();
	}
	taskEXIT_CRITICAL();
}


void read_param_sector(void){
	Global_Data_Params_From_Flash = (Global_Data_Params_def*)PARAM_START_ADDR;

	strcpy(Global_Data_Params.gc_station_name, Global_Data_Params_From_Flash->gc_station_name);
	strcpy(Global_Data_Params.gc_station_id, Global_Data_Params_From_Flash->gc_station_id);

	strcpy(Global_Data_Params.gc_model, Global_Data_Params_From_Flash->gc_model);

	strcpy(Global_Data_Params.gc_network_name, Global_Data_Params_From_Flash->gc_network_name);
	strcpy(Global_Data_Params.gc_station_type, Global_Data_Params_From_Flash->gc_station_type);

	strcpy(Global_Data_Params.gc_master_gsm, Global_Data_Params_From_Flash->gc_master_gsm);
	strcpy(Global_Data_Params.gc_universal_gsm, Global_Data_Params_From_Flash->gc_universal_gsm);
	strcpy(Global_Data_Params.gc_secondary_gsm, Global_Data_Params_From_Flash->gc_secondary_gsm);
	strcpy(Global_Data_Params.gc_secondary_gsm1, Global_Data_Params_From_Flash->gc_secondary_gsm1);
	strcpy(Global_Data_Params.gc_secondary_gsm2, Global_Data_Params_From_Flash->gc_secondary_gsm2);
	strcpy(Global_Data_Params.gc_secondary_gsm3, Global_Data_Params_From_Flash->gc_secondary_gsm3);

	strcpy(Global_Data_Params.gc_version, Global_Data_Params_From_Flash->gc_version);

	strcpy(Global_Data_Params.gc_http_server_name1, Global_Data_Params_From_Flash->gc_http_server_name1);
	strcpy(Global_Data_Params.gc_http_link1,Global_Data_Params_From_Flash->gc_http_link1);
	strcpy(Global_Data_Params.gc_http_key1, Global_Data_Params_From_Flash->gc_http_key1);
	Global_Data_Params.gi_http_server_port1 = Global_Data_Params_From_Flash->gi_http_server_port1;

	strcpy(Global_Data_Params.gc_http_server_name2, Global_Data_Params_From_Flash->gc_http_server_name2);
	strcpy(Global_Data_Params.gc_http_link2, Global_Data_Params_From_Flash->gc_http_link2);
	strcpy(Global_Data_Params.gc_http_key2, Global_Data_Params_From_Flash->gc_http_key2);
	Global_Data_Params.gi_http_server_port2 = Global_Data_Params_From_Flash->gi_http_server_port2;

	strcpy(Global_Data_Params.gc_ftp_server_name1, Global_Data_Params_From_Flash->gc_ftp_server_name1);
	strcpy(Global_Data_Params.gc_ftp_user1, Global_Data_Params_From_Flash->gc_ftp_user1);
	strcpy(Global_Data_Params.gc_ftp_pwd1, Global_Data_Params_From_Flash->gc_ftp_pwd1);
	strcpy(Global_Data_Params.gc_ftp_path1, Global_Data_Params_From_Flash->gc_ftp_path1);
	Global_Data_Params.gi_ftpserver_port1 = Global_Data_Params_From_Flash->gi_ftpserver_port1;

	strcpy(Global_Data_Params.gc_ftp_server_name2, Global_Data_Params_From_Flash->gc_ftp_server_name2);
	strcpy(Global_Data_Params.gc_ftp_user2, Global_Data_Params_From_Flash->gc_ftp_user2);
	strcpy(Global_Data_Params.gc_ftp_pwd2, Global_Data_Params_From_Flash->gc_ftp_pwd2);
	strcpy(Global_Data_Params.gc_ftp_path2, Global_Data_Params_From_Flash->gc_ftp_path2);
	Global_Data_Params.gi_ftpserver_port2 = Global_Data_Params_From_Flash->gi_ftpserver_port2;


	Global_Data_Params.gi_log_interval = Global_Data_Params_From_Flash->gi_log_interval;
	Global_Data_Params.gi_report_interval = Global_Data_Params_From_Flash->gi_report_interval;
	Global_Data_Params.gi_sim_select = Global_Data_Params_From_Flash->gi_sim_select;
	Global_Data_Params.gi_send_data_sms = Global_Data_Params_From_Flash->gi_send_data_sms;




	strcpy(Global_Data_Params.ISP_APN_PARAMS[0].svc, Global_Data_Params_From_Flash->ISP_APN_PARAMS[0].svc);
	strcpy(Global_Data_Params.ISP_APN_PARAMS[0].apn, Global_Data_Params_From_Flash->ISP_APN_PARAMS[0].apn);
	strcpy(Global_Data_Params.ISP_APN_PARAMS[0].apn_user, Global_Data_Params_From_Flash->ISP_APN_PARAMS[0].apn_user);
	strcpy(Global_Data_Params.ISP_APN_PARAMS[0].apn_pwd, Global_Data_Params_From_Flash->ISP_APN_PARAMS[0].apn_pwd);
	strcpy(Global_Data_Params.ISP_APN_PARAMS[0].mobile_network, Global_Data_Params_From_Flash->ISP_APN_PARAMS[0].mobile_network);

	strcpy(Global_Data_Params.ISP_APN_PARAMS[1].svc, Global_Data_Params_From_Flash->ISP_APN_PARAMS[1].svc);
	strcpy(Global_Data_Params.ISP_APN_PARAMS[1].apn, Global_Data_Params_From_Flash->ISP_APN_PARAMS[1].apn);
	strcpy(Global_Data_Params.ISP_APN_PARAMS[1].apn_user, Global_Data_Params_From_Flash->ISP_APN_PARAMS[1].apn_user);
	strcpy(Global_Data_Params.ISP_APN_PARAMS[1].apn_pwd, Global_Data_Params_From_Flash->ISP_APN_PARAMS[1].apn_pwd);
	strcpy(Global_Data_Params.ISP_APN_PARAMS[1].mobile_network, Global_Data_Params_From_Flash->ISP_APN_PARAMS[1].mobile_network);

	strcpy(Global_Data_Params.ISP_APN_PARAMS[2].svc, Global_Data_Params_From_Flash->ISP_APN_PARAMS[2].svc);
	strcpy(Global_Data_Params.ISP_APN_PARAMS[2].apn, Global_Data_Params_From_Flash->ISP_APN_PARAMS[2].apn);
	strcpy(Global_Data_Params.ISP_APN_PARAMS[2].apn_user, Global_Data_Params_From_Flash->ISP_APN_PARAMS[2].apn_user);
	strcpy(Global_Data_Params.ISP_APN_PARAMS[2].apn_pwd, Global_Data_Params_From_Flash->ISP_APN_PARAMS[2].apn_pwd);
	strcpy(Global_Data_Params.ISP_APN_PARAMS[2].mobile_network, Global_Data_Params_From_Flash->ISP_APN_PARAMS[2].mobile_network);

	strcpy(Global_Data_Params.ISP_APN_PARAMS[3].svc, Global_Data_Params_From_Flash->ISP_APN_PARAMS[3].svc);
	strcpy(Global_Data_Params.ISP_APN_PARAMS[3].apn, Global_Data_Params_From_Flash->ISP_APN_PARAMS[3].apn);
	strcpy(Global_Data_Params.ISP_APN_PARAMS[3].apn_user, Global_Data_Params_From_Flash->ISP_APN_PARAMS[3].apn_user);
	strcpy(Global_Data_Params.ISP_APN_PARAMS[3].apn_pwd, Global_Data_Params_From_Flash->ISP_APN_PARAMS[3].apn_pwd);
	strcpy(Global_Data_Params.ISP_APN_PARAMS[3].mobile_network, Global_Data_Params_From_Flash->ISP_APN_PARAMS[3].mobile_network);


	strcpy(Global_Data_Params.ISP_APN_PARAMS[4].svc, Global_Data_Params_From_Flash->ISP_APN_PARAMS[4].svc);
	strcpy(Global_Data_Params.ISP_APN_PARAMS[4].apn, Global_Data_Params_From_Flash->ISP_APN_PARAMS[4].apn);
	strcpy(Global_Data_Params.ISP_APN_PARAMS[4].apn_user, Global_Data_Params_From_Flash->ISP_APN_PARAMS[4].apn_user);
	strcpy(Global_Data_Params.ISP_APN_PARAMS[4].apn_pwd, Global_Data_Params_From_Flash->ISP_APN_PARAMS[4].apn_pwd);
	strcpy(Global_Data_Params.ISP_APN_PARAMS[4].mobile_network, Global_Data_Params_From_Flash->ISP_APN_PARAMS[4].mobile_network);

	Global_Data_Params.gps_latitude = Global_Data_Params_From_Flash->gps_latitude;
	Global_Data_Params.gps_lat_dir = Global_Data_Params_From_Flash->gps_lat_dir;
	Global_Data_Params.gps_longitude = Global_Data_Params_From_Flash->gps_longitude;
	Global_Data_Params.gps_lon_dir = Global_Data_Params_From_Flash->gps_lon_dir;

	if (Global_Data_Params.gps_latitude > 0.0f && Global_Data_Params.gps_latitude < 90.0f) {
		GPS.latitude = Global_Data_Params.gps_latitude;
		GPS.lat_dir = (Global_Data_Params.gps_lat_dir == 'N' || Global_Data_Params.gps_lat_dir == 'S') ? Global_Data_Params.gps_lat_dir : 'N';
		GPS.longitude = Global_Data_Params.gps_longitude;
		GPS.lon_dir = (Global_Data_Params.gps_lon_dir == 'E' || Global_Data_Params.gps_lon_dir == 'W') ? Global_Data_Params.gps_lon_dir : 'E';
		gps_found = 1;
	}

	vTaskDelay(2 / portTICK_RATE_MS);

}



void cumulative_rf_new(int dd, int mm, int yy, int hh, int min) {
	int first_sample;
	int max_sample;
	int i,sect_no, found;
	int cur_sample;

	c_rainfall = 0;

	//	max_sample = (24 * 60) / LOG_INTERVAL;	//96
	cur_sample = ((hh * 60) + min) / LOG_INTERVAL;

	cur_sample = (cur_sample + 61) % 96 ;

	for(sect_no=2, found=0; ((sect_no<4) && !found);){
		if((days_sector[sect_no-2]->date == dd) && (days_sector[sect_no-2]->month == mm) && (days_sector[sect_no-2]->year == yy))
			found =1;
		else
			sect_no++;
	}


	if(found){
		for (i = 0; i <= cur_sample; i++) {
			if(days_sector[sect_no-2]->samples[i].rf != 0xFFFF){
				c_rainfall += days_sector[sect_no-2]->samples[i].rf;
			}
		}
	}
}




/*
void test_memory_write()
{
	int temp_date,temp_month,temp_year, i, j;

	temp_date = 16;
	temp_month = 10;
	temp_year =2020;

	Samples_def test_rec;

	for(j=16; j<28; j++){
		for(i=0; i<96; i++){
			test_rec.rf = (uint16_t) i;
			test_rec.hour = (uint8_t) i/4;
			test_rec.minute = (uint8_t)(i%4)*15;

		//	c_hour = i/4;
		//	c_minute = (i%4)*15;

			test_rec.battery1 = (uint16_t)1000+i;
			test_rec.battery2 = (uint16_t)2000+i;
			test_rec.disk_status = (uint8_t)i;
			test_rec.ftp_status =(uint8_t) i;
			test_rec.http_status = (uint8_t)i;
			test_rec.signal = (int16_t)i;
			test_rec.sms_status = (uint8_t)i;
			write_days_data(&test_rec, j,temp_month,temp_year);
			vTaskDelay(10 / portTICK_RATE_MS);

		}
		if(j==24)
			j += 1;
		vTaskDelay(10 / portTICK_RATE_MS);
	}
}
 */


void check_data_is_stored_in_flash(){

	int sect, found, recNo;

	for(sect=2, found=0; ((sect<4) && !found);){
		if((days_sector[sect-2]->year == rf_close_year) &&
				(days_sector[sect-2]->month == rf_close_month) &&
				(days_sector[sect-2]->date == rf_close_date)){

			found =1;

			if(days_sector[sect-2]->samples[sampleNo].rf != 0xFFFF)
				data_not_stored_in_flash = 0;
			else
				data_not_stored_in_flash = 1;

		}
		else
			sect++;
	}

	if(!found){
		data_not_stored_in_flash = 1;
	}
}

void store_data_in_disk(void){

	float s_temp, s_humid, s_ws;
	int s_wd;


	//	cumulative_rf(xLast_Rec_15min_Date_Time.date, xLast_Rec_15min_Date_Time.month, xLast_Rec_15min_Date_Time.year, xLast_Rec_15min_Date_Time.hour, xLast_Rec_15min_Date_Time.minute);
	//	vTaskDelay(100 / portTICK_RATE_MS);

	bat_volt1 = ((((float)(days_sector[present_sector_no-2]->samples[present_sample_no].battery1) * 3.3/4095)/0.138));
	bat_volt2 = ((((float)(days_sector[present_sector_no-2]->samples[present_sample_no].battery2) * 3.3/4095)/0.138));

	s_temp = ((float)days_sector[present_sector_no-2]->samples[present_sample_no].temperature/65536) * 165 - 40;

	s_humid = ((float)days_sector[present_sector_no-2]->samples[present_sample_no].humidity/65534) * 100;

	s_wd = ((int)(days_sector[present_sector_no-2]->samples[present_sample_no].wind_direction * 0.088)) % 360;		// 360/4095 = 0.088

	if (days_sector[present_sector_no-2]->samples[present_sample_no].ws_sec_cnt > 0) {
		s_ws = ((float)(days_sector[present_sector_no-2]->samples[present_sample_no].wind_speed / 4)) / days_sector[present_sector_no-2]->samples[present_sample_no].ws_sec_cnt;
	} else {
		s_ws = 0.0f;
	}


	s_ws = (s_ws * ws_calib_factor ) + ws_offset;
	s_wd = ((int)s_wd * wd_calib_factor) + wd_offset;
	s_temp = (s_temp * temp_calib_factor) + temp_offset;
	s_humid = (s_humid * humid_calib_factor) + humid_offset;

	if((s_temp < 2) || (s_temp > 50)){
		s_temp = 0;
	}

	if((s_humid < 6) || (s_humid > 100)){
		s_humid = 0;
	}

	//	sprintf(record, "stn_no=%s,rec_time=%04d-%02d-%02d,%02d:%02d:00,temp=%.1f,humi=%.1f,ws=%.1f,wd=%d,signal=%04d,bat_vol=%.2f\r\n",
	sprintf(gc_sdc_data, "stn_no=%s,rec_time=%04d-%02d-%02d,%02d:%02d,temp=%.1f,humi=%.1f,ws=%.1f,wd=%d,signal=%d,bat_vol=%02d.%02d,bat_vol2=%02d.%02d\r\n",
			Global_Data_Params.gc_station_id,
			days_sector[present_sector_no-2]->samples[present_sample_no].year,
			days_sector[present_sector_no-2]->samples[present_sample_no].month,
			days_sector[present_sector_no-2]->samples[present_sample_no].date,
			days_sector[present_sector_no-2]->samples[present_sample_no].hour,
			days_sector[present_sector_no-2]->samples[present_sample_no].minute,
			//(xPresent_Date_Time.minute/Global_Data_Params.gi_log_interval)*Global_Data_Params.gi_log_interval,
			s_temp,
			s_humid,
			s_ws,
			s_wd,
			days_sector[present_sector_no-2]->samples[present_sample_no].signal,
			((int)bat_volt1 * 100)/100, (int)(bat_volt1 * 100) % 100,
			((int)bat_volt2 * 100)/100, (int)(bat_volt2 * 100) % 100);




	//Storing log interval data in SD Card
	XITC_Memory_Disk.date = xPresent_Date_Time.date;
	XITC_Memory_Disk.month = xPresent_Date_Time.month;
	XITC_Memory_Disk.year = xPresent_Date_Time.year;
	strcpy(XITC_Memory_Disk.record, gc_sdc_data);

	XITC_Memory_Disk.command = eLog;
	XITC_Memory_Disk.signal = 1;
	while(XITC_Memory_Disk.signal)
		vTaskDelay(100 / portTICK_RATE_MS);

	store_in_disk = 1;
}


void write_days_flag_data(){
	int i,j;

	write_buf_ptr = (int *)local_write_buf_st;
	sector_data_ptr = (int *)days_sector[present_sector_no-2];

	dest_addr = (unsigned int)days_sector[present_sector_no-2];
	for(i=0; i<1024; i++)
		*write_buf_ptr++ = *sector_data_ptr++;

	//now set the start address of the days_sector
	days_storage_ptr =  (Days_Data_def *)local_write_buf_st;

	if(update_present_day_flag == 1){
		if(http_status == 1){
			days_storage_ptr->samples[present_sample_no].signal = gi_signal_level;
			days_storage_ptr->samples[present_sample_no].http_status = 1;
		}
	}

	if(ftp_status == 1){
		for(i = 0; i< ftp_line_cnt; i++){
			days_storage_ptr->samples[ftp_flag[i]].ftp_status = 1;
		}
	}


	erase_status = 1;
	write_status = 1;

	// prepare for erase
	Prepare_Sector(present_sector_no, present_sector_no);
	vTaskDelay(2 / portTICK_RATE_MS);
	__disable_irq();
	erase_status = Erase_Sector(present_sector_no, present_sector_no);
	__enable_irq();

	if(erase_status != 0){
		Prepare_Sector(present_sector_no, present_sector_no);
		vTaskDelay(2 / portTICK_RATE_MS);
		__disable_irq();
		erase_status = Erase_Sector(present_sector_no, present_sector_no);
		__enable_irq();
	}

	vTaskDelay(2 / portTICK_RATE_MS);


	// prpare for write
	Prepare_Sector(present_sector_no, present_sector_no);
	vTaskDelay(2 / portTICK_RATE_MS);

	// write all 4 K
	__disable_irq();
	write_status = Write_Sector(dest_addr, (unsigned int) local_write_buf_st, 4096);
	__enable_irq();

	if(write_status != 0){
		//write_status = Write_Sector(dest_addr, (unsigned int) local_write_buf_st, 4096);
		// prepare for erase
		Prepare_Sector(present_sector_no, present_sector_no);
		vTaskDelay(2 / portTICK_RATE_MS);
		__disable_irq();
		erase_status = Erase_Sector(present_sector_no, present_sector_no);
		__enable_irq();

		if(erase_status == 0){
			// prpare for write
			Prepare_Sector(present_sector_no, present_sector_no);
			vTaskDelay(2 / portTICK_RATE_MS);

			// write all 4 K
			__disable_irq();
			write_status = Write_Sector(dest_addr, (unsigned int) local_write_buf_st, 4096);
			__enable_irq();
		}

	}
	vTaskDelay(2 / portTICK_RATE_MS);

}


int mark_http_sample_sent_in_flash(int sector_idx, int sample_idx) {
	int i;
	int *w_ptr, *s_ptr;
	Days_Data_def *d_ptr;
	int sector_no = sector_idx + 2;
	unsigned int d_addr;

	if (sector_idx < 0 || sector_idx >= 2 || sample_idx < 0 || sample_idx >= 96)
		return -1;

	taskENTER_CRITICAL();

	w_ptr = (int *)local_write_buf_st;
	s_ptr = (int *)days_sector[sector_idx];
	d_addr = (unsigned int)days_sector[sector_idx];

	for (i = 0; i < 1024; i++)
		*w_ptr++ = *s_ptr++;

	d_ptr = (Days_Data_def *)local_write_buf_st;
	d_ptr->samples[sample_idx].http_status = 1;

	Prepare_Sector(sector_no, sector_no);
	__disable_irq();
	erase_status = Erase_Sector(sector_no, sector_no);
	__enable_irq();

	if (erase_status != 0) {
		Prepare_Sector(sector_no, sector_no);
		__disable_irq();
		erase_status = Erase_Sector(sector_no, sector_no);
		__enable_irq();
	}

	Prepare_Sector(sector_no, sector_no);
	__disable_irq();
	write_status = Write_Sector(d_addr, (unsigned int)local_write_buf_st, 4096);
	__enable_irq();

	taskEXIT_CRITICAL();

	return (write_status == 0) ? 1 : -1;
}


void check_and_fill_data_in_flash(){

	int i,j,fill_data;
	int temp_hr, temp_min;

	int t_dd, t_mm, t_yy;

	fill_data = 0;
	write_buf_ptr = (int *)local_write_buf_st;
	sector_data_ptr = (int *)days_sector[present_sector_no-2];

	dest_addr = (unsigned int)days_sector[present_sector_no-2];

	t_dd = days_storage_ptr->date;
	t_mm = days_storage_ptr->month;
	t_yy = days_storage_ptr->year;

	for(i=0; i<1024; i++)
		*write_buf_ptr++ = *sector_data_ptr++;

	for(i=0; i<=present_sample_no; i++){
		if(days_storage_ptr->samples[i].rf == 0xFFFF){
			fill_data = 1;

			int temp_hr, temp_min;

			t_dd = days_storage_ptr->date;
			t_mm = days_storage_ptr->month;
			t_yy = days_storage_ptr->year;

			if(i < 61){
				Previous_Date(&t_dd, &t_mm, &t_yy);
			}

			//=MOD(INT((E1+35)/4), 24)
			//=MOD(INT(E1+35), 4)*15
			temp_hr = ((i+35)/4 % 24);
			temp_min = ((i+35)%4) * 15;

			days_storage_ptr->samples[i].hour = (uint8_t) temp_hr;
			days_storage_ptr->samples[i].minute = (uint8_t)temp_min;
			days_storage_ptr->samples[i].year = (uint16_t)t_yy;
			days_storage_ptr->samples[i].month = (uint8_t)t_mm;
			days_storage_ptr->samples[i].date = (uint8_t)t_dd;
			days_storage_ptr->samples[i].battery1 = 0;
			days_storage_ptr->samples[i].battery2 = 0;
			days_storage_ptr->samples[i].rf = 0;
			days_storage_ptr->samples[i].signal = -81;
			days_storage_ptr->samples[i].http_status = 0;
			days_storage_ptr->samples[i].ftp_status = 0;
		}
	}

	erase_status = 1;
	write_status = 1;

	if(fill_data == 1){
		// prepare for erase
		Prepare_Sector(present_sector_no, present_sector_no);
		vTaskDelay(2 / portTICK_RATE_MS);
		__disable_irq();
		erase_status = Erase_Sector(present_sector_no, present_sector_no);
		vTaskDelay(2 / portTICK_RATE_MS);
		__enable_irq();

		if(erase_status != 0){
			// prepare for erase
			Prepare_Sector(present_sector_no, present_sector_no);
			vTaskDelay(2 / portTICK_RATE_MS);
			__disable_irq();
			erase_status = Erase_Sector(present_sector_no, present_sector_no);
			vTaskDelay(2 / portTICK_RATE_MS);
			__enable_irq();

		}

		// prpare for write
		Prepare_Sector(present_sector_no, present_sector_no);
		vTaskDelay(2 / portTICK_RATE_MS);

		// write all 4 K
		__disable_irq();
		write_status = Write_Sector(dest_addr, (unsigned int) local_write_buf_st, 4096);
		__enable_irq();

		if(write_status != 0){
			// prepare for erase
			Prepare_Sector(present_sector_no, present_sector_no);
			vTaskDelay(2 / portTICK_RATE_MS);
			__disable_irq();
			erase_status = Erase_Sector(present_sector_no, present_sector_no);
			vTaskDelay(2 / portTICK_RATE_MS);
			__enable_irq();

			if(erase_status == 0){
				// prpare for write
				Prepare_Sector(present_sector_no, present_sector_no);
				vTaskDelay(2 / portTICK_RATE_MS);

				// write all 4 K
				__disable_irq();
				write_status = Write_Sector(dest_addr, (unsigned int) local_write_buf_st, 4096);
				__enable_irq();

			}
		}

		vTaskDelay(2 / portTICK_RATE_MS);
	}
}


int day_of_week(int dd, int mm, int yy)
{
	static int t[] = { 0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4 };
	yy -= mm < 3;

	//   yy -= (mm < 3) ? 1 : 0;
	return ( yy + yy/4 - yy/100 + yy/400 + t[mm-1] + dd) % 7;
}


void add_ws_coefficient(){

	int r_num, s_num, s_found, t_cnt, i, t_rnum;
	float temp_ws;
	int t_num;
	//360 720 1080 1440 1800 2160 2520 2880 3240 3600 3960 4320 4680 5040 5400
	//0.1 0.2 0.3   0.4  0.5 0.6   0.7  0.8  0.9  1    1.1  1.2  1.3  1.4  1.5

	int rand_ws_samples[20] = {370, 740, 1090, 1450, 1850, 2170, 2530, 2890, 3250, 3610, 3970, 4330, 4690, 5050, 5410, 5710};

	temp_ws = ((float)(last_rec_ws / 4)) / last_rec_ws_sec;

	if((xPresent_Date_Time.hour % 2) == 0)
		t_num = 3;
	else if((xPresent_Date_Time.hour % 3) == 0)
		t_num = 4;
	else
		t_num = 5;


	//	r_num = rand();
	r_num = xPresent_Date_Time.year + xPresent_Date_Time.month + (xPresent_Date_Time.date * 3) +
			(xPresent_Date_Time.hour * t_num) + (xPresent_Date_Time.minute * 60) + ((xPresent_Date_Time.second+1) * 4);


	r_num %= 15 ;

	t_rnum = r_num % 5;

	//SATISH JAN 2026
//	if((sampleNo > 4) && ((temp_ws <= 0) || (last_rec_ws < 200))){
//
//		for(s_num=2, s_found=0; ((s_num<4) && !s_found);){
//			//if(days_sector[sect-2]->date == curDate)
//			if((days_sector[s_num-2]->year == rf_close_year) && (days_sector[s_num-2]->month == rf_close_month) &&
//					(days_sector[s_num-2]->date == rf_close_date))
//				s_found =1;
//			else
//				s_num++;
//		}
//
//		t_cnt = sampleNo;
//
//		if((sampleNo % 2) == 0 ){
//			for(i = (t_cnt - 2); i <= t_cnt ; i++){
//				if(days_sector[s_num-2]->samples[i].wind_speed != 0xFFFF)
//					ws_ce +=  days_sector[s_num-2]->samples[i].wind_speed;
//			}
//			if((ws_ce < 200) || (ws_ce > 60000)){
//				ws_ce = rand_ws_samples[r_num];
//				ws_sec_ce = 845;
//			}
//		}
//
//		else if((sampleNo % 3) == 0 ){
//			for(i = (t_cnt - 3); i <= t_cnt ; i++){
//				if(days_sector[s_num-2]->samples[i].wind_speed != 0xFFFF)
//					ws_ce +=  days_sector[s_num-2]->samples[i].wind_speed;
//			}
//			if((ws_ce < 200) || (ws_ce > 60000)){
//				ws_ce = rand_ws_samples[r_num];
//				ws_sec_ce = 845;
//			}
//		}
//
//		else if((sampleNo % t_rnum) == 0 ){
//			for(i = (t_cnt - 3); i <= t_cnt ; i++){
//				if(days_sector[s_num-2]->samples[i].wind_speed != 0xFFFF)
//					ws_ce +=  days_sector[s_num-2]->samples[i].wind_speed;
//			}
//			if((ws_ce < 200) || (ws_ce > 60000)){
//				ws_ce = rand_ws_samples[r_num];
//				ws_sec_ce = 885;
//			}
//		}
//		else
//			vTaskDelay(1 / portTICK_RATE_MS);
//	}

}




void vMemory_Task(void *pvParameters)
{
	int m,i;

	int get_ui_rf;

	char new_version[16];

	char *matching_ptr;
	gi_save_params_in_flash = 0;
	data_not_stored_in_flash = 0;
	c_rainfall = 0;
	get_ui_rf = 1;
	send_msg_data = 0;
	gi_data_ready_for_report = 0;
	update_present_day_flag = 0;
	check_prev_day_ftp = 0;
	update_previous_day_flag = 0;
	wait_for_sms_process = 0;

	vTaskDelay(100 / portTICK_RATE_MS);

	sim_select = 0;
	sleep_mode = 0;
	fill_data_in_flash = 0;
	erase_data_sector = 0;
	firmware_updated = 0;

	ws_ce = 0;
	ws_sec_ce = 0;

	params_default();
	vTaskDelay(500 / portTICK_RATE_MS);

	//	init_entire_space();
	vTaskDelay(100 / portTICK_RATE_MS);

	strcpy(new_version, "TWS6-1.0.12");

	Global_Data_Params_From_Flash = (Global_Data_Params_def*)PARAM_START_ADDR;

	vTaskDelay(500 / portTICK_RATE_MS);

	const char *active_profile_model =
#if (UNIT_CFG == CFG_BIHAR_TRG)
		"TRG-BIH";
#elif (UNIT_CFG == CFG_KSNDMC_TWS)
		"TWS-DMC";
#else
		"TRG-DMC";
#endif

	/* Check if Flash Sector 1 contains valid parameters AND matches active UNIT_CFG profile */
	int need_reinit = 0;
	if (look_for_str(Global_Data_Params_From_Flash->gc_model, (char*)active_profile_model) == 0) {
		need_reinit = 1; /* Profile changed! Erase obsolete server/key data from flash */
	}
	if (look_for_str(Global_Data_Params_From_Flash->gc_version, new_version) == 0) {
		need_reinit = 1;
		firmware_updated = 1;
	}

	if (need_reinit) {
		char saved_stn[16] = {0};
		if (Global_Data_Params_From_Flash->gc_station_id[0] >= '0' &&
		    Global_Data_Params_From_Flash->gc_station_id[0] <= '9') {
			strncpy(saved_stn, Global_Data_Params_From_Flash->gc_station_id, sizeof(saved_stn) - 1);
		}

		params_default();
		vTaskDelay(200 / portTICK_RATE_MS);

		strcpy(Global_Data_Params.gc_version, new_version);
		if (strlen(saved_stn) >= 4) {
			strcpy(Global_Data_Params.gc_station_id, saved_stn);
		}
		Global_Data_Params.gi_send_data_sms = 0;

		write_param_sector();
		vTaskDelay(50 / portTICK_RATE_MS);
	}

	read_param_sector();

	if((Global_Data_Params.gi_log_interval > 31) || (Global_Data_Params.gi_report_interval > 31)){
		char temp_stn[16] = {0};
		if (Global_Data_Params_From_Flash->gc_station_id[0] >= '0' &&
		    Global_Data_Params_From_Flash->gc_station_id[0] <= '9') {
			strncpy(temp_stn, Global_Data_Params_From_Flash->gc_station_id, sizeof(temp_stn) - 1);
		}
		params_default();
		vTaskDelay(500 / portTICK_RATE_MS);

		if (strlen(temp_stn) >= 4) {
			strcpy(Global_Data_Params.gc_station_id, temp_stn);
		}

		write_param_sector();
		vTaskDelay(50 / portTICK_RATE_MS);

		read_param_sector();
		vTaskDelay(50 / portTICK_RATE_MS);
	}






	///	read_param_sector();
	vTaskDelay(500 / portTICK_RATE_MS);

	strcpy(gc_station_id1, Global_Data_Params.gc_station_id);
	strcpy(gc_version_sdc, Global_Data_Params.gc_version);

	vTaskDelay(100 / portTICK_RATE_MS);

	memory_task_started = 1;

	plus_minus_sym = '+';

	//	Global_Data_Params.gi_send_data_sms = 1;
	//	write_param_sector();

	vTaskDelay(8000 / portTICK_RATE_MS);

	//	find_old_recent_sector();

	ws_actual1 = ws_measued1 = temp_actual1 = temp_measued1 = humid_actual1 = humid_measued1 = wd_actual1 = wd_measued1 = 1;
	ws_actual2 = ws_measued2 = temp_actual2 = temp_measued2 = humid_actual2 = humid_measued2 = wd_actual2 = wd_measued2 = 2;



	temp_measued1 = 22.8;
	temp_measued2 = 23.8;
	temp_actual1 = 25.6;
	temp_actual2 = 26.6;

	humid_measued1 = 77.4;
	humid_measued2 = 78.6;
	humid_actual1 = 68.5;
	humid_actual2 = 69.4;


	if(gi_disk_ok == 1){
		XITC_Memory_Disk.command = eGet_Params;
		XITC_Memory_Disk.signal = 1;
		while(XITC_Memory_Disk.signal)
			vTaskDelay(100 / portTICK_RATE_MS);

		if((XITC_Memory_Disk.result == eNo_File) || (firmware_updated == 1)){

			// create the params file
			XITC_Memory_Disk.command = eCreate_Params;
			XITC_Memory_Disk.signal = 1;
			while(XITC_Memory_Disk.signal)
				vTaskDelay(100 / portTICK_RATE_MS);

			vTaskDelay(100 / portTICK_RATE_MS);

		}
	}

	ws_calib_factor    = ((ws_actual2-ws_actual1) / (ws_measued2 - ws_measued1)); // SATISH (2 - 1)/(2-1) = 1
	temp_calib_factor  = ((temp_actual2-temp_actual1) / (temp_measued2 - temp_measued1));		//26.6-25.6/23.8-22.8  //1
	humid_calib_factor = ((humid_actual2-humid_actual1) / (humid_measued2 - humid_measued1));	//69.4-68.5/78.6-77.4  //0.9/1.2=0.75
	wd_calib_factor    = ((wd_actual2-wd_actual1) / (wd_measued2 - wd_measued1));


	// ( a2(m2-m1) - m2(a2-a1)) / (m2-m1)
	/*	ws_offset = (((ws_actual2 * (ws_measued2 - ws_measued1)) -  (ws_measued2 * (ws_actual2-ws_actual1))) / (ws_measued2 - ws_measued1));

	temp_offset = (((temp_actual2 * (temp_measued2 - temp_measued1)) -  (temp_measued2 * (temp_actual2 - temp_actual1))) / (temp_measued2 - temp_measued1));

	humid_offset = (((humid_actual2 * (humid_measued2 - humid_measued1)) -  (humid_measued2 * (humid_actual2 - humid_actual1))) / (humid_measued2 - humid_measued1));

	wd_offset = (((wd_actual2 * (wd_measued2 - wd_measued1)) -  (wd_measued2 * (wd_actual2-wd_actual1))) / (wd_measued2 - wd_measued1));
	 */
	//offset = a2 - slope*m2

	ws_offset    = ws_actual2 - (ws_calib_factor * ws_measued2); //SATISH 2- (1*2) = 0
	temp_offset  = temp_actual2 - (temp_calib_factor * temp_measued2);			//26.6 - (1 * 23.8) = 2.8
	humid_offset = humid_actual2 - (humid_calib_factor * humid_measued2);		//69.4 - (0.75 * 78.6) = 69.4 - 58.95 = 10.45
	wd_offset    = wd_actual2 - (wd_calib_factor * wd_measued2);

	vTaskDelay(1 / portTICK_RATE_MS);

	for(;;){

		taskENTER_CRITICAL();
		gi_wdt_task_ok_stat |= WDT_MEMORY_OK;
		taskEXIT_CRITICAL();
		vTaskDelay(1000 / portTICK_RATE_MS);

		vTaskDelay(500 / portTICK_RATE_MS);

		if(erase_data_sector == 1){
			init_entire_space();
			vTaskDelay(10 / portTICK_RATE_MS);
			erase_data_sector = 0;

			data_sectors_not_allocated = 1;

		}

		if(gi_recv_pkt_ready == 1){


			check_data_is_stored_in_flash();
			vTaskDelay(100 / portTICK_RATE_MS);

			//store current data in flash
			if(data_not_stored_in_flash == 1){
				find_latest_date_from_data_sectors();
				vTaskDelay(10 / portTICK_RATE_MS);

				add_ws_coefficient();
				vTaskDelay(10 / portTICK_RATE_MS);

				if(ws_ce > 4100){
					ws_ce %= 2100;
				}

				//find_latest_date_from_data_sectors();

				store_rec.rf = (uint16_t) last_rec_15min_rf;
				store_rec.battery1 = (uint16_t)last_rec_15min_bat1;
				store_rec.battery2 =(uint16_t)  last_rec_15min_bat2;
				store_rec.hour = (uint8_t) xLast_Rec_15min_Date_Time.hour;
				store_rec.minute = (uint8_t) xLast_Rec_15min_Date_Time.minute;
				store_rec.signal = -79;
				store_rec.year = (uint16_t) xLast_Rec_15min_Date_Time.year;
				store_rec.month = (uint8_t) xLast_Rec_15min_Date_Time.month;
				store_rec.date = (uint8_t) xLast_Rec_15min_Date_Time.date;
				store_rec.temperature = (uint16_t) last_rec_temperature;
				store_rec.humidity = (uint16_t) last_rec_humid;
				store_rec.wind_direction = (uint16_t) last_rec_wd;
				store_rec.wind_speed = (uint16_t) last_rec_ws + ws_ce;
				store_rec.ws_sec_cnt = (uint16_t) last_rec_ws_sec + ws_sec_ce;


				//	write_days_data(&store_rec, xLast_Rec_15min_Date_Time.date,xLast_Rec_15min_Date_Time.month,xLast_Rec_15min_Date_Time.year);
				write_days_data(&store_rec, rf_close_date, rf_close_month, rf_close_year);

				vTaskDelay(100 / portTICK_RATE_MS);

				if(gi_disk_ok == 1){
					store_data_in_disk();
					vTaskDelay(100 / portTICK_RATE_MS);
				}

				gi_data_ready_for_report = 1;
				//store_ftp_sdc = 1;
				data_not_stored_in_flash = 0;
			}

			//for user interface cumulative rainfall
			if((gUi_Active) && (get_ui_rf)){
				//cumulative_rf_new(xLast_Rec_15min_Date_Time.date, xLast_Rec_15min_Date_Time.month, xLast_Rec_15min_Date_Time.year, xLast_Rec_15min_Date_Time.hour, xLast_Rec_15min_Date_Time.minute);
				cumulative_rf_new(rf_close_date, rf_close_month, rf_close_year, xLast_Rec_15min_Date_Time.hour, xLast_Rec_15min_Date_Time.minute);
				//cumulative_rf_new(rf_close_date, rf_close_month, rf_close_year, 8, 30);

				u_rainfall = c_rainfall;
				vTaskDelay(100 / portTICK_RATE_MS);
				get_ui_rf = 0;
			}

			gi_recv_pkt_ready = 0;
		}

		//Update present day http flag and ftp flag and also fill data gaps
		if(update_present_day_flag == 1){

			write_days_flag_data();
			vTaskDelay(100 / portTICK_RATE_MS);

			update_present_day_flag = 0;

			if((xLast_Rec_15min_Date_Time.hour == 8) && (xLast_Rec_15min_Date_Time.minute == 30))
				store_ftp_sdc = 1;
		}


		if(store_ftp_sdc == 1) {
			if(gi_disk_ok == 1){
				for(i=0;i<96;i++){
					//	present_sector_no = 3;
					if((days_sector[present_sector_no-2]->samples[i].http_status != 1) &&
							(days_sector[present_sector_no-2]->samples[i].ftp_status != 1) &&
							((days_sector[present_sector_no-2]->samples[i].rf != 0xFFFF))){


						f_bat_volt1 = ((((float)(days_sector[present_sector_no-2]->samples[i].battery1) * 3.3/4095)/0.128));
						f_bat_volt2 = ((((float)(days_sector[present_sector_no-2]->samples[i].battery2) * 3.3/4095)/0.128));

						f_temp = ((float)days_sector[present_sector_no-2]->samples[i].temperature/65536) * 165 - 40;
						f_humid = ((float)days_sector[present_sector_no-2]->samples[i].humidity/65534) * 100;
						f_wd = ((int)(days_sector[present_sector_no-2]->samples[i].wind_direction * 0.088)) % 360;		// 360/4095 = 0.088
						f_ws = ((float)(days_sector[present_sector_no-2]->samples[i].wind_speed / 4)) / days_sector[present_sector_no-2]->samples[i].ws_sec_cnt;


						f_ws = (f_ws * ws_calib_factor ) + ws_offset;
						f_wd = ((int)f_wd * wd_calib_factor) + wd_offset;
						f_temp = (f_temp * temp_calib_factor) + temp_offset;
						f_humid = (f_humid * humid_calib_factor) + humid_offset;

						if((f_temp < 2) || (f_temp > 50)){
							f_temp = 0;
						}
						if(f_humid < 6) {
							f_humid = 0;
						}

						if(f_humid > 100)
							f_humid = 100;

						if(f_ws < 0)
							f_ws = 0;

						//"%s;%04d-%02d-%02d,%02d:%02d;%c%02d.%1d;%03d.%1d;%02d.%1d;%03d;%04d;%02d.%02d\r\n",
						sprintf(ftp_rec, "%s;%04d-%02d-%02d,%02d:%02d;%c%02d.%1d;%03d.%1d;%02d.%1d;%03d;%04d;%02d.%02d;%02d.%02d\r\n",
								Global_Data_Params.gc_station_id,
								days_sector[present_sector_no-2]->samples[i].year,
								days_sector[present_sector_no-2]->samples[i].month,
								days_sector[present_sector_no-2]->samples[i].date,
								days_sector[present_sector_no-2]->samples[i].hour,
								days_sector[present_sector_no-2]->samples[i].minute,
								plus_minus_sym,
								((int)f_temp * 100)/100,(int)(f_temp * 10) % 10,
								((int)f_humid * 100)/100, (int)(f_humid * 10) % 10,
								((int)f_ws * 100)/100, (int)(f_ws * 10) % 10,
								f_wd,
								days_sector[present_sector_no-2]->samples[i].signal,
								((int)f_bat_volt1 * 100)/100, (int)(f_bat_volt1 * 100) % 100,
								((int)f_bat_volt2 * 100)/100, (int)(f_bat_volt2 * 100) % 100);

						XITC_Memory_Disk.command = eStore_Ftp_Data;
						strcpy(XITC_Memory_Disk.record, ftp_rec);
						XITC_Memory_Disk.signal = 1;
						// only after writing FTP data is done, you can open the file
						while(XITC_Memory_Disk.signal)
							vTaskDelay(100 / portTICK_RATE_MS);

					}
				}
			}
			store_ftp_sdc = 0;

		}


		if(get_gps > 2)
			get_gps = 0;


		if(gi_save_params_in_flash == 1){
			write_param_sector();
			vTaskDelay(100 / portTICK_RATE_MS);
			gi_save_params_in_flash = 0;
		}


		if(sleep_mode == 1){
			write_days_flag_data();
			vTaskDelay(100 / portTICK_RATE_MS);
			sleep_mode = 0;
			if (!gUi_Active) {
				system_enter_sleep();
			} else {
				shutdown_pwr = 1;
			}
		}


	}
}

