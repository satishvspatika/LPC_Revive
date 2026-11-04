#ifndef __DISK_H__
#define __DISK_H__

#include "FreeRTOS.h"
#include "semphr.h"

extern xSemaphoreHandle xDiskMutex;

//function prototypes
void vDisk_Task(void *pvParameters);

void check_sdc_status(void);
void prepare_write_params(void);
void get_params(void);
int getLineNoforFTP(void);
void setLineNoforFTP(int pr_line_no);
void sd_log_http(const char *msg);

#endif
