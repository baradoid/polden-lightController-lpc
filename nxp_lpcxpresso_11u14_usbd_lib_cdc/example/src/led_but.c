#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include <limits.h>

#define GPIO_PININT_INDEX		0 /* PININT index used for GPIO mapping */

void pushButtonIrqHandler()
{
	NVIC_DisableIRQ(PIN_INT0_IRQn);
	NVIC_DisableIRQ(PIN_INT1_IRQn);
	BaseType_t xHigherPriorityTaskWoken;
	xHigherPriorityTaskWoken = pdFALSE;
	uint32_t intStat = Chip_PININT_GetIntStatus(LPC_PININT);
	Chip_PININT_ClearIntStatus(LPC_PININT, PININTCH0|PININTCH1);

//	if(butTaskHandle != NULL){
//		xTaskNotifyFromISR(butTaskHandle, intStat+0x20, eSetValueWithOverwrite, &xHigherPriorityTaskWoken);
//	}
	if(mainTaskHandle != NULL){
		xTaskNotifyFromISR(mainTaskHandle, 1<<(intStat+3), eSetBits, &xHigherPriorityTaskWoken);
	}
}

void FLEX_INT0_IRQHandler(void)
{
	pushButtonIrqHandler();
}

void FLEX_INT1_IRQHandler(void)
{
	pushButtonIrqHandler();
}


void allowButtonsInput()
{
	Chip_PININT_ClearIntStatus(LPC_PININT, PININTCH0|PININTCH1);
	/* Enable interrupt in the NVIC */
	NVIC_EnableIRQ(PIN_INT0_IRQn);
	NVIC_EnableIRQ(PIN_INT1_IRQn);
}



void vLedTask(void *pvParameters)
{
	Chip_GPIO_SetPinDIROutput(LPC_GPIO, 1, 17); //POLDEN BUTTON LED

	Chip_GPIO_SetPinDIRInput(LPC_GPIO, 1, 5);
	Chip_GPIO_SetPinDIRInput(LPC_GPIO, 1, 18);



	/* Enable PININT clock */
	Chip_Clock_EnablePeriphClock(SYSCTL_CLOCK_PINT);
	/* Configure interrupt channel for the GPIO pin in SysCon block */
//	Chip_SYSCTL_SetPinInterrupt(0, 1, 5);
//	Chip_SYSCTL_SetPinInterrupt(1, 1, 18);

	/* Configure channel interrupt as edge sensitive and falling edge interrupt */
//	Chip_PININT_SetPinModeEdge(LPC_PININT, PININTCH0|PININTCH1);
//	Chip_PININT_EnableIntLow(LPC_PININT, PININTCH0|PININTCH1);


	//	Chip_GPIO_SetPinDIROutput(LPC_GPIO, 0, 18); //D0
	//Chip_GPIO_SetPinDIROutput(LPC_GPIO, 0, 19); //D1
	//Chip_GPIO_SetPinDIROutput(LPC_GPIO, 0, 17); //D2

	Chip_GPIO_SetPinDIROutput(LPC_GPIO, 1, 11); //D4
	Chip_GPIO_SetPinDIROutput(LPC_GPIO, 1, 10); //D3
	Chip_GPIO_SetPinDIROutput(LPC_GPIO, 1, 9); //D2

	Chip_GPIO_SetPinDIROutput(LPC_GPIO, 1, 8); //D1
	//Chip_GPIO_SetPinDIROutput(LPC_GPIO, 0, 10); //D0

	//Chip_GPIO_SetPinDIROutput(LPC_GPIO, 1, 5); //D1
	//Chip_GPIO_SetPinState(LPC_GPIO, 1, 5, true); //D2

	Chip_GPIO_SetPinOutHigh(LPC_GPIO, 1, 11);
	Chip_GPIO_SetPinOutLow(LPC_GPIO, 1, 10);
	//Chip_GPIO_SetPortOutLow(LPC_GPIO, 1, 9);


	//char str[25];
	int i=0;
	for(;;){
//		if(xTaskNotifyWait( 0x00, ULONG_MAX, &ulNotifiedValue,  500) == pdTRUE){
//			sprintf(str, "al %d\r\n", ulNotifiedValue);
//			vcomPrintf(str);
//		}

		//vcomPrintf("Toggle\r\n");

		if(i>50){
			Chip_GPIO_SetPinToggle(LPC_GPIO, 1, 11);
			Chip_GPIO_SetPinToggle(LPC_GPIO, 1, 10);
			i = 0;;
		}
		else{
			i++;
		}

		if(Chip_GPIO_GetPinState(LPC_GPIO, 1, 5) == true)
			Chip_GPIO_SetPinOutLow(LPC_GPIO, 1, 9); //D2
		else
			Chip_GPIO_SetPinOutHigh(LPC_GPIO, 1, 9); //D2

		if(Chip_GPIO_GetPinState(LPC_GPIO, 1, 18) == true)
			Chip_GPIO_SetPinOutLow(LPC_GPIO, 1, 8); //D1
		else
			Chip_GPIO_SetPinOutHigh(LPC_GPIO, 1, 8); //D1


		vTaskDelay(10);
	}
}



uint32_t pwmPer = 0;
void initPWM()
{
	Chip_IOCON_PinMuxSet(LPC_IOCON, 1, 24,
			(IOCON_FUNC1 | IOCON_MODE_INACT ));

	Chip_IOCON_PinMuxSet(LPC_IOCON, 1, 25,
			(IOCON_FUNC1 | IOCON_MODE_INACT ));
//	Chip_GPIO_SetPinOutHigh(LPC_GPIO, 1, 24);
//	Chip_GPIO_SetPinOutHigh(LPC_GPIO, 1, 25);
//
//	Chip_GPIO_SetPinDIROutput(LPC_GPIO, 1, 24); //D3
//	Chip_GPIO_SetPinDIROutput(LPC_GPIO, 1, 25); //D2

	uint32_t timerFreq;
	/* Enable timer 1 clock */
	Chip_TIMER_Init(LPC_TIMER32_0);

	/* Timer rate is system clock rate */
	timerFreq = Chip_Clock_GetSystemClockRate();

	/* Timer setup for match and interrupt at TICKRATE_HZ */
	Chip_TIMER_Reset(LPC_TIMER32_0);
	//Chip_TIMER_MatchEnableInt(LPC_TIMER32_0, 1);
	pwmPer = (timerFreq / 1000);

	Chip_TIMER_SetMatch(LPC_TIMER32_0, 0, pwmPer*0.1);
	Chip_TIMER_SetMatch(LPC_TIMER32_0, 1, pwmPer*0.1);
	Chip_TIMER_SetMatch(LPC_TIMER32_0, 2, pwmPer);


	Chip_TIMER_ResetOnMatchEnable(LPC_TIMER32_0, 2);

	LPC_TIMER32_0->PWMC = 0x3; //PWMEN0, PWMEN1
	Chip_TIMER_Enable(LPC_TIMER32_0);
}


//bool bLedOn = false;
#define NORMAL_DELAY 10
#define SHORT_DELAY  2
TickType_t delay = NORMAL_DELAY;
int8_t ledLevel = 0;
bool bFastBlink = false;

//void setButtonLedOff()
//{
//	bLedOn = false;
//}

typedef enum{
	GREEN,
	RED
} TBLedEnumType;

TBLedEnumType curBLedType=RED, nextBLedType=RED;
void turnOnGreenLed()
{
	nextBLedType = GREEN;
}

void turnOnRedLed()
{
	nextBLedType = RED;
}

void setButtonLedDelayNormal()
{
	bFastBlink = false;
}

void setButtonLedDelayFast()
{
	bFastBlink = true;
}


void setLedRLedLevel(uint8_t l)
{
	float level = (float)pwmPer*((float)(100-l)/100);
	Chip_TIMER_SetMatch(LPC_TIMER32_0, 0, level);
}

void setLedGLedLevel(uint8_t l)
{
	float level = (float)pwmPer*((float)(100-l)/100);
	Chip_TIMER_SetMatch(LPC_TIMER32_0, 1, level);
}

void vButtonLedTask(void *pvParameters)
{
	Chip_IOCON_PinMuxSet(LPC_IOCON, 1, 5,
			(IOCON_FUNC0 | IOCON_MODE_INACT | IOCON_DIGMODE_EN));

	Chip_IOCON_PinMuxSet(LPC_IOCON, 1, 18,
			(IOCON_FUNC0 | IOCON_MODE_INACT | IOCON_DIGMODE_EN));


	initPWM();


	const uint8_t ledLow = 15;
	const uint8_t ledHigh = 100;

	int8_t ledLevelIncr = 1;
	//char str[40];
	uint32_t ulNotifiedValue;
	setLedGLedLevel(0);
	setLedRLedLevel(0);

	for(;;){

		if(curBLedType == nextBLedType){
			if(ledLevel <= ledLow)
				ledLevelIncr = 1;
			else if(ledLevel >= ledHigh)
				ledLevelIncr = -1;

			ledLevel+=ledLevelIncr;

		}
		else{
			if(ledLevel > 0){
				ledLevel--;
			}
			else{
				curBLedType = nextBLedType;
			}
		}

		if(curBLedType == GREEN){
			setLedGLedLevel(ledLevel);
			setLedRLedLevel(0);
		}
		else if(curBLedType == RED){
			setLedGLedLevel(0);
			setLedRLedLevel(ledLevel);
		}

		if(bFastBlink == true)
			delay = SHORT_DELAY;
		else
			delay = NORMAL_DELAY;
		vTaskDelay(delay);
	}
}
