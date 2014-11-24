#include <stdbool.h>
#include <stdint.h>
#include "inc/hw_memmap.h"
#include "inc/hw_types.h"
#include "inc/hw_gpio.h"
#include "driverlib/i2c.h"
#include "driverlib/sysctl.h"
#include "driverlib/gpio.h"
#include "driverlib/rom.h"
#include "driverlib/uart.h"
#include "utils/uartstdio.h"
#include "driverlib/pin_map.h"
#include "driverlib/rom_map.h"
#include "power_task.h"
#include "priorities.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"


#define POWERTASKSTACKSIZE	128
#define BLUART_QUEUE_SIZE	64


extern	xSemaphoreHandle g_pUARTSemaphore;


volatile	char g_BluARTRx[BLUART_QUEUE_SIZE];
volatile	uint32_t g_BluARTRxRH = 0;
volatile	uint32_t g_BluARTRxWH = 0;

volatile	char g_BluARTTx[BLUART_QUEUE_SIZE];
volatile	uint32_t g_BluARTTxRH = 0;
volatile	uint32_t g_BluARTTxWH = 0;


static void
BluARTRx()
{
	while (UARTCharsAvail(UART6_BASE)) {
		g_BluARTRx[g_BluARTRxWH] = (char) UARTCharGetNonBlocking(UART6_BASE);
		g_BluARTRxWH = (g_BluARTRxWH + 1) % BLUART_QUEUE_SIZE;
	}
}

static void
BluARTTx()
{
	while (UARTSpaceAvail(UART6_BASE) && g_BluARTTxRH != g_BluARTTxWH) {
		UARTCharPutNonBlocking(UART6_BASE, g_BluARTTx[g_BluARTTxRH]);
		g_BluARTTxRH = (g_BluARTTxRH + 1) % BLUART_QUEUE_SIZE;
	}
}

static void
BluARTInterrupt(void)
{
	UARTIntClear(UART6_BASE, 0);
	BluARTRx();
	BluARTTx();
	
	if (g_BluARTTxRH == g_BluARTTxWH)
		UARTIntDisable(UART6_BASE, UART_INT_TX);
}

static void
BluARTPrimeTx(void)
{
	// load TX bytes from BluART TX buffer until UART6 FIFO is full
	UARTIntDisable(UART6_BASE, UART_INT_TX | UART_INT_RX);
	BluARTTx();
	UARTIntEnable(UART6_BASE, UART_INT_TX | UART_INT_RX);
}

static void
BluARTFlushRx(void)
{
	// Load all RX bytes from UART6 FIFO to BluART RX buffer
	UARTIntDisable(UART6_BASE, UART_INT_TX | UART_INT_RX);
	BluARTRx();
	UARTIntEnable(UART6_BASE, UART_INT_TX | UART_INT_RX);
}

static int
BluARTWait(char *str, int32_t ms)
{
	int32_t	 t = 0;
	char	*p = str;
	int8_t	 c;
	
	while (1) {
		BluARTFlushRx();
		while (g_BluARTRxWH != g_BluARTRxRH) {
			if (*p == '\0')
				return 0;
			
			UARTprintf("WAITING FOR '%s'\n", p);

			c = (int8_t) g_BluARTRx[g_BluARTRxRH];
			g_BluARTRxRH = (g_BluARTRxRH + 1) % BLUART_QUEUE_SIZE;
			
			if (c != *p)
				p = str;
			else
				p++;
		}
		vTaskDelay(10 / portTICK_RATE_MS);
		t += 10;
		if (t > ms)
			return -1;
	}
	return 0;
}

static void
BluARTInteract()
{
	char	c;
	
	while (1) {
		BluARTFlushRx();
		while (g_BluARTRxWH != g_BluARTRxRH) {
			c = (int8_t) g_BluARTRx[g_BluARTRxRH];
			g_BluARTRxRH = (g_BluARTRxRH + 1) % BLUART_QUEUE_SIZE;
			UARTprintf("%c", c);
		}
		
		while (UARTCharsAvail(UART0_BASE)) {
			g_BluARTTx[g_BluARTTxWH] = (int8_t) UARTCharGetNonBlocking(UART0_BASE);
			g_BluARTTxWH = (g_BluARTTxWH + 1) % BLUART_QUEUE_SIZE;
		}
		BluARTPrimeTx();
	}

}

static void
BluARTSend(char *str)
{
	// DEBUG
	UARTprintf("\nSENDING \"%s\"\n", str);
	
	while (*str != '\0') {
		g_BluARTTx[g_BluARTTxWH] = *str++;
		g_BluARTTxWH = (g_BluARTTxWH + 1) % BLUART_QUEUE_SIZE;
	}
	BluARTPrimeTx();
}

static int
BluARTConnect(void)
{
	// TODO: HW reset
	
	if (BluARTWait("CMD", 1000))
		return -1;
	
	BluARTSend("+\r\n");
	if (BluARTWait("Echo", 1000))
		return -1;
	
	//BluARTSend("SS,C0000000\r\n");
	BluARTSend("SS,30000000\r\n");
	if (BluARTWait("AOK", 1000))
		return -1;
	
	//BluARTSend("SR,92000000\r\n");
	BluARTSend("SR,32000000\r\n");
	if (BluARTWait("AOK", 1000))
		return -1;

	BluARTSend("R,1\r");
	if (BluARTWait("CMD", 10000))
		return -1;

	BluARTInteract();
	
	BluARTSend("I\r");
	if (BluARTWait("OK", 1000))
		return -1;

	return 0;
}


void
configBluART(void)
{
	ROM_SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOD);
	ROM_GPIOPinConfigure(GPIO_PD4_U6RX);
	ROM_GPIOPinConfigure(GPIO_PD5_U6TX);
	ROM_GPIOPinTypeUART(GPIO_PORTD_BASE, GPIO_PIN_4 | GPIO_PIN_5);

	// use 16MHz internal
	ROM_SysCtlPeripheralEnable(SYSCTL_PERIPH_UART6);
	UARTClockSourceSet(UART6_BASE, UART_CLOCK_PIOSC);
	UARTEnable(UART6_BASE);
	UARTConfigSetExpClk(
		UART6_BASE,
		16000000,
		115200,
		UART_CONFIG_WLEN_8 | UART_CONFIG_PAR_NONE | UART_CONFIG_STOP_ONE);
}

void
configBluGPIO(void)
{
	ROM_SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOF);
	
	HWREG(GPIO_PORTF_BASE + GPIO_O_LOCK) = GPIO_LOCK_KEY;
	HWREG(GPIO_PORTF_BASE + GPIO_O_CR) = 0xFF;
	
	ROM_GPIOPinTypeGPIOOutput(GPIO_PORTF_BASE, GPIO_PIN_4);
	ROM_GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_4, GPIO_PIN_4);

	ROM_SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOE);
	ROM_GPIOPinTypeGPIOOutput(GPIO_PORTE_BASE, GPIO_PIN_1);
	ROM_GPIOPinWrite(GPIO_PORTE_BASE, GPIO_PIN_1, GPIO_PIN_1);
}

static void
PowerTask(void *pvParameters)
{
	portTickType LastTime;
	uint32_t PowerDelay = 25;
	int8_t c;

	LastTime = xTaskGetTickCount();

	UARTprintf("attempt to connect...\n");
	//if (BluARTConnect())
	//	UARTprintf("connection failed!\n");
	while (1) {
		/*
		   while (UARTCharsAvail(UART0_BASE)) {
			   g_BluARTTx[g_BluARTTxWH] = (int8_t) UARTgetc();
			   g_BluARTTxWH = (g_BluARTTxWH + 1) % BLUART_QUEUE_SIZE;
		   }
		   BluARTPrimeTx();

		   xSemaphoreTake(g_pUARTSemaphore, portMAX_DELAY);
		   while (g_BluARTRxWH != g_BluARTRxRH) {
			   c = (int8_t) g_BluARTRx[g_BluARTRxRH];
			   g_BluARTRxRH = (g_BluARTRxRH + 1) % BLUART_QUEUE_SIZE;
			   UARTwrite((const char *) &c, 1);
		   }
		   BluARTFlushRx();
		   xSemaphoreGive(g_pUARTSemaphore);
		   
		   xSemaphoreTake(g_pUARTSemaphore, portMAX_DELAY);
		   if (I2CMasterBusy(I2C2_BASE) != pdTRUE) {
		   UARTprintf("free\n");
		   I2CMasterSlaveAddrSet(I2C2_BASE, 0, true);
		   I2CMasterDataPut(I2C2_BASE, 0x80);
		   I2CMasterControl(I2C2_BASE, I2C_MASTER_CMD_FIFO_SINGLE_SEND);
		   } else {
		   I2CMasterDataGet(I2C2_BASE);
		   UARTprintf("busy\n");
		   }
		   xSemaphoreGive(g_pUARTSemaphore);
		 */

		vTaskDelayUntil(&LastTime, PowerDelay / portTICK_RATE_MS);
	}
}

uint32_t
PowerTaskInit(void)
{
	configBluART();
	configBluGPIO();

	UARTIntRegister(UART6_BASE, BluARTInterrupt);
	//UARTFIFOLevelSet(UART6_BASE, UART_FIFO_TX1_2, UART_FIFO_RX1_2);
	UARTIntEnable(UART6_BASE, UART_INT_TX | UART_INT_RX);

	int32_t rv = xTaskCreate(PowerTask,
				 (signed portCHAR *) "Power",
				 POWERTASKSTACKSIZE, NULL,
				 tskIDLE_PRIORITY + PRIORITY_POWER_TASK,
				 NULL);

	return rv == pdTRUE ? 0 : 1;
}
