
#define SENSOR_PORT LPC_UART3
#define SENSOR_TX_PIN 28
#define SENSOR_RX_PIN 29

#define RBUF_SIZE 150
#define TBUF_SIZE 100


//functions prototypes

void vSensor_Task(void *pvParameters);

void sensor_recv_isr(void);
void sensor_xmit_isr(void);


void UART2_IRQHandler(void);
void decode_command(void);
void find_latest_date_from_data_sectors(void);
void sensor_send_cmd_direct(const char *cmd);
