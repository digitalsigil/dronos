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


void ConfigureADC(void)
{
	ROM_SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOE);
	ROM_GPIOPinTypeADC(GPIO_PORTE_BASE, GPIO_PIN_3);

	ROM_SysCtlPeripheralEnable(SYSCTL_PERIPH_ADC0);
	ADCIntDisable(ADC0_BASE, 0);

	ADCSequenceConfigure(ADC0_BASE, 0, ADC_TRIGGER_PROCESSOR, 0);
	ADCSequenceConfigure(ADC0_BASE, 1, ADC_TRIGGER_PROCESSOR, 1);
	ADCSequenceConfigure(ADC0_BASE, 2, ADC_TRIGGER_PROCESSOR, 2);
	ADCSequenceConfigure(ADC0_BASE, 3, ADC_TRIGGER_PROCESSOR, 3);

	ADCSequenceStepConfigure(ADC0_BASE, 0, 0, ADC_CTL_CH0);
	ADCSequenceStepConfigure(ADC0_BASE, 0, 1, ADC_CTL_CH0);
	ADCSequenceStepConfigure(ADC0_BASE, 0, 2, ADC_CTL_CH0);
	ADCSequenceStepConfigure(ADC0_BASE, 0, 3, ADC_CTL_CH0);
	ADCSequenceStepConfigure(ADC0_BASE, 0, 4, ADC_CTL_CH0);
	ADCSequenceStepConfigure(ADC0_BASE, 0, 5, ADC_CTL_CH0);
	ADCSequenceStepConfigure(ADC0_BASE, 0, 6, ADC_CTL_CH0);
	ADCSequenceStepConfigure(ADC0_BASE, 0, 7, ADC_CTL_CH0 | ADC_CTL_END);

	ADCSequenceEnable(ADC0_BASE, 0);
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

void ConfigureBluART(void)
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

void ConfigureBluGPIO(void)
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

int main(void)
{
	// Set the clocking to run at 50 MHz from the PLL.
	ROM_SysCtlClockSet(SYSCTL_SYSDIV_4 | SYSCTL_USE_PLL |
			   SYSCTL_XTAL_16MHZ | SYSCTL_OSC_MAIN);

	ConfigureUART();
	ConfigureADC();
	ConfigureI2C();
	ConfigureBluART();
	ConfigureBluGPIO();

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
