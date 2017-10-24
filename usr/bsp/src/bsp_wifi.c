#include <stdlib.h>
#include <stdint.h>

#include "ucos_ii.h"
#include "bsp_uart.h"
#include "bsp_wifi.h"
#include "common/tools.h"
#include "common/PosixError.h"

typedef struct
{
    USART_TypeDef *    uart_port;
    uint32_t    wifi_mode;

    //OS_EVENT    mutex;
}mico_device_t;

typedef struct
{
    mico_device_t *dev;
    mico_protocol_t proto;

    uint16_t local_port;
    uint32_t remote_ip;
    uint16_t remote_port;
	uint16_t socket_port_num;
}mico_socket_t;

static OS_EVENT    *mutex=NULL;

static int uart_blocking_receive(USART_TypeDef *uart, uint8_t *buf, uint32_t len, uint32_t tmout_ms);
//char *itoa(int value, char *string, int radix);
char *Bsp_IpToStr(uint32_t ip, char *pstring);
UART_T *ComToUart(USART_TypeDef* _USARTx);
int StrFindCh(u8 * _pstrsource, u8 _desch, u8 _startnum, u8 _endnum);
int StrFindStr(char * _pstrsource, char *_pdesstr, u8 _startnum, u8 _endnum);

int StrLeftMov(u8 * _pstrsource, u8 _movnum, u8 _size);

//cmd
typedef enum
{
	AT_START,
	AT_CMDMODE,
	AT_WMODE,
	AT_WSTA,
	AT_DHCP,
	AT_IPCONFIG,
	AT_SAVE,
	AT_REBOOT,
	AT_QUIT,
	AT_WSTATUS,
	AT_CON1,
	AT_EVENT,
}Enum_AtCMD;

//wifi state
typedef enum
{
	WifiAp,
	WifiStation,
	WifyAtCmd,
}Enum_WifiWorkState;

//wifi��ǰ��Ϣ�ṹ��
typedef struct
{
	u8 WifiState;	//wifi״̬��0:δ֪��1: APģʽ��2: stationģʽ
	u8 WifiConnect;	//wifi���ӷ�ʽ��0:δ֪��1: �Զ���ȡIP��ַ��ʽ���ӣ�2: �ֶ�����IP��ַ����
}Struct_WifiCurSig;
Struct_WifiCurSig gsWifiCurSig;



static mico_device_t device_pool[MICO_MAX_DEVICE_NUM];
static OS_MEM* device_mem;

static mico_socket_t sock_pool[MICO_MAX_SOCKET_NUM];
static OS_MEM* sock_mem;



extern UART_T g_tUart2;

extern UART_T g_tUart1;

#define bsp_TimDelay(ms)    OSTimeDlyHMSM(0,0,0,ms)

/************************************************
��������: ���͵���ָ��
�������ݲ�ֱ�ӷ��ͳ���
************************************************/

int mico_init_driver(void)
{
    uint8_t err;

    device_mem = OSMemCreate(&device_pool, MICO_MAX_DEVICE_NUM, sizeof(mico_device_t), &err);
    if(device_mem == 0)
        return -PERR_ENOMEM;

    sock_mem = OSMemCreate(&sock_pool, MICO_MAX_SOCKET_NUM, sizeof(mico_socket_t), &err);
    if(sock_mem == 0)
        return -PERR_ENOMEM;

    mutex = OSMutexCreate(6, &err);
    if(mutex == NULL)
    {
        printf("mico create mutex failed with err = %d\r\n",
               err);
        return -PERR_ENOMEM;
    }

    return 0;
}

/*
�������:art_port:���ں�;
		wifi_mode:wifeģʽѡ��: AP MODE OR STATION MODE.

*/
int mico_create_device(USART_TypeDef * _Uartx, uint32_t wifi_mode)
{
    mico_device_t *dev;
    uint8_t err;

	bsp_InitUart(_Uartx, 115200, USART_StopBits_1, USART_Parity_No);
	//bsp_InitUart(USART1, 115200, USART_StopBits_1, USART_Parity_No);//������Ϣ��ӡ

    dev = OSMemGet(device_mem, &err);
    if(dev == 0)
        return 0;

    dev->uart_port = _Uartx;
    dev->wifi_mode = wifi_mode;

    //uart_init(uart_port, 115200, 8, 'n' ,1);
    
 	
	return (int)dev;
}

/************************************************************
��������: �л�����ATģʽ
************************************************************/
int DeviceWifiSwApMode(UART_T *pUart)
{
	Enum_AtCMD AtSetSta = AT_START;

  bsp_UartClearRx(pUart->uart);

	while(1)
	{
		switch(AtSetSta)
		{
			case AT_START:
			{
				u8 *ackok = "a";
				u8 *ackerr = "+ERR=-2";
				u8 chack[16] = {0};
				s8 i = 0;
				u8 sendcmd[64] = "+++";
				AtSetSta = AT_WMODE;

				USART_OUT(pUart->uart, sendcmd);
				//USART_OUT(USART1, sendcmd);
				//USART_OUT(USART1, "\r\n");
				//�жϴ���2��û����Ч���ݷ���
#if 0
				while(pUart->usRxCount == 0){;}
				bsp_TimDelay(100);
	 			bsp_UartReceive(pUart->uart, chack, 16);
#else
        uart_blocking_receive(pUart->uart, chack, 16, 1000);
        //printf("LINE: %d\r\n", __LINE__);
//        chack[15] = 0;
 //       printf("%s\r\n", chack);
#endif
				//USART_OUT(USART1, chack);
				//USART_OUT(USART1, "\r\n");
				i = strncmp((char *)chack,(char *)ackok,1);
				if(0 == i)
				{
					AtSetSta = AT_CMDMODE;
				}
				else if(0 == strncmp((char *)chack,(char *)ackerr,7))	
				{//�Ѿ�����ATָ��ģʽ����ֱ������
					return 0;
				}
				else
				{
					return -1;
				}
				break;
			}
			case AT_CMDMODE:
			{
				u8 *ackok = "+OK\r\n";
				u8 chack[16] = {0};
				s8 i = 0;

				USART_OUT(pUart->uart, "a");
				//USART_OUT(USART1, "a\r\n");
#if 0
				while(pUart->usRxCount == 0){;}
				bsp_TimDelay(100);
				bsp_UartReceive(pUart->uart, chack, 16);
#else
        uart_blocking_receive(pUart->uart, chack, 16, 1000);
        //printf("LINE: %d\r\n", __LINE__);
#endif
				//USART_OUT(USART1, chack);
				//USART_OUT(USART1, "\r\n\r\n");
				i = strncmp((char *)chack,(char *)ackok,3);
				if(0 == i)
				{
					AtSetSta = AT_WMODE;
					return 0;
				}
				else
					return -1;
			}
			default:
				break;
		}
	}
		
}
/*****************************************************************
��������: �������ã���������ģ��
******************************************************************/
int DeviceWifiSaveAndReboot(UART_T *pUart)
{
	Enum_AtCMD AtSetSta = AT_SAVE;
	while(1)
	{
		switch(AtSetSta)
		{
			case AT_SAVE:
			{
				u8 *ackok = "+OK\r\n";
				u8 chack[8] = {16};
				s8 i = 0;
				u8 SendCmd[16] = "AT+SAVE\r";
				USART_OUT( pUart->uart, SendCmd);
				//USART_OUT(USART1, SendCmd);
				//USART_OUT(USART1, "\r\n");
#if 0
				while( pUart->usRxCount == 0){;}
				bsp_TimDelay(100);
				bsp_UartReceive( pUart->uart, chack, 16);
#else
        uart_blocking_receive( pUart->uart, chack, 16, 500);
#endif
				//USART_OUT(USART1, chack);
				//USART_OUT(USART1, "\r\n\r\n");
				i = strncmp((char *)chack,(char *)ackok,3);
				if(0 == i)
				{
					AtSetSta = AT_REBOOT;
				}
				else
					return -1;				
				break;
			}
			case AT_REBOOT:
			{
				u8 *ackok = "+OK\r\n";
				u8 chack[32] = {0};
				s8 i = 0;
				u8 SendCmd[16] = "AT+REBOOT\r";
				USART_OUT( pUart->uart, SendCmd);
				//USART_OUT(USART1, SendCmd);
				//USART_OUT(USART1, "\r\n");
#if 0
				while( pUart->usRxCount == 0){;}
				bsp_TimDelay(100);
				bsp_UartReceive( pUart->uart, chack, 32);
#else
        uart_blocking_receive(pUart->uart, chack, 32, 15*1000);
#endif
				//USART_OUT(USART1, chack);
				//USART_OUT(USART1, "\r\n\r\n");
				i = strncmp((char *)chack,(char *)ackok,3);
				if(0 == i)
				{
					return 0;
				}
				else
        {
					return -1;
        }
			}
			default: break;
		}
	}
}

int uartsendcmd(u8 *_cmd, u8 _len)
{
	return 0;
}

/**************************************************************
//��������:STATION ��ʼ������

***************************************************************/
int mico_init_station(int dev, char *ap_name, char*password, uint32_t ip, uint32_t mask, uint32_t gateway)
{
	mico_device_t *pdev;
	UART_T *pUart;
	
	int usFunRet = 0;
	Enum_AtCMD AtSetSta;
//	bsp_InitUart(USART2, 115200, USART_StopBits_1, USART_Parity_No);
//	bsp_InitUart(USART1, 115200, USART_StopBits_1, USART_Parity_No);
	pdev = (mico_device_t *)dev;
	pUart = ComToUart(pdev->uart_port);	
	usFunRet = DeviceWifiSwApMode(pUart);	
	if(usFunRet != 0)// AP mode failure
		return -1;
	else
		AtSetSta = AT_EVENT;
	//�ȼ���Ƿ�satationģʽ�������Ƿ�һ�£���ѯ��������ap_name
	//
	while(1)
	{
		switch(AtSetSta)
		{
			case AT_EVENT:
			{
				u8 *ackok = "+OK\r\n";
				u8 chack[16] = {0};
				s8 i = 0;
				USART_OUT(pUart->uart, "AT+EVENT=ON\r");
				//USART_OUT(USART1, "AT+EVENT=ON\r");
#if 0
				while(pUart->usRxCount == 0){;}
				bsp_TimDelay(100);
				bsp_UartReceive(pUart->uart, chack, 16);
#else
        uart_blocking_receive(pUart->uart, chack, 16, 500);
#endif
				//USART_OUT(USART1, chack);
				//USART_OUT(USART1, "\r\n\r\n");
				i = strncmp((char *)chack,(char *)ackok,3);
				if(0 == i)
				{
					AtSetSta = AT_WMODE;
				}
				else
					//AtSetSta = AT_EVENT;
					return -1;
				break;
			}
			case AT_WMODE:
			{
				u8 *ackok = "+OK\r\n";
				u8 chack[16] = {0};
				s8 i = 0;
				USART_OUT(pUart->uart, "AT+WMODE=STA\r");
				//USART_OUT(USART1, "AT+WMODE=STA\r\n");
#if 0
				while(pUart->usRxCount == 0){;}
				bsp_TimDelay(100);
				bsp_UartReceive(pUart->uart, chack, 16);
#else
        uart_blocking_receive(pUart->uart, chack, 16, 500);
#endif
				//USART_OUT(USART1, chack);
				//USART_OUT(USART1, "\r\n\r\n");
				i = strncmp((char *)chack,(char *)ackok,3);
				if(0 == i)
				{
					AtSetSta = AT_WSTA;
				}
				else
					return -1;
				break;
			}
			case AT_WSTA:
			{
				u8 *ackok = "+OK\r\n";
				u8 chack[16] = {0};
				s8 i = 0;
				u8 SendCmd[256] = "AT+WSTA=";
				strcat((char *)SendCmd, (char *)ap_name);
				strcat((char *)SendCmd, ",");
				strcat((char *)SendCmd, password);
				strcat((char *)SendCmd, "\r");
				USART_OUT(pUart->uart, SendCmd);
				//USART_OUT(USART1, SendCmd);
				//USART_OUT(USART1, "\r\n");
#if 0
				while(pUart->usRxCount == 0){;}
				bsp_TimDelay(100);
				bsp_UartReceive(pUart->uart, chack, 16);
#else
        uart_blocking_receive(pUart->uart, chack, 16, 500);
#endif
				//USART_OUT(USART1, chack);
				//USART_OUT(USART1, "\r\n\r\n");
				i = strncmp((char *)chack,(char *)ackok,3);
				if(0 == i)
				{
					AtSetSta = AT_DHCP;
				}
				else
					return -1;
				break;
			}
			case AT_DHCP:
			{
				u8 *ackok = "+OK\r\n";
				u8 chack[16] = {0};
				s8 i = 0;
				if(ip != 0)
				{
					u8 SendCmd[16] = "AT+DHCP=OFF\r";
					USART_OUT(pUart->uart, SendCmd);
					//USART_OUT(USART1, SendCmd);
					//USART_OUT(USART1, "\r\n");
#if 0
					while( pUart->usRxCount == 0){;}
					bsp_TimDelay(100);
					bsp_UartReceive(pUart->uart, chack, 16);
#else
          uart_blocking_receive(pUart->uart, chack, 16, 500);
#endif
					//USART_OUT(USART1, chack);
					//USART_OUT(USART1, "\r\n\r\n");
					i = strncmp((char *)chack,(char *)ackok,3);
					if(0 == i)
					{
						AtSetSta = AT_IPCONFIG;
						gsWifiCurSig.WifiConnect = 2;
					}
					else
						return -1;
				}
				else
				{//�Զ���ȡIP��ַģʽ
					u8 SendCmd[16] = "AT+DHCP=ON\r";
					USART_OUT( pUart->uart, SendCmd);
					//USART_OUT(USART1, SendCmd);
					//USART_OUT(USART1, "\r\n");
#if 0
					while( pUart->usRxCount == 0){;}
					bsp_TimDelay(100);
					bsp_UartReceive( pUart->uart, chack, 16);
#else
          memset(chack, 0, 16);
          uart_blocking_receive(pUart->uart, chack, 16, 500);
#endif
					//USART_OUT(USART1, chack);
					//USART_OUT(USART1, "\r\n\r\n");
					i = strncmp((char *)chack,(char *)ackok,3);
					if(0 == i)
					{
						gsWifiCurSig.WifiConnect = 1;
						goto Label1;
					}
					else
						return -1;
				}
				break;
			}
			case AT_IPCONFIG:
			{
				u8 *ackok = "+OK\r\n";
				u8 chack[16] = {0};
				s8 i = 0;
				u8 SendCmd[128] = "AT+IPCONFIG=STA,";
				char ipstring[16] = {0};
				Bsp_IpToStr(ip, ipstring);
				strcat((char *)SendCmd, ipstring);
				strcat((char *)SendCmd, ",");
				memset(ipstring,0,strlen(ipstring));
				Bsp_IpToStr(mask, ipstring);
				strcat((char *)SendCmd, ipstring);
				strcat((char *)SendCmd, ",");
				memset(ipstring,0,strlen(ipstring));
				Bsp_IpToStr(gateway, ipstring);
				strcat((char *)SendCmd, ipstring);
				strcat((char *)SendCmd, "\r");
 				USART_OUT( pUart->uart, SendCmd);
				//USART_OUT(USART1, SendCmd);
				//USART_OUT(USART1, "\r\n");
#if 0
				while( pUart->usRxCount == 0){;}
				bsp_TimDelay(100);
				bsp_UartReceive( pUart->uart, chack, 16);
#else
        memset(chack, 0, 16);
        uart_blocking_receive(pUart->uart, chack, 16, 500);
#endif
				//USART_OUT(USART1, chack);
				//USART_OUT(USART1, "\r\n\r\n");
				i = strncmp((char *)chack,(char *)ackok,3);
				if(0 == i)
				{
					goto Label1;
				}
				else
				{//�ֶ�����IP��ַʧ�ܣ�״̬λ��0
					gsWifiCurSig.WifiConnect = 0;
					return -1;
				}
			}
 			default:
				break;
		}
	}
	Label1://���沢��ѯ�Ƿ����óɹ�
	usFunRet = DeviceWifiSaveAndReboot(pUart);
	if(usFunRet != 0)// AP mode failure
		return -1;
	else//�ȴ��ϱ��¼�������Ӧ�÷������ӳɹ�
	{
		u8 *EventAckOk = "+EVENT=WIFI_LINK,STATION_UP";
		u8 chack[64] = {0};
		s8 i = 0;
#if 0
		//while(pUart->usRxCount == 0){;}
    for(i=0; i<10*1000; i+=10)
    {
        if(pUart->usRxCount != 0)
          break;

        OSTimeDlyHMSM(0,0,0,10);
    }

		bsp_TimDelay(100);
		bsp_UartReceive(pUart->uart, chack, 64);
#else
    uart_blocking_receive(pUart->uart, chack, 64, 15*1000);
#endif
		////USART_OUT(USART1, chack);
		//USART_OUT(USART1, "\r\n\r\n");
		i = strncmp((char *)chack,(char *)EventAckOk,strlen((char *)EventAckOk));
    OSTimeDlyHMSM(0,0,0,200);
		if(0 == i)
		{
			return 0;
		}
		else
    {
      printf("Wait WIFI link faild.\r\n");
			return -1;
    }
	}
}

void GetProtocolStr(char *pprostr, mico_protocol_t proto, uint32_t ip, uint16_t port_remote, u16 port_local)
{
	char chPortLocalStr[8] = {0};
	char chPortRemoteStr[8] = {0};
	char ipstring[16] = {0};

	sprintf(chPortRemoteStr, "%d", port_remote);
	sprintf(chPortLocalStr, "%d", port_local);
	Bsp_IpToStr(ip, ipstring);	

	if(TCP_SERVER == proto)
	{
		strcat(pprostr, "SERVER,");
		strcat(pprostr, (char *)chPortLocalStr);
		strcat(pprostr, ",,");
//		sprintf(pprostr,"SERVER,%d,,",port_local);
	}
	else if(TCP_CLIENT == proto)
	{
		strcat(pprostr, "CLIENT,");
		strcat(pprostr, (char *)chPortLocalStr);
		strcat(pprostr, ",");
		strcat(pprostr, (char *)ipstring);
	}
	else if(UDP_BROADCAST == proto)
	{
		strcat(pprostr, "BOARDCAST,");
		strcat(pprostr, (char *)chPortLocalStr);
		strcat(pprostr, ",");
		strcat(pprostr, (char *)chPortRemoteStr);
		strcat(pprostr, ",");
	}
	else//(UDP_UNICAST == proto)
	{
		strcat(pprostr, "UNICAST,");
		strcat(pprostr, (char *)chPortLocalStr);
		strcat(pprostr, ",");

#if 0
		strcat(pprostr, (char *)chPortRemoteStr);
#else
		strcat(pprostr, "");
#endif

		strcat(pprostr, ",");
		strcat(pprostr, (char *)ipstring);
	}
	strcat(pprostr, "\r");	
}
//Only support TCP_SERVER, UDP_UNICAST
//ip: for TCP_SERVER, it,s remote addr, for UDP_UNICAST it IS remote addr
//port: for TCP_SERVER, it,s remote port, for UDP_UNICAST it,s the local port
int mico_create_connection(int device, mico_protocol_t proto, uint32_t ip, uint16_t port_remote, u16 port_local)
{
    mico_socket_t *sock;
    uint8_t     err;
	mico_device_t *pdev;
	u8 ucCmdstring[128] = {0};
	int usFunRet = 0;
	UART_T *pUart;

	pdev = (mico_device_t *)device;
    sock = OSMemGet(sock_mem, &err);
    if(sock == 0)
        return 0;
	pUart = ComToUart(pdev->uart_port);	
    sock->dev = pdev;
    sock->proto = proto;
    sock->remote_ip = ip;
    sock->remote_port = port_remote;
	sock->local_port = port_local;
	GetProtocolStr((char *)ucCmdstring, proto, ip, port_remote, port_local);
	usFunRet = DeviceWifiSwApMode(pUart);	
	if(usFunRet != 0)// AP mode failure
		return 0;
	else
	{//����APָ��ģʽ
		u8 *ackok = "+OK\r\n";
		u8 chack[16] = {0};
		s8 i = 0;
		u8 SendCmd[256] = "AT+CON1=";				
		strcat((char *)SendCmd, (char *)ucCmdstring);				
		USART_OUT( pUart->uart, SendCmd);
#if 0
		while(pUart->usRxCount == 0){;}
		bsp_TimDelay(100);
		bsp_UartReceive(pUart->uart, chack, 16);
#else
    uart_blocking_receive(pUart->uart, chack, 16, 500);
#endif
		i = strncmp((char *)chack,(char *)ackok,3);
		if(0 == i);
		else
			return 0;
	}
	//��������
	usFunRet = DeviceWifiSaveAndReboot(pUart);
	if(usFunRet != 0)// AP mode failure
		return 0;
	switch(proto)
	{
		case UDP_UNICAST:
		{
			const char *EventAckOk = "+EVENT=UDP_UNICAST,CONNECT,";
			u8 chack[128] = {0};
			int strstation = 0;
#if 0
			while(pUart->usRxCount == 0){;}
			bsp_TimDelay(100);
			bsp_UartReceive(pUart->uart, chack, 128);
#else
        uart_blocking_receive(pUart->uart, chack, sizeof(chack), 10*1000);
        //chack[127] = 0;
        //printf("%s\r\n", chack);
#endif
			//USART_OUT(USART1, chack);
			//USART_OUT(USART1, "\r\n\r\n");			
			strstation = StrFindStr((char *)chack,  (char *)EventAckOk, 0, strlen(EventAckOk));
			if(-1 == strstation)
			{//û��UDP���ӳɹ����¼��ϱ�����Ҫ�ٵȴ�1s
        OSTimeDlyHMSM(0,0,0,500);
        uart_blocking_receive(pUart->uart, chack, sizeof(chack), 5*1000);
#if 1
				strstation = StrFindStr((char *)chack, (char *)EventAckOk, 0, strlen(EventAckOk));
				if(-1 == strstation)
#else
        char*pos = strstr(chack, EventAckOk);
        if(pos != NULL)
            strstation = pos-(char*)chack;
        else
#endif
				{//û��UDP���ӳɹ����¼��ϱ�
          printf("Wait UDP_UNICAST event failed.\r\n");
					return 0;
				}
			}
			{//UDP���ӳɹ�ʱ���ϱ�, ��ȡsoket���ӱ��,ע�⣬������Ϊ4���ַ�

				if(chack[strstation + strlen(EventAckOk) + 1] == '\r')
					sock->socket_port_num = chack[strstation + strlen(EventAckOk)] - '0';
				else if(chack[strstation + strlen(EventAckOk) + 2] == '\r')
				{
					sock->socket_port_num = (chack[strstation + strlen(EventAckOk)] - '0')*10 + \
									(chack[strstation + strlen(EventAckOk) + 1] - '0');
				}
				else if(chack[strstation + strlen(EventAckOk) + 3] == '\r')
				{
					sock->socket_port_num = (chack[strstation + strlen(EventAckOk)] - '0')*100 + \
									(chack[strstation + strlen(EventAckOk) + 1] - '0')*10 + \
									(chack[strstation + strlen(EventAckOk) + 2] - '0');
				}
				else if(chack[strstation + strlen(EventAckOk) + 4] == '\r')
				{
					sock->socket_port_num = (chack[strstation + strlen(EventAckOk)] - '0')*1000 + \
									(chack[strstation + strlen(EventAckOk) + 1] - '0')*100 + \
									(chack[strstation + strlen(EventAckOk) + 2] - '0')*10 + \
									(chack[strstation + strlen(EventAckOk) + 3] - '0');
				}
				else
					return 0;
				usFunRet = DeviceWifiSwApMode(pUart);	
				if(usFunRet != 0)// AP mode failure
					return 0;

				return (int)sock;
			}
		}
		default:			
			return 0;			
	}
}

/***************************************************************
��������: wifi��ȡ���ݣ���ȡÿ������һ֡���ݣ������ȡ�����ݳ���С��һ֡����֡ʣ�����ݶ�������,һ֡���256�ֽ�
���size����һ֡���ݣ���������һ֡����
����ֵ: ��Ч���ݵĳ���
****************************************************************/
int mico_read(int sock, uint8_t *buf, uint32_t size)
{
	mico_socket_t *  psock;
	mico_device_t *pdev;
	int usFunRet = 0;
	u8 ucFrameData[256] = {0};
	int pFrameHead = 0;	//��ѯ֡ͷ��ucFrameData��λ��ʱʹ��
	char ucFrameHead[64]="+EVENT=UNICAST_SOURCE,";//����֡ͷ����
	u8 HeadLen = 0;	//����֡ͷ���ݳ���
  uint8_t err;
  int   ret;

	u16 ucValidDatalen = 0;//��Ч���ݳ���
	u8 *pHead;		//֡ͷ��ucFrameData�е�λ��
	UART_T *pUart;

	int UartReadDataNum = 0;
	psock = (mico_socket_t *)sock;

	HeadLen = strlen(ucFrameHead);
	HeadLen+=sprintf(&ucFrameHead[HeadLen], "%d,", (int)psock->socket_port_num);
	pdev = psock->dev;
	pUart = ComToUart(pdev->uart_port);
#if 0
while(1)
{
	if(pUart->usRxCount > 0)
	{
		bsp_UartReceive(pUart->uart, ucFrameData, pUart->usRxCount);
		USART_OUT(USART1, ucFrameData);
		memset((char *)ucFrameData,0,strlen((char *)ucFrameData));
	}
}
#endif

  OSMutexPend(mutex, 0, &err);

	if(pUart->usRxCount > HeadLen)
	{//��ȡ֡ͷ
		bsp_UartReceive(pUart->uart, ucFrameData, HeadLen);
//		USART_OUT(USART1, ucFrameData);
//		USART_OUT(USART1,"\r\n0\r\n");
	}
	else
	{//����Ч����
    ret = 0;
    goto EXIT;
	}
	while(1)
	{
		usFunRet = strncmp(ucFrameHead,(char *)ucFrameData,HeadLen);
		if(0 != usFunRet)
		{//֡ͷ��ƥ�䣬�п��ܶ�ʧ����
			pFrameHead = StrFindCh(ucFrameData, '+', 1, HeadLen-1);
			if(pFrameHead == -1)
			{//ucFrameDataû��+��, 
				if(pUart->usRxCount > HeadLen)
				{//ȡ֡ͷ
					bsp_UartReceive(pUart->uart, ucFrameData, HeadLen);
					continue;
				}
				else
				{//����Ч����
					//USART_OUT(USART1, ucFrameData);
					//USART_OUT(USART1,"\r\n1\r\n");
          ret = 0;
          goto EXIT;
				}
	 		}
			else
			{//ucFrameData��+��, 
				//���ƽ��յ�������,
				//USART_OUT(USART1, ucFrameData);
				//USART_OUT(USART1,"\r\n2\r\n");
				StrLeftMov(ucFrameData, pFrameHead, HeadLen+1);
				if(pUart->usRxCount > pFrameHead)
				{//
					bsp_UartReceive(pUart->uart, &ucFrameData[HeadLen-pFrameHead], pFrameHead);				
				}
				else
				{//֡ͷ������Ϣ����, ���������ڽ���
					bsp_TimDelay(2);
					if((pUart->usRxCount > pFrameHead))
					{//
						bsp_UartReceive(pUart->uart, &ucFrameData[HeadLen-pFrameHead], pFrameHead);				
					}
					else
					{
						//USART_OUT(USART1, ucFrameData);
						//USART_OUT(USART1,"\r\n3\r\n");
            ret = 0;
            goto EXIT;
					}
				}
			}
		}
		else
		{
			break;
		}
	}
//	USART_OUT(USART1, ucFrameData);
//	USART_OUT(USART1,"\r\n4\r\n");
	memset((char *)ucFrameData,0,strlen((char *)ucFrameData));
	UartReadDataNum = bsp_UartReceive(pUart->uart, ucFrameData, 5);
	if(UartReadDataNum < 5)//
	{//���ݻ��ڴ��䣬�ȴ�10ms
		int m = 0;
		bsp_TimDelay(1);	
		m = bsp_UartReceive(pUart->uart, &ucFrameData[(u16)UartReadDataNum], 5-UartReadDataNum);
		if((m+UartReadDataNum)!=5)
		{//��֡�����Ѿ���ʧ��ֱ�ӷ���0
			//USART_OUT(USART1, ucFrameData);
			////USART_OUT(USART1,"\r\n5\r\n");
      ret = 0;
      goto EXIT;
		}
	}
	if(ucFrameData[1]!=',')
	{//��Ч���ݸ������ڵ���10���������������1�ִ���10��С��100������һ�ִ��ڵ���100��
		if(ucFrameData[2]==',')
		{//��Ч���ݴ���10����С�ڵ���100��
			ucValidDatalen = (ucFrameData[0] - '0')*10 + (ucFrameData[1] - '0') + 2;
			UartReadDataNum = bsp_UartReceive(pUart->uart, &ucFrameData[5], ucValidDatalen - 2);
			if(UartReadDataNum  < (int)ucValidDatalen - 2)
			{
 				int m = 0;
 				bsp_TimDelay((ucValidDatalen/10) + 1);
				m = bsp_UartReceive(pUart->uart, &ucFrameData[(u16)UartReadDataNum+5], ucValidDatalen-2-(u16)UartReadDataNum);
				if((m+UartReadDataNum)!=ucValidDatalen-2)
				{//��֡�����Ѿ���ʧ��ֱ�ӷ���0
					//USART_OUT(USART1, ucFrameData);
					//USART_OUT(USART1,"\r\n6\r\n");
          ret = 0;
          goto EXIT;
				}
			}
			pHead = &ucFrameData[3];
		}
		else
		{//��Ч���ݴ��ڵ���100��
			ucValidDatalen = (ucFrameData[0] - '0')*100 + (ucFrameData[1] - '0')*10 + (ucFrameData[0] - '0') + 2;
			UartReadDataNum = bsp_UartReceive(pUart->uart, &ucFrameData[5], ucValidDatalen - 1);
			if(UartReadDataNum < (int)ucValidDatalen - 1)
			{
				int m = 0;
				bsp_TimDelay((ucValidDatalen)/10+1);
				m = bsp_UartReceive(pUart->uart, &ucFrameData[(u16)UartReadDataNum+5], ucValidDatalen - 1 -(u16)UartReadDataNum);
				if((m+UartReadDataNum)!=ucValidDatalen-1)
				{//��֡�����Ѿ���ʧ��ֱ�ӷ���0
					//USART_OUT(USART1, ucFrameData);
					//USART_OUT(USART1,"\r\n7\r\n");
          ret = 0;
          goto EXIT;
				}
			}
			pHead = &ucFrameData[4];
		}
	}
	else
	{//��Ч���ݸ���С��10��
		ucValidDatalen = ucFrameData[0] - '0' + 2;
		UartReadDataNum = bsp_UartReceive(pUart->uart, &ucFrameData[5], ucValidDatalen-3);
		if(UartReadDataNum < (int)ucValidDatalen - 3)
		{
			int m = 0;
			bsp_TimDelay(1);
			m = bsp_UartReceive(pUart->uart, &ucFrameData[(u16)UartReadDataNum+5], ucValidDatalen - 3 -(u16)UartReadDataNum);
			if((m+UartReadDataNum)!=ucValidDatalen - 3)
			{//��֡�����Ѿ���ʧ��ֱ�ӷ���0
				//USART_OUT(USART1, ucFrameData);
				//USART_OUT(USART1,"\r\n8\r\n");
        ret = 0;
        goto EXIT;
			}
		}
		pHead = &ucFrameData[2];	
	}
	if(ucValidDatalen - 2 <= size)
	{
		u8 i = 0;
		for(i = 0;i < ucValidDatalen - 2;i++)
			*buf++ = *pHead++;

		ret = ucValidDatalen - 2;
	}
	else
	{
		u8 i = 0;
		for(i = 0;i < size;i++)
			*buf++ = *pHead++;
    ret = size;
	}

EXIT:
    OSMutexPost(mutex);

    return ret;
}

static int uart_blocking_receive(USART_TypeDef *uart, uint8_t *buf, uint32_t len, uint32_t tmout_ms)
{
    int cnt=0;
    int tmcnt=0;
    int ret;

    while(cnt<len)
    {
        ret = bsp_UartReceive(uart, &buf[cnt], len-cnt);
        if(ret > 0)
        {
            cnt+=ret;
            tmout_ms = tmcnt+10;    //
            continue;
        }

        if(tmcnt >= tmout_ms)
        {
            break;
        }

        OSTimeDlyHMSM(0, 0, 0, 10);
        tmcnt+=10;
    }

    //printf("Received %d bytes. tmcnt=%d \r\n", cnt, tmcnt);
    return cnt;
}

int mico_writeto(int sock, const uint8_t *buf, uint16_t len, uint32_t remote_ip, uint32_t remote_port)
{
	mico_socket_t * psock;
	UART_T *pUart;
	u8 SendAtData[256] = {0};
	u16 CmdLen = 0;
	char ipstring[16] = {0};
	u8 chack[16] = {0};
	const char ackok[5] = "+OK\r\n";
	const char ackerror[6] = "+ERR";
	int cmpvalue = 0;
  uint8_t err;
  int   ret;

  OSMutexPend(mutex, 0, &err);

	psock = (mico_socket_t *)sock;
  pUart = ComToUart(psock->dev->uart_port);	

	Bsp_IpToStr(remote_ip, ipstring);	
 	CmdLen = sprintf((char *)SendAtData,"AT+SUNSEND=%d,%d,%s,%d,",\
			psock->socket_port_num,remote_port,ipstring,len);	

	memcpy(&SendAtData[CmdLen], buf, len);
	CmdLen+=len;
	SendAtData[CmdLen] = '\r';
	CmdLen++;
	bsp_UartSend(pUart->uart, SendAtData, CmdLen);

#if 0
	while(pUart->usRxCount < 5){;}
	bsp_UartReceive(pUart->uart, chack, 5);
#else
  if(uart_blocking_receive(pUart->uart, chack, 5, 500) < 3)
    ret = -PERR_ETIMEDOUT_POSIX;
#endif

  OSMutexPost(mutex);

  if(ret < 0)
    return ret;

	cmpvalue = strncmp((char *)chack, ackok, 3);
	if(0 == cmpvalue)
		return len;
	cmpvalue = strncmp((char *)chack, ackerror, 3);
	if(0 == cmpvalue)
		return 0;

	return -1;
}

/**********************************************************************
��������: ͨ���̶�socket���ͳ���Ϊlen���ַ����ݣ�Ҫ��len С��1024
���ط��͵����ݳ���
***********************************************************************/
int mico_write(int sock, const uint8_t *buf, uint16_t len)
{
	  mico_socket_t * psock;

    psock = (mico_socket_t *)sock;

    return mico_writeto(sock, buf, len, psock->remote_ip, psock->remote_port);
}


/*********************************************************
��������: �����ַ����ַ����е�λ��
�������: _pstrsource�ַ����׵�ַ��
			_desch: Ŀ���ַ�
			_startnum: �ַ�������ʼ��ѯƫ�Ƶ�ַ����һ���ַ�ƫ�Ƶ�ַΪ0
			_endnum: �ַ����Ĳ�ѯ����ƫ�Ƶ�ַ,���һ��ƫ�Ƶ�ַΪ_endnum
**********************************************************/
int StrFindCh(u8 * _pstrsource, u8 _desch, u8 _startnum, u8 _endnum)
{
	u8 i = 0;
	for(i = _startnum; i <= _endnum; i++)
	{
		if(_desch == _pstrsource[i])
			return i;
	}
	return -1;
}

/*********************************************************
��������: ����Ŀ���ַ�����Դ�ַ����е�λ��
�������: _pstrsourceԴ�ַ����׵�ַ��
			_pdesstr: Ŀ���ַ���
			_startnum: Ŀ���ַ�������ʼ��ѯƫ�Ƶ�ַ����һ���ַ�ƫ�Ƶ�ַΪ0
			_endnum: Ŀ���ַ����Ĳ�ѯ����ƫ�Ƶ�ַ,���һ��ƫ�Ƶ�ַΪ_endnum
����ֵ: ����У�Ŀ���ַ�����Դ�ַ����п�ʼ���ֵ�λ��
		���û�У�����-1
**********************************************************/
int StrFindStr(char * _pstrsource, char *_pdesstr, u8 _startnum, u8 _endnum)
{
	u8 i = 0,j = 0;
	int psstrlen = 0; 
	u16	desstrlen = 0;//��Ҫ�Ƚ϶����ַ�������ʾ����
	psstrlen = strlen(_pstrsource);
	desstrlen = (_endnum - _startnum + 1);
	psstrlen = psstrlen - desstrlen;
	
	if(psstrlen < 0) return -1;
	for(i = 0; i <= psstrlen; i++)
	{
		if(_pstrsource[i] == _pdesstr[_startnum])
		{
			for(j = i+1; j < i + desstrlen; )
			{
				if(_pstrsource[j] == _pdesstr[_startnum + j -i])
				{
					j++;
					if(j == i + desstrlen -1)
						return i;
				}
				else 
					break;
			}
		}
	}
	return -1;
}


/*********************************************************
��������: �ַ������ƺ������ұ߲�0
�������: _pstrsource�ַ����׵�ַ��
			_movnum: ��������
			_size: ��Ҫ�ƶ��Ĵ�С�����һ��ƫ�Ƶ�ַΪ_size - 1;
**********************************************************/
int StrLeftMov(u8 * _pstrsource, u8 _movnum, u8 _size)
{
	u16 i = 0;
	char strbuf[255] = {0};
	
	for(i = 0; i < _size; i++)
	{
		strbuf[i] = _pstrsource[i];
		_pstrsource[i] = 0;
	}
	memset(_pstrsource,0,strlen((char *)_pstrsource));
	for(i = 0; i < _size - _movnum; i++)
	{
		_pstrsource[i] = strbuf[i+_movnum];
	}
	return 0;
}







/****************************************************************************
 
****************************************************************************/
void USART_OUT(USART_TypeDef* USARTx, uint8_t *Data,...)
{ 
	USART_ClearFlag(USARTx,USART_FLAG_TC);

	while(*Data!=0)
	{	
		USART_SendData(USARTx, *Data++);
		while(USART_GetFlagStatus(USARTx, USART_FLAG_TC)==RESET);
	}
}

/******************************************************
						   			  
**********************************************************/
#if 0
char *itoa(int value, char *string, int radix)
{
    int     i, d;
    int     flag = 0;
    char    *ptr = string;

    /* This implementation only works for decimal numbers. */
    if (radix != 10)
    {
        *ptr = 0;
        return string;
    }

    if (!value)
    {
        *ptr++ = 0x30;
        *ptr = 0;
        return string;
    }

    /* if this is a negative value insert the minus sign. */
    if (value < 0)
    {
        *ptr++ = '-';

        /* Make the value positive. */
        value *= -1;
    }

    for (i = 10000; i > 0; i /= 10)
    {
        d = value / i;

        if (d || flag)
        {
            *ptr++ = (char)(d + 0x30);
            value -= (d * i);
            flag = 1;
        }
    }

    /* Null terminate the string. */
    *ptr = 0;

    return string;

} /* NCL_Itoa */
#endif
char *Bsp_IpToStr(uint32_t ip, char *pstring)
{
//	u8 decdata = 0;
	char *ptr = pstring;
//	char dectostrbuf[8] = {0};
//	decdata = (ip>>24);
	sprintf(ptr,"%d.%d.%d.%d",\
		((ip>>24)&0xff),((ip>>16)&0xff),((ip>>8)&0xff),((ip>>0)&0xff));
	#if 0
	itoa((int)decdata,dectostrbuf,10);
	strcat(ptr, (char *)dectostrbuf);
	strcat(ptr, ".");
	memset(dectostrbuf,0,strlen(dectostrbuf));
	decdata = (ip>>16);
	itoa((int)decdata,dectostrbuf,10);
	strcat(ptr, (char *)dectostrbuf);
	strcat(ptr, ".");
	memset(dectostrbuf,0,strlen(dectostrbuf));
	decdata = (ip>>8);
	itoa((int)decdata,dectostrbuf,10);
	strcat(ptr, (char *)dectostrbuf);
	strcat(ptr, ".");
	memset(dectostrbuf,0,strlen(dectostrbuf));
	decdata = (ip>>0);
	itoa((int)decdata,dectostrbuf,10);
	strcat(ptr, (char *)dectostrbuf);
	#endif
	return pstring;
}

