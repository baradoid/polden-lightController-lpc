#pragma once

#define EVENT_BUTTON1_BIT 			0x0010
#define EVENT_BUTTON2_BIT 			0x0020


#define EVENT_ESP_MASK				0x1f00

#define EVENT_ESP_OK_BIT  	    	0x0100
#define EVENT_ESP_LIGHT_OFF_BIT 	0x0200
#define EVENT_ESP_LIGHT_ON_BIT  	0x0400
#define EVENT_ESP_UNKNOWN_DATA_BIT 	0x0800
#define EVENT_ESP_ERR_BIT 			0x1000

#define CMD_LIGHT_OFF "10"
#define CMD_LIGHT_ON  "11"

extern TaskHandle_t  mainTaskHandle, espTaskHandle;
