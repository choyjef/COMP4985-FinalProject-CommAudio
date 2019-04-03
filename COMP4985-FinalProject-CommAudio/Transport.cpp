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

char* getDirectory(int size) {
	DWORD fileError;
	char directory[DATA_BUFSIZE];
	char wavFileList[DATA_BUFSIZE];
	HANDLE hFind = INVALID_HANDLE_VALUE;
	WIN32_FIND_DATA findData;

	//char* wavFileList = (char *)malloc(sizeof(char) * size);

	//Find files in current server directory
	//1) find current directory
	GetCurrentDirectory(DATA_BUFSIZE, directory);
	strcat_s(directory, "\\\*");
	//2) find first file in current directory
	hFind = FindFirstFile(directory, &findData);
	strncpy_s(wavFileList, "=", DATA_BUFSIZE - 1);

	char fileName[260];
	strcpy_s(fileName, findData.cFileName);
	if (strstr(fileName, ".wav")) {

		strncat_s(wavFileList, findData.cFileName, DATA_BUFSIZE - strlen(wavFileList) - 1);
		strncat_s(wavFileList, "\0", DATA_BUFSIZE - strlen(wavFileList) - 1);
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

			strncat_s(wavFileList, findData.cFileName, DATA_BUFSIZE - strlen(wavFileList) - 1);
			strncat_s(wavFileList, "\n", DATA_BUFSIZE - strlen(wavFileList) - 1);
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
	
	DWORD Flags, Index, RecvBytes, SendBytes;
	WSAEVENT EventArray[1];
	LPSOCKET_INFORMATION SocketInfo;

	//Add wavFileList to buffer for sending
	EventArray[0] = WSACreateEvent();
	SocketInfo = (LPSOCKET_INFORMATION)lpParameter;
	SocketInfo->DataBuf.len = DATA_BUFSIZE;
	SocketInfo->DataBuf.buf = getDirectory(DATA_BUFSIZE);
	ZeroMemory(&(SocketInfo->Overlapped), sizeof(WSAOVERLAPPED));

	

	Flags = 0;
	while (TRUE) {
		
		// listen on port with specified completion routine
		if (WSASend(SocketInfo->Socket, &(SocketInfo->DataBuf), 1, &SendBytes, Flags, &(SocketInfo->Overlapped), TCPServerSendCompletionRoutine) == SOCKET_ERROR) {
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
--	FUNCTION:		TCPServerSendCompletionRoutine
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
void CALLBACK TCPServerSendCompletionRoutine(DWORD Error, DWORD BytesTransferred, LPWSAOVERLAPPED Overlapped, DWORD InFlags) {
	DWORD RecvBytes;
	DWORD Flags;
	char printout[DATA_BUFSIZE];
	//char buf[DATA_BUFSIZE];

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

	// prepare socket for another read
	Flags = 0;
	ZeroMemory(&(SI->Overlapped), sizeof(WSAOVERLAPPED));
	SI->DataBuf.len = DATA_BUFSIZE;
	SI->DataBuf.buf = recvbuf;


	// initiate another read with completion routine
	if (WSARecv(SI->Socket, &(SI->DataBuf), 1, &RecvBytes, &Flags, &(SI->Overlapped), TCPServerRecvCompRoutine) == SOCKET_ERROR) {
		if (WSAGetLastError() != WSA_IO_PENDING) {
			char errorMessage[1024];
			sprintf_s(errorMessage, "WSARecv() failed with error %d\n", WSAGetLastError());
			OutputDebugStringA(errorMessage);
			//OutputDebugStringA("WSARecv() failed");
			return;
		}
	}
}

void CALLBACK TCPServerSendWavCompRoutine(DWORD Error, DWORD BytesTransferred, LPWSAOVERLAPPED Overlapped, DWORD InFlags) {
	DWORD RecvBytes;
	LPSOCKET_INFORMATION SocketInfo = (LPSOCKET_INFORMATION)Overlapped;
	DWORD Flags = 0;
	//char errorMessage2[10000000];
	char* sendBuffer = getSendBuffer();

	OutputDebugStringA("Send complete\n");
	char errorMessage[1024];
	sprintf_s(errorMessage, "Bytes transferred: %d\n", BytesTransferred);
	OutputDebugStringA(errorMessage);

	SocketInfo->DataBuf.buf = sendBuffer;
	SocketInfo->DataBuf.len = DATA_BUFSIZE;
	ZeroMemory(&(SocketInfo->Overlapped), sizeof(WSAOVERLAPPED));


	std::ofstream file;
	file.open("debug.txt", std::ofstream::app);
	file << "sendBuffer " << sendBuffer << std::endl;
	file.close();
	if (sendBuffer < getAddressOfEnd()) {
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
void CALLBACK TCPServerRecvCompRoutine(DWORD Error, DWORD BytesTransferred, LPWSAOVERLAPPED Overlapped, DWORD InFlags) {
	DWORD RecvBytes;
	LPSOCKET_INFORMATION SocketInfo = (LPSOCKET_INFORMATION)Overlapped;
	DWORD Flags = 0;
	char* sendBuffer = getSendBuffer();

	generateTCPSendBufferData(SocketInfo->DataBuf.buf, DATA_BUFSIZE); //SocketInfo->DataBuf.buf is ding.wav (the data is send by the client)

	OutputDebugStringA("Send FIRST complete\n");
	char errorMessage[1024];
	sprintf_s(errorMessage, "Bytes transferred: %d\n", BytesTransferred);
	OutputDebugStringA(errorMessage);

	SocketInfo->DataBuf.buf = sendBuffer;
	SocketInfo->DataBuf.len = DATA_BUFSIZE;
	ZeroMemory(&(SocketInfo->Overlapped), sizeof(WSAOVERLAPPED));

	if (sendBuffer < getAddressOfEnd()) {
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
--	REVISIONS:		April 3, 2019
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

	/*char metaData[1024];

	sprintf_s(metaData, "+%d %d", getPacketSize(), getNumPackets());
	*/
	EventArray[0] = WSACreateEvent();
	SocketInfo = (LPSOCKET_INFORMATION)lpParameter;
	
	SocketInfo->DataBuf.buf = recvbuf;
	SocketInfo->DataBuf.len = DATA_BUFSIZE;
	ZeroMemory(&(SocketInfo->Overlapped), sizeof(WSAOVERLAPPED));

	Flags = 0;
	while (TRUE) {

		// listen on port with specified completion routine
		if (WSARecv(SocketInfo->Socket, &(SocketInfo->DataBuf), 1, &RecvBytes, &Flags, &(SocketInfo->Overlapped), TCPClientRecvCompRoutine) == SOCKET_ERROR) {
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
void CALLBACK TCPClientRecvCompRoutine(DWORD Error, DWORD BytesTransferred, LPWSAOVERLAPPED Overlapped, DWORD InFlags) {
	DWORD SendBytes;
	DWORD Flags = 0;
	char printout[DATA_BUFSIZE];
	//char buf[DATA_BUFSIZE];

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

	//////
	//Before assignement of SocketInfo->DataBuf.buf, it contains the list of .txt files send by the server

	SI->DataBuf.buf = getFilePath(); //filePath is ding.wav
	SI->DataBuf.len = DATA_BUFSIZE;
	ZeroMemory(&(SI->Overlapped), sizeof(WSAOVERLAPPED));

	if (WSASend(SI->Socket, &(SI->DataBuf), 1, &SendBytes, Flags, &(SI->Overlapped), TCPClientRecvDataCompRoutine) == SOCKET_ERROR) {
		if (WSAGetLastError() != WSA_IO_PENDING) {
			char errorMessage[1024];
			sprintf_s(errorMessage, "WSASend() failed with error %d\n", WSAGetLastError());
			OutputDebugStringA(errorMessage);
			//OutputDebugStringA("WSARecv() failed");
			return;
		}
	}
}

void CALLBACK TCPClientRecvDataCompRoutine(DWORD Error, DWORD BytesTransferred, LPWSAOVERLAPPED Overlapped, DWORD InFlags) {
	DWORD RecvBytes;
	DWORD Flags;
	char printout[DATA_BUFSIZE];
	//char buf[DATA_BUFSIZE];

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

	// TODO: PROCESS/STORE DATA HERE
	// debug output received data

	FILE *fp;

	fopen_s(&fp, getFilePath(), "ab");

	//byteSent += PACKETSIZE;


	if (strstr(SI->DataBuf.buf, ".wav")) {
		Flags = 0;
		ZeroMemory(&(SI->Overlapped), sizeof(WSAOVERLAPPED));
		SI->DataBuf.len = DATA_BUFSIZE;
		SI->DataBuf.buf = recvbuf;

	}
	else { //write to file
		OutputDebugStringA(SI->DataBuf.buf);

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
			//OutputDebugStringA("WSARecv() failed");
			return;
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
	server = threadParams->server;
	server_len = sizeof(server);
	EventArray[0] = WSACreateEvent();
	SocketInfo = threadParams->SI;


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