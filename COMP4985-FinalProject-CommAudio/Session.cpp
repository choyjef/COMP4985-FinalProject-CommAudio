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

char filerino[] = "ShakeYourBootay.wav";

/*------------------------------------------------------------------------------------------------------------------
--	FUNCTION:		initServer
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
--	INTERFACE:		void initServer(HWND hwnd, int protocol)
--							HWND hwnd: window handle to send async connect events to
--							int protocol: protocol selected
--
--	RETURNS:
--
--
--	NOTES:			Initializes the server of the corresponding protocol type.
----------------------------------------------------------------------------------------------------------------------*/
void initServer(HWND hwnd, int protocol) {
	if (protocol == TCP_SELECTED) {
		OutputDebugStringA("TCP Selected\n");
		listenTCP(hwnd);
	}
	else {
		OutputDebugStringA("UDP Selected\n");
		listenUDP(hwnd);
	}
}

/*------------------------------------------------------------------------------------------------------------------
--	FUNCTION:		initClient
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
--	INTERFACE:		void initClient(HWND hwnd, int protocol)
--							HWND hwnd: window handle to send async connect events to
--							int protocol: protocol selected
--
--	RETURNS:
--
--
--	NOTES:			Initializes the client of the corresponding protocol type.
----------------------------------------------------------------------------------------------------------------------*/
void initClient(HWND hwnd, int protocol) {
	if (protocol == TCP_SELECTED) {
		OutputDebugStringA("TCP Selected\n");
		connectTCP(hwnd);
	}
	else {
		OutputDebugStringA("UDP Selected\n");
		connectUDP();
	}
}

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
--	FUNCTION:		listenUDP
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
--	INTERFACE:		void listenUDP(HWND hwnd)
--							HWND hwnd: not used
--
--	RETURNS:
--
--
--	NOTES:			Initializes the socket and begins listening for datagrams.
----------------------------------------------------------------------------------------------------------------------*/
void listenUDP(HWND hwnd) {

	// create passive udp socket
	SOCKET sd;
	SOCKADDR_IN InternetAddr;
	HANDLE ThreadHandle;
	DWORD ThreadId;

	if ((sd = WSASocket(PF_INET, SOCK_DGRAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED)) < 0) {
		OutputDebugStringA("Can't create a DGRAM socket\n");
		return;
	}

	InternetAddr.sin_family = AF_INET;
	InternetAddr.sin_addr.s_addr = htonl(INADDR_ANY);
	InternetAddr.sin_port = htons(getPort());

	if (bind(sd, (PSOCKADDR)&InternetAddr, sizeof(InternetAddr)) == SOCKET_ERROR) {
		OutputDebugStringA("bind failed\n");
		return;
	}

	if ((SInfo = (LPSOCKET_INFORMATION)GlobalAlloc(GPTR,
		sizeof(SOCKET_INFORMATION))) == NULL)
	{
		char errorMessage[1024];
		sprintf_s(errorMessage, "GlobalAlloc() failed with error %d\n", GetLastError());
		OutputDebugStringA(errorMessage);
		return;
	}

	SInfo->Socket = sd;
	SInfo->BytesSEND = 0;
	SInfo->BytesRECV = 0;

	if ((ThreadHandle = CreateThread(NULL, 0, UDPServerWorkerThread, (LPVOID)SInfo, 0, &ThreadId)) == NULL) {
		OutputDebugStringA("CreateThread failed");
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

/*------------------------------------------------------------------------------------------------------------------
--	FUNCTION:		connectUDP
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
--	INTERFACE:		void connectUDP() 
--
--	RETURNS:
--
--
--	NOTES:			Initializes the socket and creates the thread for sending data through socket.
----------------------------------------------------------------------------------------------------------------------*/
void connectUDP() {
	SOCKET sd;
	struct hostent	*hp;
	struct sockaddr_in server, client;
	char errorMessage[1024];
	int iRC;
	HANDLE ThreadHandle;
	DWORD ThreadId;
	LPCLIENT_THREAD_PARAMS threadParams;

	if ((sd = socket(PF_INET, SOCK_DGRAM, 0)) < 0) {
		OutputDebugStringA("Could not create DGRAM socket\n");
		return;
	}

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

	// bind local address to the socket
	memset((char *)&client, 0, sizeof(client));
	client.sin_family = AF_INET;
	client.sin_port = htons(7000);
	client.sin_addr.s_addr = htonl(INADDR_ANY);

	if (bind(sd, (sockaddr *)&client, sizeof(client)) == -1) {
		OutputDebugStringA("Can't bind name to socket dgram");
		return;
	}

	if ((SInfo = (LPSOCKET_INFORMATION)GlobalAlloc(GPTR,
		sizeof(SOCKET_INFORMATION))) == NULL)
	{
		char errorMessage[1024];
		sprintf_s(errorMessage, "GlobalAlloc() failed with error %d\n", GetLastError());
		OutputDebugStringA(errorMessage);
		return;
	}
	SInfo->Socket = sd;
	SInfo->BytesSEND = 0;
	SInfo->BytesRECV = 0;

	threadParams = (LPCLIENT_THREAD_PARAMS)GlobalAlloc(GPTR, sizeof(CLIENT_THREAD_PARAMS));

	threadParams->SI = *SInfo;
	threadParams->sin = server;

	if ((ThreadHandle = CreateThread(NULL, 0, UDPClientWorkerThread, (LPVOID)threadParams, 0, &ThreadId)) == NULL) {
		OutputDebugStringA("CreateThread failed");
		return;
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

	// start dll
	if ((err = WSAStartup(MAKEWORD(2, 2), &WSAData)) != 0) {
		//printf("DLL not found!\n");
		OutputDebugStringA("DLL not found!");
		exit(1);
	}
	//printf("startup success \n");
	OutputDebugStringA("startup success");
	// open wave file
	if (!OpenWaveFile(filerino)) {
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
	client.sin_addr.s_addr = inet_addr(UNICAST_ADDRESS);

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

	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = INADDR_ANY; // Stack assigns the local IP address
	sin.sin_port = htons(UNICAST_PORT);

	// Name the local socket with values in sin structure
	if (bind(sock, (struct sockaddr *) &sin, sizeof(sin)) < 0) {
		printf("could not bind socket\n");
		return;
	}
	printf("socket bound \n");

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

	//if ((ThreadHandle = CreateThread(NULL, 0, ReceiveAudioWorkerThread, (LPVOID)SInfo, 0, &ThreadId)) == NULL) {
	//	printf("CreateThread failed\n");
	//	return;
	//}

}