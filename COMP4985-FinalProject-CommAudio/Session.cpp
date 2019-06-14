/*------------------------------------------------------------------------------------------------------------------
--	SOURCE FILE:	Session.cpp - The session layer. Handles creation of connection between peers, initialization
--						and binding of sockets.
--
--
--	PROGRAM:		Protocol Analysis
--
--
--	FUNCTIONS:		void listenTCP(HWND hwnd);
--					void listenUDP(HWND hwnd);
--					void initServer(HWND hwnd, int protocol);
--					void initClient(HWND hwnd, int protocol);
--					void connectTCP(HWND hwnd);
--					void connectUDP();
--
--
--
--	DATE:			Feb 13, 2019
--
--
--	REVISIONS:
--
--
--	DESIGNER:		Jeffrey Choy
--
--
--	PROGRAMMER:		Jeffrey Choy
--
--
--	NOTES:
--
----------------------------------------------------------------------------------------------------------------------*/
#include "Session.h"

#define MAXADDRSTR  16
//char filerino[] = "GoodAssIntro.wav";
#define TIMECAST_ADDR "234.5.6.7"
#define TIMECAST_PORT 8910
char achMCAddr[MAXADDRSTR] = TIMECAST_ADDR;
u_long lMCAddr;
u_short nPort = TIMECAST_PORT;
struct ip_mreq stMreq;         /* Multicast interface structure */
// SERVER
#define TIMECAST_TTL    2
#define TIMECAST_INTRVL 3
u_long  lTTL = TIMECAST_TTL;
u_short nInterval = TIMECAST_INTRVL;
int nRet;
SOCKADDR_IN stLclAddr, stSrcAddr;

//LPSOCKET_INFORMATION SInfo;


/*------------------------------------------------------------------------------------------------------------------
--	FUNCTION:		listenTCP
--
--
--	DATE:			February 13, 2019
--
--
--	REVISIONS:
--
--
--	DESIGNER:		Jeffrey Choy
--
--
--	PROGRAMMER:		Jeffrey Choy
--
--
--	INTERFACE:		void listenTCP(HWND hwnd) 
--							HWND hwnd: window handle to send async connect events to
--
--	RETURNS:
--
--
--	NOTES:			Initializes the socket and begins listening for connections.
----------------------------------------------------------------------------------------------------------------------*/
void listenTCP(HWND hwnd) {
	SOCKET Listen;
	SOCKADDR_IN InternetAddr;
	WSADATA wsaData;
	WORD wVersionRequested;

	wVersionRequested = MAKEWORD(2, 2);

	if ((WSAStartup(wVersionRequested, &wsaData)) != 0)
	{
		OutputDebugStringA("WSAStartup failed with error \n");
		return;
	}

	if ((Listen = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED)) == -1) {
		OutputDebugStringA("Can't create a stream socket");
		return;
	}

	WSAAsyncSelect(Listen, hwnd, WM_SOCKET, FD_ACCEPT | FD_CLOSE);
	InternetAddr.sin_family = AF_INET;
	InternetAddr.sin_addr.s_addr = htonl(INADDR_ANY);
	InternetAddr.sin_port = htons(getPort());

	if (bind(Listen, (PSOCKADDR)&InternetAddr, sizeof(InternetAddr)) == SOCKET_ERROR) {
		OutputDebugStringA("bind failed");
		return;
	}

	if (listen(Listen, 1)) {
		OutputDebugStringA("Listen failed");
		return;
	}
}


/*------------------------------------------------------------------------------------------------------------------
--	FUNCTION:		connectTCP
--
--
--	DATE:			February 13, 2019
--
--
--	REVISIONS:
--
--
--	DESIGNER:		Jeffrey Choy
--
--
--	PROGRAMMER:		Jeffrey Choy
--
--
--	INTERFACE:		void connectTCP(HWND hwnd)
--							HWND hwnd: the window to send async ACCEPT messages to
--
--	RETURNS:
--
--
--	NOTES:			Initializes the socket and attempts to connect to the host through async message.
----------------------------------------------------------------------------------------------------------------------*/
void connectTCP(HWND hwnd) {
	SOCKET sd;
	struct hostent	*hp;
	struct sockaddr_in server;
	char errorMessage[1024];
	int iRC;

	WSADATA wsaData;
	WORD wVersionRequested;

	wVersionRequested = MAKEWORD(2, 2);

	if ((WSAStartup(wVersionRequested, &wsaData)) != 0)
	{
		OutputDebugStringA("WSAStartup failed with error \n");
		return;
	}

	// create the socket
	if ((sd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
	{
		OutputDebugStringA("Cannot create socket");
		return;
	}

	// WSAAsyncSelect requesting only FD_CONNECT
	WSAAsyncSelect(sd, hwnd, WM_SOCKET, FD_CONNECT);

	// Initialize and set up the address structure
	memset((char *)&server, 0, sizeof(struct sockaddr_in));
	server.sin_family = AF_INET;
	server.sin_port = htons(getPort());
	if ((hp = gethostbyname(getHostIP())) == NULL)
	{
		OutputDebugStringA("Unknown server address\n");
		return;
	}

	// Copy the server address
	memcpy((char *)&server.sin_addr, hp->h_addr, hp->h_length);

	// Connecting to the server
	iRC = connect(sd, (struct sockaddr *)&server, sizeof(server));
	if (iRC == SOCKET_ERROR)
	{
		if ((iRC = WSAGetLastError()) != WSAEWOULDBLOCK) {
			OutputDebugStringA("Can't connect to server\n");
			return;
		}
	}

}


void initUnicastSend() {
	WSADATA WSAData;
	int err;
	SOCKET sock;
	struct	sockaddr_in client;
	char* buf;
	HANDLE ThreadHandle;
	DWORD ThreadId;
	LPCLIENT_THREAD_PARAMS threadParams;

	// open wave file
	if (!OpenWaveFile(filePath)) {
		return;
	}
	//printf("file read success\n");
	OutputDebugStringA("file read success");

	// create socket

	if ((sock = WSASocket(AF_INET, SOCK_DGRAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED)) == INVALID_SOCKET) {
		//printf("error creating socket");
		OutputDebugStringA("error creating socket");
		return;
	}
	//printf("socket created \n");
	OutputDebugStringA("socket created");

	// fill address 
	memset((char *)&client, 0, sizeof(client));
	client.sin_family = AF_INET;
	client.sin_port = htons(UNICAST_PORT);
	client.sin_addr.s_addr = inet_addr(getHostIP());

	if ((SInfo = (LPSOCKET_INFORMATION)GlobalAlloc(GPTR,
		sizeof(SOCKET_INFORMATION))) == NULL)
	{
		char errorMessage[1024];
		sprintf_s(errorMessage, "GlobalAlloc() failed with error %d\n", GetLastError());
		OutputDebugStringA(errorMessage);
		return;
	}
	SInfo->Socket = sock;
	SInfo->BytesSEND = 0;
	SInfo->BytesRECV = 0;
	SInfo->peer = client;

	threadParams = (LPCLIENT_THREAD_PARAMS)GlobalAlloc(GPTR, sizeof(CLIENT_THREAD_PARAMS));

	threadParams->SI = *SInfo;
	threadParams->sin = client;

	if ((ThreadHandle = CreateThread(NULL, 0, UnicastSendAudioWorkerThread, (LPVOID)threadParams, 0, &ThreadId)) == NULL) {
		//printf("CreateThread failed");
		OutputDebugStringA("createthread failed");
		return;
	}
}

void initUnicastRecv() {
	WSADATA WSAData;
	SOCKET sock;
	struct sockaddr_in sin;
	int err;
	HANDLE ThreadHandle;
	DWORD ThreadId;

	if ((sock = WSASocket(AF_INET, SOCK_DGRAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED)) == INVALID_SOCKET) {
		OutputDebugStringA("error creating socket");
		return;
	}
	OutputDebugStringA("socket created \n");

	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = INADDR_ANY; // Stack assigns the local IP address
	sin.sin_port = htons(UNICAST_PORT);

	// Name the local socket with values in sin structure
	if (bind(sock, (struct sockaddr *) &sin, sizeof(sin)) < 0) {
		OutputDebugStringA("could not bind socket\n");
		return;
	}
	OutputDebugStringA("socket bound \n");

	if ((SInfo = (LPSOCKET_INFORMATION)GlobalAlloc(GPTR,
		sizeof(SOCKET_INFORMATION))) == NULL)
	{
		char errorMessage[1024];
		sprintf_s(errorMessage, "GlobalAlloc() failed with error %d\n", GetLastError());
		OutputDebugStringA(errorMessage);
		return;
	}
	SInfo->Socket = sock;
	SInfo->BytesSEND = 0;
	SInfo->BytesRECV = 0;

	if ((ThreadHandle = CreateThread(NULL, 0, UnicastReceiveAudioWorkerThread, (LPVOID)SInfo, 0, &ThreadId)) == NULL) {
		OutputDebugStringA("CreateThread failed\n");
		return;
	}

}

VOID initMulticastRecv() {
	WSADATA WSAData;
	SOCKET sock;
	struct sockaddr_in sin;
	int err;
	HANDLE ThreadHandle;
	DWORD ThreadId;
	BOOL fFlag;

	// start dll
	if ((err = WSAStartup(MAKEWORD(2, 2), &WSAData)) != 0) {
		printf("DLL not found!\n");
		exit(1);
	}
	printf("startup success \n");

	if ((sock = WSASocket(AF_INET, SOCK_DGRAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED)) == INVALID_SOCKET) {
		printf("error creating socket");
		return;
	}
	printf("socket created \n");

	fFlag = TRUE;
	if (setsockopt(sock,
		SOL_SOCKET,
		SO_REUSEADDR,
		(char *)&fFlag,
		sizeof(fFlag)) == SOCKET_ERROR) {
		printf("setsockopt() SO_REUSEADDR failed, Err: %d\n",
			WSAGetLastError());
	};

	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = INADDR_ANY; // Stack assigns the local IP address
	sin.sin_port = htons(nPort);

	// Name the local socket with values in sin structure
	if (bind(sock, (struct sockaddr *) &sin, sizeof(sin)) < 0) {
		printf("could not bind socket\n");
		return;
	}
	printf("socket bound \n");

	/* Join the multicast group so we can receive from it */
	stMreq.imr_multiaddr.s_addr = inet_addr(achMCAddr);
	stMreq.imr_interface.s_addr = INADDR_ANY;
	nRet = setsockopt(sock,
		IPPROTO_IP,
		IP_ADD_MEMBERSHIP,
		(char *)&stMreq,
		sizeof(stMreq));
	if (nRet == SOCKET_ERROR) {
		printf(
			"setsockopt() IP_ADD_MEMBERSHIP address %s failed, Err: %d\n",
			achMCAddr, WSAGetLastError());
	}

	printf("Now waiting for time updates from the TimeCast server\n");
	printf("  multicast group address: %s, port number: %d\n",
		achMCAddr, nPort);

	if ((SInfo = (LPSOCKET_INFORMATION)GlobalAlloc(GPTR,
		sizeof(SOCKET_INFORMATION))) == NULL)
	{
		char errorMessage[1024];
		sprintf_s(errorMessage, "GlobalAlloc() failed with error %d\n", GetLastError());
		OutputDebugStringA(errorMessage);
		return;
	}
	SInfo->Socket = sock;
	SInfo->BytesSEND = 0;
	SInfo->BytesRECV = 0;

	if ((ThreadHandle = CreateThread(NULL, 0, MulticastReceiveAudioWorkerThread, (LPVOID)SInfo, 0, &ThreadId)) == NULL) {
		printf("CreateThread failed\n");
		return;
	}
}

VOID initMulticastSend() {
	WSADATA WSAData;
	int err;
	SOCKET sock;
	struct	sockaddr_in client;
	char* buf;
	HANDLE ThreadHandle;
	DWORD ThreadId;
	LPCLIENT_THREAD_PARAMS threadParams;
	BOOL fFlag;

	// start dll
	if ((err = WSAStartup(MAKEWORD(2, 2), &WSAData)) != 0) {
		printf("DLL not found!\n");
		exit(1);
	}
	printf("startup success \n");

	printf("Multicast Address:%s, Port:%d, IP TTL:%d, Interval:%d\n",
		achMCAddr, nPort, lTTL, nInterval);

	// open wave file
	if (!OpenWaveFile(filePath)) {
		printf("Failed to open file");
		return;
	}
	printf("file read success\n");

	// create socket

	if ((sock = WSASocket(AF_INET, SOCK_DGRAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED)) == INVALID_SOCKET) {
		printf("error creating socket");
		return;
	}
	printf("socket created \n");
	stLclAddr.sin_family = AF_INET;
	stLclAddr.sin_addr.s_addr = htonl(INADDR_ANY); /* any interface */
	stLclAddr.sin_port = 0;                 /* any port */
	nRet = bind(sock,
		(struct sockaddr*) &stLclAddr,
		sizeof(stLclAddr));
	if (nRet == SOCKET_ERROR) {
		printf("bind() port: %d failed, Err: %d\n", nPort,
			WSAGetLastError());
	}

	stMreq.imr_multiaddr.s_addr = inet_addr(achMCAddr);
	stMreq.imr_interface.s_addr = INADDR_ANY;
	nRet = setsockopt(sock,
		IPPROTO_IP,
		IP_ADD_MEMBERSHIP,
		(char *)&stMreq,
		sizeof(stMreq));
	if (nRet == SOCKET_ERROR) {
		printf(
			"setsockopt() IP_ADD_MEMBERSHIP address %s failed, Err: %d\n",
			achMCAddr, WSAGetLastError());
	}

	/* Set IP TTL to traverse up to multiple routers */
	nRet = setsockopt(sock,
		IPPROTO_IP,
		IP_MULTICAST_TTL,
		(char *)&lTTL,
		sizeof(lTTL));
	if (nRet == SOCKET_ERROR) {
		printf("setsockopt() IP_MULTICAST_TTL failed, Err: %d\n",
			WSAGetLastError());
	}

	/* Disable loopback */
	fFlag = FALSE;
	nRet = setsockopt(sock,
		IPPROTO_IP,
		IP_MULTICAST_LOOP,
		(char *)&fFlag,
		sizeof(fFlag));
	if (nRet == SOCKET_ERROR) {
		printf("setsockopt() IP_MULTICAST_LOOP failed, Err: %d\n",
			WSAGetLastError());
	}

	// fill address 
	memset((char *)&client, 0, sizeof(client));
	client.sin_family = AF_INET;
	client.sin_port = htons(nPort);
	client.sin_addr.s_addr = inet_addr(achMCAddr);

	if ((SInfo = (LPSOCKET_INFORMATION)GlobalAlloc(GPTR,
		sizeof(SOCKET_INFORMATION))) == NULL)
	{
		char errorMessage[1024];
		sprintf_s(errorMessage, "GlobalAlloc() failed with error %d\n", GetLastError());
		OutputDebugStringA(errorMessage);
		return;
	}
	SInfo->Socket = sock;
	SInfo->BytesSEND = 0;
	SInfo->BytesRECV = 0;
	SInfo->peer = client;

	threadParams = (LPCLIENT_THREAD_PARAMS)GlobalAlloc(GPTR, sizeof(CLIENT_THREAD_PARAMS));

	threadParams->SI = *SInfo;
	threadParams->sin = client;

	if ((ThreadHandle = CreateThread(NULL, 0, MulticastSendAudioWorkerThread, (LPVOID)threadParams, 0, &ThreadId)) == NULL) {
		printf("CreateThread failed");
		return;
	}
}

/*------------------------------------------------------------------------------------------------------------------
--	FUNCTION:		initVoipSend
--
--
--	DATE:			April 9, 2019
--
--
--	REVISIONS:
--
--
--	DESIGNER:		Kiaan Castillo
--
--
--	PROGRAMMER:		Kiaan Castillo
--
--
--	INTERFACE:		void initVoipSend()
--
--
--	RETURNS:		void
--
--
--	NOTES:			Handles setup for UDP sending for voice chat
----------------------------------------------------------------------------------------------------------------------*/
void initVoipSend()
{
	WSADATA WSAData;
	DWORD wVersionRequested;
	SOCKET sock;
	struct sockaddr_in client, peer;
	int iRC;
	HANDLE SendThreadHandle, RecvThreadHandle, RecordThreadHandle, PlayThreadHandle;
	DWORD SendThreadId, RecvThreadId, RecordThreadId, PlayThreadId;
	LPCLIENT_THREAD_PARAMS threadParams;

	//wVersionRequested = MAKEWORD(2, 2);

	//if ((WSAStartup(wVersionRequested, &WSAData)) != 0)
	//{
	//	printf("WSAStartup failed with error \n");
	//	return;
	//}



	// create socket
	if ((sock = WSASocket(AF_INET, SOCK_DGRAM, IPPROTO_UDP, NULL, 0, WSA_FLAG_OVERLAPPED)) == INVALID_SOCKET) {
		//printf("error creating socket");
		OutputDebugStringA("error creating socket");
		char errmsg[128];
		sprintf_s(errmsg, 128, "Error Number: %d\n", WSAGetLastError());
		OutputDebugString(errmsg);
		return;
	}
	//printf("socket created \n");
	OutputDebugStringA("socket created");

	// fill address 
	memset((char *)&client, 0, sizeof(client));
	client.sin_family = AF_INET;
	client.sin_port = htons(PORT);
	client.sin_addr.s_addr = INADDR_ANY;

	memset((char *)&peer, 0, sizeof(client));
	peer.sin_family = AF_INET;
	peer.sin_port = htons(PORT);
	peer.sin_addr.s_addr = inet_addr(getHostIP());



	//if (bind(sock, (sockaddr *)&client, sizeof(client)) == -1) {
	//	char errmsg[1024];
	//	OutputDebugStringA("Can't bind name to socket dgram\n");
	//	sprintf_s(errmsg, 128, "Error Number: %d\n", WSAGetLastError());
	//	OutputDebugString(errmsg);
	//	return;
	//}

	if ((SInfo = (LPSOCKET_INFORMATION)GlobalAlloc(GPTR,
		sizeof(SOCKET_INFORMATION))) == NULL)
	{
		char errorMessage[128];
		sprintf_s(errorMessage, "GlobalAlloc() failed with error %d\n", GetLastError());
		OutputDebugStringA(errorMessage);
		return;
	}
	SInfo->Socket = sock;
	SInfo->BytesSEND = 0;
	SInfo->BytesRECV = 0;
	SInfo->peer = peer;


	threadParams = (LPCLIENT_THREAD_PARAMS)GlobalAlloc(GPTR, sizeof(CLIENT_THREAD_PARAMS));

	threadParams->SI = *SInfo;
	threadParams->sin = peer;

	// Create record audio thread
	if ((RecordThreadHandle = CreateThread(NULL, 0, VoIPRecordAudioThread, (LPVOID)NULL, 0, &RecordThreadId)) == NULL)
	{
		OutputDebugStringA("CreateThread recordaudio failed");
		return;
	}

	// Create send audio thread
	if ((SendThreadHandle = CreateThread(NULL, 0, VoIPSendAudioWorkerThread, (LPVOID)threadParams, 0, &SendThreadId)) == NULL)
	{
		OutputDebugStringA("CreateThread sendaudio failed");
		return;
	}

	//// Create recv audio thread
	//if ((RecvThreadHandle = CreateThread(NULL, 0, VoIPReceiveAudioWorkerThread, (LPVOID)threadParams, 0, &RecvThreadId)) == NULL)
	//{
	//	OutputDebugStringA("CreateThread recvaudio failed");
	//	return;
	//}

	//// Create play audio thread
	//if ((PlayThreadHandle = CreateThread(NULL, 0, VoIPPlayAudioThread, (LPVOID)NULL, 0, &PlayThreadId)) == NULL) 
	//{
	//	OutputDebugStringA("CreateThread playaudio failed");
	//	return;
	//}
}

/*------------------------------------------------------------------------------------------------------------------
--	FUNCTION:		initVoipRecv
--
--
--	DATE:			April 9, 2019
--
--
--	REVISIONS:
--
--
--	DESIGNER:		Kiaan Castillo
--
--
--	PROGRAMMER:		Kiaan Castillo
--
--
--	INTERFACE:		void initVoipRecv()
--
--
--	RETURNS:		void
--
--
--	NOTES:			Handles setup for UDP receiving for voice chat
----------------------------------------------------------------------------------------------------------------------*/
void initVoipRecv()
{
	WSADATA WSAData;
	DWORD wVersionRequested;
	SOCKET sock;
	struct sockaddr_in client;
	int iRC;
	HANDLE SendThreadHandle, RecvThreadHandle, RecordThreadHandle, PlayThreadHandle;
	DWORD SendThreadId, RecvThreadId, RecordThreadId, PlayThreadId;
	LPCLIENT_THREAD_PARAMS threadParams;


	updateStatusLogDisplay("Started voice chat");

	// create socket
	if ((sock = WSASocket(AF_INET, SOCK_DGRAM, IPPROTO_UDP, NULL, 0, WSA_FLAG_OVERLAPPED)) == INVALID_SOCKET) {

		OutputDebugStringA("error creating socket");
		char errmsg[128];
		sprintf_s(errmsg, 128, "Error Number: %d\n", WSAGetLastError());
		OutputDebugString(errmsg);
		return;
	}

	OutputDebugStringA("socket created");

	// fill address 
	memset((char *)&client, 0, sizeof(client));
	client.sin_family = AF_INET;
	client.sin_port = htons(PORT);
	client.sin_addr.s_addr = INADDR_ANY;

	if (bind(sock, (sockaddr *)&client, sizeof(client)) == -1) {
		char errmsg[1024];
		OutputDebugStringA("Can't bind name to socket dgram\n");
		sprintf_s(errmsg, 128, "Error Number: %d\n", WSAGetLastError());
		OutputDebugString(errmsg);
		return;
	}

	if ((SInfo = (LPSOCKET_INFORMATION)GlobalAlloc(GPTR,
		sizeof(SOCKET_INFORMATION))) == NULL)
	{
		char errorMessage[128];
		sprintf_s(errorMessage, "GlobalAlloc() failed with error %d\n", GetLastError());
		OutputDebugStringA(errorMessage);
		return;
	}
	SInfo->Socket = sock;
	SInfo->BytesSEND = 0;
	SInfo->BytesRECV = 0;

	if (!WavePlayOpen(&wo, hwnd, NUM_CHANNELS, FREQUENCY, NUM_RECORD_BITS))
	{
		updateStatusLogDisplay("Status: Failed to open input device");
		return;
	}


	// Create recv audio thread
	if ((RecvThreadHandle = CreateThread(NULL, 0, VoIPReceiveAudioWorkerThread, (LPVOID)SInfo, 0, &RecvThreadId)) == NULL)
	{
		OutputDebugStringA("CreateThread recvaudio failed");
		return;
	}

}