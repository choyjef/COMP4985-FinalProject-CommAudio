#pragma once
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


void connectTCP(HWND hwnd);
DWORD WINAPI TCPServerWorkerThread(LPVOID lpParameter);
void CALLBACK TCPRecvCompRoutine(DWORD Error, DWORD BytesTransferred, LPWSAOVERLAPPED Overlapped, DWORD InFlags);
DWORD WINAPI TCPClientWorkerThread(LPVOID lpParameter);
void CALLBACK TCPSendCompRoutine(DWORD Error, DWORD BytesTransferred, LPWSAOVERLAPPED Overlapped, DWORD InFlags);
DWORD WINAPI UDPServerWorkerThread(LPVOID lpParameter);
void CALLBACK UDPRecvCompRoutine(DWORD Error, DWORD BytesTransferred, LPWSAOVERLAPPED Overlapped, DWORD InFlags);
DWORD WINAPI UDPClientWorkerThread(LPVOID lpParameter);
void CALLBACK UDPSendCompRoutine(DWORD Error, DWORD BytesTransferred, LPWSAOVERLAPPED Overlapped, DWORD InFlags);

DWORD WINAPI UnicastSendAudioWorkerThread(LPVOID lpParameter);
void CALLBACK UnicastAudioSendCompRoutine(DWORD Error, DWORD BytesTransferred, LPWSAOVERLAPPED Overlapped, DWORD InFlags);