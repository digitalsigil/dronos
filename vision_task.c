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

#include "inc/tm4c123gh6pm.h"


#define VISIONTASKSTACKSIZE        128	// Stack size in words
#define ADC_SCAN_BUF_SIZE 256

extern xSemaphoreHandle g_pUARTSemaphore;

volatile uint16_t g_ADCScanBuf[ADC_SCAN_BUF_SIZE];
volatile uint32_t g_ADCScanBufWH = 0;
volatile uint32_t g_ADCScanBufRH = 0;

volatile uint32_t g_count;
static void ADCInterrupt(void)
{
	uint32_t b[8];
	uint8_t i;

	ADCIntClear(ADC0_BASE, 0);
	ADCSequenceDataGet(ADC0_BASE, 0, b);

	for (i = 0; i < 8; i++) {
		g_ADCScanBuf[g_ADCScanBufWH] = b[i];
		g_ADCScanBufWH = (g_ADCScanBufWH + 1) % ADC_SCAN_BUF_SIZE;
	}
	
	g_count += 8;
}

static void VisionTask(void *pvParameters)
{
	portTickType LastTime;
	uint32_t VisionDelay = 25;
	uint32_t acc;
	uint32_t n;

	// Get the current tick count.
	LastTime = xTaskGetTickCount();

	while (1) {
		n = 0;
		acc = 0;
		while (g_ADCScanBufRH != g_ADCScanBufWH) {
			acc += g_ADCScanBuf[g_ADCScanBufRH];
			n++;
			g_ADCScanBufRH =
			    (g_ADCScanBufRH + 1) % ADC_SCAN_BUF_SIZE;
		}
		
		xSemaphoreTake(g_pUARTSemaphore, portMAX_DELAY);
		acc /= n;
		UARTprintf("\n");
		xSemaphoreGive(g_pUARTSemaphore);


		//
		// Wait for the required amount of time to check back.
		//
		vTaskDelayUntil(&LastTime, VisionDelay / portTICK_RATE_MS);
	}
}

//*****************************************************************************
//
// Initializes the switch task.
//
//*****************************************************************************
uint32_t VisionTaskInit(void)
{
	ADCSequenceDisable(ADC0_BASE, 0);

	ADCSequenceConfigure(ADC0_BASE, 0, ADC_TRIGGER_ALWAYS, 0);

	ADCSequenceStepConfigure(ADC0_BASE, 0, 0, ADC_CTL_CH0);
	ADCSequenceStepConfigure(ADC0_BASE, 0, 1, ADC_CTL_CH0);
	ADCSequenceStepConfigure(ADC0_BASE, 0, 2, ADC_CTL_CH0);
	ADCSequenceStepConfigure(ADC0_BASE, 0, 3, ADC_CTL_CH0);
	ADCSequenceStepConfigure(ADC0_BASE, 0, 4, ADC_CTL_CH0);
	ADCSequenceStepConfigure(ADC0_BASE, 0, 5, ADC_CTL_CH0);
	ADCSequenceStepConfigure(ADC0_BASE, 0, 6, ADC_CTL_CH0);
	ADCSequenceStepConfigure(ADC0_BASE, 0, 7,
				 ADC_CTL_CH0 | ADC_CTL_IE | ADC_CTL_END);
	
	ADCClockConfigSet(ADC0_BASE, ADC_CLOCK_SRC_PIOSC | ADC_CLOCK_RATE_FOURTH, 1);
	ADCIntRegister(ADC0_BASE, 0, ADCInterrupt);
	ADCIntEnable(ADC0_BASE, 0);

	ADCSequenceEnable(ADC0_BASE, 0);

	int32_t rv = xTaskCreate(VisionTask,
				 (signed portCHAR *) "Vision",
				 VISIONTASKSTACKSIZE, NULL,
				 tskIDLE_PRIORITY + PRIORITY_VISION_TASK,
				 NULL);

	return rv == pdTRUE ? 0 : 1;
}
