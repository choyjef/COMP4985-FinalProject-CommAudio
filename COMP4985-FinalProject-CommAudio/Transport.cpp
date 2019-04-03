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
sockaddr_in server;
high_resolution_clock::time_point firstPacketTime, lastPacketTime;


// jeff_unicast variables
int nPacketsSent = 0;
int nPacketsRecv = 0;
int nBytesRecv = 0;
int nBytesSent = 0;
int playbackSendPosition = 0;


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
--	PROGRAMMER:		Jeffrey Choy
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
	
	DWORD Flags, Index, RecvBytes;
	WSAEVENT EventArray[1];
	LPSOCKET_INFORMATION SocketInfo;
	
	EventArray[0] = WSACreateEvent();
	SocketInfo = (LPSOCKET_INFORMATION) lpParameter;
	SocketInfo->DataBuf.len = DATA_BUFSIZE;
	//recvbuf = (char *)malloc(sizeof(char) * getPacketSize());
	SocketInfo->DataBuf.buf = recvbuf;
	ZeroMemory(&(SocketInfo->Overlapped), sizeof(WSAOVERLAPPED));
	

	Flags = 0;
	while (TRUE) {
		
		// listen on port with specified completion routine
		if (WSARecv(SocketInfo->Socket, &(SocketInfo->DataBuf), 1, &RecvBytes, &Flags, &(SocketInfo->Overlapped), TCPRecvCompRoutine) == SOCKET_ERROR) {
			if (WSAGetLastError() != WSA_IO_PENDING) {
				char errorMessage[1024];
				sprintf_s(errorMessage, "WSARecv() failed with error %d\n", WSAGetLastError());
				OutputDebugStringA(errorMessage);
				//OutputDebugStringA("WSARecv() failed");
				free(recvbuf);
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
--	FUNCTION:		TCPRecvCompRoutine
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
--	INTERFACE:		void CALLBACK TCPRecvCompRoutine(DWORD Error, DWORD BytesTransferred,
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
void CALLBACK TCPRecvCompRoutine(DWORD Error, DWORD BytesTransferred, LPWSAOVERLAPPED Overlapped, DWORD InFlags) {
	DWORD RecvBytes;
	DWORD Flags;
	char printout[DATA_BUFSIZE];
	//char buf[DATA_BUFSIZE];

	// populate socket information through cast SOMEHOW????
	LPSOCKET_INFORMATION SI = (LPSOCKET_INFORMATION) Overlapped;

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

	// collect metadata
	if (recvPacketsCount == 0) {
		firstPacketTime = high_resolution_clock::now();
	}

	if (SI->DataBuf.buf[0] == '+') {
		std::string str(SI->DataBuf.buf);
		std::stringstream stream(str);
		int n;
		stream >> n;
		setPacketSize(n);
		stream >> n;
		setNumPackets(n);
	}

	recvPacketsCount++;
	if (recvPacketsCount == getNumPackets()) {
		lastPacketTime = high_resolution_clock::now();
		duration<double, std::milli> time_span = (lastPacketTime - firstPacketTime);
		closesocket(SI->Socket);
		char errorMessage[1024];
		sprintf_s(errorMessage, "Closing socket. Transfer time: %fms\n", time_span.count());
		OutputDebugStringA(errorMessage);
		displayStatistics(recvPacketsCount, time_span.count());
		return;
	}
	
	// TODO: PROCESS/STORE DATA HERE
	// debug output received data
	if (recvPacketsCount != 1) {
		std::ofstream file;
		file.open("output.txt", std::ios::app);
		file << SI->DataBuf.buf;
		file.close();
	}

	sprintf_s(printout, "Received %d Bytes\n", BytesTransferred);
	OutputDebugStringA(printout);
	sprintf_s(printout, "Received: %s", SI->DataBuf.buf);
	OutputDebugStringA(printout);

	// prepare socket for another read
	Flags = 0;
	ZeroMemory(&(SI->Overlapped), sizeof(WSAOVERLAPPED));
	SI->DataBuf.len = getPacketSize();
	SI->DataBuf.buf = recvbuf;


	// initiate another read with completion routine
	if (WSARecv(SI->Socket, &(SI->DataBuf), 1, &RecvBytes, &Flags, &(SI->Overlapped), TCPRecvCompRoutine) == SOCKET_ERROR) {
		if (WSAGetLastError() != WSA_IO_PENDING) {
			char errorMessage[1024];
			sprintf_s(errorMessage, "WSARecv() failed with error %d\n", WSAGetLastError());
			OutputDebugStringA(errorMessage);
			//OutputDebugStringA("WSARecv() failed");
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
--	REVISIONS:
--
--
--	DESIGNER:		Jeffrey Choy
--
--
--	PROGRAMMER:		Jeffrey Choy
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
	DWORD Flags, Index, SendBytes;
	WSAEVENT EventArray[1];
	LPSOCKET_INFORMATION SocketInfo;

	char metaData[1024];

	sprintf_s(metaData, "+%d %d", getPacketSize(), getNumPackets());
	
	EventArray[0] = WSACreateEvent();
	SocketInfo = (LPSOCKET_INFORMATION)lpParameter;
	
	SocketInfo->DataBuf.buf = metaData;
	SocketInfo->DataBuf.len = 1024;
	ZeroMemory(&(SocketInfo->Overlapped), sizeof(WSAOVERLAPPED));

	Flags = 0;
	while (TRUE) {

		// listen on port with specified completion routine
		if (WSASend(SocketInfo->Socket, &(SocketInfo->DataBuf), 1, &SendBytes, Flags, &(SocketInfo->Overlapped), TCPSendCompRoutine) == SOCKET_ERROR) {
			if (WSAGetLastError() != WSA_IO_PENDING) {
				char errorMessage[1024];
				sprintf_s(errorMessage, "WSASend() failed with error %d\n", WSAGetLastError());
				OutputDebugStringA(errorMessage);
				//OutputDebugStringA("WSARecv() failed");
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
--	FUNCTION:		TCPSendCompRoutine
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
--	INTERFACE:		void CALLBACK TCPSendCompRoutine(DWORD Error, DWORD BytesTransferred,
--						LPWSAOVERLAPPED Overlapped, DWORD InFlags)
--							DWORD Error: struct container parameters to pass to the thread
--							DWORD BytesTransferred: number of bytes transferred from operation
--							LPWSAOVERLAPPED Overlapped: Overlapped structure for overlapped reading from socket
--							DWORD InFlags: input flags for completion routine
--
--	RETURNS:
--
--
--	NOTES:			Handles completion of send action, posts another send if more data.
----------------------------------------------------------------------------------------------------------------------*/
void CALLBACK TCPSendCompRoutine(DWORD Error, DWORD BytesTransferred, LPWSAOVERLAPPED Overlapped, DWORD InFlags) {
	DWORD RecvBytes;
	LPSOCKET_INFORMATION SocketInfo = (LPSOCKET_INFORMATION)Overlapped;
	DWORD Flags = 0;


	OutputDebugStringA("Send complete\n");
	char errorMessage[1024];
	sprintf_s(errorMessage, "Bytes transferred: %d\n", BytesTransferred);
	OutputDebugStringA(errorMessage);

	SocketInfo->DataBuf.buf = getSendBuffer();
	SocketInfo->DataBuf.len = getPacketSize();
	ZeroMemory(&(SocketInfo->Overlapped), sizeof(WSAOVERLAPPED));

	if (sentPacketsCount < getNumPackets()) {
		sentPacketsCount++;
		if (sentPacketsCount == getNumPackets()) {
			return;
		}
		if (WSASend(SocketInfo->Socket, &(SocketInfo->DataBuf), 1, &RecvBytes, Flags, &(SocketInfo->Overlapped), TCPSendCompRoutine) == SOCKET_ERROR) {
			if (WSAGetLastError() != WSA_IO_PENDING) {
				char errorMessage[1024];
				sprintf_s(errorMessage, "WSASend() failed with error %d\n", WSAGetLastError());
				OutputDebugStringA(errorMessage);
				//OutputDebugStringA("WSARecv() failed");
				return;
			}
		}
	}
	
}

/*------------------------------------------------------------------------------------------------------------------
--	FUNCTION:		UDPServerWorkerThread
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
--	INTERFACE:		DWORD WINAPI UDPServerWorkerThread(LPVOID lpParameter)
--							LPVOID lpParameter: struct container parameters to pass to the thread
--
--	RETURNS:
--
--
--	NOTES:			Creates a new thread to handle UDP server functionality.
----------------------------------------------------------------------------------------------------------------------*/
DWORD WINAPI UDPServerWorkerThread(LPVOID lpParameter) {
	DWORD Flags, Index, RecvBytes;
	WSAEVENT EventArray[1];
	LPSOCKET_INFORMATION SocketInfo;
	sockaddr_in client;
	int client_len;


	EventArray[0] = WSACreateEvent();
	SocketInfo = (LPSOCKET_INFORMATION)lpParameter;
	SocketInfo->DataBuf.len = DATA_BUFSIZE;

	//recvbuf = (char *)malloc(sizeof(char) * getPacketSize());
	SocketInfo->DataBuf.buf = recvbuf;
	ZeroMemory(&(SocketInfo->Overlapped), sizeof(WSAOVERLAPPED));
	Flags = 0;
	client_len = sizeof(client);

	while (TRUE) {

		// listen on port with specified completion routine
		if (WSARecvFrom(SocketInfo->Socket, &(SocketInfo->DataBuf), 1, &RecvBytes, &Flags, (sockaddr *)&client, &client_len, &(SocketInfo->Overlapped), UDPRecvCompRoutine) == SOCKET_ERROR) {
			if (WSAGetLastError() != WSA_IO_PENDING) {
				char errorMessage[1024];
				sprintf_s(errorMessage, "WSARecv() failed with error %d\n", WSAGetLastError());
				OutputDebugStringA(errorMessage);
				//OutputDebugStringA("WSARecv() failed");
				free(recvbuf);
				return FALSE;
			}
		}

		// idle in alertable state for completion routine return
		while (TRUE) {
			Index = WSAWaitForMultipleEvents(1, EventArray, FALSE, 5000, TRUE);
			if (Index == WSA_WAIT_FAILED) {
				OutputDebugStringA("Wait for multiple event failed");
			}
			if (recvPacketsCount > 0) {
				auto int_ms = duration_cast<milliseconds>(high_resolution_clock::now() - lastPacketTime);
				if (int_ms.count() > 2000) {
					OutputDebugStringA("Timeout\n");
					duration<double, std::milli> time_span = (lastPacketTime - firstPacketTime);
					closesocket(SocketInfo->Socket);
					char errorMessage[1024];
					sprintf_s(errorMessage, "Closing socket. Transfer time: %fms\n", time_span.count());
					OutputDebugStringA(errorMessage);
					displayStatistics(recvPacketsCount - 1, time_span.count());
					return FALSE;
				}
			}
			
		}
	}

}

/*------------------------------------------------------------------------------------------------------------------
--	FUNCTION:		UDPRecvCompRoutine
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
--	INTERFACE:		void CALLBACK UDPRecvCompRoutine(DWORD Error, DWORD BytesTransferred,
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
void CALLBACK UDPRecvCompRoutine(DWORD Error, DWORD BytesTransferred, LPWSAOVERLAPPED Overlapped, DWORD InFlags) {
	DWORD RecvBytes;
	DWORD Flags;
	char printout[DATA_BUFSIZE];
	sockaddr_in client;
	int client_len;

	// populate socket information through cast SOMEHOW????
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

	if (recvPacketsCount == 0) {
		firstPacketTime = high_resolution_clock::now();
	}
	lastPacketTime = high_resolution_clock::now();
	recvPacketsCount++;
	// debug output received data
	sprintf_s(printout, "Received %d Bytes\n", BytesTransferred);
	OutputDebugStringA(printout);
	sprintf_s(printout, "Received: %s", SI->DataBuf.buf);
	OutputDebugStringA(printout);

	if (!getPacketSize() > 0) {
		std::string str(SI->DataBuf.buf);
		std::stringstream stream(str);
		int n;
		stream >> n;
		setPacketSize(n);
		stream >> n;
		setNumPackets(n);
	}


	// TODO: PROCESS/STORE DATA HERE
	std::ofstream file;
	file.open("output.txt", std::ios::app);
	file << SI->DataBuf.buf;
	file.close();

	// prepare socket for another read
	Flags = 0;
	ZeroMemory(&(SI->Overlapped), sizeof(WSAOVERLAPPED));
	SI->DataBuf.len = DATA_BUFSIZE;
	
	SI->DataBuf.buf = recvbuf;
	client_len = sizeof(client);

	if (WSARecvFrom(SI->Socket, &(SI->DataBuf), 1, &RecvBytes, &Flags, (sockaddr *)&client, &client_len, &(SI->Overlapped), UDPRecvCompRoutine) == SOCKET_ERROR) {
		if (WSAGetLastError() != WSA_IO_PENDING) {
			char errorMessage[1024];
			sprintf_s(errorMessage, "WSARecv() failed with error %d\n", WSAGetLastError());
			OutputDebugStringA(errorMessage);
			//OutputDebugStringA("WSARecv() failed");
			return;
		}
	}
}

/*------------------------------------------------------------------------------------------------------------------
--	FUNCTION:		UDPClientWorkerThread
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
--	INTERFACE:		DWORD WINAPI UDPClientWorkerThread(LPVOID lpParameter)
--							LPVOID lpParameter: struct container parameters to pass to the thread
--
--	RETURNS:
--
--
--	NOTES:			Creates a new thread to handle UDP client functionality.
----------------------------------------------------------------------------------------------------------------------*/
DWORD WINAPI UDPClientWorkerThread(LPVOID lpParameter) {
	DWORD Flags, Index, SendBytes;
	WSAEVENT EventArray[1];
	LPCLIENT_THREAD_PARAMS threadParams;
	LPSOCKET_INFORMATION SocketInfo;
	
	int server_len;

	char metaData[1024];

	sprintf_s(metaData, "+%d %d", getPacketSize(), getNumPackets());
	
	threadParams = (LPCLIENT_THREAD_PARAMS)lpParameter;
	server = threadParams->sin;
	server_len = sizeof(server);
	EventArray[0] = WSACreateEvent();
	SocketInfo = &(threadParams->SI);


	generateUDPSendBufferData(getPacketSize());
	SocketInfo->DataBuf.buf = getSendBuffer();
	SocketInfo->DataBuf.len = getPacketSize();

	ZeroMemory(&(SocketInfo->Overlapped), sizeof(WSAOVERLAPPED));
	Flags = 0;

	while (TRUE) {

		// listen on port with specified completion routine
		if (WSASendTo(SocketInfo->Socket, &(SocketInfo->DataBuf), 1, &SendBytes, Flags, (sockaddr *)&server, server_len, &(SocketInfo->Overlapped), UDPSendCompRoutine) == SOCKET_ERROR) {
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
--	FUNCTION:		UDPSendCompRoutine
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
--	INTERFACE:		void CALLBACK UDPSendCompRoutine(DWORD Error, DWORD BytesTransferred, 
--						LPWSAOVERLAPPED Overlapped, DWORD InFlags)
--							DWORD Error: struct container parameters to pass to the thread
--							DWORD BytesTransferred: number of bytes transferred from operation
--							LPWSAOVERLAPPED Overlapped: Overlapped structure for overlapped reading from socket
--							DWORD InFlags: input flags for completion routine
--
--	RETURNS:
--
--
--	NOTES:			Handles completion of send action, posts another send if more data.
----------------------------------------------------------------------------------------------------------------------*/
void CALLBACK UDPSendCompRoutine(DWORD Error, DWORD BytesTransferred, LPWSAOVERLAPPED Overlapped, DWORD InFlags) {
	DWORD SendBytes;
	LPSOCKET_INFORMATION SocketInfo = (LPSOCKET_INFORMATION)Overlapped;
	DWORD Flags = 0;

	OutputDebugStringA("Send complete\n");
	char errorMessage[1024];
	sprintf_s(errorMessage, "Bytes transferred: %d\n", BytesTransferred);
	OutputDebugStringA(errorMessage);

	SocketInfo->DataBuf.buf = getSendBuffer();
	SocketInfo->DataBuf.len = getPacketSize();

	if (sentPacketsCount < getNumPackets()) {
		sentPacketsCount++;
		if (WSASendTo(SocketInfo->Socket, &(SocketInfo->DataBuf), 1, &SendBytes, Flags, (sockaddr *)&server, sizeof(server), &(SocketInfo->Overlapped), UDPSendCompRoutine) == SOCKET_ERROR) {
			if (WSAGetLastError() != WSA_IO_PENDING) {
				char errorMessage[1024];
				sprintf_s(errorMessage, "WSASend() failed with error %d\n", WSAGetLastError());
				OutputDebugStringA(errorMessage);
				//OutputDebugStringA("WSARecv() failed");
				return;
			}
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

	int peer_len = sizeof(SocketInfo->peer);

	nPacketsSent++;
	nBytesSent += BytesTransferred;
	//printf("Packets sent: %d\n", TransferDetails::nPacketsSent);
	//printf("Bytes sent: %d\n", TransferDetails::nBytesSent);

	//sprintf_s(msg, "Packets sent: %d\n", TransferDetails::nPacketsSent);
	//OutputDebugStringA(msg);
	//sprintf_s(msg, "Bytes sent: %d\n", TransferDetails::nBytesSent);
	//OutputDebugStringA(msg);

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
			Sleep(300);
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