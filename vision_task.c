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


// don't use these... they're just silly.
//#define READ_QUEUE(v, q, size, h)	v = q[h]; h = (h + 1) % size
//#define WRITE_QUEUE(v, q, size, h)	q[h] = v; h = (h + 1) % size

#define VISIONTASKSTACKSIZE	128	// in words
#define ADCqSIZE		256
#define SEEqSIZE		32
#define CLEAR_BITS		16
#define SIGNAL_BITS		8
#define ADC_THRESH		1000


extern	xSemaphoreHandle g_pUARTSemaphore;


static	volatile uint16_t ADCq[ADCqSIZE];
static	volatile uint32_t ADCqWH = 0;
static	volatile uint32_t ADCqRH = 0;

static	volatile uint32_t sample_count = 0;

volatile	uint32_t SEEq[SEEqSIZE];
static volatile	uint32_t SEEqWH = 0;
volatile	uint32_t SEEqRH = 0;


static void
ADCInterrupt(void)
{
	uint32_t	b[8];
	uint8_t		i;

	ADCIntClear(ADC0_BASE, 0);
	ADCSequenceDataGet(ADC0_BASE, 0, b);

	i = 0;
	for (i = 0; i < 8; i++) {
		ADCq[ADCqWH] = b[i];
		ADCqWH = (ADCqWH + 1) % ADCqSIZE;
	}
	
	sample_count++;
}

static void
decodeADC(void)
{
	static	uint32_t data = 0;
	static	uint8_t bits = 0;
	static	uint8_t clear_bits = 0;
	
	uint32_t	b;
	
	while (ADCqRH != ADCqWH) {
		b = ADCq[ADCqRH] > ADC_THRESH ? 0 : 1;
		ADCqRH = (ADCqRH + 1) % ADCqSIZE;
		
		clear_bits = b ? clear_bits + 1 : 0;
		data = (data << 1) | b;
		bits++;

		if (bits == 32 || clear_bits == CLEAR_BITS) {
			if (data > 0) {
				SEEq[SEEqWH] = data;
				SEEqWH = (SEEqWH + 1) % SEEqSIZE;
			}
			
			data = bits = clear_bits = 0;
			data = 1;
		}
	}
}

static void
VisionTask(void *pvParameters)
{
	portTickType	LastTime;
	//uint32_t	VisionDelay = 25;
	uint32_t	VisionDelay = 500;
	
	uint32_t	lsc = 0;

	// Get the current tick count.
	LastTime = xTaskGetTickCount();

	while (1) {
		decodeADC();

		xSemaphoreTake(g_pUARTSemaphore, portMAX_DELAY);
		UARTprintf("%d\n", (sample_count - lsc) * 8);
		lsc = sample_count;
		//while (SEEqRH != SEEqWH) {
		//	UARTprintf("0x%x\n", SEEq[SEEqRH]);
		//	SEEqRH = (SEEqRH + 1) % SEEqSIZE;
		//}
		xSemaphoreGive(g_pUARTSemaphore);

		vTaskDelayUntil(&LastTime, VisionDelay / portTICK_RATE_MS);
	}
}

static void
initADC(void)
{
	uint32_t	i;
	uint32_t	opt;
	
	ROM_SysCtlPeripheralEnable(SYSCTL_PERIPH_ADC0);
		
	ROM_SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOE);
	ROM_GPIOPinTypeADC(GPIO_PORTE_BASE, GPIO_PIN_3);

	ADCSequenceDisable(ADC0_BASE, 0);
	ADCSequenceConfigure(ADC0_BASE, 0, ADC_TRIGGER_ALWAYS, 0);
	for (i = 0; i < 8; i++) {
		opt = (i == 7) ? (ADC_CTL_IE | ADC_CTL_END) : 0;
		ADCSequenceStepConfigure(ADC0_BASE, 0, i, ADC_CTL_CH0 | opt);
	}

	ADCClockConfigSet(ADC0_BASE, ADC_CLOCK_SRC_PIOSC | ADC_CLOCK_RATE_EIGHTH, 1);
	ADCSoftwareOversampleConfigure(ADC0_BASE, 0, 8);
	//ADCClockConfigSet(ADC0_BASE, ADC_CLOCK_SRC_PIOSC | ADC_CLOCK_RATE_HALF, 1);
	
	ADCIntRegister(ADC0_BASE, 0, ADCInterrupt);
	ADCIntEnable(ADC0_BASE, 0);
	ADCSequenceEnable(ADC0_BASE, 0);
}

uint32_t
VisionTaskInit(void)
{
	initADC();
	
	int32_t	rv = xTaskCreate(
		VisionTask,
		(signed portCHAR *) "Vision",
		VISIONTASKSTACKSIZE, NULL,
		tskIDLE_PRIORITY + PRIORITY_VISION_TASK,
		NULL);

	return rv == pdTRUE ? 0 : 1;
}
