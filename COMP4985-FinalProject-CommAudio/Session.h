#pragma once
#include "Application.h"
#include "Transport.h"
//#include <winsock2.h>
//#include <windows.h>
//#include <windowsx.h>
#include <string>
#include <mmsystem.h> 
#include <ws2tcpip.h>

void listenTCP(HWND hwnd);
void connectTCP(HWND hwnd);

void initUnicastSend();
void initUnicastRecv();

void initMulticastSend();
void initMulticastRecv();

// VoIP Functions
void initVoipSend();
void initVoipRecv();