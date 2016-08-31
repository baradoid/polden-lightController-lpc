#pragma once


//events to ESP task
#define EVENT_BUTTON_1_BIT 			0x0010
#define EVENT_BUTTON_2_BIT 			0x0020
#define EVENT_BUTTON_CANCEL_BIT 	0x0040
#define EVENT_RECV_BYTE_BIT			0x0008
//#define EVENT_LIGHTOFFCMD_BIT 		0x0100

//events to main task
#define EVENT_LIGHTON_CMD	 		0x0001
#define EVENT_LIGHTOFF_CMD	 		0x0002

#define EVENT_ESP_MASK				0x1f00

#define EVENT_ESP_OK_BIT  	    	0x0100
#define EVENT_ESP_LIGHT_OFF_BIT 	0x0200
#define EVENT_ESP_LIGHT_ON_BIT  	0x0400
#define EVENT_ESP_UNKNOWN_DATA_BIT 	0x0800
#define EVENT_ESP_ERR_BIT 			0x1000

#define CMD_LIGHT_OFF "10"
#define CMD_LIGHT_ON  "11"

extern TaskHandle_t  mainTaskHandle, espTaskHandle;
