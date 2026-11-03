#include "lpc17xx_pinsel.h"
#include "lpc17xx_gpio.h"
#include "kbd.h"
#include "FreeRTOS.h"
#include "task.h"
#include "globals.h"

/*-------------------------------------------------------------------------------------------------------------------*
* 	FUNCTION NAME : init_kbd_pins
 * 	DESCRIPTION : This function is used to initialize all the keyboard related pins
* 	VERSION : 1.5
* 	ARGUMENTS : None
* 	RETURN VALUES : None
* 	GLOBALS INPUT : None
*	GLOBALS AFFECTED : None
*	CALLED FUNCTION : vKbd_Task
*	CALLING FUNCTION : None
*

/*---------------------------------------------------------------------------------------------------------------------*/

void init_kbd_pins(void)
{
	PINSEL_CFG_Type KBD_config;

	KBD_config.Funcnum   = PINSEL_FUNC_0;
	KBD_config.Pinmode   = PINSEL_PINMODE_PULLUP;
	KBD_config.OpenDrain = PINSEL_PINMODE_NORMAL;

	// for row 0 pin
	KBD_config.Portnum   = KBD_ROW0_PORT;
	KBD_config.Pinnum    = KBD_ROW0_PINNO;
	PINSEL_ConfigPin(&KBD_config);
	GPIO_SetDir(KBD_ROW0_PORT, KBD_ROW0_MASK, DIR_OUT);

	// for row 1 pin
	KBD_config.Portnum   = KBD_ROW1_PORT;
	KBD_config.Pinnum    = KBD_ROW1_PINNO;
	PINSEL_ConfigPin(&KBD_config);
	GPIO_SetDir(KBD_ROW1_PORT, KBD_ROW1_MASK, DIR_OUT);

	// for row 2 pin
	KBD_config.Portnum   = KBD_ROW2_PORT;
	KBD_config.Pinnum    = KBD_ROW2_PINNO;
	PINSEL_ConfigPin(&KBD_config);
	GPIO_SetDir(KBD_ROW2_PORT, KBD_ROW2_MASK, DIR_OUT);

	// Ensure all rows are idle HIGH
	GPIO_SetValue(KBD_ROW0_PORT, KBD_ROW0_MASK);
	GPIO_SetValue(KBD_ROW1_PORT, KBD_ROW1_MASK);
	GPIO_SetValue(KBD_ROW2_PORT, KBD_ROW2_MASK);

	// for column 0 pin
	KBD_config.Portnum   = KBD_COL0_PORT;
	KBD_config.Pinnum    = KBD_COL0_PINNO;
	PINSEL_ConfigPin(&KBD_config);
	GPIO_SetDir(KBD_COL0_PORT, KBD_COL0_MASK, DIR_IN);


	// for column 1 pin
	KBD_config.Portnum   = KBD_COL1_PORT;
	KBD_config.Pinnum    = KBD_COL1_PINNO;
	PINSEL_ConfigPin(&KBD_config);
	GPIO_SetDir(KBD_COL1_PORT, KBD_COL1_MASK, DIR_IN);

	// for column 2 pin
	KBD_config.Portnum   = KBD_COL2_PORT;
	KBD_config.Pinnum    = KBD_COL2_PINNO;
	PINSEL_ConfigPin(&KBD_config);
	GPIO_SetDir(KBD_COL2_PORT, KBD_COL2_MASK, DIR_IN);
}

void kbd_release_pins_for_sleep(void)
{
	PINSEL_CFG_Type pin_cfg;

	/* Configure Row and Column pins as Tristate (no pullup/pulldown) High-Z inputs */
	pin_cfg.Funcnum = PINSEL_FUNC_0;
	pin_cfg.Pinmode = PINSEL_PINMODE_TRISTATE;
	pin_cfg.OpenDrain = PINSEL_PINMODE_NORMAL;

	pin_cfg.Portnum = KBD_ROW0_PORT; pin_cfg.Pinnum = KBD_ROW0_PINNO; PINSEL_ConfigPin(&pin_cfg);
	pin_cfg.Portnum = KBD_ROW1_PORT; pin_cfg.Pinnum = KBD_ROW1_PINNO; PINSEL_ConfigPin(&pin_cfg);
	pin_cfg.Portnum = KBD_ROW2_PORT; pin_cfg.Pinnum = KBD_ROW2_PINNO; PINSEL_ConfigPin(&pin_cfg);
	pin_cfg.Portnum = KBD_COL0_PORT; pin_cfg.Pinnum = KBD_COL0_PINNO; PINSEL_ConfigPin(&pin_cfg);
	pin_cfg.Portnum = KBD_COL1_PORT; pin_cfg.Pinnum = KBD_COL1_PINNO; PINSEL_ConfigPin(&pin_cfg);
	pin_cfg.Portnum = KBD_COL2_PORT; pin_cfg.Pinnum = KBD_COL2_PINNO; PINSEL_ConfigPin(&pin_cfg);

	GPIO_SetDir(KBD_ROW0_PORT, KBD_ROW0_MASK, DIR_IN);
	GPIO_SetDir(KBD_ROW1_PORT, KBD_ROW1_MASK, DIR_IN);
	GPIO_SetDir(KBD_ROW2_PORT, KBD_ROW2_MASK, DIR_IN);
	GPIO_SetDir(KBD_COL0_PORT, KBD_COL0_MASK, DIR_IN);
	GPIO_SetDir(KBD_COL1_PORT, KBD_COL1_MASK, DIR_IN);
	GPIO_SetDir(KBD_COL2_PORT, KBD_COL2_MASK, DIR_IN);
}


/*-------------------------------------------------------------------------------------------------------------------*
* 	FUNCTION NAME : Kbd_Scan
 * 	DESCRIPTION : This function is used to scan the keyboard to get key value.
* 	VERSION : 1.5
* 	ARGUMENTS : None
* 	RETURN VALUES : iKey_Value
* 	GLOBALS INPUT : None
*	GLOBALS AFFECTED : None
*	CALLED FUNCTION : vKbd_Task
*	CALLING FUNCTION : Output_Row, Read_Col_Pressed

/*---------------------------------------------------------------------------------------------------------------------*/

int Kbd_Scan(void) {
	int iRow_No; // Variable to store the row number
	int iCol_No; // Variable to store the column number
	int iKey_Value = 0; // Variable to store the key value calculated

	for (iRow_No = 0; iRow_No < MAX_ROWS; iRow_No++) {
		Output_Row(iRow_No); // Output bit patterns to locate the key
		for (volatile int d = 0; d < 300; d++); // Fast GPIO pin settling delay (~10us)
		iCol_No = Read_Col_Pressed(); // Read the active coloumn value

		if (iCol_No) {
			iKey_Value = iRow_No * MAX_ROWS + iCol_No; // Calculate key value
			break;
		}
	}

	return iKey_Value; // Return the key value
}

/*-------------------------------------------------------------------------------------------------------------------*
* 	FUNCTION NAME : Output_Row
 * 	DESCRIPTION : This function is used to Outputs the signals for scanning corresponding Row numbers.
* 	VERSION : 1.5
* 	ARGUMENTS : iRow
* 	RETURN VALUES : None
* 	GLOBALS INPUT : None
*	GLOBALS AFFECTED : None
*	CALLED FUNCTION : Kbd_Scan
*	CALLING FUNCTION : GPIO_SetValue, GPIO_ClearValue

/*---------------------------------------------------------------------------------------------------------------------*/

void Output_Row(int iRow)
{
	// set all row pins to logic high
	GPIO_SetValue(KBD_ROW2_PORT, KBD_ROW2_MASK);
	GPIO_SetValue(KBD_ROW1_PORT, KBD_ROW1_MASK);
	GPIO_SetValue(KBD_ROW0_PORT, KBD_ROW0_MASK);

	// now drive a logic zero on the selected row only
	if(iRow == 0)
		GPIO_ClearValue(KBD_ROW0_PORT, KBD_ROW0_MASK);
	else if (iRow == 1)
		GPIO_ClearValue(KBD_ROW1_PORT, KBD_ROW1_MASK);
	else if (iRow == 2)
		GPIO_ClearValue(KBD_ROW2_PORT, KBD_ROW2_MASK);
}

/*-------------------------------------------------------------------------------------------------------------------*
* 	FUNCTION NAME : Read_Col_Pressed
 * 	DESCRIPTION : This function is used to Outputs the signals for scanning corresponding Column numbers
* 	VERSION : 1.5
* 	ARGUMENTS : None
* 	RETURN VALUES : iCol_Number
* 	GLOBALS INPUT : None
*	GLOBALS AFFECTED : None
*	CALLED FUNCTION : Kbd_Scan
*	CALLING FUNCTION : GPIO_ReadValue, GPIO_SetValue

/*---------------------------------------------------------------------------------------------------------------------*/

int Read_Col_Pressed(void) {
	int iCol_Number = 0;
	iCol_Number = 0;

	if((GPIO_ReadValue(KBD_COL2_PORT) & KBD_COL2_MASK) == 0)
		iCol_Number = 3;

	else if ((GPIO_ReadValue(KBD_COL1_PORT) & KBD_COL1_MASK) == 0)
		iCol_Number = 2;

	else if ((GPIO_ReadValue(KBD_COL0_PORT) & KBD_COL0_MASK) == 0)
		iCol_Number = 1;

	// set all row pins to logic high
	GPIO_SetValue(KBD_ROW2_PORT, KBD_ROW2_MASK);
	GPIO_SetValue(KBD_ROW1_PORT, KBD_ROW1_MASK);
	GPIO_SetValue(KBD_ROW0_PORT, KBD_ROW0_MASK);


	return iCol_Number;
}

/*-------------------------------------------------------------------------------------------------------------------*
* 	FUNCTION NAME : vKbd_Task
 * 	DESCRIPTION : 	1.	This Task mainly used to perform keyboard related Tasks. In our keyboard we are using following keys.
 * 						Up,Down,Left,Right,Back,Enter,Menu,Calib,Disp
					2.	Whenever we press the key, the request and signal will be send to UI Task. Based on Key values UI will
						display the corresponding message.
						1.	Initialize all the keyboard pins
						2.	Initialize with correct tick count by using vTaskDelayUntil function
						3.	Scan the key board to get key value
						4.	Output_Row and Read_col_pressed function is used to get the corresponding row and column values
						5.	Initialize with proper debounce count.
						6.	Outgoing Request :
							KBD to UI:  Key board task will send signal and key values to UI task.
							Then UI Task will give corresponding messages based on our key value


* 	VERSION : 1.5
* 	ARGUMENTS : pvParameters
* 	RETURN VALUES : None
* 	GLOBALS INPUT : None
*	GLOBALS AFFECTED : None
*	CALLED FUNCTION : main
*	CALLING FUNCTION : init_kbd_pins, Kbd_Scan

/*---------------------------------------------------------------------------------------------------------------------*/

void vKbd_Task(void *pvParameters)
{
	int iOld_Key = 0;
	int iNew_Key = 0;

	XITC_Kbd_Log.sig_key = 0;
	XITC_Kbd_Log.key_value = 0;
	init_kbd_pins();
	kbd_task_started = 1;

	for (;;) {
		vTaskDelay(20 / portTICK_RATE_MS);

		iNew_Key = Kbd_Scan(); // Scan matrix for pressed key
		if (iNew_Key) {
			if (iNew_Key != iOld_Key) {
				iOld_Key = iNew_Key;
				XITC_Kbd_Log.key_value = iNew_Key;
				XITC_Kbd_Log.sig_key = 1;
			}
		} else {
			iOld_Key = 0;
		}
	}
}






