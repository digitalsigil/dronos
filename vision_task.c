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
#define TRIG_BITCOUNT		4
#define DATA_BITCOUNT		8
#define ADC_THRESH		2300


extern	xSemaphoreHandle g_pUARTSemaphore;


static	volatile uint16_t ADCq[ADCqSIZE];
static	volatile uint32_t ADCqWH = 0;
static	volatile uint32_t ADCqRH = 0;

volatile	uint32_t SEEq[SEEqSIZE];
static volatile	uint32_t SEEqWH = 0;
volatile	uint32_t SEEqRH = 0;


static void
ADCInterrupt(void)
{
	static	uint32_t last = 0;
	static	uint32_t sb = 0;
	static	uint32_t sc = 0;
	
	uint32_t	b[8];
	uint32_t	i;

	// update shift buffer
	ADCIntClear(ADC0_BASE, 0);
	ADCSequenceDataGet(ADC0_BASE, 0, b);
	for (i = 0; i < 8; i++)	sb = (sb << 1) | (b[i] < ADC_THRESH);
	
	// check for edge. If detected, it has been 32 cycles.
	if (~sb == 0 && !last) {
		last = 1;
		sc = 32;
	} else if (sb == 0 && last) {
		last = 0;
		sc = 32;
	}
	
	// if BIT_TIME cycles have elapsed, acknowledge a new bit
	if (sc++ % 128 == 0) {
		ADCq[ADCqWH] = last;
		ADCqWH = (ADCqWH + 1) % ADCqSIZE;
	}
}

static void
decodeADC(void)
{
	static	uint32_t d = 0;
	static	uint8_t bits = 0;
	static	uint8_t trig_bits = 0;
	static	uint8_t triggering = 1;
	
	uint32_t	b;
	
	while (ADCqRH != ADCqWH) {
		b = ADCq[ADCqRH];
		ADCqRH = (ADCqRH + 1) % ADCqSIZE;
		
		// demand a sequence of at least TRIG_BITCOUNT
		if (triggering) {
			if (trig_bits >= TRIG_BITCOUNT) {
				if (b) {
					trig_bits = TRIG_BITCOUNT;
					continue;
				}
				triggering = 0;
			} else {
				if (b)	trig_bits++;
				else	trig_bits = 0;
				
				continue;
			}
		}
		
		// record DATA_BITCOUNT bits into the data byte
		d = (d << 1) | b;
		bits++;
		if (bits < DATA_BITCOUNT)	continue;
		
		// send the data byte to SEEq
		SEEq[SEEqWH] = d;
		SEEqWH = (SEEqWH + 1) % SEEqSIZE;
		
		// put back into triggering mode and wait for more data
		d = bits = trig_bits = 0;
		triggering = 1;
	}
}

static void
VisionTask(void *pvParameters)
{
	portTickType	LastTime;
	//uint32_t	VisionDelay = 25;
	uint32_t	VisionDelay = 500;
	uint32_t	n, c;
	
	// Get the current tick count.
	LastTime = xTaskGetTickCount();

	c = n = 0;
	while (1) {
		xSemaphoreTake(g_pUARTSemaphore, portMAX_DELAY);
		
		decodeADC();
		while (SEEqRH != SEEqWH) {
			UARTprintf("%032x\n", SEEq[SEEqRH]);
			SEEqRH = (SEEqRH + 1) % SEEqSIZE;
		}

		/*
		while (ADCqRH != ADCqWH) {
			if(ADCq[ADCqRH]) c++;
			else c = 0;
			
			UARTprintf("%s%s",
				ADCq[ADCqRH] ? "#" : ".",
				++n % 128 == 0 ? "\n" : "");
				
			if (c == TRIG_BITCOUNT) {
				n = 0;
				c = 0;
				UARTprintf("\n");
			}
			ADCqRH = (ADCqRH + 1) % ADCqSIZE;
		}
		*/

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

	ADCClockConfigSet(ADC0_BASE, ADC_CLOCK_SRC_PIOSC | ADC_CLOCK_RATE_HALF, 1);
	
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
