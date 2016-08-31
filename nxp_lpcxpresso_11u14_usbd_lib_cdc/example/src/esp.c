#include "board.h"
#include <limits.h>
#include <stdint.h>

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "esp.h"
#include "main.h"

bool bConnected, bIsPingable, bIsTermPingable;
#define fillArr(s) AP0_##s, AP1_##s,  AP2_##s
uint8_t ssidCnt = 3;
uint8_t ssidInd = 0;
const char *ssid[] =    {fillArr(SSID)};
const char *pass[] =    {fillArr(PASS)};
const char *localIp[] = {fillArr(LOCAL_IP)};
const char *localPort[] = {fillArr(LOCAL_PORT)};
const char *hostIp[] =  {fillArr(SERVER_IP)};
SemaphoreHandle_t xEspMutex;

/* Transmit and receive ring buffers */
STATIC RINGBUFF_T txring, rxring;

/* Transmit and receive ring buffer sizes */
#define UART_SRB_SIZE 64	/* Send */
#define UART_RRB_SIZE 64	/* Receive */

/* Transmit and receive buffers */
static uint8_t rxbuff[UART_RRB_SIZE], txbuff[UART_SRB_SIZE];

SemaphoreHandle_t xUartMsgSem = NULL;
void UART_IRQHandler(void)
{
	BaseType_t xHigherPriorityTaskWoken;
	/* Want to handle any errors? Do it here. */

	/* Use default ring buffer handler. Override this with your own
	   code if you need more capability. */
	Chip_UART_IRQRBHandler(LPC_USART, &rxring, &txring);
	if(RingBuffer_GetCount(&rxring) > 0){

		xHigherPriorityTaskWoken = pdFALSE;
		xSemaphoreGiveFromISR( xUartMsgSem, &xHigherPriorityTaskWoken );
		xTaskNotifyFromISR(espTaskHandle, EVENT_RECV_BYTE_BIT, eSetBits, &xHigherPriorityTaskWoken);
	}
}



void initUart()
{
	Chip_IOCON_PinMuxSet(LPC_IOCON, 0, 18, IOCON_FUNC1 | IOCON_MODE_INACT);	/* PIO0_18 used for RXD */
	Chip_IOCON_PinMuxSet(LPC_IOCON, 0, 19, IOCON_FUNC1 | IOCON_MODE_INACT);	/* PIO0_19 used for TXD */

	/* Setup UART for 115.2K8N1 */
	Chip_UART_Init(LPC_USART);
	Chip_UART_SetBaud(LPC_USART, 115200);
	Chip_UART_ConfigData(LPC_USART, (UART_LCR_WLEN8 | UART_LCR_SBS_1BIT));
	Chip_UART_SetupFIFOS(LPC_USART, (UART_FCR_FIFO_EN | UART_FCR_TRG_LEV2));
	Chip_UART_TXEnable(LPC_USART);

	/* Before using the ring buffers, initialize them using the ring
	   buffer init function */
	RingBuffer_Init(&rxring, rxbuff, 1, UART_RRB_SIZE);
	RingBuffer_Init(&txring, txbuff, 1, UART_SRB_SIZE);

	/* Enable receive data and line status interrupt */
	Chip_UART_IntEnable(LPC_USART, (UART_IER_RBRINT | UART_IER_RLSINT));
	//Chip_UART_IntEnable(LPC_USART, UART_IER_RBRINT );

	/* preemption = 1, sub-priority = 1 */
	NVIC_SetPriority(UART0_IRQn, 1);
	NVIC_EnableIRQ(UART0_IRQn);

	/* Send initial messages */
	//Chip_UART_SendRB(LPC_USART, &txring, inst1, sizeof(inst1) - 1);
	//Chip_UART_SendRB(LPC_USART, &txring, inst2, sizeof(inst2) - 1);

}


void uartPrintf(const char str[])
{
	Chip_UART_SendRB(LPC_USART, &txring, str, strlen(str));
}


//static bool espReadString(char *recvStr, int *strLen, uint32_t to)
//{
//	char str[50];
//	int rdCnt, rdInd = 0, lastCharInd = 0;
//	recvStr[lastCharInd] = 0;
//	for (;;) {
//		if(xSemaphoreTake( xUartMsgSem, to  ) == pdFALSE)
//			return false;
//
//		rdCnt = Chip_UART_ReadRB(LPC_USART, &rxring, &(recvStr[rdInd]), 50);
//
//		//recvStr[rdInd+rdCnt] = 0;
//		//sprintf(str, " > %d %s \r\n", rdCnt, &(recvStr[rdInd]));//debug
//		//vcomPrintf(str); //debug
//
//		rdInd += rdCnt;
//		lastCharInd = (rdInd == 0) ? 0 : rdInd - 1;
//
//		if(recvStr[lastCharInd] == '\n')
//			break;
//
//		if(rdInd >= strlen("+IPD")){
//			if(memcmp(&(recvStr[0]), "+IPD", strlen("+IPD")-1) == 0){
//				vcomPrintf(" ipd detected\r\n");
//			}
//		}
//
//	}
//	recvStr[rdInd] = 0;
//	*strLen = rdInd;
//	return true;
//}

bool isConnected()
{
	return bConnected;
}

bool isServerPingable()
{
	return bIsPingable;
}

void lockEsp()
{
	xSemaphoreTake( xEspMutex, portMAX_DELAY);
}

void unLockEsp()
{
	xSemaphoreGive( xEspMutex );
}

typedef enum{
	COMMON,
	IPD
} TStringType;

static bool espReadStringByChar(char *recvStr, int *strLen, uint32_t to, TStringType *st)
{
	int rdCnt, rdInd = 0, ret = 0;
	char ch;

	char tempStr[20];

	bool isIpdSearchOn = true;
	bool isIpdConfirm = false;
	bool isIpdParsed = false;
	int ipdDetectStrLen = strlen("+IPD,0,2:");
	int ipdPrefixLen = 0;
	int payLoadLen = 0;
	char ipdTempStr[10];

	*st = COMMON;

	for (;;) {
		//vcomPrintf("11\r\n");
		if(RingBuffer_IsEmpty(&rxring) == 1){
			if(xSemaphoreTake( xUartMsgSem, to  ) == pdFALSE)
				return false;
		}
		//vcomPrintf("12\r\n");
		for(;;){
			ret = RingBuffer_Pop(&rxring, &ch);
			if(ret == 1){
				recvStr[rdInd++] = ch;
			}
			else{
				recvStr[rdInd] = 0;
				*strLen = rdInd;
				break;
			}
			if(isIpdConfirm == false){
				if(ch == '\n'){
					recvStr[rdInd] = 0;
					*strLen = rdInd;
					return true;
				}
			}
			else if(isIpdParsed == true){
				if(rdInd == ipdDetectStrLen){
					memcpy(recvStr,
							&(recvStr[ipdPrefixLen]),
							payLoadLen);
					recvStr[payLoadLen] = 0;
					*strLen = ipdDetectStrLen;
					*st = IPD;
					return true;
				}

			}

			if(isIpdSearchOn == true){
				if(isIpdConfirm == false){
					if(recvStr[0] == '+'){
						if(rdInd >= 4){ //when recv "+IPD"
							if(memcmp(recvStr, "+IPD", 4) == 0){
								isIpdConfirm = true;
								//vcomPrintf(" ipd detected\r\n");
							}
							else{
								isIpdSearchOn = false;
							}
						}
					}
					else{
						isIpdSearchOn = false;
					}
				}
				else{
					if(rdInd > 9){ //when recv "+IPD,0,2:"
						//vcomPrintf(" found\r\n");
						char *pch=memchr(recvStr, ':', rdInd);
						if(pch != NULL){
							ipdPrefixLen = pch-recvStr + 1;
							memcpy(ipdTempStr, recvStr, ipdPrefixLen);

							pch = strtok(ipdTempStr, ",:");
							for(int tokInd=0;pch != NULL;tokInd++){
								if(tokInd == 1){
									payLoadLen = atoi(pch);
									ipdDetectStrLen = payLoadLen + ipdPrefixLen;
									//sprintf(tempStr, " len %d %d \r\n", payLoadLen, ipdDetectStrLen);
									//vcomPrintf(tempStr);
									break;
								}
								pch = strtok(NULL, ",:");
							}
							isIpdParsed = true;
						}
						else{
							//vcomPrintf(" no ipd detected\r\n");
						}
						isIpdSearchOn = false;
					}
				}
			}

		}
		//vcomPrintfLen(recvStr, *strLen);
	}
//
//
//	}

	return true;
}

static bool waitForEspAnswerToBuf(const char *answStr, uint32_t toTicks, bool bTrace)
{
	char recvStr[150], str[150];
	int strLen;
	TStringType strType;

	if(espReadStringByChar(recvStr, &strLen, toTicks, &strType) == false){
		return false;
	}
	if(bTrace == true){
		snprintf(str, 150, "string> %d:%s", strLen, recvStr);
		vcomPrintf(str);
	}
	strcpy(answStr, recvStr);

	return true;
}

static bool waitForEspAnswerString(const char *answStr, uint32_t toTicks, bool bTrace)
{
	char recvStr[150], str[150];
	int strLen;
	int answStrLen = strlen(answStr);
	TStringType strType = COMMON;
	while(1){
		//vcomPrintf("1\r\n");
		 if(espReadStringByChar(recvStr, &strLen, toTicks, &strType) == false){
			 return false;
		 }

		//vcomPrintf("2\r\n");
		 if(bTrace == true){
			 snprintf(str, 150, "string> %s", recvStr);
			 vcomPrintf(str);
		 }

		 //vcomPrintf("3\r\n");
		if(strLen >= answStrLen){
			//vcomPrintf(" string> expected Ok\r\n");
			if(strcmp(&(recvStr[strLen-answStrLen]), answStr) == 0){
				//vcomPrintf(" Ok detected\r\n");
				break;
			}

		}

	}
	return true;
}

static bool waitForEspAnswerOk(uint32_t toTicks, bool bTrace)
{
	return waitForEspAnswerString("OK\r\n", toTicks, bTrace);
}




bool espSendCommand(const char *ip, const char * port, const char *cmdStr, uint32_t to)
{
	char str[50];
	sprintf(str, "AT+CIPSTART=\"TCP\",\"%s\",%s\r\n", ip, port);
	vcomPrintf(str);
	uartPrintf(str);
	if(waitForEspAnswerString("OK\r\n", 10000, false) == false){
		vcomPrintf("CIPSTART to\r\n");
		return false;
	}

	//vcomPrintf("try to send \r\n");
	//vcomPrintf(cmdStr);
	//vcomPrintf("\r\n");

	uartPrintf("AT+CIPSEND=4\r\n");
	vTaskDelay(50);
	uartPrintf(cmdStr);
	if(waitForEspAnswerString("SEND OK\r\n", 10000, false) == false){
		vcomPrintf("CIPSEND  to\r\n");
		return false;;
	}

	uartPrintf("AT+CIPCLOSE\r\n");
	if(waitForEspAnswerString("OK\r\n", 1000, false) == false){
		vcomPrintf("CIPCLOSE  to\r\n");
		return false;
	}

	vcomPrintf("exchange OK\r\n");
	return true;
}

bool espSend(const char *cmd)
{
	bool ret = true;
//	if(bConnected == false){
//		vcomPrintf("no connection\r\n");
//		return false;
//	}
	//vcomPrintf("try lock esp\r\n");
	//xTaskNotify(espTaskHandle, '1', eSetValueWithoutOverwrite );
	//lockEsp();

	char str[50];
	sprintf(str, "AT+CIPSEND=%d\r\n", strlen(cmd));
	uartPrintf(str);
	vTaskDelay(50);
	uartPrintf(cmd);
	if(waitForEspAnswerString("SEND OK\r\n", 10000, false) == false){
		vcomPrintf("CIPSEND  to\r\n");
		return false;
	}

//	if(espSendCommand(hostIp[ssidInd], serverPort, cmd, 10000) == false){
//		vcomPrintf("send error\r\n");
//		ret = false;
//	}
	//unLockEsp();
	return ret;
}

bool waitCommand(char *dataBuf, uint32_t to)
{
	bool ret = true;

	if(bConnected == false){
		vcomPrintf("no connection\r\n");
		return false;
	}
	int strLen;
	TStringType strType;

	lockEsp();

	//vcomPrintf("AT+CIPMUX=1\r\n");
	uartPrintf("AT+CIPMUX=1\r\n");
	if(waitForEspAnswerString("OK\r\n", 1000, false) == false){
		vcomPrintf("no esanswer on AT+CIPMUX=1\r\n");
		//return false;
	}
	//vcomPrintf("AT+CIPSERVER=1,"localPort"\r\n");
	char str[100];
	sprintf(str, "AT+CIPSERVER=1,%s\r\n",localPort[ssidInd]);
	uartPrintf(str);
	if(waitForEspAnswerString("OK\r\n", 1000, false) == false){
		sprintf(str, "no answer on  AT+CIPSERVER=1,%s\r\n",localPort[ssidInd]);
		vcomPrintf(str);
		//return false;
	}

	//vcomPrintf("wait for end command\r\n");

	for(;;){
		if(espReadStringByChar(dataBuf, &strLen, to, &strType) == false){
			vcomPrintf("no answer to \r\n");
			ret = false;
			break;
		}
		else{
			if(strType == IPD){
				break;
			}
		}
	}

	//vcomPrintf("\r\nAT+CIPCLOSE=0\r\n");
	uartPrintf("AT+CIPCLOSE=0\r\n");
	if(waitForEspAnswerString("OK\r\n", 1000, false) == false){
		vcomPrintf("no answer on CIPCLOSE\r\n");
	}

	//vcomPrintf("AT+CIPSERVER=0\r\n");
	uartPrintf("AT+CIPSERVER=0\r\n");
	if(waitForEspAnswerString("OK\r\n", 1000, false) == false){
		vcomPrintf("no answer  \r\n");
		//return false;
	}

	//vcomPrintf("AT+CIPMUX=0\r\n");
	uartPrintf("AT+CIPMUX=0\r\n");
	if(waitForEspAnswerString("OK\r\n", 1000, false) == false){
		vcomPrintf("no answer on AT+CIPMUX=0\r\n");
		//return false;
	}

	unLockEsp();

	return ret;
}

bool sendAT()
{
	bool ret = false;
	uartPrintf("AT\r\n");
	//vTaskDelay(10);
	if(waitForEspAnswerOk(300, false) == false){
		vcomPrintf("no AT answer\r\n");
		ret = false;
	}
	else{
		//vcomPrintf("vEspTask> AT OK\r\n");
		ret = true;
	}
	return ret;
}

bool isWifiConnected()
{
	bool ret = false;
	char str[50];
	uartPrintf("AT+CWJAP_CUR?\r\n");

	if(waitForEspAnswerToBuf(str, 500, false) == false){
		//vcomPrintf("no AT+CWJAP_CUR? answer\r\n");
		ret = false;
	}
	else{
		vcomPrintf(str);
		if(waitForEspAnswerString("OK\r\n", 1000, false) == false){
			//vcomPrintf("no OK answer\r\n");
			ret = false;
		}
		else{
			if(strcmp(str, "OK\r\n") == 0){
				ret = true;
			}
			else if(strcmp(str, "ERROR\r\n") == 0){
				ret = false;
			}
			else if(strcmp(str, "No AP\r\n") == 0){
				//vcomPrintf("No AP detected\r\n");
				ret = false;
			}
			else if(strncmp(str, "+CWJAP_CUR:", strlen("+CWJAP_CUR:")) == 0){
				//vcomPrintf("connected\r\n");
				ret = true;
			}
			else{
				//vcomPrintf("unknown answer\r\n");
				ret = false;
			}
		}
	}

	return ret;
}

bool connectToAp(const char *ssid, const char *pass)
{
	//vcomPrintf("AT+CWJAP_CUR=\""ssid"\",\""pass"\"\r\n");
	bool ret = false;
	char buf[70];
	sprintf(buf, "AT+CWJAP_CUR=\"%s\",\"%s\"\r\n", ssid, pass);
	//vcomPrintf(buf);
	uartPrintf(buf);

	for(;;){
		if(waitForEspAnswerToBuf(buf, 15000, false) == false){
			//vcomPrintf("no AT+CWJAP_CUR? answer\r\n");
			ret = false;
			break;
		}

		if(strcmp(buf, "OK\r\n") == 0){
			//vcomPrintf("detected OK\r\n");
			break;
		}
		else if(strcmp(buf, "FAIL\r\n") == 0){
			//vcomPrintf("detected FAIL\r\n");
			ret = false;
			break;
		}
		else if(strcmp(buf, "WIFI CONNECTED\r\n") == 0){
			//vcomPrintf("detected wifi connected\r\n");
			ret = true;
		}
		else if(strcmp(buf, "WIFI GOT IP\r\n") == 0){
			//vcomPrintf("detected got ip\r\n");
			ret = true;
		}
		else{
			//vcomPrintf(buf);
		}
	}


//	if(waitForEspAnswerOk(20) == false){
//		vcomPrintf("no answer\r\n");
//	}
//	else{
//		sprintf(buf, "connected to \"%s\"\r\n", ssid);
//		vcomPrintf(buf);
//	}
	return ret;

}

bool setLocalIp(const char *ip)
{
	char str[100];
	bool ret = false;
	sprintf(str, "AT+CIPSTA_CUR=\"%s\",\"192.168.0.1\",\"255.255.255.0\"\r\n", ip);
	vcomPrintf(str);
	uartPrintf(str);
	if(waitForEspAnswerOk(3000, true) == false){
		//vcomPrintf("no answer on AT+CIPSTA_CUR="localIP"\r\n");
		ret = false;
	}
	else
		ret = true;

	return ret;
}

bool getApList()
{
	bool ret = false;
	char buf[70];
	//vcomPrintf(buf);
	vcomPrintf("get AP list\r\n");
	uartPrintf("AT+CWLAP\r\n");

	vcomPrintf("check to lazy OK\r\n");
	if(waitForEspAnswerOk(20000, true) == false){
		vcomPrintf(" --- false\r\n");
	}

}

bool connectApList()
{
	bool ret = false;
	char str[100];
	for(int i=0; i<ssidCnt; i++){
		sprintf(str, "try to connect to \"%s\":\"%s\" with ip %s\r\n", ssid[i], pass[i], localIp[i]);
		vcomPrintf(str);
		if(setLocalIp(localIp[i]) == false){
			ret = false;
			break;
		}

		if(connectToAp(ssid[i], pass[i]) == true){
			vcomPrintf("success\r\n");
			ssidInd = i;
			ret = true;
			bConnected = true;
			break;
		}
	}
	return ret;
}

bool pingHost(const char *host)
{
	//vcomPrintf("AT+CWJAP_CUR=\""ssid"\",\""pass"\"\r\n");
	bool ret = false;
	char buf[70];
	char outBuf[70];
	sprintf(outBuf, "AT+PING=\"%s\"", host);
	vcomPrintf(buf);
	sprintf(buf, "AT+PING=\"%s\"\r\n", host);
	uartPrintf(buf);

	for (;;) {
		if (waitForEspAnswerToBuf(buf, 25000, false) == false) {
			//vcomPrintf("no AT+CWJAP_CUR? answer\r\n");
			ret = false;
			break;
		}

		if (strcmp(buf, "OK\r\n") == 0) {
			//vcomPrintf("detected OK\r\n");
			ret = true;
			break;
		}
		else if (strcmp(buf, "ERROR\r\n") == 0) {
			//vcomPrintf("detected ERROR\r\n");
			ret = false;
			break;
		}
		else {
			if(strcmp(buf, "\r\n") != 0){
				sprintf(outBuf, "AT+PING=\"%s\"  %s", host, buf);
				vcomPrintf(outBuf);
			}
			//else{
				//vcomPrintf(buf);
			//}

		}
	}
	//vcomPrintf("\r\n");
	return ret;
}

bool connectToTcpHost(const char *host)
{
	bool ret = false;
	char buf[100];
	sprintf(buf, "AT+CIPSTART=\"TCP\",\"%s\",23\r\n", host);
	vcomPrintf(buf);
	uartPrintf(buf);
	for (;;) {
		if (waitForEspAnswerToBuf(buf, 25000, true) == false) {
			//vcomPrintf("no AT+CWJAP_CUR? answer\r\n");
			ret = false;
			break;
		}

		if (strcmp(buf, "OK\r\n") == 0) {
			//vcomPrintf("detected OK\r\n");
			ret = true;
			break;
		}
		else if (strcmp(buf, "ERROR\r\n") == 0) {
			//vcomPrintf("detected ERROR\r\n");
			ret = false;
			break;
		}
		else if (strcmp(buf, "ALREADY CONNECTED\r\n") == 0) {
			ret = true;
			break;
		}
	}
	//vcomPrintf("\r\n");
	return ret;

}

bool pingTerminal()
{
	bool ret = true;
	char buf[70];
	vcomPrintf("try lock esp\r\n");
	//xTaskNotify(espTaskHandle, '1', eSetValueWithoutOverwrite );

	uartPrintf("AT+CIPSEND=6\r\n");
	vTaskDelay(200);
	uartPrintf("ping\r\n");
	if(waitForEspAnswerString("SEND OK\r\n", 10000, true) == false){
		vcomPrintf("CIPSEND  to\r\n");
		return false;
	}


	while (waitForEspAnswerToBuf(buf, 25000, true) == true) {
		vcomPrintf("pong wait TO\r\n");
	}
	return false;

	if (strcmp(buf, "pong\r\n") == 0) {
		vcomPrintf(" \"pong\" detected\r\n");
		return true;
	}
	else {
		//vcomPrintf("detected ERROR\r\n");

		strcat(buf, "\r\n");
		vcomPrintf(buf);

		return false;
	}
	return ret;
}


bool checkIPStatus()
{
	bool ret = false;
	char buf[70];
	//vcomPrintf("try lock esp\r\n");
	//xTaskNotify(espTaskHandle, '1', eSetValueWithoutOverwrite );

	uartPrintf("AT+CIPSTATUS\r\n");

	//result:
	//2 - got IP
	//3 - connected
	//4 - disconnected

	while (waitForEspAnswerToBuf(buf, 100, false) == true) {
		//vcomPrintf("AT+CIPSTATUS ret wait fail\r\n");
		if (strcmp(buf, "STATUS:2\r\n") == 0){
			vcomPrintf("IP status GOT IP\r\n");
			ret = false;
		}
		if (strcmp(buf, "STATUS:3\r\n") == 0){
			vcomPrintf("IP status CONNECTED\r\n");
			ret = true;
		}
		else if(strcmp(buf, "STATUS:4\r\n") == 0){
			vcomPrintf("IP status DISCONNECTED\r\n");
			ret = false;
		}
		else{
			//vcomPrintf(buf);
		}
	}
	return ret;
}

void vEspTask(void *pvParameters)
{

	bool ret = false;
	uint32_t ulNotifiedValue;
	xEspMutex = xSemaphoreCreateMutex();
	xUartMsgSem = xSemaphoreCreateBinary();
    if( xUartMsgSem == NULL ){
    	//while(1)
    		//vcomPrintf("err create binary sem, xUartMsgSem");
    }

	lockEsp();

	initUart();

	vTaskDelay(2000);
	uartPrintf("ATE0\r\n");
	if(waitForEspAnswerOk(300, false) == false){
		vcomPrintf("ATE0 no answer\r\n");
	}

	vcomPrintf("check to lazy OK\r\n");
	if(waitForEspAnswerOk(2000, true) == false){
		vcomPrintf(" --- false\r\n");
	}

	vcomPrintf("check to lazy OK\r\n");
	if(waitForEspAnswerOk(2000, true) == false){
		vcomPrintf(" --- false\r\n");
	}

	vcomPrintf("check to lazy OK\r\n");
	if(waitForEspAnswerOk(2000, true) == false){
		vcomPrintf(" --- false\r\n");
	}

	uartPrintf("AT+GMR\r\n");
	if(waitForEspAnswerOk(3000, true) == false){
		vcomPrintf("no answer on AT+GMR\r\n");
		//vTaskDelay(portMAX_DELAY);
	}

	vcomPrintf("configure CWMODE \r\n");
	uartPrintf("AT+CWMODE_CUR=1\r\n");
	if(waitForEspAnswerOk(3000, false) == false){
		vcomPrintf("no answer on AT+CWMODE_CUR=1\r\n");
		//vTaskDelay(portMAX_DELAY);
	}

	vcomPrintf("disable dhcp\r\n");
	uartPrintf("AT+CWDHCP_CUR=1,0\r\n");
	if(waitForEspAnswerOk(3000, false) == false){
		//vcomPrintf("no answer on AT+CWDHCP_CUR=1,0\r\n");
	}

	vcomPrintf("Check connection to wifi\r\n");
	if(isWifiConnected() == false)
		vcomPrintf("no wifi conn\r\n");
	else
		vcomPrintf("wifi connected\r\n");

	getApList();
//	vcomPrintf("vEspTask> disable dhcp\r\n");
//	uartPrintf("AT+CWDHCP_CUR=1,0\r\n");
//	if(waitForEspAnswerOk(3000, true) == false){
//		vcomPrintf("vEspTask> no esp answer on AT+CWDHCP=1,0\r\n");
//	}

	//vTaskDelay(4000);

	//unLockEsp();

	char waitForCmdStr[] = "wait cmd\r\n";
	vcomPrintf(waitForCmdStr);

//	for(;;){
//		vTaskDelay(50);
//	}


checkAt:
	vcomPrintf("Check AT\r\n");
	while(sendAT() == false){
		vcomPrintf("ESP fail\r\n");
		vTaskDelay(50);
	}
	vcomPrintf("ESP OK\r\n");
	vTaskDelay(50);

	if(isWifiConnected() == false){
		vcomPrintf("wifi not connected. connecting\r\n");
		if(connectApList() == false){
			goto checkAt;
		}
	}
	vTaskDelay(50);

	vcomPrintf("wifi connected. try ping\r\n");

	bool bPingRes = false;
checkPing:
	for(int i=0; i<5;i++){
		bPingRes |= pingHost(hostIp[ssidInd]);
		vTaskDelay(250);
	}

	char str[200];
	if(bPingRes == false){
		sprintf(str, "no ping to server IP %s\r\n", hostIp[ssidInd]);
		vcomPrintf(str);
		bIsPingable = false;
		goto checkAt;
	}

	bIsPingable = true;

	sprintf(str, "try to connect to server IP %s:23\r\n", hostIp[ssidInd]);
	vcomPrintf(str);

checkConnect:
	bConnected = false;
	if(connectToTcpHost(hostIp[ssidInd]) == true){
		vcomPrintf("terminal connected\r\n");
	}
	else{
		vcomPrintf("terminal connect error\r\n");
		goto checkPing;
	}

	if(espSend("wo\r\n") == true){
		vcomPrintf("wo send OK\r\n");
	}
	else{
		vcomPrintf("wo send FAIL\r\n");
		goto checkConnect;
	}

	if(checkIPStatus() == false)
		goto checkConnect;
	vcomPrintf("check IP status OK\r\n");
	bConnected = true;

	//while(checkIPStatus() == true){
	while(true){
		if(xTaskNotifyWait( ULONG_MAX, ULONG_MAX, &ulNotifiedValue, 250 ) == true){
			//sprintf(str, "0x%x\r\n", ulNotifiedValue);
			//vcomPrintf(str);
			if((ulNotifiedValue&EVENT_BUTTON_1_BIT) != 0){ //warm up
				//vcomPrintf("event but1\r\n");
				if(espSend("but1\r\n") == true){
					vcomPrintf("\"but1\" send OK\r\n");
				}
				else{
					vcomPrintf("\"but1\" send FAIL\r\n");
				}
			}
			else if((ulNotifiedValue&EVENT_BUTTON_2_BIT) != 0){
				//vcomPrintf("event but2\r\n");
				if(espSend("but2\r\n") == true){
					vcomPrintf("\"but2\" send OK\r\n");
				}
				else{
					vcomPrintf("\"but2\" send FAIL\r\n");
					goto checkConnect;
				}
			}
			else if((ulNotifiedValue&EVENT_BUTTON_CANCEL_BIT) != 0){
				//vcomPrintf("event but cancel\r\n");
				if(espSend("cancel\r\n") == true){
					vcomPrintf("\"cancel\" send OK\r\n");
				}
				else{
					vcomPrintf("\"cancel\" send FAIL\r\n");
					goto checkConnect;
				}
			}
			else if((ulNotifiedValue&EVENT_RECV_BYTE_BIT) != 0){
				while(waitForEspAnswerToBuf(str, 300, false) == true){
					vcomPrintf(str);
					if (strcmp(str, "CLOSED\r\n") == 0){
						goto checkConnect;
					}
					else if(strcmp(str, "light ON\r\n") == 0){
						vcomPrintf("\"light ON\" detected\r\n");
						xTaskNotify(mainTaskHandle, EVENT_LIGHTON_CMD, eSetBits);
					}
					else if(strcmp(str, "light OFF\r\n") == 0){
						vcomPrintf("\"light OFF\" detected\r\n");
						xTaskNotify(mainTaskHandle, EVENT_LIGHTOFF_CMD, eSetBits );
					}
				}
				/*else{
					vcomPrintf("rcv byte ev, but no data rcvd\r\n");
				}*/

			}
		}
//		while(waitForEspAnswerToBuf(str, 250, true) == true){
//			vcomPrintf(str);
//			if (strcmp(str, "CLOSED\r\n") == 0){
//				goto checkConnect;
//			}
//		}
	}
	bConnected = false;
	vcomPrintf("check IP status fail\r\n");


	goto checkAt;



	for(;;){
		if(xTaskNotifyWait( 0x00, ULONG_MAX, &ulNotifiedValue,  1000 ) == true){
			lockEsp();
			switch((char)ulNotifiedValue){
				case 't':
					uartPrintf("AT\r\n");
					//readEspAnsOk();
					break;
//				case 'k':
//					uartPrintf("AT+CIPSTATUS\r\n");
//					break;
				case 'e':
					vcomPrintf("try to connect to 192.168.0.103:23\r\n");
					uartPrintf("AT+CIPSTART=\"TCP\",\"192.168.0.103\",23\r\n");
					if(waitForEspAnswerOk(10000, true) == false){
						vcomPrintf("no answer\r\n");
						break;
					}
					break;
//				case 'o':
//					uartPrintf("AT+CIPSEND=4\r\n2345\r\n");
//					break;
//				case 's':
//
//					vcomPrintf("start serv\r\n");
//					waitCommand(1000000);
//					break;
				case 'p':
					//uartPrintf("AT+CIFSR\r\n");
					vcomPrintf("AT+CIPSEND ping\r\n");
					uartPrintf("AT+CIPSEND=4\r\nping\r\n");
					if(waitForEspAnswerOk(10000, true) == false){
						vcomPrintf("no answer\r\n");
						break;
					}
					break;
//				case 'c':
//					vcomPrintf("try to connect to TL-WR842ND\r\n");
//					//uartPrintf("AT+CWJAP=\"Polden\",\"studio123\"\r\n");
//					uartPrintf("AT+CWJAP=\"TL-WR842ND\",\"kkkknnnn\"\r\n");
//					if(waitForEspAnswerOk(10000, false) == false){
//						vcomPrintf("no answer\r\n");
//						break;
//					}
//					vcomPrintf("connected to TL-WR842ND\r\n");
//
//					break;
//				case 'u':
//					uartPrintf("AT+CIUPDATE\r\n");
//					if(waitForEspAnswerOk(100000, true) == false){
//						vcomPrintf("no answer on AT+CIPUPDATE\r\n");
//						break;
//					}
//					break;

//				case 'l':
//					uartPrintf("AT+CWLAP\r\n");
//					break;
//				case 'v':
//					uartPrintf("AT+GMR\r\n");
//					break;
//				case '0':
//					uartPrintf("AT+CWMODE_CUR\r\n");
//					break;
				case '1':
//					if(sendCommand("1234") == true){
//						vcomPrintf("start serv with 3 min timeout\r\n");
//						waitCommand(3*60*1000);
//					}
//					else{
//						if(mainTaskHandle != NULL){
//							xTaskNotify(mainTaskHandle, EVENT_ESP_ERR_BIT, eSetBits);
//						}
//					}
					break;
				case '2':
					espSendCommand(hostIp[ssidInd], serverPort, "2345", 10000);
//					strcpy(&g_rxBuff[0], "AT+CIPSERVER=1,1488\r\n");
//					uartPrintf("AT+CIPSERVER=1,1488\r\n");
					break;

				default:
					uartPrintf("AT\r\n");
			}
			unLockEsp();
			vcomPrintf(waitForCmdStr);
		}
		else{
			lockEsp();

			if(bConnected == true){
				//vcomPrintf("try ping server\r\n");
//				if(pingTerminal() == false){
//					vcomPrintf("ping terminal fail\r\n");

//					if(pingHost(hostIp[ssidInd]) == false){
//						bIsPingable = false;
//						vcomPrintf("no ping to host\r\n");
//						if(isWifiConnected() == false){
//							vcomPrintf("wifi not connected. Check ESP\r\n");
//							bConnected = false;
//							if(sendAT() == false){
//								vcomPrintf("ESP fail\r\n");
//							}
//							else{
//								vcomPrintf("ESP OK\r\n");
//							}
//						}
//					}
//					else{
//						bIsPingable = true;
//					}
//				}
//				else{
//					vcomPrintf("ping terminal ok\r\n");
//				}
			}
			else{
				vcomPrintf("try to connect to wifi\r\n");

				ret = false;
				ret = connectApList();
				if(ret == true){
					//vcomPrintf("connected to\r\n");

					vcomPrintf("try to connect to 192.168.0.103:23\r\n");
					uartPrintf("AT+CIPSTART=\"TCP\",\"192.168.0.103\",23\r\n");
					if(waitForEspAnswerOk(10000, true) == true){
						vcomPrintf("terminal connected\r\n");
					}
					else{
						vcomPrintf("no answer\r\n");
						//break;
					}
				}
				else{
					vcomPrintf("fail. Check ESP\r\n");
					if(sendAT() == true){
						vcomPrintf("ESP OK\r\n");
					}
					else{
						vcomPrintf("no AT answer\r\n");
					}
				}
			}

			unLockEsp();
		}
	}
}
