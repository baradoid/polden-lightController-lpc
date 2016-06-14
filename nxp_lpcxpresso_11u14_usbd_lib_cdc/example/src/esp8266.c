#include <stdlib.h>
#include "esp8266.h"
#include "uartUtils.h"
#include "utils.h"
#include "httpService.h"
#include <cr_section_macros.h>

//#define DEBUGPRINTF

TWifiAp wifiApList[WIFI_APLISTMAX];

char APIP[20], STAPIP[20];

TCmdType parseCIFSR(char *str)
{
	debugPrintf(str);
	//debugPrintf("parseCIFSR\r\n");
	TCmdType cmdType = UNKNWON;
	if(MEMCMPx(str, "+CIFSR:APIP") == 0){
		//debugPrintf("!!! +APIP !!! ");
		strcpy(APIP, &(str[12]));
		cmdType = CIFSR_APIP;
	}
	else if(MEMCMPx(str, "+CIFSR:APMAC") == 0){
		//debugPrintf("!!! +CIFSR:APMAC !!! ");
		cmdType = CIFSR_APMAC;
	}
	else if(MEMCMPx(str, "+CIFSR:STAIP") == 0){
		//debugPrintf("!!! +CIFSR:STAIP !!! ");
		strcpy(STAPIP, &(str[13]));
		cmdType = CIFSR_STAIP;
	}
	else if(MEMCMPx(str, "+CIFSR:STAMAC") == 0){
	//	debugPrintf("!!! +CIFSR:STAMAC !!! ");
		cmdType = CIFSR_STAMAC;
	}
	return cmdType;
}

uint8_t wifiApInd = 0;
void resetApList()
{
	wifiApInd = 0;
}
void parseCWLAP(char *str)
{
	char *pch = strtok (str, ":(\"");
	TWifiAp *carWifiAap = &(wifiApList[wifiApInd]);
	uint8_t i;
	for(i=0; pch != NULL; i++){
		//debugPrintf(pch);
		//debugPrintf("\r\n");
		switch(i){
		case 2:
			//pch[strlen(pch)]='\0';
			memcpy(&(carWifiAap->name[0]), pch+1, MIN(10, strlen(pch)-2)); //need debug
			//strcpy(&(carWifiAap->name[0]), pch+1);
			break;
		case 3:
			carWifiAap->rssi = atoi(pch);
			break;
		default:
			break;
		}
		pch = strtok (NULL, ",()");
	}
	wifiApInd++;
}

void scanWiFiAp()
{
	wifiPrintf("AT+CWLAP\r\n");
	waitForRespOK();
}

__RODATA(text) static char msgParseIPD[] = "parseIPD \r\n";
__RODATA(text) static char msgMsgLen[] = "msg length:";
__RODATA(text) static char msgMsgSpace[] = "  ";
__RODATA(text) static char msgRN[] = "\r\n";

__RAM_FUNC uint16_t parseIPD(char *str, uint8_t *curConnInd, int16_t *msgLen)
{
	uint8_t i;
	char *pch, *msg, key[] = ",:";
	uint16_t tailLen = 0;
	//debugPrintf(msgParseIPD);
	//debugPrintf(str);
	pch = strpbrk (str, key);
	for(i=0; (pch != NULL)||(i<4); i++){
		pch++;
		if(i == 0){
			*curConnInd = (uint8_t)atoi(pch);
		}
		else if(i == 1){
			char numToStr[10];
			*msgLen = atoi(pch);
			/*debugPrintf(msgMsgLen);
			debugPrintf(msgMsgSpace);
			itoa(tailLen, numToStr, 10);
			debugPrintf(numToStr);
			debugPrintf(msgMsgSpace);
			debugPrintf(pch);
			debugPrintf(msgMsgSpace);*/
			//debugPrintf("\r\n");

			//debugPrintf(pch);
			//debugPrintf("\r\n");
		}
		else if(i == 2){
			tailLen = strlen(pch);
			msg = pch;

			/*debugPrintf(" tail:\"");
			debugPrintf(msg);
			debugPrintf("\"  ");*/

			//itoa(strlen(pch), tailLen, 10);
			//atoi(pch);
			//debugPrintf(pch);
			//debugPrintf(" => ");
			///debugPrintf(numToStr);
			//debugPrintf("\r\n");

			break;
		}
		pch = strpbrk (pch, key);
	}
	memcpy(str, msg, tailLen);
	str[tailLen] = '\0';

	*msgLen -= tailLen;

	//debugPrintf(msgRN);

	/*char numToStr[10];
	//debugPrintf("parseIPD ");
	debugPrintf("conNum ");
	itoa(curConnInd, numToStr, 10);
	debugPrintf(numToStr);

	debugPrintf(" start recv msg with length ");
	itoa(*msgLen, numToStr, 10);
	debugPrintf(numToStr);
	debugPrintf(" and ");
	itoa(tailLen, numToStr, 10);
	debugPrintf(numToStr);
	debugPrintf(" chars already recvd\r\n");*/
	return tailLen;
}


void getNextWifiCmd(TCmd *cmd, int16_t to_msec)
{
	char recvBuf[BUF_LEN];
	getNextWifiCmdExtBuf(recvBuf, cmd, to_msec);
}

//#define DEBUGPRINTF
void getNextWifiCmdExtBuf(char *recvBuf, TCmd *cmd, int16_t to_msec)
{
	cmd->type = UNKNWON;

	uint16_t msgLen = recvWifiMsgTO(recvBuf, to_msec);
#ifdef DEBUGPRINTF
	if(msgLen == -1)
		debugPrintf("timeout!\r\n");
	else{
		debugPrintf(" esp-> ");
		debugPrintflen(recvBuf, msgLen);
	}
#endif
	if(msgLen != -1){
		if(recvBuf[0] == '+'){
			if(MEMCMPx(recvBuf, "+IPD") == 0){

#ifdef DEBUGPRINTF
				debugPrintf(" esp-> !!! +IPD !!!\r\n");
#endif
				int16_t msgLength = 0;
				//void parseIPD(char *str, char **msg, uint8_t &curConnInd, int16_t *msgLen);

				parseIPD(recvBuf, &(cmd->curConnInd), &msgLength);
				/*debugPrintf(" esp-> !!! ");
				debugPrintf(msg);
				debugPrintf(" !!!  \r\n");*/

				//cmd.charsInHead = strlen(msg);
				parseHttpReq(recvBuf, &(cmd->htmlReqType));

				uint8_t strInd = 0;
				while(msgLength > 0){

					uint16_t wifiMsgLen = 0;

					//debugPrintf(" get 1\r\n ");
					if(cmd->htmlReqType == POST){
						//debugPrintf(" get 2\r\n ");
						wifiMsgLen = recvWifiMsg(recvBuf);
						//debugPrintf(" get 3\r\n ");
						parsePostReqHead(recvBuf, cmd, wifiMsgLen, strInd);
						//debugPrintf(" get 4\r\n ");
						strInd++;
					}
					else{
						//debugPrintf(" get 5\r\n ");
						wifiMsgLen = waitWifiMsgAndStartRecv();
					}

					//debugPrintf(" get 2\r\n ");
					msgLength -= wifiMsgLen;

					char numToStr[10];
					debugPrintf(" esp =>  ");
					itoa(wifiMsgLen, numToStr, 10);
					debugPrintf(numToStr);
					debugPrintf(" rn ");
					itoa(msgLength, numToStr, 10);
					debugPrintf(numToStr);
					debugPrintf(" chs\r\n");
				}
#ifdef DEBUGPRINTF
				debugPrintf(" esp-> message recvd! \r\n");
#endif
				cmd->type = IPD;
			}
			else if(MEMCMPx(recvBuf, "+CIFSR") == 0){
				cmd->type = parseCIFSR(recvBuf);
			}
			else if(MEMCMPx(recvBuf, "+CWLAP") == 0){
				debugPrintf(" parse CWLAP\r\n");
				debugPrintf(recvBuf);
				parseCWLAP(recvBuf);
				debugPrintf(" parse CWLAP OK\r\n");
			}
			else if(MEMCMPx(recvBuf, "+CIPSTO") == 0)
				cmd->type = CIPSTO;
		}
		else if(strlen(recvBuf)==4){
			if(strcmp(recvBuf, "OK\r\n") == 0)
				cmd->type = CMD_OK;
		}
		else if(strcmp(recvBuf, "ERROR\r\n") == 0)
			cmd->type = RESP_ERROR;
		else if(strlen(recvBuf)==11){
			if(strcmp(recvBuf, "busy s...\r\n") == 0)
				cmd->type = busy_s;
		}
		else if(strcmp(recvBuf+1, ",CONNECT\r\n") == 0){
			//debugPrintf("!!!CONNECT!!!\r\n");
			cmd->type = CMD_CONNECT;

		}
		else if(strcmp(recvBuf+1, ",CLOSED\r\n") == 0){
			//debugPrintf("!!!CLOSED!!!\r\n");
			cmd->type = CMD_CLOSED;
		}
		else if(strcmp(recvBuf, "SEND OK\r\n") == 0){
#ifdef DEBUGPRINTF
			debugPrintf("!!!RESP_SEND_OK!!!\r\n");
#endif
			cmd->type = RESP_SEND_OK;
		}
		else if(strcmp(recvBuf, "> \r\n") == 0){
#ifdef DEBUGPRINTF
			debugPrintf("!!!SEND_READY!!!\r\n");
#endif
			cmd->type = SEND_READY;
		}
		else if(strcmp(recvBuf, "WIFI GOT IP\r\n") == 0){
			//debugPrintf("!!!WIFI GOT IP!!!\r\n");
			cmd->type = wifi_gotip;
		}
		else if(strcmp(recvBuf, "WIFI DISCONNECT\r\n") == 0){
			cmd->type = wifi_discon;
		}
		else if(strcmp(recvBuf, "FAIL\r\n") == 0){
#ifdef DEBUGPRINTF
			debugPrintf("!!!FAIL!!!\r\n");
#endif
			cmd->type = FAIL;
		}
		else if(strcmp(recvBuf, "ready\r\n") == 0)
			cmd->type = ready;
		else if(strcmp(recvBuf, "> ") == 0){
			cmd->type = ready_to_send;
			//debugPrintf("!!! ready_to_send detected !!!\r\n");
		}
		else if(MEMCMPx(recvBuf, "Recv") == 0){
			cmd->type = recv_bytes_report;
			//debugPrintf("!!! recv_bytes_report detected !!!\r\n");
		}
		else{
			cmd->type = TEXT;
	//		debugPrintf("!!! TEXT !!! \r\n");
	//		if((msgLength == 0) && (strcmp(wifiMsg, "\r\n") != 0) )
	//			debugPrintf("!!! unknown cmd!!! \r\n");
		}
	}
}
//#undef DEBUGPRINTF

//TCmdType blockWaitCmd()
//{
//	TCmd cmd;
//	getNextWifiCmd(cmd, INFINITY);
//	return cmd.type;
//}

void blockWaitSendOK()
{
	TCmd cmd;
	while(1){
		getNextWifiCmd(&cmd, INFINITY);
		if(cmd.type == RESP_SEND_OK)
			break;
		if(cmd.type == RESP_ERROR)
			break;
		if(cmd.type == busy_s)
			debugPrintf("busy!!\r\n");
	}
}

void blockWaitForReadyToSend()
{
	TCmd cmd;
	while(1){
		getNextWifiCmd(&cmd, INFINITY);
		if(cmd.type == ready_to_send){
			return;
		}
		if(cmd.type == CMD_CLOSED){
			debugPrintf("detect CLOSED \r\n");
			return;
		}
		if(cmd.type == busy_s){
			//while (1) ;
		}
	}
}


void blockWaitRecvBytesReport()
{
	TCmd cmd;
	while(1){
		getNextWifiCmd(&cmd, INFINITY);
		if(cmd.type == RESP_SEND_OK)
			break;
		if(cmd.type == RESP_ERROR)
			break;
		if(cmd.type == busy_s)
			debugPrintf("busy!!\r\n");
		if(cmd.type == recv_bytes_report){
#ifdef DEBUGPRINTF
			debugPrintf("recv_bytes_report\r\n");
#endif
			break;
		}
	}
}



uint16_t blockWaitTEXT()
{
	uint16_t ret = 0;
	waitWiFiMsg();
	ret = getUartIrqMsgLength(); //strlen(uart1Buffer);
	//uint16_t msgLen = uart1MsgLen;
	//memcpy(uart1ProcBuffer, uart1Buffer, BUF_LEN);
	enableWiFiMsg();
	return ret;

//	extern char tempRecvBuf[];
//	uint8_t tempRecvBufInd = 0;
//
//	extern RINGBUFF_T rxring;
//
//	tempRecvBuf[0] = 0;
//
//	while(1){
//		if(RingBuffer_Pop(&rxring, &(tempRecvBuf[tempRecvBufInd]))){
//			if( (tempRecvBuf[tempRecvBufInd] == '\n') ||
//				((tempRecvBuf[0] == '>') && (tempRecvBufInd==1))
//			  )
//			{
//				tempRecvBufInd++;
//				tempRecvBuf[tempRecvBufInd] = '\0';
//
//				uint16_t msgSize = tempRecvBufInd;
//				Chip_UART_SendBlocking(LPC_USART0, tempRecvBuf, tempRecvBufInd);
//				tempRecvBufInd = 0 ;
//				return msgSize;
//			}
//			else{
//				if((msgLength > 0) && (tempRecvBufInd == (msgLength-1))){
//					tempRecvBufInd++;
//					tempRecvBuf[tempRecvBufInd] = '\0';
//					//debugPrintf("!!! affected !!! \r\n");
//					//Chip_UART_SendBlocking(LPC_USART0, tempRecvBuf, tempRecvBufInd);
//					TCmdType cmdType = TEXT;
//					//processMsg(cmdType, tempRecvBufInd);
//					uint16_t msgSize = tempRecvBufInd;
//					tempRecvBufInd = 0 ;
//					return msgSize;
//				}
//				else
//					tempRecvBufInd++;
//			}
//		}
//	}
}

void sendCipClose(uint8_t id)
{
	char numToStr[10];
	wifiPrintf("AT+CIPCLOSE=");
	itoa(id, numToStr, 10);
	wifiPrintf(numToStr);
	wifiPrintf("\r\n");
}

void sendWifiData(char *str, uint8_t connId)
{
	char numToStr[5];
	wifiPrintf("AT+CIPSEND=");

#ifdef DEBUGPRINTF
	debugPrintf("AT+CIPSEND=");
#endif

	itoa(connId, numToStr, 10);
	wifiPrintf(numToStr);

#ifdef DEBUGPRINTF
		debugPrintf(numToStr);
#endif
	wifiPrintf(",");

#ifdef DEBUGPRINTF
	debugPrintf(",");
#endif

	itoa(strlen(str), numToStr, 10);

	wifiPrintf(numToStr);

#ifdef DEBUGPRINTF
	debugPrintf(numToStr);
#endif

	wifiPrintf("\r\n");

#ifdef DEBUGPRINTF
	debugPrintf("\r\n");
#endif

	blockWaitForReadyToSend();
	wifiPrintf(str);
	blockWaitSendOK();
}
//#define DEBUGPRINTF
void sendWifiDataToBuf(char *str, uint8_t connId)
{
	char numToStr[10];
	wifiPrintf("AT+CIPSENDBUF=");

#ifdef DEBUGPRINTF
	debugPrintf("AT+CIPSENDBUF=");
#endif

	itoa(connId, numToStr, 10);
	wifiPrintf(numToStr);

#ifdef DEBUGPRINTF
	debugPrintf(numToStr);
#endif
	wifiPrintf(",");

#ifdef DEBUGPRINTF
	debugPrintf(",");
#endif

	itoa(strlen(str), numToStr, 10);

	wifiPrintf(numToStr);

#ifdef DEBUGPRINTF
	debugPrintf(numToStr);
#endif

	wifiPrintf("\r\n");

#ifdef DEBUGPRINTF
	debugPrintf("\r\n");
#endif

	blockWaitForReadyToSend();
#ifdef DEBUGPRINTF
	debugPrintf("sending data\r\n");
#endif
	wifiPrintf(str);
#ifdef DEBUGPRINTF
	debugPrintf("data send ok\r\n");
#endif
	//blockWaitSendOK();
	blockWaitRecvBytesReport();
}
//#undef DEBUGPRINTF

void startSendWifiDataWithLen(uint16_t strLen, uint8_t connId, uint8_t segInd)
{
	char numToStr[5];
	wifiPrintf("AT+CIPSEND=");

#ifdef DEBUGPRINTF
	debugPrintf("AT+CIPSEND=");
#endif

	itoa(connId, numToStr, 10);
	wifiPrintf(numToStr);

#ifdef DEBUGPRINTF
	debugPrintf(numToStr);
#endif
//---------------------
//	wifiPrintf(",");
//
//	if(debugPrintf == true)
//		debugPrintf(",");
//
//	itoa(segInd, numToStr, 10);
//	wifiPrintf(numToStr);
//
//	if(debugPrintf == true)
//		debugPrintf(numToStr);

//---------------------
	wifiPrintf(",");

#ifdef DEBUGPRINTF
	debugPrintf(",");
#endif

	itoa(strLen, numToStr, 10);

	wifiPrintf(numToStr);

#ifdef DEBUGPRINTF
	debugPrintf(numToStr);
#endif


	wifiPrintf("\r\n");

#ifdef DEBUGPRINTF
	debugPrintf("\r\n");
#endif

	blockWaitForReadyToSend();
}

//TCmdType getNextWifiCmd(bool debugPrintf)
//{
//	TCmdType cmdType = UNKNWON;
//	char recvBuf[BUF_LEN];
//
//	uint16_t msgLen = recvWifiMsg(recvBuf);
//
//	if(recvBuf[0] == '+'){
//		if(MEMCMPx(recvBuf, "+IPD") == 0){
//			char numToStr[10];
//			debugPrintf("!!! +IPD !!! 0 \r\n");
//			parseIPD(recvBuf, &curConnInd, &msgLength);
//
//			while(msgLength > 0){
//				uint16_t wifiMsgLen = blockWaitTEXT();
//				msgLength -= wifiMsgLen;
//
//				debugPrintf(" => ");
//				itoa(wifiMsgLen, numToStr, 10);
//				debugPrintf(numToStr);
//				debugPrintf(" rn ");
//				itoa(msgLength, numToStr, 10);
//				debugPrintf(numToStr);
//				debugPrintf(" chs\r\n");
//			}
//
//			debugPrintf(" message recvd! \r\n");
//		}
//	}
//	debugPrintf("got msg! -> ");
//	debugPrintflen(recvBuf, msgLen);
//
//	return cmdType;
//}


void waitForRespOK()
{
	TCmd cmd;
	while(1){
		getNextWifiCmd(&cmd, INFINITY);
		if( cmd.type == CMD_OK)
			break;
	}
}

void waitForRespOKorRespError()
{
	TCmd cmd;
	while(1){
		getNextWifiCmd(&cmd, INFINITY);
		if(cmd.type == CMD_OK)
			break;
		if(cmd.type == RESP_ERROR)
			break;
	}
}

void waitForCIPSENDResp()
{
	TCmd cmd;
	while(1){
		getNextWifiCmd(&cmd, INFINITY);
		if(cmd.type == CMD_OK)
			break;
		if(cmd.type == RESP_ERROR)
			break;
	}
}

__RODATA(text) static char msgGetNextStr[] = " getWifiNextString=> IPD detected. \r\n";
__RODATA(text) static char msgIPD[] = "+IPD";
__RODATA(text) static char msgGetNextStrDet[] = " getWifiNextString \\r\\n detected \r\n";

__RAM_FUNC uint16_t getWifiNextString(char recvBuf[], int16_t *curPacketLenLeft)
{
	char numToStr[10] /*, *msg*/;
	uint8_t curConnInd;
	int16_t stringLen = 0;
	int16_t wifiMsgLen = 0;


	wifiMsgLen = recvWifiMsgBlocking(recvBuf, BUF_LEN);

	/*debugPrintf(" getWifiNextString=>");
	itoa(*curPacketLenLeft, numToStr, 10);
	debugPrintf(numToStr);
	debugPrintf(" => ");
	debugPrintflen(recvBuf, wifiMsgLen);

	debugPrintf("\r\n");*/

	if(*curPacketLenLeft == 0){
		if(recvBuf[0] == '+'){
			if(memcmp(recvBuf, msgIPD, 4) == 0){
				debugPrintf(msgGetNextStr);

				stringLen = parseIPD(recvBuf, &curConnInd, curPacketLenLeft);
				//memcpy(recvBuf, msg, stringLen);
				/*debugPrintf(" getWifiNextString=> msg:");
				debugPrintf(recvBuf);
				debugPrintf("\r\n");*/
			}
		}
		else{
			stringLen = wifiMsgLen;
		}
		//debugPrintf(" getWifiNextString=>");
		//debugPrintflen(recvBuf, wifiMsgLen);
	}
	else{
		if(wifiMsgLen > *curPacketLenLeft){
			wifiMsgLen -=2;
			//curPacketLenLeft -= wifiMsgLen;
			//debugPrintf(" getWifiNextString end detected\r\n");
			if(memcmp(&(recvBuf[*curPacketLenLeft-1]), msgRN, 2) == 0){
				stringLen = *curPacketLenLeft;
				*curPacketLenLeft = 0;
				debugPrintf(msgGetNextStrDet);
			}
			else{
				//debugPrintf(" getWifiNextString add next string \r\n");
				char *pBufTail = &(recvBuf[*curPacketLenLeft]);
				wifiMsgLen = recvWifiMsgBlocking(pBufTail, BUF_LEN);

				stringLen = *curPacketLenLeft;

				//debugPrintf(" getWifiNextString =>");
				//debugPrintf(pBufTail);

				wifiMsgLen = parseIPD(pBufTail, &curConnInd, curPacketLenLeft);
				//memcpy(pBufTail, msg, wifiMsgLen);
				//pBufTail[wifiMsgLen] = '\0';

				stringLen += wifiMsgLen;

				//curPacketLenLeft = 0;
				//debugPrintf(recvBuf);

				//debugPrintf(" getWifiNextString add next string end\r\n");
			}

		}
		else{
			*curPacketLenLeft -= wifiMsgLen;
			stringLen = wifiMsgLen;
		}

	}

	return stringLen;
}

void resetEspModule()
{
	wifiPrintf("AT+RST\r\n");
	waitForRespOK();
}
