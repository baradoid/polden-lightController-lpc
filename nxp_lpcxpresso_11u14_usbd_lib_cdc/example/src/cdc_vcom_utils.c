#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include "cdc_vcom.h"
#include "semphr.h"

SemaphoreHandle_t xVComMutex = NULL;

static USBD_HANDLE_T g_hUsb;


extern const  USBD_HW_API_T hw_api;
extern const  USBD_CORE_API_T core_api;
extern const  USBD_CDC_API_T cdc_api;
/* Since this example only uses CDC class link functions for that clas only */
static const  USBD_API_T g_usbApi = {
	&hw_api,
	&core_api,
	0,
	0,
	0,
	&cdc_api,
	0,
	0x02221101,
};

const  USBD_API_T *g_pUsbApi = &g_usbApi;

/* Initialize pin and clocks for USB0/USB1 port */
static void usb_pin_clk_init(void)
{
	/* enable USB main clock */
	Chip_Clock_SetUSBClockSource(SYSCTL_USBCLKSRC_PLLOUT, 1);
	/* Enable AHB clock to the USB block and USB RAM. */
	Chip_Clock_EnablePeriphClock(SYSCTL_CLOCK_USB);
	Chip_Clock_EnablePeriphClock(SYSCTL_CLOCK_USBRAM);
	/* power UP USB Phy */
	Chip_SYSCTL_PowerUp(SYSCTL_POWERDOWN_USBPAD_PD);
}

/**
 * @brief	Handle interrupt from USB0
 * @return	Nothing
 */
void USB_IRQHandler(void)
{
	USBD_API->hw->ISR(g_hUsb);
}

/* Find the address of interface descriptor for given class type. */
USB_INTERFACE_DESCRIPTOR *find_IntfDesc(const uint8_t *pDesc, uint32_t intfClass)
{
	USB_COMMON_DESCRIPTOR *pD;
	USB_INTERFACE_DESCRIPTOR *pIntfDesc = 0;
	uint32_t next_desc_adr;

	pD = (USB_COMMON_DESCRIPTOR *) pDesc;
	next_desc_adr = (uint32_t) pDesc;

	while (pD->bLength) {
		/* is it interface descriptor */
		if (pD->bDescriptorType == USB_INTERFACE_DESCRIPTOR_TYPE) {

			pIntfDesc = (USB_INTERFACE_DESCRIPTOR *) pD;
			/* did we find the right interface descriptor */
			if (pIntfDesc->bInterfaceClass == intfClass) {
				break;
			}
		}
		pIntfDesc = 0;
		next_desc_adr = (uint32_t) pD + pD->bLength;
		pD = (USB_COMMON_DESCRIPTOR *) next_desc_adr;
	}

	return pIntfDesc;
}

void initCdcVcom()
{
	USBD_API_INIT_PARAM_T usb_param;
	USB_CORE_DESCS_T desc;
	ErrorCode_t ret = LPC_OK;
	/* enable clocks and pinmux */
	usb_pin_clk_init();
	/* initilize call back structures */
	memset((void *) &usb_param, 0, sizeof(USBD_API_INIT_PARAM_T));
	usb_param.usb_reg_base = LPC_USB0_BASE;
	usb_param.max_num_ep = 3;
	usb_param.mem_base = USB_STACK_MEM_BASE;
	usb_param.mem_size = USB_STACK_MEM_SIZE;

	/* Set the USB descriptors */
	desc.device_desc = (uint8_t *) &USB_DeviceDescriptor[0];
	desc.string_desc = (uint8_t *) &USB_StringDescriptor[0];
	/* Note, to pass USBCV test full-speed only devices should have both
	   descriptor arrays point to same location and device_qualifier set to 0.
	 */
	desc.high_speed_desc = (uint8_t *) &USB_FsConfigDescriptor[0];
	desc.full_speed_desc = (uint8_t *) &USB_FsConfigDescriptor[0];
	desc.device_qualifier = 0;

	/* USB Initialization */
	ret = USBD_API->hw->Init(&g_hUsb, &desc, &usb_param);
	if (ret == LPC_OK) {

		/* Init VCOM interface */
		ret = vcom_init(g_hUsb, &desc, &usb_param);
		if (ret == LPC_OK) {
			/*  enable USB interrrupts */
			NVIC_EnableIRQ(USB0_IRQn);
			/* now connect */
			USBD_API->hw->Connect(g_hUsb, 1);
		}

	}

	xVComMutex = xSemaphoreCreateMutex();
    if( xVComMutex == NULL ){
    	//while(1)
    		//vcomPrintf("err create mutex");
    }
}


char tempBufstr[300];
void vcomPrintf(const char str[])
{
	xSemaphoreTake(xVComMutex, portMAX_DELAY);

	if (vcom_connected() != 0) {

		snprintf(tempBufstr, 300, "%s> %s", pcTaskGetTaskName(xTaskGetCurrentTaskHandle()),
									str);
		vcom_write(tempBufstr, strlen(tempBufstr));
	}
	vTaskDelay(15);
	xSemaphoreGive( xVComMutex );
}

void vcomPrintfLen(const char str[], uint32_t len)
{
	if (vcom_connected() != 0) {
		vcom_write(str, len);
	}
}

void setLedLevel(int l);
//uint32_t ledLev = 0;
static uint8_t g_rxBuff[256];
void vVcomTask(void *pvParameters)
{
	uint32_t prompt = 0, rdCnt = 0;
	bool bRelayState = false;

	for(;;){
		/* Check if host has connected and opened the VCOM port */
		if ((vcom_connected() != 0) && (prompt == 0)) {
			vcomPrintf("conn\r\n");
			prompt = 1;
		}

		//Chip_UART_SendRB(LPC_USART, &txring, "AT", sizeof("AT") - 1);

		if (prompt) {
			//Chip_UART_SendRB(LPC_USART, &txring, "AT", sizeof("AT") - 1);

			//Chip_UART_SendRB(LPC_USART, &txring, "AT", sizeof("AT") - 1);
			//vTaskDelay(10);
			rdCnt = vcom_bread(&g_rxBuff[0], 256);
			if (rdCnt > 0) {
				char s = g_rxBuff[0];
				switch(s){
				case 'b':
					sprintf(g_rxBuff, "rel %d\r\n", bRelayState);

					vcomPrintf(g_rxBuff);
					if(bRelayState == true){
						turnOffRelay();
					}
					else{
						turnOnRelay();

					}
					bRelayState = !bRelayState;
					break;

				case 'c':

					break;

//				case '+':
//					ledLev++;
//					setLedLevel(ledLev);
//					sprintf(g_rxBuff, "ledLev %d\r\n", ledLev);
//					vcomPrintf(g_rxBuff);
//					break;
//				case '-':
//					ledLev--;
//					setLedLevel(ledLev);
//					sprintf(g_rxBuff, "ledLev %d\r\n", ledLev);
//					vcomPrintf(g_rxBuff);
//					break;

				case 'e':
				case 's':
				case 'p':
				case 'u':
				case '1':
				case '2':
					if(espTaskHandle != NULL){
						snprintf(g_rxBuff, 256, "try %c\r\n", s);
						vcomPrintf(g_rxBuff);
						xTaskNotify(espTaskHandle, s, eSetValueWithoutOverwrite );
					}
					else{
						//vcomPrintf("espTaskHandle NULL\r\n");
					}
					break;

				case '5':
					xTaskNotify(mainTaskHandle, 25, eSetValueWithoutOverwrite );
					break;
				case '6':
					xTaskNotify(mainTaskHandle, 11, eSetValueWithoutOverwrite );
					break;
				case '7':
					xTaskNotify(mainTaskHandle, 10, eSetValueWithoutOverwrite );
					break;

				}
				g_rxBuff[0]=0;

				//Chip_UART_SendBlocking(LPC_USART, &g_rxBuff[0], sizeof(&g_rxBuff[0]) - 1);

			}

			//vTaskDelay(10);
		}
		//Chip_UART_SendRB(LPC_USART, &txring, inst2, sizeof(inst2) - 1);
		//vTaskDelay(100);
		vTaskDelay(50);
	}
}
