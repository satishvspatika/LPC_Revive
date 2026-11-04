#include "lpc17xx_pinsel.h"
#include "lpc17xx_gpio.h"
#include "kbd.h"
#include "FreeRTOS.h"
#include "task.h"
#include "globals.h"

volatile int dbg_last_key = 0;
volatile int dbg_raw_cols = 7;
volatile uint32_t dbg_key_count = 0;

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

	// for row 0 pin (P3.25)
	KBD_config.Portnum   = KBD_ROW0_PORT;
	KBD_config.Pinnum    = KBD_ROW0_PINNO;
	PINSEL_ConfigPin(&KBD_config);
	GPIO_SetDir(KBD_ROW0_PORT, KBD_ROW0_MASK, DIR_OUT);

	// for row 1 pin (P3.26)
	KBD_config.Portnum   = KBD_ROW1_PORT;
	KBD_config.Pinnum    = KBD_ROW1_PINNO;
	PINSEL_ConfigPin(&KBD_config);
	GPIO_SetDir(KBD_ROW1_PORT, KBD_ROW1_MASK, DIR_OUT);

	// for row 2 pin (P1.21)
	KBD_config.Portnum   = KBD_ROW2_PORT;
	KBD_config.Pinnum    = KBD_ROW2_PINNO;
	PINSEL_ConfigPin(&KBD_config);
	GPIO_SetDir(KBD_ROW2_PORT, KBD_ROW2_MASK, DIR_OUT);

	// for column 0 pin (P0.22)
	KBD_config.Portnum   = KBD_COL0_PORT;
	KBD_config.Pinnum    = KBD_COL0_PINNO;
	PINSEL_ConfigPin(&KBD_config);
	GPIO_SetDir(KBD_COL0_PORT, KBD_COL0_MASK, DIR_IN);

	// for column 1 pin (P0.21)
	KBD_config.Portnum   = KBD_COL1_PORT;
	KBD_config.Pinnum    = KBD_COL1_PINNO;
	PINSEL_ConfigPin(&KBD_config);
	GPIO_SetDir(KBD_COL1_PORT, KBD_COL1_MASK, DIR_IN);

	// for column 2 pin (P0.20)
	KBD_config.Portnum   = KBD_COL2_PORT;
	KBD_config.Pinnum    = KBD_COL2_PINNO;
	PINSEL_ConfigPin(&KBD_config);
	GPIO_SetDir(KBD_COL2_PORT, KBD_COL2_MASK, DIR_IN);

	/* Direct hardware register writes to guarantee 100% pin function and pull-up configuration */
	LPC_PINCON->PINSEL1 &= ~((0x03UL << 8) | (0x03UL << 10) | (0x03UL << 12)); /* P0.20, P0.21, P0.22 as GPIO */
	LPC_PINCON->PINMODE1 &= ~((0x03UL << 8) | (0x03UL << 10) | (0x03UL << 12)); /* Internal Pull-Up on P0.20, P0.21, P0.22 */
	LPC_PINCON->PINMODE_OD0 &= ~((1UL << 20) | (1UL << 21) | (1UL << 22)); /* Normal push-pull */
	LPC_GPIO0->FIODIR &= ~((1UL << 20) | (1UL << 21) | (1UL << 22)); /* Inputs */
	LPC_GPIO0->FIOMASK &= ~((1UL << 20) | (1UL << 21) | (1UL << 22)); /* Unmasked */

	LPC_PINCON->PINSEL7 &= ~((0x03UL << 18) | (0x03UL << 20)); /* P3.25, P3.26 as GPIO */
	LPC_PINCON->PINMODE7 &= ~((0x03UL << 18) | (0x03UL << 20)); /* Pull-Up on P3.25, P3.26 */
	LPC_PINCON->PINMODE_OD3 &= ~((1UL << 25) | (1UL << 26));
	LPC_GPIO3->FIODIR |= ((1UL << 25) | (1UL << 26)); /* Outputs */
	LPC_GPIO3->FIOSET = ((1UL << 25) | (1UL << 26)); /* Idle HIGH */

	LPC_PINCON->PINSEL3 &= ~(0x03UL << 10); /* P1.21 as GPIO */
	LPC_PINCON->PINMODE3 &= ~(0x03UL << 10); /* Pull-Up on P1.21 */
	LPC_PINCON->PINMODE_OD1 &= ~(1UL << 21);
	LPC_GPIO1->FIODIR |= (1UL << 21); /* Output */
	LPC_GPIO1->FIOSET = (1UL << 21); /* Idle HIGH */
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
	// set all row pins to logic high first
	LPC_GPIO3->FIOSET = (KBD_ROW0_MASK | KBD_ROW1_MASK);
	LPC_GPIO1->FIOSET = KBD_ROW2_MASK;

	// now drive a logic zero on the selected row only
	if (iRow == 0)
		LPC_GPIO3->FIOCLR = KBD_ROW0_MASK;
	else if (iRow == 1)
		LPC_GPIO3->FIOCLR = KBD_ROW1_MASK;
	else if (iRow == 2)
		LPC_GPIO1->FIOCLR = KBD_ROW2_MASK;
}

/*-------------------------------------------------------------------------------------------------------------------*
* 	FUNCTION NAME : Read_Col_Pressed
 * 	DESCRIPTION : This function is kept for backward compatibility and column sampling
* 	VERSION : 1.5
* 	ARGUMENTS : None
* 	RETURN VALUES : iCol_Number
*-------------------------------------------------------------------------------------------------------------------*/

int Read_Col_Pressed(void) {
	int iCol_Number = 0;
	uint32_t port0 = LPC_GPIO0->FIOPIN;

	dbg_raw_cols = (int)((port0 >> 20) & 0x07);

	if ((port0 & KBD_COL0_MASK) == 0)
		iCol_Number = 1;
	else if ((port0 & KBD_COL1_MASK) == 0)
		iCol_Number = 2;
	else if ((port0 & KBD_COL2_MASK) == 0)
		iCol_Number = 3;

	// set all row pins to logic high
	LPC_GPIO3->FIOSET = (KBD_ROW0_MASK | KBD_ROW1_MASK);
	LPC_GPIO1->FIOSET = KBD_ROW2_MASK;

	return iCol_Number;
}

/*-------------------------------------------------------------------------------------------------------------------*
* 	FUNCTION NAME : Kbd_Scan
 * 	DESCRIPTION : This function scans the keyboard matrix to get exact key value.
* 	VERSION : 1.5
* 	ARGUMENTS : None
* 	RETURN VALUES : iKey_Value
*-------------------------------------------------------------------------------------------------------------------*/

int Kbd_Scan(void) {
	int iRow_No;
	int iCol_No;
	int iKey_Value = 0;

	// Ensure all rows are idle HIGH before scanning
	LPC_GPIO3->FIOSET = (KBD_ROW0_MASK | KBD_ROW1_MASK);
	LPC_GPIO1->FIOSET = KBD_ROW2_MASK;

	for (iRow_No = 0; iRow_No < MAX_ROWS; iRow_No++) {
		Output_Row(iRow_No);
		/* Hardware settling delay (~15us at 100MHz) for line capacitance */
		for (volatile int d = 0; d < 500; d++);
		iCol_No = Read_Col_Pressed();

		if (iCol_No != 0) {
			iKey_Value = iRow_No * MAX_ROWS + iCol_No;
			break;
		}
	}

	// Ensure all rows remain idle HIGH after scan completes
	LPC_GPIO3->FIOSET = (KBD_ROW0_MASK | KBD_ROW1_MASK);
	LPC_GPIO1->FIOSET = KBD_ROW2_MASK;

	return iKey_Value;
}

/*-------------------------------------------------------------------------------------------------------------------*
* 	FUNCTION NAME : vKbd_Task
 * 	DESCRIPTION : 	1.	This Task mainly used to perform keyboard related Tasks. In our keyboard we are using following keys.
 * 						Up,Down,Left,Right,Back,Enter,Menu,Calib,Disp
					2.	Whenever we press the key, the request and signal will be send to UI Task. Based on Key values UI will
						display the corresponding message.
						1.	Initialize all the keyboard pins
						2.	Initialize with correct tick count
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
	int iDebounce_Count = 0;
	static int pins_released = 0;

	XITC_Kbd_Log.sig_key = 0;
	XITC_Kbd_Log.key_value = 0;
	init_kbd_pins();
	kbd_task_started = 1;

	/* 200ms power-on rail settling delay before first scan */
	vTaskDelay(200 / portTICK_RATE_MS);

	for (;;) {
		vTaskDelay(10 / portTICK_RATE_MS);

		if (shutdown_pwr == 1) {
			if (!pins_released) {
				kbd_release_pins_for_sleep();
				pins_released = 1;
			}
			vTaskDelay(200 / portTICK_RATE_MS);
			continue;
		} else if (pins_released) {
			init_kbd_pins();
			pins_released = 0;
			vTaskDelay(50 / portTICK_RATE_MS);
		}

		iNew_Key = Kbd_Scan(); // Scan matrix for pressed key
		if (iNew_Key != 0) {
			if (iNew_Key != iOld_Key) {
				iOld_Key = iNew_Key;
				iDebounce_Count = 3; /* 30ms stable hold confirmation */
			} else if (iDebounce_Count > 0) {
				iDebounce_Count--;
				if (iDebounce_Count == 0) {
					XITC_Kbd_Log.key_value = iNew_Key;
					XITC_Kbd_Log.sig_key = 1;
					dbg_last_key = iNew_Key;
					dbg_key_count++;
				}
			}
		} else {
			iOld_Key = 0;
			iDebounce_Count = 0;
		}
	}
}
