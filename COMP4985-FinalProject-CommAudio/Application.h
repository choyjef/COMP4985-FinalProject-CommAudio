#pragma once
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define TCP_SELECTED 999
#define UDP_SELECTED 998
#define DATA_BUFSIZE 66000
#define WM_SOCKET (WM_USER + 1)

#define UNICAST_PORT 8080
#define PACKET_SIZE 8192
#define NUM_BUFFS 2
#define UNICAST_ADDRESS "127.0.0.1" // TODO: change to user input

#define VOIP_PORT 8888
#define NUM_CHANNELS 2
#define FREQUENCY 44100
#define NUM_RECORD_BITS 16

// Windows Header Files
#include <winsock2.h>
#include <windows.h>
#include <windowsx.h>
#include <commdlg.h>
#include <Mmsystem.h>
#include "LinkedList.h"
#include "resource.h"

typedef struct _SOCKET_INFORMATION {
	OVERLAPPED Overlapped;
	SOCKET Socket;
	WSABUF DataBuf;
	DWORD BytesSEND;
	DWORD BytesRECV;
	sockaddr_in peer;
} SOCKET_INFORMATION, *LPSOCKET_INFORMATION;

typedef struct _CLIENT_THREAD_PARAMS {
	SOCKET_INFORMATION SI;
	SOCKET sock;
	sockaddr_in sin;
} CLIENT_THREAD_PARAMS, *LPCLIENT_THREAD_PARAMS;

extern PCMWAVEFORMAT PCMWaveFmtRecord;
extern WAVEHDR WaveHeader;
extern WAVEHDR wh[NUM_BUFFS];
extern HANDLE waveDataBlock;
extern HWAVEOUT wo;
extern linked_list playQueue;
extern LPSOCKET_INFORMATION SInfo;
extern char *sendBuffer;
extern char *addressOfEnd;
extern char filePath[1024];
extern linked_list sendQueue;
extern int PORT;
extern HWND hwnd;

void generateTCPSendBufferData(char* fileName, int size);
void openInputDialog();
int getPort();
char* getHostIP();

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int);
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
int initializeWindow(HINSTANCE);
void displayGUIControls();
void changeApplicationType();
void updateStatusLogDisplay(const CHAR *);


BOOL OpenWaveFile(char* FileNameAndPath);
VOID printPCMInfo();
VOID streamPlayback();
BOOL OpenOutputDevice();

DWORD WINAPI VoIPRecordAudioThread(LPVOID lpParameter);
DWORD WINAPI VoIPPlayAudioThread(LPVOID lpParameter);

// VoIP Functions
BOOL WaveMakeHeader(unsigned long ulSize, HGLOBAL HData, HGLOBAL HWaveHdr, LPSTR lpData, LPWAVEHDR lpWaveHdr);
void WaveFreeHeader(HGLOBAL HData, HGLOBAL HWaveHdr);

BOOL WaveRecordOpen(LPHWAVEIN lphwi, HWND Hwnd, int nChannels, long lFrequency, int nBits);
BOOL WaveRecordBegin(HWAVEIN hwi, LPWAVEHDR lpWaveHdr);
void WaveRecordEnd(HWAVEIN hwi, LPWAVEHDR lpWaveHdr);
void WaveRecordClose(HWAVEIN hwi);

BOOL WavePlayOpen(LPHWAVEOUT lphwi, HWND Hwnd, int nChannels, long lFrequency, int nBits);
BOOL WavePlayBegin(HWAVEOUT, LPWAVEHDR);
void WavePlayEnd(HWAVEOUT, LPWAVEHDR);
void WavePlayClose(HWAVEOUT);
void CALLBACK waveInProc(HWAVEIN hwi, UINT uMsg,DWORD_PTR dwInstance, DWORD_PTR dwParam1, DWORD_PTR dwParam2);
