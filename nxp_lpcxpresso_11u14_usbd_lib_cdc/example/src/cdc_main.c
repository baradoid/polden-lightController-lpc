#include "board.h"
#include <stdio.h>
#include <string.h>
#include "app_usbd_cfg.h"

#include "relay.h"


#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#include "esp.h"
#include <limits.h>

/* Application specific configuration options. */
#include "FreeRTOSConfig.h"
#include "main.h"
#include "led_but.h"
#include "cdc_vcom_utils.h"

TaskHandle_t  espTaskHandle = NULL, mainTaskHandle = NULL;
TaskHandle_t  butTaskHandle = NULL, ledButTaskHandle = NULL;
//SemaphoreHandle_t xUartMsgSem = NULL;

//SemaphoreHandle_t xSendStartSem = NULL, xStopSem = NULL;

/* System oscillator rate and clock rate on the CLKIN pin */
const uint32_t OscRateIn = 12000000;

bool waitCmd()
{

}

static void vMainTask(void *pvParameters)
{
	uint32_t ulNotifiedValue;
	char str[60];
	char recvStr[150];
	bool ret;
	int secondButPushState = 0;
	vTaskDelay(1000);
	//for(;;){

	//}
	for(;;){

		if((isConnected() == true) && (isServerPingable() == true)){
			//vcomPrintf(" --- connected \r\n");
			//allowButtonsInput(); /* Enable interrupt in the NVIC */
			turnOnGreenLed();
			if(secondButPushState == 0)
				setButtonLedDelayNormal();

			if(xTaskNotifyWait( ULONG_MAX, ULONG_MAX, &ulNotifiedValue,  1000  ) == true){
				sprintf(str, "notify val %x\r\n", ulNotifiedValue);
				vcomPrintf(str);
				if((ulNotifiedValue&EVENT_BUTTON_2_BIT) != 0){ //warm up
					vcomPrintf("But2. Esp send \"but2\" .\r\n");

					if(espSend("but1") == true){
						setButtonLedDelayFast();
						secondButPushState ++;
					}
					else
						continue;
				}
				else if((ulNotifiedValue&EVENT_BUTTON_1_BIT) != 0){
					secondButPushState = 0;
					vcomPrintf("But1. Esp send \"but1\"\r\n");
					//xTaskNotify(espTaskHandle, '1', eSetValueWithoutOverwrite );
					if(espSend("but2") == false){
						vcomPrintf("send start command error. restart\r\n");
						continue;
					}
				}
				else{
					continue;
				}

				if(secondButPushState == 1){
					vcomPrintf("second But pushed once\r\n");
					continue;
				}
				secondButPushState = 0;
				turnOnRedLed();

				//setButtonLedOff();
				vcomPrintf("wait light OFF cmd. start serv with 60 min timeout\r\n");

				if(waitCommand(recvStr, 60*60*1000) == false){
					vcomPrintf("wait command error. restart\r\n");
					continue;
				}
				sprintf(str, "wait command ok. recv - \"%s\" \r\n", recvStr);
				vcomPrintf(str);

				if(strcmp(recvStr, CMD_LIGHT_OFF) != 0){
					vcomPrintf(" expected \""CMD_LIGHT_OFF"\". restart\r\n");
					continue;
				}

				vcomPrintf("\r\n");

		//		vcomPrintf("wait light off cmd. 3 min timeout\r\n");
		//		uint32_t startTicks = xTaskGetTickCount();
		//		for(;;){
		//			int32_t to = (3*60*1000) - (xTaskGetTickCount()-startTicks);
		//			if(to <= 0 )
		//				break;
		//			sprintf(str, "it to %d\r\n", to);
		//			vcomPrintf(str);
		//			if(xTaskNotifyWait( 0x00, ULONG_MAX, &ulNotifiedValue,  to) == pdFALSE){
		//				break;
		//			}
		//			//sprintf(str, "notify val %x\r\n", ulNotifiedValue);
		//			//vcomPrintf(str);
		//			if((ulNotifiedValue&EVENT_ESP_MASK) != 0){
		//				sprintf(str, "esp cmd 0x%x\r\n", ulNotifiedValue);
		//				vcomPrintf(str);
		//				break;
		//			}
		//			else{
		//				sprintf(str, "unexpected 0x%x\r\n", ulNotifiedValue);
		//				vcomPrintf(str);
		//				continue;
		//			}
		//		}
		//		if((ulNotifiedValue&EVENT_ESP_LIGHT_OFF_BIT) == 0){
		//			vcomPrintf("no turnOff cmd\r\n");
		//			continue;
		//		}

				vcomPrintf("turnOff cmd light recvd. Process...\r\n");
				turnOnRelay();

				vcomPrintf("wait light ON cmd. start serv with 5 min timeout\r\n");

				if(waitCommand(recvStr, 5*60*1000) == false){
					vcomPrintf("wait command error. Auto turn on.\r\n");
					turnOffRelay();
					continue;
				}
				sprintf(str, "wait command ok. recv -\"%s\"\r\n", recvStr);
				vcomPrintf(str);

				if(strcmp(recvStr, CMD_LIGHT_ON) != 0){
					vcomPrintf(" expected \""CMD_LIGHT_ON"\". Auto turn on.\r\n");
				}

				vcomPrintf("\r\ncmd to turn ON light recvd. Process...\r\n");


				turnOffRelay();
			}
		}
		else{

			turnOnRedLed();
			if(isConnected() == true){
				vcomPrintf(" --- not pingable \r\n");
				setButtonLedDelayNormal();

			}
			else{
				vcomPrintf(" --- not connected \r\n");
				setButtonLedDelayFast();
			}
		}
		vTaskDelay(2*configTICK_RATE_HZ);

	}
}



//static void vBut1Task(void *pvParameters)
//{
//	/* Configure GPIO pin as input pin */
//	Chip_GPIO_SetPinDIRInput(LPC_GPIO, 1, 25);
//	Chip_IOCON_PinMuxSet(LPC_IOCON, 1, 25,
//			(IOCON_FUNC0 | IOCON_MODE_PULLDOWN | IOCON_DIGMODE_EN));
//
//	Chip_GPIO_SetPinDIRInput(LPC_GPIO, 1, 24);
//	Chip_IOCON_PinMuxSet(LPC_IOCON, 1, 24,
//			(IOCON_FUNC0 | IOCON_MODE_PULLDOWN | IOCON_DIGMODE_EN));
//
//
////	/* Configure interrupt channel for the GPIO pin in SysCon block */
////	Chip_SYSCTL_SetPinInterrupt(GPIO_PININT_INDEX, 1, 25);
////
////	/* Configure channel interrupt as edge sensitive and falling edge interrupt */
////	Chip_PININT_SetPinModeEdge(LPC_PININT, PININTCH(GPIO_PININT_INDEX));
////	Chip_PININT_EnableIntHigh(LPC_PININT, PININTCH(GPIO_PININT_INDEX));
////
////	/* Enable interrupt in the NVIC */
////	NVIC_EnableIRQ(PIN_INT0_IRQn);
//
//	uint32_t ulNotifiedValue;
//	char str[25];
//	for(;;){
//		xTaskNotifyWait( 0x00, ULONG_MAX, &ulNotifiedValue,  500  );
//		//vcomPrintf("alive\r\n");
////		sprintf(str, " al %d %d\r\n", Chip_GPIO_GetPinState(LPC_GPIO, 1, 25),
////										Chip_GPIO_GetPinState(LPC_GPIO, 1, 24));
////		vcomPrintf(str);
//	}
//
//}

void initPWM();
void vButtonLedTask(void *pvParameters);

int main(void)
{

	SystemCoreClockUpdate();
	/* Initialize board and chip */
	//Board_Init();
	Chip_GPIO_Init(LPC_GPIO);
	initRelayPins();
	initCdcVcom();

//    xStartSem  = xSemaphoreCreateMutex();
//    if( xVComMutex == NULL ){
//    	while(1)
//    		vcomPrintf("err create sem");
//    }
//    xStopSem  = xSemaphoreCreateMutex();
//    if( xVComMutex == NULL ){
//    	while(1)
//    		vcomPrintf("err create sem");
//    }

	xTaskCreate(vEspTask, (signed char *) "vTaskEsp",
					5*configMINIMAL_STACK_SIZE, NULL, (tskIDLE_PRIORITY + 1UL),
					(xTaskHandle *) &espTaskHandle);


	xTaskCreate(vVcomTask, (signed char *) "vVcomTask",
				2*configMINIMAL_STACK_SIZE, NULL, (tskIDLE_PRIORITY + 1UL),
				(xTaskHandle *) NULL);

	xTaskCreate(vLedTask, (signed char *) "vLedTask",
					configMINIMAL_STACK_SIZE, NULL, (tskIDLE_PRIORITY + 1UL),
					(xTaskHandle *) &butTaskHandle);

	xTaskCreate(vButtonLedTask, (signed char *) "vButtonLedTask",
					configMINIMAL_STACK_SIZE, NULL, (tskIDLE_PRIORITY + 1UL),
					(xTaskHandle *) NULL);


	xTaskCreate(vMainTask, (signed char *) "vMainTask",
				5*configMINIMAL_STACK_SIZE, NULL, (tskIDLE_PRIORITY + 1UL),
				(xTaskHandle *) &mainTaskHandle);


//	xTaskCreate(vBut1Task, (signed char *) "vBut1Task",
//					2*configMINIMAL_STACK_SIZE, NULL, (tskIDLE_PRIORITY + 1UL),
//					(xTaskHandle *) &butTaskHandle);


//	xTaskCreate(vBut1Task, (signed char *) "vButton1Task",
//				configMINIMAL_STACK_SIZE, NULL, (tskIDLE_PRIORITY + 1UL),
//				(xTaskHandle *) NULL);
	//vUartTask0
	/* Start the scheduler */
	vTaskStartScheduler();
}


void vApplicationMallocFailedHook( void )
{
	/* vApplicationMallocFailedHook() will only be called if
	configUSE_MALLOC_FAILED_HOOK is set to 1 in FreeRTOSConfig.h.  It is a hook
	function that will get called if a call to pvPortMalloc() fails.
	pvPortMalloc() is called internally by the kernel whenever a task, queue,
	timer or semaphore is created.  It is also called by various parts of the
	demo application.  If heap_1.c or heap_2.c are used, then the size of the
	heap available to pvPortMalloc() is defined by configTOTAL_HEAP_SIZE in
	FreeRTOSConfig.h, and the xPortGetFreeHeapSize() API function can be used
	to query the size of free heap space that remains (although it does not
	provide information on how the remaining heap might be fragmented). */
	taskDISABLE_INTERRUPTS();
	for( ;; );
}
/*-----------------------------------------------------------*/

//void vApplicationIdleHook( void )
//{
//	/* vApplicationIdleHook() will only be called if configUSE_IDLE_HOOK is set
//	to 1 in FreeRTOSConfig.h.  It will be called on each iteration of the idle
//	task.  It is essential that code added to this hook function never attempts
//	to block in any way (for example, call xQueueReceive() with a block time
//	specified, or call vTaskDelay()).  If the application makes use of the
//	vTaskDelete() API function (as this demo application does) then it is also
//	important that vApplicationIdleHook() is permitted to return to its calling
//	function, because it is the responsibility of the idle task to clean up
//	memory allocated by the kernel to any task that has since been deleted. */
//}
/*-----------------------------------------------------------*/

void vApplicationStackOverflowHook( TaskHandle_t pxTask, char *pcTaskName )
{
	( void ) pcTaskName;
	( void ) pxTask;

	vcomPrintf("Stack overflow!!\r\n");
	/* Run time stack overflow checking is performed if
	configCHECK_FOR_STACK_OVERFLOW is defined to 1 or 2.  This hook
	function is called if a stack overflow is detected. */
	taskDISABLE_INTERRUPTS();
	for( ;; );
}
/*-----------------------------------------------------------*/

//void vApplicationTickHook( void )
//{
//#if mainCHECK_INTERRUPT_STACK == 1
//extern unsigned long _pvHeapStart[];
//
//	/* This function will be called by each tick interrupt if
//	configUSE_TICK_HOOK is set to 1 in FreeRTOSConfig.h.  User code can be
//	added here, but the tick hook is called from an interrupt context, so
//	code must not attempt to block, and only the interrupt safe FreeRTOS API
//	functions can be used (those that end in FromISR()). */
//
//	/* Manually check the last few bytes of the interrupt stack to check they
//	have not been overwritten.  Note - the task stacks are automatically
//	checked for overflow if configCHECK_FOR_STACK_OVERFLOW is set to 1 or 2
//	in FreeRTOSConifg.h, but the interrupt stack is not. */
//	configASSERT( memcmp( ( void * ) _pvHeapStart, ucExpectedInterruptStackValues, sizeof( ucExpectedInterruptStackValues ) ) == 0U );
//#endif /* mainCHECK_INTERRUPT_STACK */
//}


