
#define GPRS_UART LPC_UART2

#define GPRS_PORT_NO 0
#define GPRS_TX_PIN 10
#define GPRS_RX_PIN 11


#define PWR_KEY_PORT 0
#define PWR_KEY_PIN 1
#define PWR_KEY_MASK (1 << PWR_KEY_PIN)


#define STATUS_KEY_PORT 0
#define STATUS_KEY_PIN 0
#define STATUS_KEY_MASK (1 << STATUS_KEY_PIN)

/*
#define GPRS_REST_PORT 2
#define GPRS_REST_PIN 13
#define GPRS_RESET_MASK (1 << GPRS_REST_PIN)
#define GPRS_ON_OFF_PORT 2
#define GPRS_ON_OFF_PIN 12
#define GPRS_ON_OFF_MASK (1 << GPRS_ON_OFF_PIN)*/



int gi_dota_state, gi_dota_error;
int gi_gps_error;
char ftp_buff[1064];


enum {eHttp_Post, eFtp_Post, eFtp_Get, eHttp_Get};
enum { eAck=1, eNack=2, eTimeout=3, eXfer_Success=4, eXfer_Fail=5,
				eXfer_Rejected=6, eXfer_In_Progress=7, eXfer_Tmo=8, eSent=9,eCmd_Over=10 };



enum {eFile_Open, eFile_Get_Line, eFile_Close, eFile_Opened, eFile_End, eLine_Read, eLine_Read_In_Progress,
eFile_Not_Found, eFile_Closed, eSave_Params, eGet_Rec, eFile_Write, eLine_Recd, eFTP_File_Create,eFTP_File_Create1,
eFTP_Write_Buf, eBuf_Written, eDelete_Dota_File, eDelete_Dota_File1, eDel_Ftp_File, eDel_Param_File,  ePrepare_Auto_Dump, eCheck_Dota_File, eCheck_Dota_File1,
eGetLineNo, eSkipLines, eSetLineNo};


typedef struct {
	int signal;
	int command;
	int line_no;
	int result;
	int date;
	int month;
	int year;
	int hour;
	int minute;
	char filename[16];
	char data[150];		// parameterise this while going for TWS
} XITC_Gprs_Disk_def;
XITC_Gprs_Disk_def XITC_Gprs_Disk;

typedef struct{
	int sms;
	int http;
	int ftp;
	int remote_ftp;
}GPRS_Done_Def;
GPRS_Done_Def GPRS_Done;


typedef struct{
	int signal;
	int command;
	int result;
	char *buf_ptr;
	int count;
}XITC_Gprs_Disk_Fw_def;
XITC_Gprs_Disk_Fw_def XITC_Gprs_Disk_Fw;


//functions prototypes

void vGprs_Task( void *pvParameters);


void UART2_IRQHandler(void);
void gprs_recv_isr(void);
void gprs_xmit_isr(void);

int Send_And_Check_Gprs(char* xmit_str, char *ack, char *nack, int timeout);

void power_on_sim_module(void);
void check_basic_commands(void);
void check_sim_registration(void);
void check_sim_cpin(void);
void get_signal_level(void);
void get_apn(void);
int send_message(char *gsm_no, char *msg_text);
void get_gps_info(void);
void disconnect_http1(void);

void process_msg(int msg_no);
int process_tcpip_xfer(int xfer_type, int server_set);
int http_post(int server_set);
int ftp_post();
void prepare_and_send_status(char *gsm_no);
int ftp_dota(void);
void save_err_det(void);


int connect_http1();
int http_send_data();
int prepare_http_record(int sector_idx, int sample_idx);
void process_http_backlogs(void);

int connect_ftp1();
int ftp_send_data();

int connect_ftp2();
int ftp_dota();

void switch_sim_card1(void);
void switch_sim_card2(void);

void select_sim_0(void);
void select_sim_1(void);

void prepare_present_rec_for_http_sms(void);
char *mv2endof_str(char* src, char* match, int len);
void gprs_send_cmd_direct(const char *cmd);
//void check_for_receive_msg(void);


