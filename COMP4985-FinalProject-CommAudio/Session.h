#pragma once
#pragma comment(lib, "Winmm.lib")

#include "Application.h"
#include "Transport.h"
//#include <winsock2.h>
//#include <windows.h>
//#include <windowsx.h>
#include <string>
#include <mmsystem.h> 
#include <ws2tcpip.h>


void listenTCP(HWND hwnd);
void listenUDP(HWND hwnd);
void initServer(HWND hwnd, int protocol);
void initClient(HWND hwnd, int protocol);
void connectTCP(HWND hwnd);
void connectUDP();

void initUnicastSend();
void initUnicastRecv();

void initMulticastSend();
void initMulticastRecv();

// VoIP Functions
BOOL WaveMakeHeader(unsigned long ulSize, HGLOBAL HData, HGLOBAL HWaveHdr, LPSTR lpData, LPWAVEHDR lpWaveHdr);
void WaveFreeHeader(HGLOBAL HData, HGLOBAL HWaveHdr);
BOOL WaveRecordOpen(LPHWAVEIN lphwi, HWND Hwnd, int nChannels, long lFrequency, int nBits);
BOOL WaveRecordBegin(HWAVEIN hwi, LPWAVEHDR lpWaveHdr);
void WaveRecordEnd(HWAVEIN hwi, LPWAVEHDR lpWaveHdr);
void WaveRecordClose(HWAVEIN hwi);