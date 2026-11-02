#include "lcd.h"
#include "ui.h"
#include "FreeRTOS.h"
#include "lpc17xx_pinsel.h"
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

static void Output_Lcd_Nibble(unsigned char nibble) {
	unsigned int temp1 = 0;
	// Command mode: RS = 0
	GPIO_ClearValue(LCD_CTRL_PORT, LCD_RS_MASK);

	if(nibble & 0x01) temp1 |= LCD_D4_MASK;
	if(nibble & 0x02) temp1 |= LCD_D5_MASK;
	if(nibble & 0x04) temp1 |= LCD_D6_MASK;
	if(nibble & 0x08) temp1 |= LCD_D7_MASK;

	GPIO_SetValue(LCD_CTRL_PORT, LCD_EN_MASK);
	GPIO_ClearValue(LCD_DATA_PORT, LCD_DATA_MASK);
	GPIO_SetValue(LCD_DATA_PORT, temp1);
	GPIO_ClearValue(LCD_CTRL_PORT, LCD_EN_MASK);
}

void Initialize_Lcd(void) {
	/* Wait at least 40ms after VCC rises to 4.5V */
	vTaskDelay(50 / portTICK_RATE_MS);

	/* Hitachi HD44780 4-bit mode initialization sequence from unknown state:
	 * Send 0x03 nibble three times to sync into 8-bit mode, then 0x02 to switch to 4-bit mode.
	 * This recovers cleanly from power loss, partial transfers, or 8-bit default mode. */
	Output_Lcd_Nibble(0x03);
	vTaskDelay(5 / portTICK_RATE_MS); /* > 4.1 ms */
	Output_Lcd_Nibble(0x03);
	vTaskDelay(2 / portTICK_RATE_MS); /* > 100 us */
	Output_Lcd_Nibble(0x03);
	vTaskDelay(2 / portTICK_RATE_MS); /* > 100 us */
	Output_Lcd_Nibble(0x02);          /* Set 4-bit interface */
	vTaskDelay(2 / portTICK_RATE_MS);

	/* Now in 4-bit mode: send standard 2-nibble commands */
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
  unsigned char temp=0;
  unsigned int temp1=0;


  // first let us take care of RS signal
  if(cCd == 1)
	GPIO_SetValue(LCD_CTRL_PORT, LCD_RS_MASK);

  else
	 GPIO_ClearValue(LCD_CTRL_PORT, LCD_RS_MASK);


  // first MS Nibble of the command / data
  temp=cData;
  temp=(temp>>4)&0x0F;
  temp1 = 0;
  if(temp & 0x01)
	  temp1 |= LCD_D4_MASK;
  if(temp & 0x02)
	  temp1 |= LCD_D5_MASK;
  if(temp & 0x04)
	  temp1 |= LCD_D6_MASK;
  if(temp & 0x08)
	  temp1 |= LCD_D7_MASK;



 // now raise EN, put the data and clear EN
  GPIO_SetValue(LCD_CTRL_PORT, LCD_EN_MASK);
  GPIO_ClearValue(LCD_DATA_PORT, LCD_DATA_MASK);
  GPIO_SetValue(LCD_DATA_PORT, temp1);
  GPIO_ClearValue(LCD_CTRL_PORT, LCD_EN_MASK);


 // next the LS Nibble of command / data
  temp=cData;
  temp&=0x0F;
  temp1 = 0;
  if(temp & 0x01)
	  temp1 |= LCD_D4_MASK;
  if(temp & 0x02)
	  temp1 |= LCD_D5_MASK;
  if(temp & 0x04)
	  temp1 |= LCD_D6_MASK;
  if(temp & 0x08)
	  temp1 |= LCD_D7_MASK;



 // now raise EN, put the data and clear EN
 GPIO_SetValue(LCD_CTRL_PORT, LCD_EN_MASK);
  GPIO_ClearValue(LCD_DATA_PORT, LCD_DATA_MASK);
  GPIO_SetValue(LCD_DATA_PORT, temp1);
  GPIO_ClearValue(LCD_CTRL_PORT, LCD_EN_MASK);

  wait_lcd();
  //vTaskDelay(2 / portTICK_RATE_MS);

}

/*-------------------------------------------------------------------------------------------------------------------*
* 	FUNCTION NAME : wait_lcd
 * 	DESCRIPTION : This function is used to give some delay whenever read the LCD data.
* 	VERSION : 1.5
* 	ARGUMENTS : None
* 	RETURN VALUES : None
* 	GLOBALS INPUT : None
*	GLOBALS AFFECTED : None
*	CALLED FUNCTION : Output_Lcd
*	CALLING FUNCTION : GPIO_ClearValue, GPIO_SetValue, GPIO_ReadValue, GPIO_SetDir
*

/*---------------------------------------------------------------------------------------------------------------------*/

void wait_lcd( void )
{
   GPIO_SetDir(LCD_DATA_PORT, LCD_DATA_MASK, DIR_IN);
  GPIO_ClearValue(LCD_CTRL_PORT, LCD_RS_MASK);
  GPIO_SetValue(LCD_CTRL_PORT, (LCD_RW_MASK|LCD_EN_MASK));

  while((GPIO_ReadValue(LCD_DATA_PORT)) & LCD_BUSY_FLAG)
	  vTaskDelay(2 / portTICK_RATE_MS);

  GPIO_ClearValue(LCD_CTRL_PORT, LCD_RW_MASK|LCD_EN_MASK);
  GPIO_SetDir(LCD_DATA_PORT, LCD_DATA_MASK, DIR_OUT);


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

	for(i=0; i<1000; i++);
	Initialize_Lcd();
	for (;;){
		vTaskDelay(5 / portTICK_RATE_MS);
		if(XITC_Ui_Lcd.sig){

			if (XITC_Ui_Lcd.lcd_cmd == eLcd_Init){
				Initialize_Lcd();
			}
			else if (XITC_Ui_Lcd.lcd_cmd == eLcd_Display){
				// set address
				Output_Lcd(XITC_Ui_Lcd.lcd_posn, eLcd_Cmd);
				// display the string
				i = 0;
				while(XITC_Ui_Lcd.disp_str[i]){
					Output_Lcd(XITC_Ui_Lcd.disp_str[i], eLcd_Data);
					i++;
				}
			}
			else if (XITC_Ui_Lcd.lcd_cmd == eLcd_Nop){
				// do nothing
			}
			else if (XITC_Ui_Lcd.lcd_cmd == eLcd_Off){
				Output_Lcd(LCD_OFF_CMD, eLcd_Cmd);
				GPIO_ClearValue(LCD_BL_PORT, LCD_BL_MASK);
			}
			else if (XITC_Ui_Lcd.lcd_cmd == eLcd_On){
				Output_Lcd(LCD_ON_CMD, eLcd_Cmd);
				GPIO_SetValue(LCD_BL_PORT, LCD_BL_MASK);
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



}

