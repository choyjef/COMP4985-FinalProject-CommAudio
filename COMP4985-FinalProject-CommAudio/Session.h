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
void listenUDP(HWND hwnd);
void initServer(HWND hwnd, int protocol);
void initClient(HWND hwnd, int protocol);
void connectTCP(HWND hwnd);
void connectUDP();

void initUnicastSend();
void initUnicastRecv();

void initMulticastSend();
void initMulticastRecv();

void initVoipSend();
void initVoipRecv();