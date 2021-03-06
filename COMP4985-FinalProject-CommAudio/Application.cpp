/*------------------------------------------------------------------------------------------------------------------
--	SOURCE FILE:	Application.cpp - The application layer. The main entry point
--										for the program. Sets up the window, WinMain, WndProc,
--										and handles user input and displaying information to screen.
--
--
--	PROGRAM:		Protocol Analysis
--
--
--	FUNCTIONS:		
--					void displayStatistics(int received, double duration);
--					char* getSendBuffer();
--					void generateTCPSendBufferData(int size);
--					void generateUDPSendBufferData(int size);
--					void openInputDialog();
--					int getPort();
--					char* getHostIP();
--					int getPacketSize();
--					void setPacketSize(int packetSize);
--					int getNumPackets();
--					void setNumPackets(int numPackets);
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
#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers


#include "Application.h"
#include "Transport.h"
#include "Session.h"
#include "targetver.h"

// C RunTime Header Files
#include <stdlib.h>
#include <malloc.h>
#include <memory.h>
#include <tchar.h>
#include "winmenu.h"
#include <string>
#include "resource1.h"
#include <sstream>
#include <vector>

TCHAR Name[] = TEXT("Protocol Analysis");
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
#pragma warning (disable: 4096)
LPSOCKET_INFORMATION SocketInfo;
HANDLE ThreadHandle;
DWORD ThreadId;
char *sendBuffer;
char *addressOfEnd;
int PORT = 0;
char HOSTIP[255];
char filePath[1024];
int PACKETSIZE = 0;
int NUMPACKETS = 0;
bool fileFlag = false;
int fileIndex = 0;
int filenameRecvFlag = 0;
WSAEVENT eventArray[1];

WNDCLASSEX wc;
MSG Msg;
HDC dc;

DWORD applicationType;

// Window and Control Handles
HWND hwnd;

enum applicationType { CLIENT = 2, SERVER };

// Labels
HWND applicationTypeTitle;

// Edit Texts/Static Texts
HWND ipEditText;
HWND portEditText;
HWND inputEditText;

HWND statusLogDisplayText;

// Buttons				Client						Server
HWND btn1;			//  Request for File			Start Listening
HWND btn2;			//	Listen Radio				Start Radio
HWND btn3;			//	Voicechat					Voicechat
HWND btn4;			//	Stream						---


// Begin jeff_unicast global variables
// TODO: consolidate global variables to eliminate redundancy
PCMWAVEFORMAT PCMWaveFmtRecord;
WAVEHDR WaveHeader;
WAVEHDR wh[NUM_BUFFS];
HANDLE waveDataBlock;
HWAVEOUT wo;
linked_list playQueue = linked_list();
LPSOCKET_INFORMATION SInfo;
sockaddr_in peer;

int playBackFlag = 0;

std::vector<std::string> waveList;
// end jeff_unicast global variables 

// Kiaan VoIP Variables
HWAVEIN wi;
WAVEHDR recordingWh[NUM_BUFFS];
WAVEFORMATEX wfx;
linked_list sendQueue = linked_list();

BOOL CALLBACK WaveListProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	

	switch (uMsg)
	{

	case WM_INITDIALOG: {
		HWND hwndList = GetDlgItem(hwndDlg, IDC_WAVELIST);
		for (int i = 0; i < waveList.size(); ++i) {
			int pos = (int)SendMessage(hwndList, LB_ADDSTRING, 0, (LPARAM)((LPCSTR)waveList[i].c_str()));
			SendMessage(hwndList, LB_SETITEMDATA, pos, (LPARAM)i);
		}
		//SetFocus(hwndList);
		return TRUE;
	}
	case WM_COMMAND:
		switch (LOWORD(wParam))
		{
		case IDC_WAVELIST_OK: {
			DWORD SendBytes;
			HWND hwndList = GetDlgItem(hwndDlg, IDC_WAVELIST);
			int lbItem = (int)SendMessage(hwndList, LB_GETCURSEL, 0, 0);
			int i = (int)SendMessage(hwndList, LB_GETITEMDATA, lbItem, 0);
			strcpy_s(filePath, waveList[i].c_str());
			OutputDebugStringA(filePath);

			if (IsDlgButtonChecked(hwndDlg, IDC_FILE_TRANSFER) == BST_CHECKED) {
				if ((ThreadHandle = CreateThread(NULL, 0, TCPClientWorkerThread, (LPVOID)SocketInfo, 0, &ThreadId)) == NULL) {
					OutputDebugStringA("CreateThread failed");
					return 0;
				}
			}
			else if (IsDlgButtonChecked(hwndDlg, IDC_UNICAST_STREAM) == BST_CHECKED) {
				char metaData[DATA_BUFSIZE];
				sprintf_s(metaData, "S%s", filePath);

				SocketInfo->DataBuf.buf = metaData; //filePath is ding.wav or whateve the user entered in the UI
				SocketInfo->DataBuf.len = DATA_BUFSIZE;

				if (WSASend(SocketInfo->Socket, &(SocketInfo->DataBuf), 1, &SendBytes, 0, NULL, NULL) == SOCKET_ERROR) {
					if (WSAGetLastError() != WSAEWOULDBLOCK) {
						OutputDebugStringA("WSA send metadata failed in fd_connect");
					}
				}

				initUnicastRecv();
			}
			else {
				MessageBoxA(NULL, "Please select a transfer type!", "Error", MB_OK | MB_ICONINFORMATION);
				return FALSE;
			}

			
			// fall through
		}
		case IDC_WAVELIST_CANCEL:
			EndDialog(hwndDlg, 0);
			return TRUE;
		}
	}

	return FALSE;
}


/*------------------------------------------------------------------------------------------------------------------
--	FUNCTION:		WinMain
--
--
--	DATE:			Feb 13, 2019
--
--
--	REVISIONS:
--
--
--	DESIGNER:		Jeffrey Choy, Microsoft, Aman Abdulla
--
--
--	PROGRAMMER:		Jeffrey Choy
--
--
--	INTERFACE:		int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hprevInstance, LPTSTR lspszCmdParam, int nCmdShow)
--							HINSTANCE hInst: The handle to the current application instance
--							HINSTANCE hPrevInstance: The handle to the previous application instance
--							LPTSTR lspszCmdParam: The command line parameters
--							int nCmdShow: A flag that says how the window will be displayed
--
--
--	RETURNS:		int
--
--
--	NOTES:			Sets up application window and registers it with the OS. Runs main messsage loop.
----------------------------------------------------------------------------------------------------------------------*/
int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hprevInstance, LPSTR lspszCmdParam, int nCmdShow)
{
	initializeWindow(hInst);

	if (hwnd == NULL)
	{
		MessageBox(NULL, "Window Creation Failed!", "Error!",
			MB_ICONEXCLAMATION | MB_OK);
		return 0;
	}

	ShowWindow(hwnd, nCmdShow);
	UpdateWindow(hwnd);
	displayGUIControls();

	RedrawWindow(hwnd, NULL, NULL, RDW_ERASE | RDW_INVALIDATE);

	while (GetMessage(&Msg, NULL, 0, 0) > 0)
	{
		TranslateMessage(&Msg);
		DispatchMessage(&Msg);
	}
	return Msg.wParam;
}

/*------------------------------------------------------------------------------------------------------------------
--	FUNCTION:		WndProc
--
--
--	DATE:			January 22, 2019
--
--
--	REVISIONS:
--
--
--	DESIGNER:		Jeffrey Choy, Microsoft, Aman Abdulla
--
--
--	PROGRAMMER:		Jeffrey Choy
--
--
--	INTERFACE:		LRESULT CALLBACK WndProc(HWND hwnd, UINT Message, WPARAM wParam, LPARAM lParam)
--							HWND hwnd: The handle to the window
--							UINT Message: The message received from the operating system
--							WPARAM wParam: Additional information about the message
--							LPARAM lParam: Additional information about the message
--
--
--	RETURNS:		int
--
--
--	NOTES:			Handles messages sent from Windows to application. Examples include connecting to Comm ports,
--					sending characters when in Connect mode, escaping Connect mode, accesing Help menu, and exiting
--					the program.
----------------------------------------------------------------------------------------------------------------------*/
LRESULT CALLBACK WndProc(HWND hwnd, UINT Message, WPARAM wParam, LPARAM lParam)
{

	SOCKET Accept;
	int iRc;
	char errorMessage[1024];

	switch (Message)
	{
	case WM_COMMAND:
		switch (LOWORD(wParam))
		{
		case IDM_DESTROY: // quit the program
			GlobalFree(SocketInfo);
			PostQuitMessage(0);
			break;
		case ID_CLIENT:		// Switch to Client application
			applicationType = CLIENT;
			changeApplicationType();
			break;
		case ID_SERVER:		// Switch to Server application
			applicationType = SERVER;
			changeApplicationType();
			break;
		case ID_HELP:		// Display help button
			MessageBox(NULL, HELP_MSG, HELP_TITLE, MB_ICONQUESTION | MB_OK);
			break;
		case ID_BTN1:
			if (applicationType == CLIENT)
			{
				// Request for file
				char portText[256];
				GetWindowTextA(portEditText, portText, 256);
				PORT = atoi(portText);
				//char HOSTIP[255];
				GetWindowTextA(ipEditText, HOSTIP, 256);
				connectTCP(hwnd);
			}
			else if (applicationType == SERVER)
			{
				// Start listening
				char portText[256];
				GetWindowTextA(portEditText, portText, 256);
				PORT = atoi(portText);
				//char HOSTIP[255];
				GetWindowTextA(ipEditText, HOSTIP, 256);
				listenTCP(hwnd);
			}
			break;
		case ID_BTN2:
			if (applicationType == CLIENT)
			{
				// Listen Radio
				initMulticastRecv();

			}
			else if (applicationType == SERVER)
			{
				// Start Radio
				openInputDialog();
				initMulticastSend();
			}
			break;
		case ID_BTN3:
			if (applicationType == CLIENT)
			{
				// Voice chat
				WSADATA WSAData;
				DWORD wVersionRequested;
				char portText[256];
				GetWindowTextA(portEditText, portText, 256);
				PORT = atoi(portText);

				GetWindowTextA(ipEditText, HOSTIP, 256);

				wVersionRequested = MAKEWORD(2, 2);

				if ((WSAStartup(wVersionRequested, &WSAData)) != 0)
				{
					printf("WSAStartup failed with error \n");
					return 0;
				}
				initVoipSend();
				initVoipRecv();
			}
			else if (applicationType == SERVER)
			{
				// Voice chat

			}
			break;
		case ID_BTN4:
			if (applicationType == CLIENT)
			{
				// Stream

			}
			break;
		default:
			break;
		}
		break;
	case WM_DESTROY:	// Terminate program
		GlobalFree(SocketInfo);
		PostQuitMessage(0);
		break;
	case WM_SOCKET:
		if (WSAGETSELECTERROR(lParam)) {
			char errorMessage[1024];
			sprintf_s(errorMessage, "Socket failed with error %d\n", WSAGETSELECTERROR(lParam));
			OutputDebugStringA(errorMessage);
			closesocket(SocketInfo->Socket);
			GlobalFree(SocketInfo);
		}
		else {
			switch (WSAGETSELECTEVENT(lParam)) 
			{
			case FD_ACCEPT: {
				DWORD SendBytes;
				WSABUF waveListData;

				// accept connection
				if ((Accept = accept(wParam, NULL, NULL)) == INVALID_SOCKET)
				{
					printf("accept() failed with error %d\n", WSAGetLastError());
					break;
				}

				// Create a socket information structure and populate to associate with the
				// socket for processing I/O.
				if ((SocketInfo = (LPSOCKET_INFORMATION)GlobalAlloc(GPTR,
					sizeof(SOCKET_INFORMATION))) == NULL)
				{
					printf("GlobalAlloc() failed with error %d\n", GetLastError());
					return 0;
				}
				SocketInfo->Socket = Accept;
				SocketInfo->BytesSEND = 0;
				SocketInfo->BytesRECV = 0;


				sprintf_s(errorMessage, "Socket number %d connected\n", Accept);
				OutputDebugStringA(errorMessage);

				waveListData.buf = getDirectory(PACKET_SIZE);
				waveListData.len = PACKET_SIZE;
				if (WSASend(SocketInfo->Socket, &waveListData, 1, &SendBytes, 0, NULL, NULL) == SOCKET_ERROR) {
					if (WSAGetLastError() != WSAEWOULDBLOCK) {
						OutputDebugStringA("WSA send failed in fd_connect");
					}
				}
				OutputDebugStringA("Send Success\n");
				WSAAsyncSelect(Accept, hwnd, WM_SOCKET, FD_READ | FD_CLOSE);
				break;
			}
			case FD_CONNECT: {
				WSABUF controlData;
				char controlFrame[PACKET_SIZE];
				DWORD SendBytes;
				WSABUF data;
				DWORD RecvBytes;
				DWORD flags = 0;
				WSAEVENT EventArray[1];
				EventArray[0] = WSACreateEvent();


				data.buf = controlFrame;
				data.len = PACKET_SIZE;

				if ((iRc = WSAGETSELECTERROR(lParam)) == 0) {
					OutputDebugStringA("FD_CONNECT success\n");
				}
				else {
					OutputDebugStringA("FD_CONNECT fail\n");
				}

				if ((SocketInfo = (LPSOCKET_INFORMATION)GlobalAlloc(GPTR,
					sizeof(SOCKET_INFORMATION))) == NULL)
				{
					printf("GlobalAlloc() failed with error %d\n", GetLastError());
					return 0;
				}
				SocketInfo->Socket = wParam;
				SocketInfo->BytesSEND = 0;
				SocketInfo->BytesRECV = 0;

				// session control test
				WSAWaitForMultipleEvents(1, EventArray, FALSE, 1000, TRUE);
				if (WSARecv((SOCKET)wParam, &data, 1, &RecvBytes, &flags, NULL, NULL) == SOCKET_ERROR) {
					if (WSAGetLastError() != WSAEWOULDBLOCK) {
						char erroMessage[1024];
						sprintf_s(erroMessage, "WSA receive: %d", WSAGetLastError());
						OutputDebugString("WSARecv failed in FD_READ");
						OutputDebugString(erroMessage);
					}
				}
				else {

					// display dialog box and get user input
					OutputDebugString("\nReceived from Server:\n");
					OutputDebugString(data.buf);

					std::stringstream ss(data.buf);
					std::string to;

					while (std::getline(ss, to, '\n')) {
						waveList.push_back(to);
					}

					DialogBoxA(NULL, MAKEINTRESOURCE(IDD_FORMVIEW), hwnd, WaveListProc);				
				}

				break;
			}
			case FD_READ: {
				int n;
				char controlFrame[PACKET_SIZE];
				DWORD flags = 0;
				WSABUF data;
				DWORD RecvBytes;

				data.buf = controlFrame;
				data.len = PACKET_SIZE;

				if (filenameRecvFlag == 1) {
					break;
				}
				Sleep(500);

				if (WSARecv((SOCKET)wParam, &data, 1, &RecvBytes, &flags, NULL, NULL) == SOCKET_ERROR) {
					if (WSAGetLastError() != WSAEWOULDBLOCK) {
						OutputDebugString("WSARecv failed in FD_READ");
					}
				}
				else {
					OutputDebugString("Control:");
					OutputDebugString(data.buf);
					strcpy_s(filePath, data.buf + 1);
					filenameRecvFlag = 1;
					WSAAsyncSelect((SOCKET)wParam, hwnd, WM_SOCKET, FD_CLOSE);

					if (data.buf[0] == 'T') {
						if ((ThreadHandle = CreateThread(NULL, 0, TCPServerWorkerThread, (LPVOID)SocketInfo, 0, &ThreadId)) == NULL) {
							OutputDebugStringA("CreateThread failed");
							return 0;
						}
					}
					else if (data.buf[0] == 'S') {
						initUnicastSend();
					}
					

				}

				break;
			}

			case FD_CLOSE:
				OutputDebugStringA("Closing socket\n");
				closesocket(SocketInfo->Socket);
				GlobalFree(SocketInfo);
				break;
			}
		}
		break;

	case MM_WOM_DONE: {
		MMRESULT mmr;
		char msg[1024];
		node *n;

		mmr = waveOutUnprepareHeader((HWAVEOUT)wParam, (LPWAVEHDR)lParam, sizeof(WAVEHDR));
		if (mmr != MMSYSERR_NOERROR) {
			printf("Error unpreparing header - MMERROR: %d", mmr);
			ExitProcess(4);
		}


		n = playQueue.get_next();
		if (n != NULL) {
			memcpy(wh[playBackFlag].lpData, n->data, n->size);
			wh[playBackFlag].dwBufferLength = n->size;
			linked_list::free_node(n);


			mmr = waveOutPrepareHeader((HWAVEOUT)wParam, &wh[playBackFlag], sizeof(WAVEHDR));
			if (mmr != MMSYSERR_NOERROR) {
				printf("Error preparing header - MMERROR: %d", mmr);
				waveOutGetErrorText(mmr, msg, 1024);
				ExitProcess(2);
			}

			mmr = waveOutWrite((HWAVEOUT)wParam, &wh[playBackFlag], sizeof(WAVEHDR));
			if (mmr != MMSYSERR_NOERROR) {
				printf("Error waveoutwrite - MMERROR: %d", mmr);
				waveOutGetErrorText(mmr, msg, 1024);
				ExitProcess(3);
			}
			playBackFlag++;
			if (playBackFlag > NUM_BUFFS - 1) {
				playBackFlag = 0;
			}
		}


		break;
	}
	default:
		return DefWindowProc(hwnd, Message, wParam, lParam);
	}
	return 0;
}


int initializeWindow(HINSTANCE hInstance)
{
	wc.cbSize = sizeof(WNDCLASSEX);
	wc.style = 0;
	wc.lpfnWndProc = WndProc;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hInstance = hInstance;
	wc.hIcon = LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(IDC_MYICON));
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	wc.hbrBackground = (HBRUSH)(COLOR_WINDOW);
	wc.lpszMenuName = MAKEINTRESOURCE(IDR_MYMENU);;
	wc.lpszClassName = windowName;
	wc.hIconSm = (HICON)LoadImage(GetModuleHandle(NULL), MAKEINTRESOURCE(IDC_MYICON), IMAGE_ICON, 16, 16, 0);

	if (!RegisterClassEx(&wc))
	{
		MessageBox(NULL, "Window Registration Failed!", "Error!",
			MB_ICONEXCLAMATION | MB_OK);
		return 0;
	}

	hwnd = CreateWindowEx(
		WS_EX_CLIENTEDGE,
		windowName,
		windowName,
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, CW_USEDEFAULT, WINDOW_WIDTH, WINDOW_LENGTH,
		NULL, NULL, hInstance, NULL);

	return 1;
}

/*------------------------------------------------------------------------------------------------------------------
-- FUNCTION: displayControls()
--
-- DATE: January 19, 2019
--
-- DESIGNER: Kiaan Castillo
--
-- PROGRAMMER: Kiaan Castillo
--
-- INTERFACE: void displayControls()
--
-- RETURNS: void
--
-- NOTES:
-- Creates all of the edit text and button controls.
----------------------------------------------------------------------------------------------------------------------*/
void displayGUIControls()
{
	applicationTypeTitle = CreateWindow("STATIC", APPLICATION_TYPE_LABEL_DEFAULT, WS_VISIBLE | WS_CHILD, X_COORDINATE, Y_COORDINATE, STATICTEXT_LENGTH, EDITTEXT_WIDTH, hwnd, NULL, NULL, NULL);
	CreateWindow("STATIC", STATUS_LOG_LABEL, WS_VISIBLE | WS_CHILD, X_COORDINATE, Y_COORDINATE + Y_COORDINATE_ADDITION, EDITTEXT_LENGTH, EDITTEXT_WIDTH, hwnd, NULL, NULL, NULL);
	statusLogDisplayText = CreateWindow("STATIC", STATUS_LOG_DEFAULT, WS_VISIBLE | WS_CHILD, X_COORDINATE, Y_COORDINATE + (Y_COORDINATE_ADDITION * 2), STATICTEXT_LENGTH, EDITTEXT_WIDTH, hwnd, NULL, NULL, NULL);

	CreateWindow("STATIC", IP_LABEL, WS_VISIBLE | WS_CHILD, X_COORDINATE, Y_COORDINATE + (Y_COORDINATE_ADDITION * 5), EDITTEXT_LENGTH, EDITTEXT_WIDTH, hwnd, NULL, NULL, NULL);
	ipEditText = CreateWindow("EDIT", "", WS_VISIBLE | WS_CHILD | WS_BORDER, X_COORDINATE, Y_COORDINATE + (Y_COORDINATE_ADDITION * 6), EDITTEXT_LENGTH, EDITTEXT_WIDTH, hwnd, NULL, NULL, NULL);

	CreateWindow("STATIC", PORT_LABEL, WS_VISIBLE | WS_CHILD, X_COORDINATE + (X_COORDINATE_ADDITION * 5), Y_COORDINATE + (Y_COORDINATE_ADDITION * 5), EDITTEXT_LENGTH, EDITTEXT_WIDTH, hwnd, NULL, NULL, NULL);
	portEditText = CreateWindow("EDIT", "", WS_VISIBLE | WS_CHILD | WS_BORDER, X_COORDINATE + (X_COORDINATE_ADDITION * 5), Y_COORDINATE + (Y_COORDINATE_ADDITION * 6), EDITTEXT_LENGTH, EDITTEXT_WIDTH, hwnd, NULL, NULL, NULL);

	//CreateWindow("STATIC", INPUT_LABEL, WS_VISIBLE | WS_CHILD, X_COORDINATE + (X_COORDINATE_ADDITION * 6.5), Y_COORDINATE + (Y_COORDINATE_ADDITION * 5), EDITTEXT_LENGTH, EDITTEXT_WIDTH, hwnd, NULL, NULL, NULL);
	//inputEditText = CreateWindow("EDIT", "", WS_VISIBLE | WS_CHILD | WS_BORDER, X_COORDINATE + (X_COORDINATE_ADDITION * 6.5), Y_COORDINATE + (Y_COORDINATE_ADDITION * 6), EDITTEXT_LENGTH * 1.4, EDITTEXT_WIDTH, hwnd, NULL, NULL, NULL);

	btn1 = CreateWindow("BUTTON", BTN_NA_LABEL, WS_VISIBLE | WS_CHILD, X_COORDINATE, Y_COORDINATE + (Y_COORDINATE_ADDITION * 7.5), BTN_LENGTH, EDITTEXT_WIDTH, hwnd, (HMENU)ID_BTN1, NULL, NULL);
	btn2 = CreateWindow("BUTTON", BTN_NA_LABEL, WS_VISIBLE | WS_CHILD, X_COORDINATE + (X_COORDINATE_ADDITION * 2.75), Y_COORDINATE + (Y_COORDINATE_ADDITION * 7.5), BTN_LENGTH, EDITTEXT_WIDTH, hwnd, (HMENU)ID_BTN2, NULL, NULL);
	btn3 = CreateWindow("BUTTON", BTN_NA_LABEL, WS_VISIBLE | WS_CHILD, X_COORDINATE + (X_COORDINATE_ADDITION * 5.5), Y_COORDINATE + (Y_COORDINATE_ADDITION * 7.5), BTN_LENGTH, EDITTEXT_WIDTH, hwnd, (HMENU)ID_BTN3, NULL, NULL);
	//btn4 = CreateWindow("BUTTON", BTN_NA_LABEL, WS_VISIBLE | WS_CHILD, X_COORDINATE + (X_COORDINATE_ADDITION * 8.25), Y_COORDINATE + (Y_COORDINATE_ADDITION * 7.5), BTN_LENGTH, EDITTEXT_WIDTH, hwnd, (HMENU)ID_BTN4, NULL, NULL);

}

/*------------------------------------------------------------------------------------------------------------------
-- FUNCTION: changeMode()
--
-- DATE: January 19, 2019
--
-- DESIGNER: Kiaan Castillo
--
-- PROGRAMMER: Kiaan Castillo
--
-- INTERFACE: void changeMode()
--
-- RETURNS: void
--
-- NOTES:
-- Changes the title and labels on the window depending on the mode chosen.
----------------------------------------------------------------------------------------------------------------------*/
void changeApplicationType()
{
	switch (applicationType)
	{
	case -1:
		SetWindowText(applicationTypeTitle, APPLICATION_TYPE_LABEL_DEFAULT);
		SetWindowText(btn1, BTN_NA_LABEL);
		SetWindowText(btn2, BTN_NA_LABEL);
		SetWindowText(btn3, BTN_NA_LABEL);
		SetWindowText(btn4, BTN_NA_LABEL);
		break;
	case CLIENT:
		SetWindowText(applicationTypeTitle, APPLICATION_TYPE_LABEL_CLIENT);
		SetWindowText(btn1, REQUESTFORFILE_BTN_LABEL);
		SetWindowText(btn2, RADIO_BTN_LABEL_CLIENT);
		SetWindowText(btn3, VOICECHAT_BTN_LABEL);
		SetWindowText(btn4, STREAM_BTN_LABEL);
		break;
	case SERVER:
		SetWindowText(applicationTypeTitle, APPLICATION_TYPE_LABEL_SERVER);
		SetWindowText(btn1, STARTLISTENING_BTN_LABEL);
		SetWindowText(btn2, RADIO_BTN_LABEL_SERVER);
		SetWindowText(btn3, VOICECHAT_BTN_LABEL);
		SetWindowText(btn4, BTN_NA_LABEL);
		break;
	default:
		break;
	}

	updateStatusLogDisplay(STATUS_LOG_CHANGEDAPP);
	RedrawWindow(hwnd, NULL, NULL, RDW_ERASE | RDW_INVALIDATE);
}

void updateStatusLogDisplay(const CHAR * newStatus)
{
	SetWindowText(statusLogDisplayText, newStatus);
}


/*------------------------------------------------------------------------------------------------------------------
--	FUNCTION:		generateTCPSendBufferData
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
--	INTERFACE:		void generateTCPSendBufferData(int size)
--							int size: size of packet
--
--	RETURNS:		
--
--
--	NOTES:			Sets the send buffer for TCP with data
----------------------------------------------------------------------------------------------------------------------*/
void generateTCPSendBufferData(char* fileName, int size) {
	FILE *fp;
	int32_t lSize;

	fopen_s(&fp, fileName, "rb");
	fseek(fp, 0L, SEEK_END);
	lSize = ftell(fp);
	rewind(fp);

	sendBuffer = (char *)malloc(sizeof(char) * lSize);
	if (fread(sendBuffer, lSize, 1, fp) == -1) {
		exit(1);
	}

	addressOfEnd = &sendBuffer[lSize];
	fclose(fp);

}


/*------------------------------------------------------------------------------------------------------------------
--	FUNCTION:		openInputDialog
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
--	INTERFACE:		void openInputDialog()
--
--	RETURNS:
--
--
--	NOTES:			Opens dialog that gets and stores user input for a file path.
----------------------------------------------------------------------------------------------------------------------*/
void openInputDialog() {
	OPENFILENAMEA ofn;       // common dialog box structure
							 // buffer for file name
	//HWND hwnd = { 0 };      // owner window
	HANDLE hf;              // file handle
	char buffer[1024];

	LPDWORD numByteRead = 0;

	// Initialize OPENFILENAME
	ZeroMemory(&ofn, sizeof(ofn));
	ofn.lStructSize = sizeof(ofn);
	ofn.hwndOwner = hwnd;
	ofn.lpstrFile = filePath;
	// Set lpstrFile[0] to '\0' so that GetOpenFileName does not
	// use the contents of filePath to initialize itself.
	ofn.lpstrFile[0] = '\0';
	ofn.nMaxFile = sizeof(filePath);
	ofn.lpstrFilter = "wave\0*.WAV\0";
	ofn.nFilterIndex = 1;
	ofn.lpstrFileTitle = NULL;
	ofn.nMaxFileTitle = 0;
	ofn.lpstrInitialDir = NULL;
	ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

	// Display the Open dialog box.
	if (GetOpenFileNameA(&ofn) == TRUE) {
		hf = CreateFileA(ofn.lpstrFile, GENERIC_READ | GENERIC_WRITE, 0,
			(LPSECURITY_ATTRIBUTES)NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, (HANDLE)NULL);
		int filesize = GetFileSize(hf, NULL);
		int tmp;
		OutputDebugStringA(filePath);
		CloseHandle(hf);
	}
}

/*------------------------------------------------------------------------------------------------------------------
--	FUNCTION:		getPort
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
--	INTERFACE:		int getPort()
--
--	RETURNS:		Port number
--
--
--	NOTES:			Gets port number specifed by the user.
----------------------------------------------------------------------------------------------------------------------*/
int getPort() {
	return PORT;
}

/*------------------------------------------------------------------------------------------------------------------
--	FUNCTION:		getHostIP
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
--	INTERFACE:		char *getHostIP()
--
--	RETURNS:		Host IP number
--
--
--	NOTES:			Gets IP number specifed by the user.
----------------------------------------------------------------------------------------------------------------------*/
char *getHostIP() {
	return &(HOSTIP[0]);
}



BOOL OpenWaveFile(char* FileNameAndPath)
{
	MMCKINFO MMCkInfoParent;
	MMCKINFO MMCkInfoChild;
	int errorCode;
	HWAVEOUT hWaveOut;

	// open multimedia file
	HMMIO hmmio = mmioOpen((LPSTR)FileNameAndPath, NULL, MMIO_READ);
	if (!hmmio) {
		printf("mmioOpen error\n");
		return FALSE;
	}

	// initialize WAVE header struct
	MMCkInfoParent.fccType = mmioFOURCC('W', 'A', 'V', 'E');

	// enter child chunk
	errorCode = mmioDescend(hmmio, &MMCkInfoParent, NULL, MMIO_FINDRIFF);
	if (errorCode) {
		printf("mmioDescend error\n");
		mmioClose(hmmio, 0);
		return FALSE;
	}

	// initialize "FMT" sub-chunk struct
	MMCkInfoChild.ckid = mmioFOURCC('f', 'm', 't', ' ');

	// enter fmt sub-chunk
	errorCode = mmioDescend(hmmio, &MMCkInfoChild, &MMCkInfoParent, MMIO_FINDCHUNK);
	if (errorCode) {
		printf("mmioDescend Child error\n");
		mmioClose(hmmio, 0);
		return FALSE;
	}

	// read fmt subchunk data into Waveformat structure
	DWORD bytesRead = mmioRead(hmmio, (LPSTR)&PCMWaveFmtRecord, MMCkInfoChild.cksize);
	if (bytesRead <= 0) {
		printf("mmioRead error\n");
		mmioClose(hmmio, 0);
		return FALSE;
	}

	// check if output device can play format
	errorCode = waveOutOpen(&hWaveOut, WAVE_MAPPER, (WAVEFORMATEX*)&PCMWaveFmtRecord, 0l, 0l, WAVE_FORMAT_QUERY);
	if (errorCode) {
		printf("Incompatible WAVE format\n");
		mmioClose(hmmio, 0);
		return FALSE;
	}

	// exit fmt subchunk
	errorCode = mmioAscend(hmmio, &MMCkInfoChild, 0);
	if (errorCode) {
		printf("mmioAescend Child error\n");
		mmioClose(hmmio, 0);
		return FALSE;
	}

	// init data subchunk
	MMCkInfoChild.ckid = mmioFOURCC('d', 'a', 't', 'a');

	// enter data subchunk
	errorCode = mmioDescend(hmmio, &MMCkInfoChild, &MMCkInfoParent, MMIO_FINDCHUNK);
	if (errorCode) {
		printf("mmioDescend Child error\n");
		mmioClose(hmmio, 0);
		return FALSE;
	}

	// get data size from data subchunk
	long lDataSize = MMCkInfoChild.cksize;
	HANDLE waveDataBlock = ::GlobalAlloc(GMEM_MOVEABLE, lDataSize);
	if (waveDataBlock == NULL) {
		printf("error alloc mem\n");
		mmioClose(hmmio, 0);
		return FALSE;
	}

	LPBYTE pWave = (LPBYTE)::GlobalLock(waveDataBlock);

	if (mmioRead(hmmio, (LPSTR)pWave, lDataSize) != lDataSize) {
		printf("error reading data\n");
		mmioClose(hmmio, 0);
		::GlobalFree(waveDataBlock);
		return FALSE;
	}

	WaveHeader.lpData = (LPSTR)pWave;
	WaveHeader.dwBufferLength = lDataSize;
	WaveHeader.dwFlags = 0L;
	WaveHeader.dwLoops = 0L;

	mmioClose(hmmio, 0);
	return TRUE;

}

VOID printPCMInfo() {
	char pcm[4096];
	sprintf_s(pcm, "PCM Wave Format Record:\n");
	sprintf_s(pcm, "wFormatTag: %d\n", PCMWaveFmtRecord.wf.wFormatTag);
	sprintf_s(pcm, "nChannels: %d\n", PCMWaveFmtRecord.wf.nChannels);
	sprintf_s(pcm, "nSamplesPerSec: %d\n", PCMWaveFmtRecord.wf.nSamplesPerSec);
	sprintf_s(pcm, "nAvgBytesPerSec: %d\n", PCMWaveFmtRecord.wf.nAvgBytesPerSec);
	sprintf_s(pcm, "wBitsPerSample: %d\n\n", PCMWaveFmtRecord.wBitsPerSample);
	OutputDebugStringA(pcm);
}

VOID streamPlayback() {
	MMRESULT mmr;
	node* n;
	char msg[1024];

	for (int i = 0; i < NUM_BUFFS; i++) {
		wh[i].lpData = (char*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, PACKET_SIZE);
		n = playQueue.get_next();
		memcpy(wh[i].lpData, n->data, n->size);
		wh[i].dwBufferLength = n->size;
		linked_list::free_node(n);
		mmr = waveOutPrepareHeader(wo, &wh[i], sizeof(WAVEHDR));
		if (mmr != MMSYSERR_NOERROR) {
			printf("Error preparing header - MMERROR: %d", mmr);
			waveOutGetErrorText(mmr, msg, 1024);
			ExitProcess(2);
		}

		mmr = waveOutWrite(wo, &wh[i], sizeof(WAVEHDR));
		if (mmr != MMSYSERR_NOERROR) {
			printf("Wave out write error - MMERROR: %d", mmr);
			waveOutGetErrorText(mmr, msg, 1024);
			ExitProcess(3);
		}

	}
}

BOOL OpenOutputDevice() {

	//HWAVEOUT wo;
	MMRESULT mmr;
	char err[1024];

	mmr = waveOutOpen(&wo, WAVE_MAPPER, (WAVEFORMATEX*)&PCMWaveFmtRecord, (DWORD)hwnd, 0, CALLBACK_WINDOW);
	if (mmr != MMSYSERR_NOERROR) {
		sprintf_s(err, "Error opening device - MMERROR: %d", mmr);
		OutputDebugStringA(err);
		return (FALSE);
	}

	return (TRUE);
}

/*------------------------------------------------------------------------------------------------------------------
--	FUNCTION:		VoIPRecordAudioThread
--
--
--	DATE:			April 9, 2019
--
--
--	REVISIONS:
--
--
--	DESIGNER:		Kiaan Castillo
--
--
--	PROGRAMMER:		Kiaan Castillo
--
--
--	INTERFACE:		DWORD WINAPI VoIPRecordAudioThread(LPVOID lpParameter)
--
--	RETURNS:		DWORD
--
--
--	NOTES:			Continuously records the user's voice input for voice chat and adds the audio recording to a 
--					send queue
--					upon reaching the max packet size.
----------------------------------------------------------------------------------------------------------------------*/
DWORD WINAPI VoIPRecordAudioThread(LPVOID lpParameter)
{
	HGLOBAL hData = GlobalAlloc(GMEM_MOVEABLE | GMEM_SHARE, PACKET_SIZE);
	HGLOBAL hwaveHdr = GlobalAlloc(GMEM_MOVEABLE | GMEM_SHARE, sizeof(WAVEHDR));
	CHAR lpData;

	eventArray[0] = WSACreateEvent();

	if (!WaveRecordOpen(&wi, hwnd, NUM_CHANNELS, FREQUENCY, NUM_RECORD_BITS))
	{
		updateStatusLogDisplay("Status: Failed to open input device");
		return FALSE;
	}

	if (!WaveMakeHeader(PACKET_SIZE, hData, hwaveHdr, &lpData, recordingWh))
	{
		updateStatusLogDisplay("Failed to make header for recording audio");
		return FALSE;
	}

	if (!WaveRecordBegin(wi, recordingWh))
	{
		updateStatusLogDisplay("Failed to begin recording audio");
		return FALSE;
	}

	updateStatusLogDisplay("Started voice chat");
	while (true)
	{
		//while (recordingWh->dwBytesRecorded < PACKET_SIZE) { continue; }		// Keep recording audio until data packet is full
		WSAWaitForMultipleEvents(1, eventArray, FALSE, WSA_INFINITE, TRUE);

		// Send the recorded audio data
		//OutputDebugString("Sending new audio packet\n");
		sendQueue.add_node(recordingWh->lpData, recordingWh->dwBytesRecorded);
		WSAResetEvent(eventArray[0]);

		// Clean out header
		WaveFreeHeader(hData, hwaveHdr);										// Free the header
		if (!WaveMakeHeader(PACKET_SIZE, hData, hwaveHdr, &lpData, recordingWh))
		{
			updateStatusLogDisplay("Failed to remake header for recording audio");
			return FALSE;
		}

		if (!WaveRecordBegin(wi, recordingWh))
		{
			updateStatusLogDisplay("Failed to begin re-recording audio");
			return FALSE;
		}
	}

	return TRUE;
}


DWORD WINAPI VoIPPlayAudioThread(LPVOID lpParameter)
{
	PCMWaveFmtRecord.wBitsPerSample = NUM_RECORD_BITS;
	PCMWaveFmtRecord.wf.wFormatTag = WAVE_FORMAT_PCM;
	PCMWaveFmtRecord.wf.nChannels = NUM_CHANNELS;
	PCMWaveFmtRecord.wf.nSamplesPerSec = FREQUENCY;
	PCMWaveFmtRecord.wf.nAvgBytesPerSec = (PCMWaveFmtRecord.wf.nSamplesPerSec * PCMWaveFmtRecord.wf.nBlockAlign);

	if (!OpenOutputDevice())
	{
		updateStatusLogDisplay("Status: Failed to open output device");
		return FALSE;
	}

	streamPlayback();

	return TRUE;
}

/*------------------------------------------------------------------------------------------------------------------
--	FUNCTION:		WaveMakeHeader
--
--
--	DATE:			April 9, 2019
--
--
--	REVISIONS:
--
--
--	DESIGNER:		Kiaan Castillo
--
--
--	PROGRAMMER:		Kiaan Castillo
--
--
--	INTERFACE:		BOOL WaveMakeHeader(unsigned long ulSize, HGLOBAL HData, HGLOBAL HWaveHdr, LPSTR lpData, 
--						LPWAVEHDR lpWaveHdr)
--
--	RETURNS:		BOOL
--
--
--	NOTES:			Prepares a wave header for storing audio data
----------------------------------------------------------------------------------------------------------------------*/
BOOL WaveMakeHeader(unsigned long ulSize, HGLOBAL HData, HGLOBAL HWaveHdr, LPSTR lpData, LPWAVEHDR lpWaveHdr)
{
	HData = GlobalAlloc(GMEM_MOVEABLE | GMEM_SHARE, ulSize);
	if (!HData) return FALSE;

	lpData = (LPSTR)GlobalLock(HData);
	if (!lpData)
	{
		GlobalFree(HData);
		return FALSE;
	}

	HWaveHdr = GlobalAlloc(GMEM_MOVEABLE | GMEM_SHARE, sizeof(WAVEHDR));
	if (!HWaveHdr)
	{
		GlobalUnlock(HData);
		GlobalFree(HData);
		return FALSE;
	}

	if (!lpWaveHdr)
	{
		GlobalUnlock(HWaveHdr);
		GlobalFree(HWaveHdr);
		GlobalUnlock(HData);
		GlobalFree(HData);
		return FALSE;
	}

	ZeroMemory(lpWaveHdr, sizeof(WAVEHDR));
	lpWaveHdr->lpData = lpData;
	lpWaveHdr->dwBufferLength = ulSize;

	return TRUE;
}

/*------------------------------------------------------------------------------------------------------------------
--	FUNCTION:		WaveFreeHeader
--
--
--	DATE:			April 9, 2019
--
--
--	REVISIONS:
--
--
--	DESIGNER:		Kiaan Castillo
--
--
--	PROGRAMMER:		Kiaan Castillo
--
--
--	INTERFACE:		void WaveFreeHeader(HGLOBAL HData, HGLOBAL HWaveHdr)

--
--	RETURNS:		BOOL
--
--
--	NOTES:			Frees a wave header
----------------------------------------------------------------------------------------------------------------------*/
void WaveFreeHeader(HGLOBAL HData, HGLOBAL HWaveHdr)
{
	GlobalUnlock(HWaveHdr);
	GlobalFree(HWaveHdr);
	GlobalUnlock(HData);
	GlobalFree(HData);
}

/*------------------------------------------------------------------------------------------------------------------
--	FUNCTION:		WaveRecordOpen
--
--
--	DATE:			April 9, 2019
--
--
--	REVISIONS:
--
--
--	DESIGNER:		Kiaan Castillo
--
--
--	PROGRAMMER:		Kiaan Castillo
--
--
--	INTERFACE:		BOOL WaveRecordOpen(LPHWAVEIN lphwi, HWND Hwnd, int nChannels, long lFrequency, int nBits)
--
--
--	RETURNS:		BOOL
--
--
--	NOTES:			Opens an input device for audio recording
----------------------------------------------------------------------------------------------------------------------*/
BOOL WaveRecordOpen(LPHWAVEIN lphwi, HWND Hwnd, int nChannels, long lFrequency, int nBits)
{
	wfx.wFormatTag = WAVE_FORMAT_PCM;
	wfx.nChannels = (WORD)nChannels;
	wfx.nSamplesPerSec = (DWORD)lFrequency;
	wfx.wBitsPerSample = (WORD)nBits;
	wfx.nBlockAlign = (WORD)((wfx.nChannels * wfx.wBitsPerSample) / 8);
	wfx.nAvgBytesPerSec = (wfx.nSamplesPerSec * wfx.nBlockAlign);
	wfx.cbSize = 0;

	MMRESULT result = waveInOpen(lphwi, WAVE_MAPPER, (WAVEFORMATEX*)&wfx, (DWORD)waveInProc, (DWORD)Hwnd,
		CALLBACK_FUNCTION);

	if (result == MMSYSERR_NOERROR)
	{
		OutputDebugString("Successfully opened input device\n");
		return TRUE;
	}

	if (result == MMSYSERR_ALLOCATED)
		OutputDebugString("ERROR: WaveRecordOpen [MMSYSERR_ALLOCATED]\n");
	if (result == MMSYSERR_BADDEVICEID)
		OutputDebugString("ERROR: WaveRecordOpen [MMSYSERR_BADDEVICEID]\n");
	if (result == MMSYSERR_NODRIVER)
		OutputDebugString("ERROR: WaveRecordOpen [MMSYSERR_NODRIVER]\n");
	if (result == MMSYSERR_NOMEM)
		OutputDebugString("ERROR: WaveRecordOpen [MMSYSERR_NOMEM]\n");
	if (result == WAVERR_BADFORMAT)
		OutputDebugString("ERROR: WaveRecordOpen [WAVERR_BADFORMAT]\n");
	if (result == WAVERR_SYNC)
		OutputDebugString("ERROR: WaveRecordOpen [WAVERR_SYNC]\n");

	return FALSE;
}

/*------------------------------------------------------------------------------------------------------------------
--	FUNCTION:		WaveRecordBegin
--
--
--	DATE:			April 9, 2019
--
--
--	REVISIONS:
--
--
--	DESIGNER:		Kiaan Castillo
--
--
--	PROGRAMMER:		Kiaan Castillo
--
--
--	INTERFACE:		BOOL WaveRecordBegin(HWAVEIN hwi, LPWAVEHDR lpWaveHdr)
--
--
--	RETURNS:		BOOL
--
--
--	NOTES:			Begins recording on the passed in input device handle and stores it in the passed in wave header
----------------------------------------------------------------------------------------------------------------------*/
BOOL WaveRecordBegin(HWAVEIN hwi, LPWAVEHDR lpWaveHdr)
{
	MMRESULT result = waveInPrepareHeader(hwi, lpWaveHdr, sizeof(WAVEHDR));
	if (result == MMSYSERR_NOERROR)
	{
		MMRESULT result = waveInAddBuffer(hwi, lpWaveHdr, sizeof(WAVEHDR));
		if (result == MMSYSERR_NOERROR)
		{
			MMRESULT result = waveInStart(hwi);
			if (result == MMSYSERR_NOERROR) return TRUE;
		}
	}

	if (result == MMSYSERR_INVALHANDLE)
		OutputDebugStringA("ERROR: WaveRecordBegin [MMSYSERR_INVALHANDLE]\n");
	if (result == MMSYSERR_NODRIVER)
		OutputDebugStringA("ERROR: WaveRecordBegin [MMSYSERR_NODRIVER]\n");
	if (result == MMSYSERR_NOMEM)
		OutputDebugStringA("ERROR: WaveRecordBegin [MMSYSERR_NOMEM]\n");
	if (result == MMSYSERR_INVALPARAM)
		OutputDebugStringA("ERROR: WaveRecordBegin [MMSYSERR_INVALPARAM]\n");

	return FALSE;
}

/*------------------------------------------------------------------------------------------------------------------
--	FUNCTION:		WaveRecordEnd
--
--
--	DATE:			April 9, 2019
--
--
--	REVISIONS:
--
--
--	DESIGNER:		Kiaan Castillo
--
--
--	PROGRAMMER:		Kiaan Castillo
--
--
--	INTERFACE:		void WaveRecordEnd(HWAVEIN hwi, LPWAVEHDR lpWaveHdr)
--
--
--	RETURNS:		void
--
--
--	NOTES:			Stops recording audio input on the given input device and wave header
----------------------------------------------------------------------------------------------------------------------*/
void WaveRecordEnd(HWAVEIN hwi, LPWAVEHDR lpWaveHdr)
{
	waveInStop(hwi);
	waveInReset(hwi);
	waveInUnprepareHeader(hwi, lpWaveHdr, sizeof(WAVEHDR));
}

/*------------------------------------------------------------------------------------------------------------------
--	FUNCTION:		WaveRecordClose
--
--
--	DATE:			April 9, 2019
--
--
--	REVISIONS:
--
--
--	DESIGNER:		Kiaan Castillo
--
--
--	PROGRAMMER:		Kiaan Castillo
--
--
--	INTERFACE:		void WaveRecordClose(HWAVEIN hwi)
--
--
--	RETURNS:		void
--
--
--	NOTES:			Closes an input device
----------------------------------------------------------------------------------------------------------------------*/
void WaveRecordClose(HWAVEIN hwi)
{
	waveInReset(hwi);
	waveInClose(hwi);
}

/*------------------------------------------------------------------------------------------------------------------
--	FUNCTION:		WavePlayOpen
--
--
--	DATE:			April 9, 2019
--
--
--	REVISIONS:
--
--
--	DESIGNER:		Kiaan Castillo
--
--
--	PROGRAMMER:		Kiaan Castillo
--
--
--	INTERFACE:		BOOL WavePlayOpen(LPHWAVEOUT lphwo, HWND Hwnd, int nChannels, long lFrequency, int nBits)
--
--
--	RETURNS:		BOOL
--
--
--	NOTES:			Opens an output device for audio playback
----------------------------------------------------------------------------------------------------------------------*/
BOOL WavePlayOpen(LPHWAVEOUT lphwo, HWND Hwnd, int nChannels, long lFrequency, int nBits)
{
	WAVEFORMATEX wfx;
	wfx.wFormatTag = WAVE_FORMAT_PCM;
	wfx.nChannels = (WORD)nChannels;
	wfx.nSamplesPerSec = (DWORD)lFrequency;
	wfx.wBitsPerSample = (WORD)nBits;
	wfx.nBlockAlign = (WORD)((wfx.nChannels * wfx.wBitsPerSample) / 8);
	wfx.nAvgBytesPerSec = (wfx.nSamplesPerSec * wfx.nBlockAlign);
	wfx.cbSize = 0;

	MMRESULT result = waveOutOpen(lphwo, WAVE_MAPPER, &wfx, (LONG)Hwnd, NULL,
		CALLBACK_WINDOW);

	if (result == MMSYSERR_NOERROR) return true;
	return false;
}

/*------------------------------------------------------------------------------------------------------------------
--	FUNCTION:		WavePlayBegin
--
--
--	DATE:			April 9, 2019
--
--
--	REVISIONS:
--
--
--	DESIGNER:		Kiaan Castillo
--
--
--	PROGRAMMER:		Kiaan Castillo
--
--
--	INTERFACE:		BOOL WavePlayBegin(HWAVEOUT hwo, LPWAVEHDR lpWaveHdr)
--
--
--	RETURNS:		BOOL
--
--
--	NOTES:			Begins playback on the specified output device using data from the wave header passed in
----------------------------------------------------------------------------------------------------------------------*/
BOOL WavePlayBegin(HWAVEOUT hwo, LPWAVEHDR lpWaveHdr)
{
	MMRESULT result = waveOutPrepareHeader(hwo, lpWaveHdr, sizeof(WAVEHDR));
	if (result == MMSYSERR_NOERROR)
	{
		MMRESULT result = waveOutWrite(hwo, lpWaveHdr, sizeof(WAVEHDR));
		if (result == MMSYSERR_NOERROR) return true;
	}
	return false;
}

/*------------------------------------------------------------------------------------------------------------------
--	FUNCTION:		WavePlayEnd
--
--
--	DATE:			April 9, 2019
--
--
--	REVISIONS:
--
--
--	DESIGNER:		Kiaan Castillo
--
--
--	PROGRAMMER:		Kiaan Castillo
--
--
--	INTERFACE:		void WavePlayEnd(HWAVEOUT hwo, LPWAVEHDR lpWaveHdr)
--
--
--	RETURNS:		void
--
--
--	NOTES:			Ends playback on the specified output device
----------------------------------------------------------------------------------------------------------------------*/
void WavePlayEnd(HWAVEOUT hwo, LPWAVEHDR lpWaveHdr)
{
	waveOutReset(hwo);
	waveOutUnprepareHeader(hwo, lpWaveHdr, sizeof(WAVEHDR));
}

/*------------------------------------------------------------------------------------------------------------------
--	FUNCTION:		WavePlayClose
--
--
--	DATE:			April 9, 2019
--
--
--	REVISIONS:
--
--
--	DESIGNER:		Kiaan Castillo
--
--
--	PROGRAMMER:		Kiaan Castillo
--
--
--	INTERFACE:		void WavePlayClose(HWAVEOUT hwo)
--
--
--	RETURNS:		void
--
--
--	NOTES:			Closes an audio output device
----------------------------------------------------------------------------------------------------------------------*/
void WavePlayClose(HWAVEOUT hwo)
{
	waveOutReset(hwo);
	waveOutClose(hwo);
}

/*------------------------------------------------------------------------------------------------------------------
--	FUNCTION:		waveInProc
--
--
--	DATE:			April 9, 2019
--
--
--	REVISIONS:
--
--
--	DESIGNER:		Jeffrey Choy, Kiaan Castillo
--
--
--	PROGRAMMER:		Jeffrey Choy, Kiaan Castillo
--
--
--	INTERFACE:		void CALLBACK waveInProc(HWAVEIN hwi, UINT uMsg, DWORD_PTR dwInstance, DWORD_PTR dwParam1, 
--						DWORD_PTR dwParam2) 
--
--
--	RETURNS:		void
--
--
--	NOTES:			Handles audio events
----------------------------------------------------------------------------------------------------------------------*/
void CALLBACK waveInProc(HWAVEIN hwi, UINT uMsg, DWORD_PTR dwInstance, DWORD_PTR dwParam1, DWORD_PTR dwParam2) 
{
	switch (uMsg) 
	{
		case WIM_DATA:
			WSASetEvent(eventArray[0]);
			break;
	}
}