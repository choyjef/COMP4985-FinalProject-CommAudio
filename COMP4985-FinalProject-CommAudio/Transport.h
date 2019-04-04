#pragma once
#define DIRECTORY_LENGTH 1024
#include "Application.h"
// Windows Header Files
#include <winsock2.h>
#include <windows.h>
#include <windowsx.h>
#include <string>
#include <sstream>
#include <chrono>
#include <iostream>
#include <fstream>

using namespace std::chrono;

extern char wavFileList[DATA_BUFSIZE];
void connectTCP(HWND hwnd);
DWORD WINAPI TCPServerWorkerThread(LPVOID lpParameter);
void CALLBACK TCPClientRecvCompRoutine(DWORD Error, DWORD BytesTransferred, LPWSAOVERLAPPED Overlapped, DWORD InFlags);
void CALLBACK TCPClientRecvDataCompRoutine(DWORD Error, DWORD BytesTransferred, LPWSAOVERLAPPED Overlapped, DWORD InFlags);
DWORD WINAPI TCPClientWorkerThread(LPVOID lpParameter);
void CALLBACK TCPServerSendWavCompRoutine(DWORD Error, DWORD BytesTransferred, LPWSAOVERLAPPED Overlapped, DWORD InFlags);
void CALLBACK TCPServerRecvCompRoutine(DWORD Error, DWORD BytesTransferred, LPWSAOVERLAPPED Overlapped, DWORD InFlags);
void CALLBACK TCPServerSendCompletionRoutine(DWORD Error, DWORD BytesTransferred, LPWSAOVERLAPPED Overlapped, DWORD InFlags);
DWORD WINAPI UDPServerWorkerThread(LPVOID lpParameter);
void CALLBACK UDPRecvCompRoutine(DWORD Error, DWORD BytesTransferred, LPWSAOVERLAPPED Overlapped, DWORD InFlags);
DWORD WINAPI UDPClientWorkerThread(LPVOID lpParameter);
void CALLBACK UDPSendCompRoutine(DWORD Error, DWORD BytesTransferred, LPWSAOVERLAPPED Overlapped, DWORD InFlags);

