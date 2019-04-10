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

DWORD WINAPI UnicastSendAudioWorkerThread(LPVOID lpParameter);
void CALLBACK UnicastAudioSendCompRoutine(DWORD Error, DWORD BytesTransferred, LPWSAOVERLAPPED Overlapped, DWORD InFlags);
DWORD WINAPI UnicastReceiveAudioWorkerThread(LPVOID lpParameter);
void CALLBACK UnicastAudioReceiveCompRoutine(DWORD Error, DWORD BytesTransferred, LPWSAOVERLAPPED Overlapped, DWORD InFlags);

DWORD WINAPI MulticastSendAudioWorkerThread(LPVOID lpParameter);
void CALLBACK MulticastAudioSendCompRoutine(DWORD Error, DWORD BytesTransferred, LPWSAOVERLAPPED Overlapped, DWORD InFlags);

DWORD WINAPI MulticastReceiveAudioWorkerThread(LPVOID lpParameter);
void CALLBACK MulticastAudioReceiveCompRoutine(DWORD Error, DWORD BytesTransferred, LPWSAOVERLAPPED Overlapped, DWORD InFlags);


// VOIP

DWORD WINAPI VoIPSendAudioWorkerThread(LPVOID lpParameter);
void CALLBACK VoIPAudioSendCompRoutine(DWORD Error, DWORD BytesTransferred, LPWSAOVERLAPPED Overlapped, DWORD InFlags);

DWORD WINAPI VoIPReceiveAudioWorkerThread(LPVOID lpParameter);
void CALLBACK VoIPAudioReceiveCompRoutine(DWORD Error, DWORD BytesTransferred, LPWSAOVERLAPPED Overlapped, DWORD InFlags);

