/*
    FreeRTOS V6.0.0 - Copyright (C) 2009 Real Time Engineers Ltd.

    This file is part of the FreeRTOS distribution.

    FreeRTOS is free software; you can redistribute it and/or modify it under
    the terms of the GNU General Public License (version 2) as published by the
    Free Software Foundation AND MODIFIED BY the FreeRTOS exception.
 ***NOTE*** The exception to the GPL is included to allow you to distribute
    a combined work that includes FreeRTOS without being obliged to provide the
    source code for proprietary components outside of the FreeRTOS kernel.
    FreeRTOS is distributed in the hope that it will be useful, but WITHOUT
    ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
    FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
    more details. You should have received a copy of the GNU General Public
    License and the FreeRTOS license exception along with FreeRTOS; if not it
    can be viewed here: http://www.freertos.org/a00114.html and also obtained
    by writing to Richard Barry, contact details for whom are available on the
    FreeRTOS WEB site.

    1 tab == 4 spaces!

    http://www.FreeRTOS.org - Documentation, latest information, license and
    contact details.

    http://www.SafeRTOS.com - A version that is certified for use in safety
    critical systems.

    http://www.OpenRTOS.com - Commercial support, development, porting,
    licensing and training services.
 */

/* FreeRTOS.org includes. */



/*******************************************************************************************************************************************
 * 								version 1.5 updation details
 *
 * 1. Params file modification while updating the new version
 * 2. FTP buffer has been prepared after HTTp failed
 * 3. Reset command is not given to Q26
 * 4. Checking firmware file validation
 * 5. Message processing has been done in starting and ending of GPRS Communication(i.e before goes to hibernate we are processing the message once
 * 6. Reset Dota Command by SMS
 * 7. Time synch has been done for both HTTP succes and Reject(i.e every 15mins)
 * 8. Timing Problem has been solved i.e reduced delay from 30sec to 5sec for saving power.
 *
 *
 ***********************************************************************************************************************************************/



#include "FreeRTOS.h"
#include "task.h"

#include "globals.h"

#include "basic_io.h"
#include "lpc17xx_pinsel.h"

#include "memory.h"
#include "sensor.h"
#include "disk.h"
#include "gprs.h"
#include "kbd.h"
#include "lcd.h"
#include "ui.h"
#include "wdt.h"
#include "ispuart.h"


/* Used as a loop counter to create a very crude delay. */


/*-----------------------------------------------------------*/

int main( void )
{

	int i, j;
	for(i=0; i < 4000; i++)
		for (j=0; j < 10000; j++);


	log_task_started = 0;
	memory_task_started = 0;
	sensor_task_started = 0;
	disk_task_started = 0;
	shutdown_pwr = 0;


	/*
	LPC_SC->PCONP &= 0x10118821;

	PINSEL_CFG_Type notused;
	notused.Portnum   = 0;
	notused.Pinnum    = 23;
	notused.Funcnum   = 0;
	notused.Pinmode   = PINSEL_PINMODE_TRISTATE;
	notused.OpenDrain = PINSEL_PINMODE_NORMAL;
	PINSEL_ConfigPin(&notused);
	GPIO_SetDir(0, 23, DIR_IN);
	notused.Pinnum    = 24;
	PINSEL_ConfigPin(&notused);
	GPIO_SetDir(0, 24, DIR_IN);
	 */

	xTaskCreate( vMemory_Task,"Memory Task", 260, NULL, 1, NULL );
	xTaskCreate( vSensor_Task, "Sensor Task", 260, NULL, 1, NULL );
	xTaskCreate( vDisk_Task,"Disk Task", 260, NULL, 1, NULL );
	xTaskCreate( vGprs_Task, "GPRS Task", 1300, NULL, 1, NULL );
	xTaskCreate( vKbd_Task, "KBD Task", 200, NULL, 1, NULL );
	xTaskCreate( vUi_Task, "UI Task", 320, NULL, 1, NULL );
	xTaskCreate( vLcd_Task, "LCD Task", 200, NULL, 1, NULL );
	xTaskCreate( vISPUart_Task, "ISP UART Task", 260, NULL, 1, NULL );
	xTaskCreate( vWDT_Task, "WDT Task", 120, NULL, 1, NULL );



	vTaskStartScheduler();

	/* If all is well we will never reach here as the scheduler will now be
	running.  If we do reach here then it is likely that there was insufficient
	heap available for the idle task to be created. */
	for( ;; );
	return 0;
}

/*
 * TO DOs:
 * to allocate and copy the buffer from log task to disk task rather than just passing pointer
 *
 *
 */



