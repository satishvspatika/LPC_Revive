#define KBD_ROW0_PORT	3
#define KBD_ROW0_PINNO	25
#define KBD_ROW0_MASK	(1<<KBD_ROW0_PINNO)

#define KBD_ROW1_PORT	3
#define KBD_ROW1_PINNO	26
#define KBD_ROW1_MASK	(1<<KBD_ROW1_PINNO)

#define KBD_ROW2_PORT	1
#define KBD_ROW2_PINNO	21
#define KBD_ROW2_MASK	(1<<KBD_ROW2_PINNO)

#define KBD_COL0_PORT	0
#define KBD_COL0_PINNO	22
#define KBD_COL0_MASK	(1<<KBD_COL0_PINNO)

#define KBD_COL1_PORT	0
#define KBD_COL1_PINNO	21
#define KBD_COL1_MASK	(1<<KBD_COL1_PINNO)

#define KBD_COL2_PORT	0
#define KBD_COL2_PINNO	20
#define KBD_COL2_MASK	(1<<KBD_COL2_PINNO)

#define MAX_ROWS 3

#define KEY_TIMEOUT 0xff
#define WILD_KEY 0x2A
#define NO_SUCH_KEY '!'
#define MAX_TIME_KEYTIMEOUT 18000	// 3 minutes


// variables and structures
// inter-task communication struture
typedef struct{
	volatile int sig_key;
	volatile int key_value;
} XITC_Kbd_Log_Def;

enum KEYNAMES{
	eNo_Key,
	eMenu_Key,
	eUp_Key,
	eCalib_Key,
	eLeft_Key,
	eSet_Key,
	eRight_Key,
	eBack_Key,
	eDown_Key,
	eDisp_Key
};


XITC_Kbd_Log_Def XITC_Kbd_Log;

extern volatile int dbg_last_key;
extern volatile int dbg_raw_cols;
extern volatile uint32_t dbg_key_count;



// prototypes
void init_kbd_pins(void);
void kbd_release_pins_for_sleep(void);
int Kbd_Scan(void);
void Output_Row(int iRow);
int Read_Col_Pressed(void);
void vKbd_Task(void *pvParameters);




