#include <stdbool.h>
#include <stdint.h>
#include "inc/hw_memmap.h"
#include "inc/hw_types.h"
#include "inc/hw_gpio.h"
#include "driverlib/adc.h"
#include "driverlib/gpio.h"
#include "driverlib/i2c.h"
#include "driverlib/pin_map.h"
#include "driverlib/rom_map.h"
#include "driverlib/rom.h"
#include "driverlib/sysctl.h"
#include "driverlib/uart.h"
#include "utils/uartstdio.h"
#include "vision_task.h"
#include "power_task.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"


xSemaphoreHandle g_pUARTSemaphore;


void vApplicationStackOverflowHook(xTaskHandle * pxTask, char *pcTaskName)
{
	//
	// This function can not return, so loop forever.  Interrupts are disabled
	// on entry to this function, so no processor interrupts will interrupt
	// this loop.
	//
	while (1) {
	}
}

void ConfigureUART(void)
{
	ROM_SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOA);
	ROM_GPIOPinConfigure(GPIO_PA0_U0RX);
	ROM_GPIOPinConfigure(GPIO_PA1_U0TX);
	ROM_GPIOPinTypeUART(GPIO_PORTA_BASE, GPIO_PIN_0 | GPIO_PIN_1);

	ROM_SysCtlPeripheralEnable(SYSCTL_PERIPH_UART0);
	UARTClockSourceSet(UART0_BASE, UART_CLOCK_PIOSC);
	UARTStdioConfig(0, 115200, 16000000);
}


void ConfigureI2C(void)
{
	ROM_SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOE);
	ROM_GPIOPinConfigure(GPIO_PE4_I2C2SCL);
	ROM_GPIOPinConfigure(GPIO_PE5_I2C2SDA);
	ROM_GPIOPinTypeI2C(GPIO_PORTE_BASE, GPIO_PIN_4 | GPIO_PIN_5);

	ROM_SysCtlPeripheralEnable(SYSCTL_PERIPH_I2C2);
	I2CMasterEnable(I2C2_BASE);
	I2CMasterInitExpClk(I2C2_BASE, 100000, false);
}

int main(void)
{
	// Set the clocking to run at 50 MHz from the PLL.
	ROM_SysCtlClockSet(SYSCTL_SYSDIV_4 | SYSCTL_USE_PLL |
			   SYSCTL_XTAL_16MHZ | SYSCTL_OSC_MAIN);

	ConfigureUART();
	//ConfigureI2C();

	UARTprintf("\033[2J");
	UARTprintf("\nStarting up...\n");

	g_pUARTSemaphore = xSemaphoreCreateMutex();

	if (VisionTaskInit() != 0)
		while (1);
	//if (PowerTaskInit() != 0)
	//	while (1);
	
	vTaskStartScheduler();
	while (1);
}
