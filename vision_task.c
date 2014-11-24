#include <stdbool.h>
#include <stdint.h>
#include "inc/hw_memmap.h"
#include "inc/hw_types.h"
#include "driverlib/adc.h"
#include "driverlib/sysctl.h"
#include "driverlib/gpio.h"
#include "driverlib/rom.h"
#include "utils/uartstdio.h"
#include "vision_task.h"
#include "priorities.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"


#define VISIONTASKSTACKSIZE	128	// in words
#define ADCqSIZE		256


extern	xSemaphoreHandle g_pUARTSemaphore;


static	volatile uint16_t ADCq[ADCqSIZE];
static	volatile uint32_t ADCqWH = 0;
static	volatile uint32_t ADCqRH = 0;


static void
ADCInterrupt(void)
{
	uint32_t	b[8];
	uint8_t		i;

	ADCIntClear(ADC0_BASE, 0);
	ADCSequenceDataGet(ADC0_BASE, 0, b);

	for (i = 0; i < 8; i++) {
		ADCq[ADCqWH] = b[i];
		ADCqWH = (ADCqWH + 1) % ADCqSIZE;
	}
}

static void
VisionTask(void *pvParameters)
{
	portTickType	LastTime;
	uint32_t	VisionDelay = 25;
	uint32_t	acc;
	uint32_t	n;

	// Get the current tick count.
	LastTime = xTaskGetTickCount();

	while (1) {
		n = 0;
		acc = 0;
		while (ADCqRH != ADCqWH) {
			acc += ADCq[ADCqRH];
			n++;
			ADCqRH = (ADCqRH + 1) % ADCqSIZE;
		}
		
		xSemaphoreTake(g_pUARTSemaphore, portMAX_DELAY);
		acc /= n;
		UARTprintf("\n");
		xSemaphoreGive(g_pUARTSemaphore);

		vTaskDelayUntil(&LastTime, VisionDelay / portTICK_RATE_MS);
	}
}

static void
initADC(void)
{
	uint32_t	i;
	uint32_t	opt;
	
	ROM_SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOE);
	ROM_GPIOPinTypeADC(GPIO_PORTE_BASE, GPIO_PIN_3);

	ROM_SysCtlPeripheralEnable(SYSCTL_PERIPH_ADC0);

	ADCSequenceConfigure(ADC0_BASE, 0, ADC_TRIGGER_ALWAYS, 0);
	for (i = 0; i < 8; i++) {
		opt = i == 7 ? ADC_CTL_IE | ADC_CTL_END : 0;
		ADCSequenceStepConfigure(ADC0_BASE, 0, i, ADC_CTL_CH0 | opt);
	}
		
	ADCClockConfigSet(ADC0_BASE, ADC_CLOCK_SRC_PIOSC | ADC_CLOCK_RATE_FOURTH, 1);
	ADCIntRegister(ADC0_BASE, 0, ADCInterrupt);
	ADCIntEnable(ADC0_BASE, 0);
	
	ADCSequenceEnable(ADC0_BASE, 0);
}

uint32_t
VisionTaskInit(void)
{
	int32_t	rv = xTaskCreate(
		VisionTask,
		(signed portCHAR *) "Vision",
		VISIONTASKSTACKSIZE, NULL,
		tskIDLE_PRIORITY + PRIORITY_VISION_TASK,
		NULL);

	return rv == pdTRUE ? 0 : 1;
}
