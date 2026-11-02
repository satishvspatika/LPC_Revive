

#define PARAM_SECTOR 1
#define PARAM_START_ADDR 0x1000


#define DATA_START_SECTOR 2
#define DATA_END_SECTOR   10

#define DATA_START_ADDR 0x2000


#define IAP_LOCATION 0X1FFF1FF1
#define XOSC  398000



char gc_sdc_data[150];
char gc_http_data[256];
char gc_sms_data[150];

enum CMD_MEMORY_DISK {eNop, eLog, eGetLast,
	eStore_Ftp_Data, eDelete_Ftp_File, eGet_Params, eCreate_Params, eDelete_Params, eGet_Stn_Details};

enum STAT_MEMORY_DISK {eDone, eNo_File};

enum CMD_MEMOEY_GPRS {eLog_Gprs_Done, eHttp, eFtp, eFtp_No_File,  eAutodump, eProcess_Msg};


typedef struct {
	int signal;
	int command;
	int result;
	int server;
	char data[150];
} XITC_Memory_Gprs_def;
XITC_Memory_Gprs_def XITC_Memory_Gprs;

typedef struct {
	int signal;
	int command;
	int result;
	int date;
	int month;
	int year;
	char record[150];
	char read_buf[150];
} XITC_Memory_Disk_def;

XITC_Memory_Disk_def XITC_Memory_Disk;

//function prototypes
void vMemory_Task(void *pvParameters);

short int Erase_Sector(int Start_Sec_No, int End_Sec_No);
short int Prepare_Sector(int Start_Sec_No, int End_Sec_No);
short int Write_Sector(unsigned int Dest_Addr, unsigned int Source_Addr,int Bytes);
short int Blank_Check_Sector(int Start_Sec_No, int End_Sec_No);
short int Compare_Sector(unsigned int Dest_Addr, unsigned int Source_Addr,int Bytes);

void params_default(void);
void write_param_sector(void);
void read_param_sector(void);

int write_days_data(Samples_def *rec_data,uint8_t curDate, uint8_t curMonth, uint16_t curYear);
void erase_n_format_sector(int sector);
void init_entire_space(void);

void cumulative_rf_new(int dd, int mm, int yy, int hh, int min);

int get_allocation_map(uint8_t curDate, uint8_t curMonth, uint16_t curYear);
int allocate_space(uint8_t curDate, uint8_t curMonth, uint16_t curYear);

//int day_of_week(int dd, int mm, int yy);

void store_data_in_disk(void);

