#include "board.h"

uint8_t portNum[] = {0,0,0,0,0, 1, 0, 1};
uint8_t pinNum[]  = {4,5,7,9,8,29, 2, 27};

#define setPinOut(ri) Chip_GPIO_SetPinDIROutput(LPC_GPIO, portNum[ri], pinNum[ri])
#define setPinState(ri, st) Chip_GPIO_SetPinState(LPC_GPIO, portNum[ri], pinNum[ri], st)
void relaySwitch(uint8_t rInd, bool state)
{
	if(rInd>7)
		return;
	setPinState(rInd, state);
}

void initRelayPins()
{


		Chip_IOCON_PinMuxSet(LPC_IOCON, 0, 4, IOCON_FUNC0 | IOCON_STDI2C_EN);
		Chip_IOCON_PinMuxSet(LPC_IOCON, 0, 5, IOCON_FUNC0 | IOCON_STDI2C_EN);
		Chip_IOCON_PinMuxSet(LPC_IOCON, 0, 2, IOCON_FUNC0 | IOCON_MODE_INACT);

		for(int i=0; i<8; i++){
			setPinState(i, false);
			setPinOut(i);
		}

}

#include "FreeRTOS.h"
#include "task.h"
#include <limits.h>

void turnOnRelay()
{
	for(int i=0; i<8; i++){
		relaySwitch(i, true);
		vTaskDelay(configTICK_RATE_HZ);
	}
}
void turnOffRelay()
{
	for(int i=7; i>=0; i--){
		relaySwitch(i, false);
		vTaskDelay(configTICK_RATE_HZ);
	}
}

//void vRelayTask(void *pvParameters)
//{
//
//	int i=0;
//	char str[20];
//	uint32_t ulNotifiedValue;
//	bool bLastState = false;
//	for (;;) {
//		 xTaskNotifyWait( 0x00, ULONG_MAX, &ulNotifiedValue,  portMAX_DELAY );
//
//		 bool bNewState = (bool)ulNotifiedValue;
//		 if(bNewState != bLastState){
//			 if(bNewState == true){
//
//
//			 }
//			 else{
//
//			 }
//			 bLastState = bNewState;
//		 }
//	}
//}

