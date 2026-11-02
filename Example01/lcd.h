
#define TOP_LINE	0x80
#define BOTTOM_LINE 0xC0
#define LCD_OFF_CMD 0x08
#define LCD_ON_CMD	0x0C
#define CURSOR_OFF_CMD	0x0C
#define CURSOR_ON_CMD	0x0E


// command for LCD conroller h/w
#define eLcd_Cmd 0
#define eLcd_Data 1


#define LCD_DATA_PORT 	    	2
#define LCD_D4_PINNO			9
#define LCD_D4_MASK				(1<<LCD_D4_PINNO)

#define LCD_D5_PINNO			6
#define LCD_D5_MASK				(1<<LCD_D5_PINNO)

#define LCD_D6_PINNO			7
#define LCD_D6_MASK				(1<<LCD_D6_PINNO)

#define LCD_D7_PINNO			8
#define LCD_D7_MASK				(1<<LCD_D7_PINNO)

#define LCD_CTRL_PORT			2
#define LCD_RS_PINNO 			4
#define LCD_RS_MASK				(1<<LCD_RS_PINNO)

#define LCD_RW_PINNO			3
#define LCD_RW_MASK				(1<<LCD_RW_PINNO)

#define LCD_EN_PINNO			2
#define LCD_EN_MASK				(1<<LCD_EN_PINNO)

#define LCD_BL_PORT 	    	2
#define LCD_BL_PINNO			11
#define LCD_BL_MASK				(1<<LCD_BL_PINNO)

#define LCD_BUSY_FLAG           LCD_D7_MASK
#define LCD_DATA_MASK			LCD_D4_MASK|LCD_D5_MASK|LCD_D6_MASK|LCD_D7_MASK
#define LCD_DATA		   		LPC_GPIO1->FIOPIN
// variables and structures
// inter-task communication struture

enum {
	eLcd_Nop = 0,
	eLcd_Init = 'I',
	eLcd_Off = 'F',
	eLcd_On = 'N',
	eLcd_Cur_On = 'n',
	eLcd_Cur_Off = 'f',
	eLcd_Display = 'D',
	eLcd_Set_Addr = 'A'
};







 void init_lcd_pins(void);
 void wait_lcd( void );
 void Output_Lcd(char cData, char cCd);
 void Initialize_Lcd(void);
 void vLcd_Task(void *pvParameters);


/*
#define TOP_LINE	0x80
#define BOTTOM_LINE 0xC0
#define LCD_OFF_CMD 0x08
#define LCD_ON_CMD	0x0C
#define CURSOR_OFF_CMD	0x0C
#define CURSOR_ON_CMD	0x0E


// command for LCD conroller h/w
#define eLcd_Cmd 0
#define eLcd_Data 1


#define LCD_DATA_PORT 	    	1
#define LCD_D4_PINNO			25
#define LCD_D4_MASK				(1<<LCD_D4_PINNO)

#define LCD_D5_PINNO			28
#define LCD_D5_MASK				(1<<LCD_D5_PINNO)

#define LCD_D6_PINNO			29
#define LCD_D6_MASK				(1<<LCD_D6_PINNO)

#define LCD_D7_PINNO			31
#define LCD_D7_MASK				(1<<LCD_D7_PINNO)

#define LCD_CTRL_PORT			0
#define LCD_RS_PINNO 			17
#define LCD_RS_MASK				(1<<LCD_RS_PINNO)

#define LCD_RW_PINNO			18
#define LCD_RW_MASK				(1<<LCD_RW_PINNO)

#define LCD_EN_PINNO			19
#define LCD_EN_MASK				(1<<LCD_EN_PINNO)

#define LCD_BUSY_FLAG           LCD_D7_MASK
#define LCD_DATA_MASK			LCD_D4_MASK|LCD_D5_MASK|LCD_D6_MASK|LCD_D7_MASK
#define LCD_DATA		   		LPC_GPIO1->FIOPIN
// variables and structures
// inter-task communication struture

enum {
	eLcd_Nop = 0,
	eLcd_Init = 'I',
	eLcd_Off = 'F',
	eLcd_On = 'N',
	eLcd_Cur_On = 'n',
	eLcd_Cur_Off = 'f',
	eLcd_Display = 'D',
	eLcd_Set_Addr = 'A'
};







 void init_lcd_pins(void);
 void wait_lcd( void );
 void Output_Lcd(char cData, char cCd);
 void Initialize_Lcd(void);
 void vLcd_Task(void *pvParameters);
*/

void lcd_backlight_on(void);
void lcd_backlight_off(void);

