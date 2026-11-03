#include "lcd.h"
#include "ui.h"
#include "FreeRTOS.h"
#include "task.h"
#include "lpc17xx_pinsel.h"
#include "lpc17xx_gpio.h"
#include "globals.h"

/*-------------------------------------------------------------------------------------------------------------------*
* 	FUNCTION NAME : init_lcd_pins
 * 	DESCRIPTION : This function is used to initialize all the LCD related pins
* 	VERSION : 1.5
* 	ARGUMENTS : None
* 	RETURN VALUES : None
* 	GLOBALS INPUT : None
*	GLOBALS AFFECTED : None
*	CALLED FUNCTION : vLcd_Task
*	CALLING FUNCTION : None
*

/*---------------------------------------------------------------------------------------------------------------------*/

void init_lcd_pins(void){
	PINSEL_CFG_Type LCD_config;
	// common parameters
	LCD_config.Funcnum   = PINSEL_FUNC_0;
	LCD_config.Pinmode   = PINSEL_PINMODE_PULLUP; //PINSEL_PINMODE_TRISTATE;  //PINSEL_PINMODE_PULLUP;

	LCD_config.OpenDrain = PINSEL_PINMODE_NORMAL;


	LCD_config.Portnum =   LCD_DATA_PORT;
	LCD_config.Portnum =   LCD_CTRL_PORT;

	//d4
	LCD_config.Pinnum  = LCD_D4_PINNO;
	PINSEL_ConfigPin(&LCD_config);
	GPIO_SetDir(LCD_DATA_PORT, LCD_D4_MASK, DIR_OUT);

	//d5
	LCD_config.Pinnum  = LCD_D5_PINNO;
	PINSEL_ConfigPin(&LCD_config);
	GPIO_SetDir(LCD_DATA_PORT, LCD_D5_MASK, DIR_OUT);

	//d6
	LCD_config.Pinnum  = LCD_D6_PINNO;
	PINSEL_ConfigPin(&LCD_config);
	GPIO_SetDir(LCD_DATA_PORT, LCD_D6_MASK, DIR_OUT);

	//d7
	LCD_config.Pinnum  = LCD_D7_PINNO;
	PINSEL_ConfigPin(&LCD_config);
	GPIO_SetDir(LCD_DATA_PORT, LCD_D7_MASK, DIR_OUT);

	//RS
	LCD_config.Pinnum  = LCD_RS_PINNO;
	PINSEL_ConfigPin(&LCD_config);
	GPIO_SetDir(LCD_CTRL_PORT, LCD_RS_MASK, DIR_OUT);

	//RW
	LCD_config.Pinnum  = LCD_RW_PINNO;
	PINSEL_ConfigPin(&LCD_config);
	GPIO_SetDir(LCD_CTRL_PORT, LCD_RW_MASK, DIR_OUT);

	//EN
	LCD_config.Pinnum  = LCD_EN_PINNO;
	PINSEL_ConfigPin(&LCD_config);
	GPIO_SetDir(LCD_CTRL_PORT, LCD_EN_MASK, DIR_OUT);


	//BL
	LCD_config.Portnum =   LCD_BL_PORT;
	LCD_config.Pinnum  = LCD_BL_PINNO;
	PINSEL_ConfigPin(&LCD_config);
	GPIO_SetDir(LCD_BL_PORT, LCD_BL_MASK, DIR_OUT);

	/* Initialize all control and data lines to 0 (Write mode, Command mode, EN low, BL off) */
	GPIO_ClearValue(LCD_CTRL_PORT, LCD_RS_MASK | LCD_RW_MASK | LCD_EN_MASK);
	GPIO_ClearValue(LCD_DATA_PORT, LCD_DATA_MASK);
	GPIO_ClearValue(LCD_BL_PORT, LCD_BL_MASK);
}

void lcd_backlight_on(void)
{
	GPIO_SetValue(LCD_BL_PORT, LCD_BL_MASK);
}

void lcd_backlight_off(void)
{
	GPIO_ClearValue(LCD_BL_PORT, LCD_BL_MASK);
}

/*-------------------------------------------------------------------------------------------------------------------*
* 	FUNCTION NAME : Initialize_Lcd
 * 	DESCRIPTION : This function is used to Initialize the LCD related Tasks
* 	VERSION : 1.5
* 	ARGUMENTS : None
* 	RETURN VALUES : None
* 	GLOBALS INPUT : None
*	GLOBALS AFFECTED : None
*	CALLED FUNCTION : vLcd_Task
*	CALLING FUNCTION : Output_Lcd, GPIO_SetValue
*

/*---------------------------------------------------------------------------------------------------------------------*/

static void lcd_write_nibble(unsigned char nibble, char cCd)
{
	unsigned int mask = 0;

	/* 1. Setup RS: 1 = data, 0 = command. RW is always 0 (write mode) */
	if (cCd == eLcd_Data) {
		GPIO_SetValue(LCD_CTRL_PORT, LCD_RS_MASK);
	} else {
		GPIO_ClearValue(LCD_CTRL_PORT, LCD_RS_MASK);
	}
	GPIO_ClearValue(LCD_CTRL_PORT, LCD_RW_MASK);

	/* 2. Map nibble bits to GPIO pin masks (D4=P2.9, D5=P2.6, D6=P2.7, D7=P2.8) */
	if (nibble & 0x01) mask |= LCD_D4_MASK;
	if (nibble & 0x02) mask |= LCD_D5_MASK;
	if (nibble & 0x04) mask |= LCD_D6_MASK;
	if (nibble & 0x08) mask |= LCD_D7_MASK;

	/* 3. Output data onto D4..D7 WHILE EN IS LOW (strictly satisfies HD44780 setup time) */
	GPIO_ClearValue(LCD_DATA_PORT, LCD_DATA_MASK);
	GPIO_SetValue(LCD_DATA_PORT, mask);

	/* 4. Address and data setup time before EN goes HIGH (tAS >= 40ns, tDS >= 80ns) */
	for (volatile int d = 0; d < 50; d++); /* ~500 ns */

	/* 5. Pulse Enable HIGH (tW >= 230ns; safe 2us) */
	GPIO_SetValue(LCD_CTRL_PORT, LCD_EN_MASK);
	for (volatile int d = 0; d < 200; d++); /* ~2 us pulse width */

	/* 6. Drive Enable LOW (falling edge latches data into HD44780) */
	GPIO_ClearValue(LCD_CTRL_PORT, LCD_EN_MASK);

	/* 7. Hold time and cycle recovery before next transfer (tH >= 10ns, tC >= 500ns) */
	for (volatile int d = 0; d < 200; d++); /* ~2 us hold/recovery */
}

void Initialize_Lcd(void) {
	/* 1. Wait at least 60ms after VCC rises to 4.5V */
	vTaskDelay(60 / portTICK_RATE_MS);

	/* 2. Hitachi HD44780 4-bit mode initialization sequence from unknown state:
	 * Send 0x03 nibble three times to sync into 8-bit mode, then 0x02 to switch to 4-bit mode.
	 * This recovers cleanly from power loss, partial transfers, or 8-bit default mode. */
	lcd_write_nibble(0x03, eLcd_Cmd);
	vTaskDelay(5 / portTICK_RATE_MS); /* > 4.1 ms */
	lcd_write_nibble(0x03, eLcd_Cmd);
	vTaskDelay(2 / portTICK_RATE_MS); /* > 100 us */
	lcd_write_nibble(0x03, eLcd_Cmd);
	vTaskDelay(2 / portTICK_RATE_MS); /* > 100 us */
	lcd_write_nibble(0x02, eLcd_Cmd); /* Set 4-bit interface */
	vTaskDelay(2 / portTICK_RATE_MS);

	/* 3. Now in 4-bit mode: send standard 2-nibble commands */
	Output_Lcd(0x28, eLcd_Cmd); /* 4-bit mode, 2 lines, 5x8 dots */
	vTaskDelay(2 / portTICK_RATE_MS);
	Output_Lcd(0x08, eLcd_Cmd); /* Display off */
	vTaskDelay(2 / portTICK_RATE_MS);
	Output_Lcd(0x01, eLcd_Cmd); /* Clear display */
	vTaskDelay(5 / portTICK_RATE_MS);
	Output_Lcd(0x06, eLcd_Cmd); /* Entry mode: cursor increment, no shift */
	vTaskDelay(2 / portTICK_RATE_MS);
	Output_Lcd(0x0C, eLcd_Cmd); /* Display on, cursor off, blink off */
	vTaskDelay(2 / portTICK_RATE_MS);

	/* Backlight stays OFF until explicitly enabled by UI task */
	GPIO_ClearValue(LCD_BL_PORT, LCD_BL_MASK);

	/* Mark LCD task fully initialized and ready to receive commands */
	lcd_task_started = 1;
}

/*-------------------------------------------------------------------------------------------------------------------*
* 	FUNCTION NAME : Output_Lcd
 * 	DESCRIPTION : This function is used to display the data in LCD based on Lcd commands.
* 	VERSION : 1.5
* 	ARGUMENTS : None
* 	RETURN VALUES : None
* 	GLOBALS INPUT : None
*	GLOBALS AFFECTED : None
*	CALLED FUNCTION : Initialize_Lcd
*	CALLING FUNCTION : GPIO_ClearValue, GPIO_SetValue, wait_lcd
*

/*---------------------------------------------------------------------------------------------------------------------*/

void Output_Lcd(char cData, char cCd){
	/* High nibble first */
	lcd_write_nibble((unsigned char)(cData >> 4) & 0x0F, cCd);
	/* Low nibble second */
	lcd_write_nibble((unsigned char)(cData & 0x0F), cCd);

	/* Command execution delay:
	 * Clear Display (0x01) and Return Home (0x02) take up to 1.64 ms.
	 * All other commands and data writes take 37 us to 43 us.
	 */
	if (cCd == eLcd_Cmd && ((unsigned char)cData == 0x01 || (unsigned char)cData == 0x02)) {
		vTaskDelay(3 / portTICK_RATE_MS);
	} else {
		for (volatile int d = 0; d < 1500; d++); /* ~60 us */
	}
}

/*-------------------------------------------------------------------------------------------------------------------*
* 	FUNCTION NAME : wait_lcd
 * 	DESCRIPTION : This function provides execution delay for HD44780 commands & writes (~50us).
* 	VERSION : 1.5
* 	ARGUMENTS : None
* 	RETURN VALUES : None
* 	GLOBALS INPUT : None
*	GLOBALS AFFECTED : None
*	CALLED FUNCTION : Output_Lcd
*	CALLING FUNCTION : None
*

/*---------------------------------------------------------------------------------------------------------------------*/

void wait_lcd( void )
{
	/* HD44780 standard command execution is 37us to 43us.
	 * Volatile loop delay of ~50us ensures command execution completes
	 * without busy flag deadlock, floating pin false-busy, or task blocking. */
	for (volatile int d = 0; d < 1000; d++);
}

/*-------------------------------------------------------------------------------------------------------------------*
* 	FUNCTION NAME : vLcd_Task
 * 	DESCRIPTION : 1.	This Task is mainly used to switch the LCD to Various states. States are
						•	eLcd_Nop
						•	eLcd_Init
						•	eLcd_Off
						•	eLcd_On
						•	eLcd_Cur_On
						•	eLcd_Cur_Off
						•	eLcd_Display
						•	eLcd_Set_Addr
				2.	Whenever we press the key, the key value will be send to UI Task.
					UI task will take the key values and it will display message in LCD based on Key values.

					Main Flow:
					1.	Initialize all the pins
					2.	Check any signal is coming from UI Task. If any signal has come, then display the data which is coming from UI Task.
					3.	Incoming requests:
					UI to LCD: this request is used to give signal and commands to LCD for Display Data properly.

* 	VERSION : 1.5
* 	ARGUMENTS : pvParameters
* 	RETURN VALUES : None
* 	GLOBALS INPUT : None
*	GLOBALS AFFECTED : XITC_Ui_Lcd.lcd_cmd
*	CALLED FUNCTION : main
*	CALLING FUNCTION : init_lcd_pins, Initialize_Lcd, Output_Lcd
*

/*---------------------------------------------------------------------------------------------------------------------*/

void vLcd_Task(void *pvParameters) {

	int i;

	init_lcd_pins();
	Initialize_Lcd();

	for (;;){
		if (!XITC_Ui_Lcd.sig) {
			vTaskDelay(1 / portTICK_RATE_MS);
			continue;
		}

		if (XITC_Ui_Lcd.lcd_cmd == eLcd_Init){
			Initialize_Lcd();
		}
		else if (XITC_Ui_Lcd.lcd_cmd == eLcd_Display){
			// set address
			Output_Lcd(XITC_Ui_Lcd.lcd_posn, eLcd_Cmd);
			// display the string up to 16 characters
			i = 0;
			while(XITC_Ui_Lcd.disp_str[i] && i < 16){
				Output_Lcd(XITC_Ui_Lcd.disp_str[i], eLcd_Data);
				i++;
			}
		}
		else if (XITC_Ui_Lcd.lcd_cmd == eLcd_Nop){
			// do nothing
		}
		else if (XITC_Ui_Lcd.lcd_cmd == eLcd_Off){
			Output_Lcd(LCD_OFF_CMD, eLcd_Cmd);
			lcd_backlight_off();
		}
		else if (XITC_Ui_Lcd.lcd_cmd == eLcd_On){
			Output_Lcd(LCD_ON_CMD, eLcd_Cmd);
			lcd_backlight_on();
		}
		else if (XITC_Ui_Lcd.lcd_cmd == eLcd_Cur_Off){
			Output_Lcd(CURSOR_OFF_CMD, eLcd_Cmd);
		}
		else if (XITC_Ui_Lcd.lcd_cmd == eLcd_Cur_On){
			Output_Lcd(CURSOR_ON_CMD, eLcd_Cmd);
		}
		else if (XITC_Ui_Lcd.lcd_cmd == eLcd_Set_Addr){
			Output_Lcd(XITC_Ui_Lcd.lcd_posn, eLcd_Cmd);
		}
		XITC_Ui_Lcd.sig = 0;
	}
}

