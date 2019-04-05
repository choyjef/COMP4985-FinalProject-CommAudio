#pragma once
#include "Application.h"
#include "Transport.h"
//#include <winsock2.h>
//#include <windows.h>
//#include <windowsx.h>
#include <string>


void listenTCP(HWND hwnd);
void listenUDP(HWND hwnd);
void initServer(HWND hwnd, int protocol);
void initClient(HWND hwnd, int protocol);
void connectTCP(HWND hwnd);
void connectUDP();