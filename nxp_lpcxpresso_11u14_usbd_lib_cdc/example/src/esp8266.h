#pragma once
#include <stdint.h>

typedef enum {
	ready,
	wifi_discon,
	wifi_conn,
	wifi_gotip,
	IPD,
	CIFSR_APIP,
	CIFSR_APMAC,
	CIFSR_STAIP,
	CIFSR_STAMAC,
	CIPSTO,
	CMD_OK,
	RESP_OK,
	RESP_ERROR,
	CMD_CONNECT,
	CMD_CLOSED,
	TEXT,
	RESP_SEND_OK,
	SEND_READY,
	busy_s,
	ready_to_send,
	recv_bytes_report,
	FAIL,
	UNKNWON
} TCmdType;

typedef enum{
	GET_ROOT,
	GET_FAVICON,
	POST
} THtmlReqType;

typedef struct {
	TCmdType type;
	union{
		struct{
			uint8_t curConnInd;
			THtmlReqType htmlReqType;
			uint32_t contentLength;
			//uint16_t charsInHead;
		};
	};

} TCmd;

typedef struct {
	uint8_t secureType;
	int8_t	rssi;
	char name[10];
} TWifiAp;

#define WIFI_APLISTMAX 15
extern TWifiAp wifiApList[WIFI_APLISTMAX];

extern char APIP[20], STAPIP[20];

//TCmdType blockWaitCmd();
void blockWaitSendOK();

void getNextWifiCmd(TCmd *cmd, int16_t to_msec);
void getNextWifiCmdExtBuf(char recvBuf[], TCmd *cmd, int16_t to_msec);

void startSendWifiDataWithLen(uint16_t strLen, uint8_t connId, uint8_t segInd);
void sendWifiData(char *str, uint8_t connId);
void sendWifiDataToBuf(char *str, uint8_t connId);
void sendCipClose(uint8_t id);

void waitForRespOK();

uint16_t parseIPD(char *str, uint8_t *curConnInd, int16_t *msgLen);

uint16_t getWifiNextString(char recvBuf[], int16_t *curPacketLenLeft);
void resetEspModule();

