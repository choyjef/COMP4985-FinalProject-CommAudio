/*------------------------------------------------------------------------------------------------------------------
--	SOURCE FILE:	Transport.cpp - The transport layer. Handles all reading and writing to/from ports with 
--											worker threads and completion routines.
--
--
--	PROGRAM:		Protocol Analysis
--
--
--	FUNCTIONS:		void connectTCP(HWND hwnd);
--					DWORD WINAPI TCPServerWorkerThread(LPVOID lpParameter);
--					void CALLBACK TCPRecvCompRoutine(DWORD Error, 
--							DWORD BytesTransferred, LPWSAOVERLAPPED Overlapped, DWORD InFlags);
--					DWORD WINAPI TCPClientWorkerThread(LPVOID lpParameter);
--					void CALLBACK TCPSendCompRoutine(DWORD Error, 
--							DWORD BytesTransferred, LPWSAOVERLAPPED Overlapped, DWORD InFlags);
--					DWORD WINAPI UDPServerWorkerThread(LPVOID lpParameter);
--					void CALLBACK UDPRecvCompRoutine(DWORD Error, 
--							DWORD BytesTransferred, LPWSAOVERLAPPED Overlapped, DWORD InFlags);
--					DWORD WINAPI UDPClientWorkerThread(LPVOID lpParameter);
--					void CALLBACK UDPSendCompRoutine(DWORD Error, 
--							DWORD BytesTransferred, LPWSAOVERLAPPED Overlapped, DWORD InFlags);
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
#include "Transport.h"

char recvbuf[DATA_BUFSIZE];
int i = 1;
int sentPacketsCount = 0;
int recvPacketsCount = 0;
char wavFileList[DATA_BUFSIZE];
sockaddr_in server;

// jeff_unicast variables
int nPacketsSent = 0;
int nPacketsRecv = 0;
int nBytesRecv = 0;
int nBytesSent = 0;
int playbackSendPosition = 0;

/*------------------------------------------------------------------------------------------------------------------
--	FUNCTION:		getDirectory
--
--
--	DATE:			April 9, 2019
--
--
--	REVISIONS:
--
--
--	DESIGNER:		Jenny Ly
--
--
--	PROGRAMMER:		Jenny Ly
--
--
--	INTERFACE:		char* getDirectory(int size)
--						int size: size of buffer
--
--	RETURNS:		char*
--
--
--	NOTES:			Get all the .wav file in current server directory and returns the files in an array.
----------------------------------------------------------------------------------------------------------------------*/
char* getDirectory(int size) {
	DWORD fileError;
	char directory[PACKET_SIZE];
	
	HANDLE hFind = INVALID_HANDLE_VALUE;
	WIN32_FIND_DATA findData;


	//Find files in current server directory
	//1) find current directory
	GetCurrentDirectory(PACKET_SIZE, directory);
	strcat_s(directory, "\\\*");
	//2) find first file in current directory
	hFind = FindFirstFile(directory, &findData);

	char fileName[260];
	strcpy_s(fileName, findData.cFileName);
	if (strstr(fileName, ".wav")) {

		strncat_s(wavFileList, findData.cFileName, PACKET_SIZE - strlen(wavFileList) - 1);
		strncat_s(wavFileList, "\0", PACKET_SIZE - strlen(wavFileList) - 1);
	}

	if (INVALID_HANDLE_VALUE == hFind)
	{
		char errorMessage[1024];
		sprintf_s(errorMessage, "Unable to find first file\n", WSAGetLastError());
		OutputDebugStringA(errorMessage);
		return FALSE;
	}
	//3) file rest of files in directory 
	while (FindNextFile(hFind, &findData) != 0) {
		strcpy_s(fileName, findData.cFileName);
		if (strstr(fileName, ".wav")) {

			strncat_s(wavFileList, findData.cFileName, PACKET_SIZE - strlen(wavFileList) - 1);
			strncat_s(wavFileList, "\n", PACKET_SIZE - strlen(wavFileList) - 1);
		}
	}

	fileError = GetLastError();
	if (fileError != ERROR_NO_MORE_FILES)
	{
		char errorMessage[1024];
		sprintf_s(errorMessage, "Unable to find first file\n", WSAGetLastError());
		OutputDebugStringA(errorMessage);
	}

	FindClose(hFind);

	return wavFileList;
}

/*------------------------------------------------------------------------------------------------------------------
--	FUNCTION:		TCPServerWorkerThread
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
--	PROGRAMMER:		Jeffrey Choy, Jenny Ly
--
--
--	INTERFACE:		DWORD WINAPI TCPServerWorkerThread(LPVOID lpParameter)
--							LPVOID lpParameter: struct container parameters to pass to the thread
--
--	RETURNS:
--
--
--	NOTES:			Creates a new thread to handle TCP server functionality.
----------------------------------------------------------------------------------------------------------------------*/
DWORD WINAPI TCPServerWorkerThread(LPVOID lpParameter) {
	
	DWORD Flags, Index, RecvBytes, SendBytes;
	WSAEVENT EventArray[1];
	LPSOCKET_INFORMATION SocketInfo = (LPSOCKET_INFORMATION)lpParameter;;
	EventArray[0] = WSACreateEvent();

	generateTCPSendBufferData(filePath, DATA_BUFSIZE);

	SocketInfo->DataBuf.buf = sendBuffer;
	SocketInfo->DataBuf.len = DATA_BUFSIZE;
	ZeroMemory(&(SocketInfo->Overlapped), sizeof(WSAOVERLAPPED));

	if (sendBuffer < addressOfEnd) {
		sendBuffer += DATA_BUFSIZE;
	}
	else {
		return FALSE;
	}
	

	Flags = 0;
	while (TRUE) {
		
		if (WSASend(SocketInfo->Socket, &(SocketInfo->DataBuf), 1, &RecvBytes, Flags, &(SocketInfo->Overlapped), TCPServerSendWavCompRoutine) == SOCKET_ERROR) {
			if (WSAGetLastError() != WSA_IO_PENDING) {
				char errorMessage[1024];
				sprintf_s(errorMessage, "WSASend() failed with error %d\n", WSAGetLastError());
				OutputDebugStringA(errorMessage);
				return FALSE;
			}
		}

		// idle in alertable state for completion routine return
		while (TRUE) {
			Index = WSAWaitForMultipleEvents(1, EventArray, FALSE, WSA_INFINITE, TRUE);
			if (Index == WSA_WAIT_FAILED) {
				OutputDebugStringA("Wait for multiple event failed");
			}

		}
	}
	
}


/*------------------------------------------------------------------------------------------------------------------
--	FUNCTION:		TCPServerSendWavCompRoutine
--
--
--	DATE:			February 13, 2019
--
--
--	REVISIONS:
--
--
--	DESIGNER:		Jenny Ly
--
--
--	PROGRAMMER:		Jenny Ly
--
--
--	INTERFACE:		void CALLBACK TCPServerSendWavCompRoutine(DWORD Error, DWORD BytesTransferred,
--						LPWSAOVERLAPPED Overlapped, DWORD InFlags)
--							DWORD Error: struct container parameters to pass to the thread
--							DWORD BytesTransferred: number of bytes transferred from operation
--							LPWSAOVERLAPPED Overlapped: Overlapped structure for overlapped reading from socket
--							DWORD InFlags: input flags for completion routine
--
--	RETURNS:
--
--
--	NOTES:			Handles completion of receive action, parses data, stores meta data
--						and posts another receive.
----------------------------------------------------------------------------------------------------------------------*/
void CALLBACK TCPServerSendWavCompRoutine(DWORD Error, DWORD BytesTransferred, LPWSAOVERLAPPED Overlapped, DWORD InFlags) {
	DWORD RecvBytes;
	LPSOCKET_INFORMATION SocketInfo = (LPSOCKET_INFORMATION)Overlapped;
	DWORD Flags = 0;

	OutputDebugStringA("Send complete\n");
	char errorMessage[1024];
	sprintf_s(errorMessage, "Bytes transferred: %d\n", BytesTransferred);
	OutputDebugStringA(errorMessage);

	SocketInfo->DataBuf.buf = sendBuffer;
	SocketInfo->DataBuf.len = DATA_BUFSIZE;
	ZeroMemory(&(SocketInfo->Overlapped), sizeof(WSAOVERLAPPED));

	if (sendBuffer < addressOfEnd) {
		sendBuffer += DATA_BUFSIZE;
	}
	else {
		return;
	}


	if (WSASend(SocketInfo->Socket, &(SocketInfo->DataBuf), 1, &RecvBytes, Flags, &(SocketInfo->Overlapped), TCPServerSendWavCompRoutine) == SOCKET_ERROR) {
		if (WSAGetLastError() != WSA_IO_PENDING) {
			char errorMessage[1024];
			sprintf_s(errorMessage, "WSASend() failed with error %d\n", WSAGetLastError());
			OutputDebugStringA(errorMessage);
			return;
		}
	}

}


/*------------------------------------------------------------------------------------------------------------------
--	FUNCTION:		TCPServerRecvCompRoutine
--
--
--	DATE:			February 13, 2019
--
--
--	REVISIONS:
--
--
--	DESIGNER:		Jenny Ly
--
--
--	PROGRAMMER:		Jenny Ly
--
--
--	INTERFACE:		void CALLBACK TCPServerRecvCompRoutine(DWORD Error, DWORD BytesTransferred,
--						LPWSAOVERLAPPED Overlapped, DWORD InFlags)
--							DWORD Error: struct container parameters to pass to the thread
--							DWORD BytesTransferred: number of bytes transferred from operation
--							LPWSAOVERLAPPED Overlapped: Overlapped structure for overlapped reading from socket
--							DWORD InFlags: input flags for completion routine
--
--	RETURNS:
--
--
--	NOTES:			Handles completion of receive action, parses data, stores meta data
--						and posts another receive.
----------------------------------------------------------------------------------------------------------------------*/
void CALLBACK TCPServerRecvCompRoutine(DWORD Error, DWORD BytesTransferred, LPWSAOVERLAPPED Overlapped, DWORD InFlags) {
	DWORD RecvBytes;
	LPSOCKET_INFORMATION SocketInfo = (LPSOCKET_INFORMATION)Overlapped;
	DWORD Flags = 0;
	
	generateTCPSendBufferData((SocketInfo->DataBuf.buf) + 1, DATA_BUFSIZE); //SocketInfo->DataBuf.buf is ding.wav (the filename is send by the client)

	SocketInfo->DataBuf.buf = sendBuffer;
	SocketInfo->DataBuf.len = DATA_BUFSIZE;
	ZeroMemory(&(SocketInfo->Overlapped), sizeof(WSAOVERLAPPED));

	if (sendBuffer < addressOfEnd) {
		sendBuffer += DATA_BUFSIZE;
	}
	else {
		return;
	}


	if (WSASend(SocketInfo->Socket, &(SocketInfo->DataBuf), 1, &RecvBytes, Flags, &(SocketInfo->Overlapped), TCPServerSendWavCompRoutine) == SOCKET_ERROR) {
		if (WSAGetLastError() != WSA_IO_PENDING) {
			char errorMessage[1024];
			sprintf_s(errorMessage, "WSASend() failed with error %d\n", WSAGetLastError());
			OutputDebugStringA(errorMessage);
			return;
		}
	}
}



/*------------------------------------------------------------------------------------------------------------------
--	FUNCTION:		TCPClientWorkerThread
--
--
--	DATE:			February 13, 2019
--
--
--	REVISIONS:		April 10, 2019
--
--
--	DESIGNER:		Jeffrey Choy
--
--
--	PROGRAMMER:		Jeffrey Choy, Jenny Ly
--
--
--	INTERFACE:		DWORD WINAPI TCPClientWorkerThread(LPVOID lpParameter)
--							LPVOID lpParameter: struct container parameters to pass to the thread
--
--	RETURNS:
--
--
--	NOTES:			Creates a new thread to handle TCP client functionality.
----------------------------------------------------------------------------------------------------------------------*/
DWORD WINAPI TCPClientWorkerThread(LPVOID lpParameter) {
	DWORD Flags, Index, RecvBytes, SendBytes;
	WSAEVENT EventArray[1];
	LPSOCKET_INFORMATION SocketInfo;
	char metaData[DATA_BUFSIZE];
	EventArray[0] = WSACreateEvent();
	SocketInfo = (LPSOCKET_INFORMATION)lpParameter;

	sprintf_s(metaData, "T%s", filePath);
	
	SocketInfo->DataBuf.buf = metaData; //filePath is ding.wav or whateve the user entered in the UI
	SocketInfo->DataBuf.len = DATA_BUFSIZE;
	ZeroMemory(&(SocketInfo->Overlapped), sizeof(WSAOVERLAPPED));

	Flags = 0;
	while (TRUE) {

		//Sends the .wav file title entered in the user UI
		if (WSASend(SocketInfo->Socket, &(SocketInfo->DataBuf), 1, &SendBytes, Flags, &(SocketInfo->Overlapped), TCPClientRecvDataCompRoutine) == SOCKET_ERROR) {
			if (WSAGetLastError() != WSA_IO_PENDING) {
				char errorMessage[1024];
				sprintf_s(errorMessage, "WSASend() failed with error %d\n", WSAGetLastError());
				OutputDebugStringA(errorMessage);
				return FALSE;
			}
		}

		// idle in alertable state for completion routine return
		while (TRUE) {
			Index = WSAWaitForMultipleEvents(1, EventArray, FALSE, WSA_INFINITE, TRUE);
			if (Index == WSA_WAIT_FAILED) {
				OutputDebugStringA("Wait for multiple event failed");
			}

		}
	}
}


/*------------------------------------------------------------------------------------------------------------------
--	FUNCTION:		TCPClientRecvDataCompRoutine
--
--
--	DATE:			February 13, 2019
--
--
--	REVISIONS:
--
--
--	DESIGNER:		Jenny Ly
--
--
--	PROGRAMMER:		Jenny Ly
--
--
--	INTERFACE:		void CALLBACK TCPClientRecvDataCompRoutine(DWORD Error, DWORD BytesTransferred,
--						LPWSAOVERLAPPED Overlapped, DWORD InFlags)
--							DWORD Error: struct container parameters to pass to the thread
--							DWORD BytesTransferred: number of bytes transferred from operation
--							LPWSAOVERLAPPED Overlapped: Overlapped structure for overlapped reading from socket
--							DWORD InFlags: input flags for completion routine
--
--	RETURNS:
--
--
--	NOTES:			Handles completion of receive action, parses data, stores meta data
--						and posts another receive.
----------------------------------------------------------------------------------------------------------------------*/
void CALLBACK TCPClientRecvDataCompRoutine(DWORD Error, DWORD BytesTransferred, LPWSAOVERLAPPED Overlapped, DWORD InFlags) {
	DWORD RecvBytes;
	DWORD Flags;
	char printout[DATA_BUFSIZE];

	// populate socket information through cast 
	LPSOCKET_INFORMATION SI = (LPSOCKET_INFORMATION)Overlapped;

	// error checking
	if (Error != 0) {
		char errorMessage[1024];
		sprintf_s(errorMessage, "I/O operation failed with error %d\n", Error);
		OutputDebugStringA(errorMessage);
		closesocket(SI->Socket);
		return;
	}
	if (BytesTransferred == 0) {
		char errorMessage[1024];
		sprintf_s(errorMessage, "Closing socket %d\n", SI->Socket);
		OutputDebugStringA(errorMessage);
		closesocket(SI->Socket);
		return;
	}

	FILE *fp;
	fopen_s(&fp, filePath, "ab");

	//first packet send by client is the filename, need to make sure filename is not included in .wav
	if (strstr(SI->DataBuf.buf, ".wav")) {
		Flags = 0;
		ZeroMemory(&(SI->Overlapped), sizeof(WSAOVERLAPPED));
		SI->DataBuf.len = DATA_BUFSIZE;
		SI->DataBuf.buf = recvbuf;

	}
	else { //write to file

		if (fwrite(SI->DataBuf.buf, sizeof(char), DATA_BUFSIZE, fp) == -1) {
			exit(1);
		}
	}

	fclose(fp);

	// prepare socket for another read
	Flags = 0;
	ZeroMemory(&(SI->Overlapped), sizeof(WSAOVERLAPPED));
	SI->DataBuf.len = DATA_BUFSIZE;
	SI->DataBuf.buf = recvbuf;

	// initiate another read with completion routine
	if (WSARecv(SI->Socket, &(SI->DataBuf), 1, &RecvBytes, &Flags, &(SI->Overlapped), TCPClientRecvDataCompRoutine) == SOCKET_ERROR) {
		if (WSAGetLastError() != WSA_IO_PENDING) {
			char errorMessage[1024];
			sprintf_s(errorMessage, "WSARecv() failed with error %d\n", WSAGetLastError());
			OutputDebugStringA(errorMessage);
			return;
		}
	}
}


DWORD WINAPI UnicastSendAudioWorkerThread(LPVOID lpParameter) {

	DWORD Flags, Index, SendBytes;
	WSAEVENT EventArray[1];
	CLIENT_THREAD_PARAMS* params;
	LPSOCKET_INFORMATION SocketInfo;
	char *buf;
	int peer_len = sizeof(params->sin);


	// send audio format data
	buf = (char *)malloc(sizeof(PCMWAVEFORMAT));
	memcpy(buf, &PCMWaveFmtRecord, sizeof(PCMWAVEFORMAT));

	params = (CLIENT_THREAD_PARAMS*)lpParameter;
	EventArray[0] = WSACreateEvent();
	SocketInfo = &(params->SI);

	SocketInfo->DataBuf.len = sizeof(PCMWAVEFORMAT);

	SocketInfo->DataBuf.buf = buf;
	ZeroMemory(&(SocketInfo->Overlapped), sizeof(WSAOVERLAPPED));
	Flags = 0;

	while (TRUE) {

		// listen on port with specified completion routine
		if (WSASendTo(SocketInfo->Socket, &(SocketInfo->DataBuf), 1, &SendBytes, Flags,
			(sockaddr *) &(params->sin), peer_len, &(SocketInfo->Overlapped), UnicastAudioSendCompRoutine) == SOCKET_ERROR) {
			if (WSAGetLastError() != WSA_IO_PENDING) {
				char err[1024];
				//printf("WSARecv() failed with error %d\n", WSAGetLastError());
				sprintf_s(err, "WSASendTo() failed with error: %d", WSAGetLastError());
				OutputDebugStringA(err);
				free(buf);
				return FALSE;
			}
		}

		// idle in alertable state for completion routine return
		while (TRUE) {
			Index = WSAWaitForMultipleEvents(1, EventArray, FALSE, WSA_INFINITE, TRUE);
			if (Index == WSA_WAIT_FAILED) {
				//printf("Wait for multiple event failed");
				OutputDebugStringA("wait for multiple event failed");
			}

		}
	}

}

void CALLBACK UnicastAudioSendCompRoutine(DWORD Error, DWORD BytesTransferred, LPWSAOVERLAPPED Overlapped, DWORD InFlags) {
	//using namespace TransferDetails;
	DWORD SendBytes, Flags;
	LPSOCKET_INFORMATION SocketInfo;
	sockaddr_in client;
	int client_len;
	SocketInfo = (LPSOCKET_INFORMATION)Overlapped;
	char *buf;
	char msg[1024];
	WSAEVENT EventArray[1];
	EventArray[0] = WSACreateEvent();
	int peer_len = sizeof(SocketInfo->peer);

	nPacketsSent++;
	nBytesSent += BytesTransferred;

	free(SocketInfo->DataBuf.buf);
	buf = (char *)malloc(PACKET_SIZE);
	Flags = 0;


	if (playbackSendPosition < WaveHeader.dwBufferLength) {
		DWORD bytesToRead;
		buf = (char *)malloc(PACKET_SIZE);


		if (playbackSendPosition + PACKET_SIZE > WaveHeader.dwBufferLength) {
			bytesToRead = WaveHeader.dwBufferLength - playbackSendPosition;
		}
		else {
			bytesToRead = PACKET_SIZE;
		}

		if (nPacketsSent % 10 == 0) {
			WSAWaitForMultipleEvents(1, EventArray, FALSE, 300, TRUE);
		}

		memcpy(buf, WaveHeader.lpData + playbackSendPosition, bytesToRead);
		SocketInfo->DataBuf.buf = buf;
		SocketInfo->DataBuf.len = PACKET_SIZE;
		playbackSendPosition += PACKET_SIZE;
		if (WSASendTo(SocketInfo->Socket, &(SocketInfo->DataBuf), 1, &SendBytes, Flags,
			(sockaddr *) &(SocketInfo->peer), peer_len, &(SocketInfo->Overlapped), UnicastAudioSendCompRoutine) == SOCKET_ERROR) {
			if (WSAGetLastError() != WSA_IO_PENDING) {
				printf("WSARecv() failed with error %d\n", WSAGetLastError());
				free(buf);
				return;
			}
		}
	}
}

DWORD WINAPI UnicastReceiveAudioWorkerThread(LPVOID lpParameter) {
	DWORD Flags, Index, RecvBytes;
	WSAEVENT EventArray[1];
	LPSOCKET_INFORMATION SocketInfo;
	sockaddr_in server;
	int server_len;
	char * buf;

	EventArray[0] = WSACreateEvent();
	SocketInfo = (LPSOCKET_INFORMATION)lpParameter;

	buf = (char *)malloc(PACKET_SIZE);
	SocketInfo->DataBuf.buf = buf;
	SocketInfo->DataBuf.len = PACKET_SIZE;
	ZeroMemory(&(SocketInfo->Overlapped), sizeof(WSAOVERLAPPED));
	Flags = 0;
	server_len = sizeof(server);

	while (TRUE) {
		if (WSARecvFrom(SocketInfo->Socket, &(SocketInfo->DataBuf), 1, &RecvBytes, &Flags,
			(sockaddr *)&server, &server_len, &(SocketInfo->Overlapped), UnicastAudioReceiveCompRoutine) == SOCKET_ERROR) {
			if (WSAGetLastError() != WSA_IO_PENDING) {
				printf("WSARecv() failed with error %d\n", WSAGetLastError());

				free(buf);
				return FALSE;
			}
		}

		while (TRUE) {
			Index = WSAWaitForMultipleEvents(1, EventArray, FALSE, 5000, TRUE);
			if (Index == WSA_WAIT_FAILED) {
				OutputDebugStringA("Wait for multiple event failed");
			}
		}
	}

}

void CALLBACK UnicastAudioReceiveCompRoutine(DWORD Error, DWORD BytesTransferred, LPWSAOVERLAPPED Overlapped, DWORD InFlags) {
	DWORD RecvBytes;
	DWORD Flags;
	sockaddr_in server;
	int server_len;
	char *buf;

	LPSOCKET_INFORMATION SI = (LPSOCKET_INFORMATION)Overlapped;

	if (Error != 0) {
		char errorMessage[1024];
		sprintf_s(errorMessage, "I/O operation failed with error %d\n", Error);
		OutputDebugStringA(errorMessage);
		closesocket(SI->Socket);
		return;
	}
	if (BytesTransferred == 0) {
		printf("Closing socket %d\n", SI->Socket);
		closesocket(SI->Socket);
		return;
	}

	nPacketsRecv++;
	nBytesRecv += BytesTransferred;
	printf("Received %d bytes\n", BytesTransferred);
	printf("Packets received: %d\n", nPacketsRecv);
	printf("Bytes received: %d\n", nBytesRecv);

	if (nPacketsRecv == 1) {
		memcpy(&PCMWaveFmtRecord, SI->DataBuf.buf, BytesTransferred);
		printPCMInfo();
		OpenOutputDevice();
	}
	else {

		playQueue.add_node(SI->DataBuf.buf, BytesTransferred);
		if (nPacketsRecv == NUM_BUFFS + 1) {
			waveOutPause(wo);
			streamPlayback();
			waveOutRestart(wo);
		}
	}

	ZeroMemory(&(SI->Overlapped), sizeof(WSAOVERLAPPED));
	free(SI->DataBuf.buf);
	buf = (char *)malloc(PACKET_SIZE);
	SI->DataBuf.buf = buf;
	SI->DataBuf.len = PACKET_SIZE;
	server_len = sizeof(server);
	Flags = 0;

	if (WSARecvFrom(SI->Socket, &(SI->DataBuf), 1, &RecvBytes, &Flags,
		(sockaddr *)&server, &server_len, &(SI->Overlapped), UnicastAudioReceiveCompRoutine) == SOCKET_ERROR) {
		if (WSAGetLastError() != WSA_IO_PENDING) {
			printf("WSARecv() failed with error %d\n", WSAGetLastError());

			free(buf);
			return;
		}
	}

}

DWORD WINAPI MulticastSendAudioWorkerThread(LPVOID lpParameter) {
	DWORD Flags, Index, SendBytes;
	WSAEVENT EventArray[1];
	CLIENT_THREAD_PARAMS* params;
	LPSOCKET_INFORMATION SocketInfo;
	char *buf;
	int peer_len = sizeof(params->sin);


	// send audio format data
	// this is apppending header info only to first packet (cause it's in the worker thread)
	// this will need to be appended to each packet
	buf = (char *)malloc(sizeof(PCMWAVEFORMAT));
	memcpy(buf, &PCMWaveFmtRecord, sizeof(PCMWAVEFORMAT));

	params = (CLIENT_THREAD_PARAMS*)lpParameter;
	EventArray[0] = WSACreateEvent();
	SocketInfo = &(params->SI);

	SocketInfo->DataBuf.len = sizeof(PCMWAVEFORMAT);

	SocketInfo->DataBuf.buf = buf;
	ZeroMemory(&(SocketInfo->Overlapped), sizeof(WSAOVERLAPPED));
	Flags = 0;

	while (TRUE) {

		// listen on port with specified completion routine
		if (WSASendTo(SocketInfo->Socket, &(SocketInfo->DataBuf), 1, &SendBytes, Flags,
			(sockaddr *) &(params->sin), peer_len, &(SocketInfo->Overlapped), MulticastAudioSendCompRoutine) == SOCKET_ERROR) {
			if (WSAGetLastError() != WSA_IO_PENDING) {
				printf("WSARecv() failed with error %d\n", WSAGetLastError());
				free(buf);
				return FALSE;
			}
		}

		// idle in alertable state for completion routine return
		while (TRUE) {
			Index = WSAWaitForMultipleEvents(1, EventArray, FALSE, WSA_INFINITE, TRUE);
			if (Index == WSA_WAIT_FAILED) {
				printf("Wait for multiple event failed");
			}

		}
	}

}

void CALLBACK MulticastAudioSendCompRoutine(DWORD Error, DWORD BytesTransferred, LPWSAOVERLAPPED Overlapped, DWORD InFlags) {
	DWORD SendBytes, Flags;
	LPSOCKET_INFORMATION SocketInfo;
	SocketInfo = (LPSOCKET_INFORMATION)Overlapped;
	char *buf;
	WSAEVENT EventArray[1];
	EventArray[0] = WSACreateEvent();
	int headerLength = sizeof(PCMWAVEFORMAT);

	int peer_len = sizeof(SocketInfo->peer);

	nPacketsSent++;
	nBytesSent += BytesTransferred;
	printf("Packets sent: %d\n", nPacketsSent);
	printf("Bytes sent: %d\n", nBytesSent);

	free(SocketInfo->DataBuf.buf);
	buf = (char *)malloc(PACKET_SIZE);
	Flags = 0;


	if (playbackSendPosition < WaveHeader.dwBufferLength) {
		DWORD bytesToRead;
		buf = (char *)malloc(PACKET_SIZE);


		if (playbackSendPosition + PACKET_SIZE - headerLength > WaveHeader.dwBufferLength) {
			bytesToRead = WaveHeader.dwBufferLength - playbackSendPosition;
		}
		else {
			bytesToRead = PACKET_SIZE - headerLength;
		}

		if (nPacketsSent % 10 == 0) {
			WSAWaitForMultipleEvents(1, EventArray, FALSE, 200, TRUE);
		}

		memcpy(buf, &PCMWaveFmtRecord, sizeof(PCMWAVEFORMAT));
		memcpy(buf + headerLength, WaveHeader.lpData + playbackSendPosition, bytesToRead);

		SocketInfo->DataBuf.buf = buf;
		SocketInfo->DataBuf.len = PACKET_SIZE;
		playbackSendPosition += PACKET_SIZE - headerLength;
		if (WSASendTo(SocketInfo->Socket, &(SocketInfo->DataBuf), 1, &SendBytes, Flags,
			(sockaddr *) &(SocketInfo->peer), peer_len, &(SocketInfo->Overlapped), MulticastAudioSendCompRoutine) == SOCKET_ERROR) {
			if (WSAGetLastError() != WSA_IO_PENDING) {
				printf("WSARecv() failed with error %d\n", WSAGetLastError());
				free(buf);
				return;
			}
		}
	}
}

DWORD WINAPI MulticastReceiveAudioWorkerThread(LPVOID lpParameter) {
	DWORD Flags, Index, RecvBytes;
	WSAEVENT EventArray[1];
	LPSOCKET_INFORMATION SocketInfo;
	sockaddr_in server;
	int server_len;
	char * buf;

	EventArray[0] = WSACreateEvent();
	SocketInfo = (LPSOCKET_INFORMATION)lpParameter;


	buf = (char *)malloc(PACKET_SIZE);
	SocketInfo->DataBuf.buf = buf;
	SocketInfo->DataBuf.len = PACKET_SIZE;
	ZeroMemory(&(SocketInfo->Overlapped), sizeof(WSAOVERLAPPED));
	Flags = 0;
	server_len = sizeof(server);

	while (TRUE) {
		if (WSARecvFrom(SocketInfo->Socket, &(SocketInfo->DataBuf), 1, &RecvBytes, &Flags,
			(sockaddr *)&server, &server_len, &(SocketInfo->Overlapped), MulticastAudioReceiveCompRoutine) == SOCKET_ERROR) {
			if (WSAGetLastError() != WSA_IO_PENDING) {
				printf("WSARecv() failed with error %d\n", WSAGetLastError());

				free(buf);
				return FALSE;
			}
		}

		while (TRUE) {
			Index = WSAWaitForMultipleEvents(1, EventArray, FALSE, 5000, TRUE);
			if (Index == WSA_WAIT_FAILED) {
				OutputDebugStringA("Wait for multiple event failed");
			}
		}
	}

}

void CALLBACK MulticastAudioReceiveCompRoutine(DWORD Error, DWORD BytesTransferred, LPWSAOVERLAPPED Overlapped, DWORD InFlags) {
	DWORD RecvBytes;
	DWORD Flags;
	sockaddr_in server;
	int server_len;
	char *buf;
	int headerLength = sizeof(PCMWAVEFORMAT);

	LPSOCKET_INFORMATION SI = (LPSOCKET_INFORMATION)Overlapped;

	if (Error != 0) {
		char errorMessage[1024];
		sprintf_s(errorMessage, "I/O operation failed with error %d\n", Error);
		OutputDebugStringA(errorMessage);
		closesocket(SI->Socket);
		return;
	}
	if (BytesTransferred == 0) {
		printf("Closing socket %d\n", SI->Socket);
		closesocket(SI->Socket);
		return;
	}

	nPacketsRecv++;
	nBytesRecv += BytesTransferred;
	printf("Received %d bytes\n", BytesTransferred);
	printf("Packets received: %d\n", nPacketsRecv);
	printf("Bytes received: %d\n", nBytesRecv);

	if (PCMWaveFmtRecord.wBitsPerSample == 0) {
		memcpy(&PCMWaveFmtRecord, SI->DataBuf.buf, headerLength);
		printPCMInfo();
		OpenOutputDevice();
	}
	else {

		playQueue.add_node(SI->DataBuf.buf + headerLength, BytesTransferred - headerLength);
		if (nPacketsRecv == NUM_BUFFS + 1) {
			waveOutPause(wo);
			streamPlayback();
			waveOutRestart(wo);
		}
	}

	ZeroMemory(&(SI->Overlapped), sizeof(WSAOVERLAPPED));
	free(SI->DataBuf.buf);
	buf = (char *)malloc(PACKET_SIZE);
	SI->DataBuf.buf = buf;
	SI->DataBuf.len = PACKET_SIZE;
	server_len = sizeof(server);
	Flags = 0;

	if (WSARecvFrom(SI->Socket, &(SI->DataBuf), 1, &RecvBytes, &Flags,
		(sockaddr *)&server, &server_len, &(SI->Overlapped), MulticastAudioReceiveCompRoutine) == SOCKET_ERROR) {
		if (WSAGetLastError() != WSA_IO_PENDING) {
			printf("WSARecv() failed with error %d\n", WSAGetLastError());

			free(buf);
			return;
		}
	}

}

/*------------------------------------------------------------------------------------------------------------------
--	FUNCTION:		VoIPSendAudioWorkerThread
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
--	INTERFACE:		DWORD WINAPI VoIPSendAudioWorkerThread(LPVOID lpParameter)
--
--
--	RETURNS:		DWORD
--
--
--	NOTES:			Creates a thread to handle sending for choice chat
----------------------------------------------------------------------------------------------------------------------*/
DWORD WINAPI VoIPSendAudioWorkerThread(LPVOID lpParameter)
{
	DWORD Flags, Index, SendBytes;
	WSAEVENT EventArray[1];
	CLIENT_THREAD_PARAMS* params;
	LPSOCKET_INFORMATION SocketInfo;
	char *buf;
	int peer_len = sizeof(params->sin);
	node * n = (node *)malloc(sizeof(node));

	//// send audio format data
	buf = (char *)malloc(sizeof(PCMWAVEFORMAT));
	//memcpy(buf, &PCMWaveFmtRecord, sizeof(PCMWAVEFORMAT));

	params = (CLIENT_THREAD_PARAMS*)lpParameter;
	EventArray[0] = WSACreateEvent();
	SocketInfo = &(params->SI);

	//SocketInfo->DataBuf.len = sizeof(PCMWAVEFORMAT);
	SocketInfo->DataBuf.buf = (char *)malloc(PACKET_SIZE);
	//SocketInfo->DataBuf.buf = buf;
	ZeroMemory(&(SocketInfo->Overlapped), sizeof(WSAOVERLAPPED));
	Flags = 0;


	while (TRUE) {
		// Constantly check for data in sendQueue
		if ((n = sendQueue.get_next()) == NULL) { continue; }

		//SocketInfo->DataBuf.len = n->size;
		//SocketInfo->DataBuf.buf = n->data;
		memcpy(SocketInfo->DataBuf.buf, n->data, n->size);
		SocketInfo->DataBuf.len = n->size;

		// listen on port with specified completion routine
		if (WSASendTo(SocketInfo->Socket, &(SocketInfo->DataBuf), 1, &SendBytes, Flags,
			(sockaddr *) &(params->sin), peer_len, &(SocketInfo->Overlapped), VoIPAudioSendCompRoutine) == SOCKET_ERROR) {
			if (WSAGetLastError() != WSA_IO_PENDING) {
				char err[1024];
				//printf("WSARecv() failed with error %d\n", WSAGetLastError());
				sprintf_s(err, "WSASendTo() failed with error: %d", WSAGetLastError());
				OutputDebugStringA(err);
				free(buf);
				return FALSE;
			}
		}

		linked_list::free_node(n);
		//OutputDebugString("Sent audio packet\n");

		// idle in alertable state for completion routine return
		while (TRUE) {
			Index = WSAWaitForMultipleEvents(1, EventArray, FALSE, WSA_INFINITE, TRUE);
			if (Index == WSA_WAIT_FAILED) {
				//printf("Wait for multiple event failed");
				OutputDebugStringA("wait for multiple event failed");
			}

		}
	}
}

/*------------------------------------------------------------------------------------------------------------------
--	FUNCTION:		VoIPAudioSendCompRoutine
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
--	INTERFACE:		void CALLBACK VoIPAudioSendCompRoutine(DWORD Error, DWORD BytesTransferred, 
--						LPWSAOVERLAPPED Overlapped, DWORD InFlags)
--
--
--	RETURNS:		void
--
--
--	NOTES:			Handles completion of sending audio data for voice chat
----------------------------------------------------------------------------------------------------------------------*/
void CALLBACK VoIPAudioSendCompRoutine(DWORD Error, DWORD BytesTransferred, LPWSAOVERLAPPED Overlapped, DWORD InFlags)
{
	//using namespace TransferDetails;
	DWORD SendBytes, Flags;
	LPSOCKET_INFORMATION SocketInfo;
	sockaddr_in client;
	int client_len;
	SocketInfo = (LPSOCKET_INFORMATION)Overlapped;
	char *buf;
	char msg[1024];

	int peer_len = sizeof(SocketInfo->peer);

	nPacketsSent++;
	nBytesSent += BytesTransferred;

	//free(SocketInfo->DataBuf.buf);
	buf = (char *)malloc(PACKET_SIZE);
	Flags = 0;

	node * n;

	// Constantly check for data in sendQueue
	while ((n = sendQueue.get_next()) == NULL) { continue; }
	//sendQueue.print();
	OutputDebugString("Found data in sendQueue\n");

	//SocketInfo->DataBuf.len = PACKET_SIZE;
	//SocketInfo->DataBuf.buf = n->data;

	memcpy(SocketInfo->DataBuf.buf, n->data, n->size);
	SocketInfo->DataBuf.len = n->size;
	//playbackSendPosition += PACKET_SIZE;
	if (WSASendTo(SocketInfo->Socket, &(SocketInfo->DataBuf), 1, &SendBytes, Flags,
		(sockaddr *) &(SocketInfo->peer), peer_len, &(SocketInfo->Overlapped), VoIPAudioSendCompRoutine) == SOCKET_ERROR) {
		if (WSAGetLastError() != WSA_IO_PENDING) {
			OutputDebugString("Failed to send packet\n");
			printf("WSARecv() failed with error %d\n", WSAGetLastError());
			//free(buf);
			return;
		}
	}
	//OutputDebugString("Sent audio packet\n");
	linked_list::free_node(n);
}

/*------------------------------------------------------------------------------------------------------------------
--	FUNCTION:		VoIPReceiveAudioWorkerThread
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
--	INTERFACE:		DWORD WINAPI VoIPReceiveAudioWorkerThread(LPVOID lpParameter)
--
--
--	RETURNS:		DWORD
--
--
--	NOTES:			Creates a thread to handle receiving of audio data for voice chat
----------------------------------------------------------------------------------------------------------------------*/
DWORD WINAPI VoIPReceiveAudioWorkerThread(LPVOID lpParameter)
{
	DWORD Flags, Index, RecvBytes;
	WSAEVENT EventArray[1];
	LPSOCKET_INFORMATION SocketInfo;
	sockaddr_in server;
	int server_len;
	char * buf;

	EventArray[0] = WSACreateEvent();
	SocketInfo = (LPSOCKET_INFORMATION)lpParameter;

	buf = (char *)malloc(PACKET_SIZE);
	SocketInfo->DataBuf.buf = buf;
	SocketInfo->DataBuf.len = PACKET_SIZE;
	ZeroMemory(&(SocketInfo->Overlapped), sizeof(WSAOVERLAPPED));
	Flags = 0;
	server_len = sizeof(server);

	while (TRUE) {
		if (WSARecvFrom(SocketInfo->Socket, &(SocketInfo->DataBuf), 1, &RecvBytes, &Flags,
			(sockaddr *)&server, &server_len, &(SocketInfo->Overlapped), VoIPAudioReceiveCompRoutine) == SOCKET_ERROR) {
			if (WSAGetLastError() != WSA_IO_PENDING) {
				char errmsg[128];
				sprintf_s(errmsg, "WSARecv() failed with error %d\n", WSAGetLastError());
				updateStatusLogDisplay(errmsg);
				free(buf);
				return FALSE;
			}
		}

		//OutputDebugString("Recv audio data\n");

		while (TRUE) {
			Index = WSAWaitForMultipleEvents(1, EventArray, FALSE, 5000, TRUE);
			if (Index == WSA_WAIT_FAILED) {
				OutputDebugStringA("Wait for multiple event failed");
			}
		}
	}
}

/*------------------------------------------------------------------------------------------------------------------
--	FUNCTION:		VoIPAudioReceiveCompRoutine
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
--	INTERFACE:		void CALLBACK VoIPAudioReceiveCompRoutine(DWORD Error, DWORD BytesTransferred, 
--						LPWSAOVERLAPPED Overlapped, DWORD InFlags)
--
--
--	RETURNS:		void
--
--
--	NOTES:			Handles completion of receiving audio data for voice chat
----------------------------------------------------------------------------------------------------------------------*/
void CALLBACK VoIPAudioReceiveCompRoutine(DWORD Error, DWORD BytesTransferred, LPWSAOVERLAPPED Overlapped, DWORD InFlags)
{
	DWORD RecvBytes;
	DWORD Flags;
	sockaddr_in server;
	int server_len;
	char *buf;


	LPSOCKET_INFORMATION SI = (LPSOCKET_INFORMATION)Overlapped;

	if (Error != 0) {
		char errorMessage[1024];
		sprintf_s(errorMessage, "I/O operation failed with error %d\n", Error);
		OutputDebugStringA(errorMessage);
		closesocket(SI->Socket);
		return;
	}
	if (BytesTransferred == 0) {
		printf("Closing socket %d\n", SI->Socket);
		closesocket(SI->Socket);
		return;
	}

	nPacketsRecv++;
	//nBytesRecv += BytesTransferred;
	//printf("Received %d bytes\n", BytesTransferred);
	//printf("Packets received: %d\n", nPacketsRecv);
	//printf("Bytes received: %d\n", nBytesRecv);

	OutputDebugStringA("received\n");

	playQueue.add_node(SI->DataBuf.buf, BytesTransferred);
	if (nPacketsRecv == NUM_BUFFS) {
		waveOutPause(wo);
		streamPlayback();
		waveOutRestart(wo);
	}


	ZeroMemory(&(SI->Overlapped), sizeof(WSAOVERLAPPED));
	free(SI->DataBuf.buf);
	buf = (char *)malloc(PACKET_SIZE);
	SI->DataBuf.buf = buf;
	SI->DataBuf.len = PACKET_SIZE;
	server_len = sizeof(server);
	Flags = 0;

	if (WSARecvFrom(SI->Socket, &(SI->DataBuf), 1, &RecvBytes, &Flags,
		(sockaddr *)&server, &server_len, &(SI->Overlapped), VoIPAudioReceiveCompRoutine) == SOCKET_ERROR) {
		if (WSAGetLastError() != WSA_IO_PENDING) {
			printf("WSARecv() failed with error %d\n", WSAGetLastError());

			free(buf);
			return;
		}
	}
	//OutputDebugString("Recv audio data\n");
}
