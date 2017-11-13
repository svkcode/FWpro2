// FWpro2.cpp : Defines the entry point for the application.
//


#include "stdafx.h"
#include "FWpro2.h"
#include "strsafe.h"
#include "CmdProcessor.h"
#include "Log.h"
#include <io.h>
#include <fcntl.h>
#include <time.h>

#define MAX_LOADSTRING 100

// Global Variables:
HINSTANCE hInst;                                // current instance
TCHAR szTitle[MAX_LOADSTRING];                  // The title bar text
TCHAR szWindowClass[MAX_LOADSTRING];            // the main window class name
HWND hWnd;
HWND hLogEdit;
HWND hTool;
HWND hStatus;
DWORD state;
CmdProcessor cmdProc;
HANDLE hThread;
HANDLE hstdout;


// Forward declarations of functions included in this code module:
ATOM                MyRegisterClass(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    PropDlg(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    Options(HWND, UINT, WPARAM, LPARAM);
void initToolbar(void);
void logAppend(const char * text);
void logClear(void);
void updateButtonStates(void);
DWORD WINAPI cmdProcThread(LPVOID lpParam);
DWORD WINAPI logThread(LPVOID lpParam);

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
	_In_opt_ HINSTANCE hPrevInstance,
	_In_ LPWSTR    lpCmdLine,
	_In_ int       nCmdShow)
{

	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);

	// TODO: Place code here.

	// Initialize global strings
	LoadString(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
	LoadString(hInstance, IDC_FWPRO2, szWindowClass, MAX_LOADSTRING);
	MyRegisterClass(hInstance);

	// Perform application initialization:
	if (!InitInstance(hInstance, nCmdShow))
	{
		return FALSE;
	}

	HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_FWPRO2));

	MSG msg;

	// Main message loop:
	while (GetMessage(&msg, nullptr, 0, 0))
	{
		if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}

	return (int)msg.wParam;
}



//
//  FUNCTION: MyRegisterClass()
//
//  PURPOSE: Registers the window class.
//
ATOM MyRegisterClass(HINSTANCE hInstance)
{
	WNDCLASSEX wcex;

	wcex.cbSize = sizeof(WNDCLASSEX);

	wcex.style = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc = WndProc;
	wcex.cbClsExtra = 0;
	wcex.cbWndExtra = 0;
	wcex.hInstance = hInstance;
	wcex.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_FWPRO2));
	wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
	wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wcex.lpszMenuName = MAKEINTRESOURCEA(IDC_FWPRO2);
	wcex.lpszClassName = szWindowClass;
	wcex.hIconSm = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

	return RegisterClassEx(&wcex);
}

//
//   FUNCTION: InitInstance(HINSTANCE, int)
//
//   PURPOSE: Saves instance handle and creates main window
//
//   COMMENTS:
//
//        In this function, we save the instance handle in a global variable and
//        create and display the main program window.
//
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
	hInst = hInstance; // Store instance handle in our global variable

	hWnd = CreateWindow(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, nullptr, nullptr, hInstance, nullptr);

	if (!hWnd)
	{
		return FALSE;
	}

	ShowWindow(hWnd, SW_SHOWMAXIMIZED);
	UpdateWindow(hWnd);

	return TRUE;
}

//
//  FUNCTION: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  PURPOSE:  Processes messages for the main window.
//
//  WM_COMMAND  - process the application menu
//  WM_PAINT    - Paint the main window
//  WM_DESTROY  - post a quit message and return
//
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_CREATE:
	{
		// Toolbar
		hTool = CreateWindowEx(0, TOOLBARCLASSNAME, NULL, WS_CHILD | WS_VISIBLE | TBSTYLE_FLAT, 0, 0, 0, 0, hWnd, NULL, NULL, NULL);
		initToolbar();

		// Log window
		hLogEdit = CreateWindowEx(0, "EDIT", NULL, WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY,
			0, 0, 0, 0, hWnd, (HMENU)IDC_LOG, NULL, NULL);
		SendMessage(hLogEdit, EM_SETLIMITTEXT, 1024 * 1024, 0);
		HFONT hFont = CreateFont(16, 8, 0, 0, 0, FALSE, 0, 0, OEM_CHARSET, OUT_RASTER_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, FIXED_PITCH, "Consolas");
		SendMessage(hLogEdit, WM_SETFONT, WPARAM(hFont), TRUE);
		
		// Statusbar
		hStatus = CreateWindowEx(0, STATUSCLASSNAME, NULL, WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP, 0, 0, 0, 0, hWnd, NULL, NULL, NULL);

		// update button states
		state = FW_LAUNCH;
		updateButtonStates();		

		// create pipe to stdout
		string pipename = "\\\\.\\pipe\\" + to_string(time(NULL));	// randomize pipename
		hstdout = CreateNamedPipe(pipename.c_str(), PIPE_ACCESS_INBOUND, PIPE_TYPE_BYTE |PIPE_READMODE_BYTE | PIPE_WAIT, 1, 1024, 1024, 0, NULL);
		if (hstdout == INVALID_HANDLE_VALUE)
		{
			logAppend("Failed to create named pipe for logging \r\n");
			break;
		}
		FILE *fp;
		int stdh = freopen_s(&fp, pipename.c_str(), "w", stdout);
		setvbuf(stdout, NULL, _IONBF, 0);
		if (!(ConnectNamedPipe(hstdout, NULL) ? TRUE : (GetLastError() == ERROR_PIPE_CONNECTED)))
		{
			logAppend("Failed to connect to pipe for logging \r\n");
			break;
		}

		// create thread to read stdout into log edit
		if (CreateThread(NULL, 0, logThread, NULL, 0, NULL) == NULL)
		{
			logAppend("Failed to create log thread \r\n");
			break;
		}	
	}
	break;
	case WM_SIZE:
	{
		RECT rc;
		int iToolHeight;
		int iStatusHeight;
		int iEditHeight;

		// Size toolbar and get height
		SendMessage(hTool, TB_AUTOSIZE, 0, 0);
		GetWindowRect(hTool, &rc);
		iToolHeight = rc.bottom - rc.top;

		// Size status bar and get height
		SendMessage(hStatus, WM_SIZE, 0, 0);
		GetWindowRect(hStatus, &rc);
		iStatusHeight = rc.bottom - rc.top;

		// Calculate remaining height and size edit
		GetClientRect(hWnd, &rc);
		iEditHeight = rc.bottom - iToolHeight - iStatusHeight;
		SetWindowPos(hLogEdit, NULL, 0, iToolHeight, rc.right, iEditHeight, SWP_NOZORDER);
	}
	break;
	case WM_COMMAND:
	{
		/*if (HIWORD(wParam) == EN_ERRSPACE)
		{

		}*/
		int wmId = LOWORD(wParam);
		// Parse the menu selections:
		switch (wmId)
		{
		case IDM_OPEN:
		{
			OPENFILENAME ofn;
			CHAR szFileName[MAX_PATH];
			ZeroMemory(&ofn, sizeof(ofn));
			ofn.lStructSize = sizeof(ofn);
			ofn.hwndOwner = hWnd;
			ofn.lpstrFilter = "FWPro2 Files (*.fwpro2)\0*.fwpro2\0\0";
			ofn.lpstrFile = szFileName;
			ofn.lpstrFile[0] = '\0';
			ofn.nMaxFile = MAX_PATH;
			ofn.Flags = OFN_EXPLORER | OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

			if (!GetOpenFileName(&ofn)) break;
			if (!cmdProc.open(ofn.lpstrFile)) break;
			std::string title = std::string(ofn.lpstrFile) + " - " + szTitle;
			SetWindowText(hWnd, title.c_str());
			state = FW_IDLE;
				
			break;
		}
		case IDM_PROP:
			DialogBox(hInst, MAKEINTRESOURCE(IDD_PROPDLG), hWnd, PropDlg);
			break;
		case IDM_RUNPAUSE:
		{		
			if (state == FW_IDLE)
			{
				if ((hThread = CreateThread(NULL, 0, cmdProcThread, NULL, 0, NULL)) == NULL)
				{
					log("Failed to create new thread, unable to run");
					break;
				}
				state = FW_RUNNING;
			}
			else if (state == FW_RUNNING)
			{
				SuspendThread(hThread);
				state = FW_PAUSED;
			}
			else if (state == FW_PAUSED)
			{
				ResumeThread(hThread);
				state = FW_RUNNING;				
			}
			break;
		}
		case IDM_STOP:
			state = FW_BUSY;
			cmdProc.stop();
			log("\r\nStopping...");
			break;
		case IDM_LOOP:
		{
			DWORD tb_state = SendMessage(hTool, TB_GETSTATE, IDM_LOOP, 0);
			if (tb_state & TBSTATE_CHECKED)
				cmdProc.setLoop(TRUE);
			else
				cmdProc.setLoop(FALSE);
			break;
		}
		case IDM_CLEAR:
			logClear();
			break;
		case IDM_OPTIONS:
			DialogBox(hInst, MAKEINTRESOURCE(IDD_OPTIONS), hWnd, Options);
			break;
		case IDM_ABOUT:
			DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
			break;
		case IDM_EXIT:
			DestroyWindow(hWnd);
			break;
		default:
			return DefWindowProc(hWnd, message, wParam, lParam);
		}
		updateButtonStates();
	}
	break;
	case WM_PAINT:
	{
		PAINTSTRUCT ps;
		HDC hdc = BeginPaint(hWnd, &ps);
		// TODO: Add any drawing code that uses hdc here...
		EndPaint(hWnd, &ps);
	}
	break;
	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}
	return 0;
}

// Message handler for about box.
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER(lParam);
	switch (message)
	{
	case WM_INITDIALOG:
	{
		HWND hwndOwner;
		RECT rc, rcDlg, rcOwner;
		// Get the owner window and dialog box rectangles. 
		hwndOwner = GetParent(hDlg);

		GetWindowRect(hwndOwner, &rcOwner);
		GetWindowRect(hDlg, &rcDlg);
		CopyRect(&rc, &rcOwner);

		// Offset the owner and dialog box rectangles  
		OffsetRect(&rcDlg, -rcDlg.left, -rcDlg.top);
		OffsetRect(&rc, -rc.left, -rc.top);
		OffsetRect(&rc, -rcDlg.right, -rcDlg.bottom);

		// The new position is the sum of half the remaining space and the owner's original position. 
		SetWindowPos(hDlg, HWND_TOP, rcOwner.left + (rc.right / 2), rcOwner.top + (rc.bottom / 2), 0, 0, SWP_NOSIZE);

		return (INT_PTR)TRUE;
	}
	case WM_COMMAND:
		if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
		{
			EndDialog(hDlg, LOWORD(wParam));
			return (INT_PTR)TRUE;
		}
		break;
	}
	return (INT_PTR)FALSE;
}

// Message handler for options
INT_PTR CALLBACK Options(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER(lParam);
	switch (message)
	{
	case WM_INITDIALOG:
	{
		HWND hwndOwner;
		RECT rc, rcDlg, rcOwner;
		// Get the owner window and dialog box rectangles. 
		hwndOwner = GetParent(hDlg);

		GetWindowRect(hwndOwner, &rcOwner);
		GetWindowRect(hDlg, &rcDlg);
		CopyRect(&rc, &rcOwner);

		// Offset the owner and dialog box rectangles  
		OffsetRect(&rcDlg, -rcDlg.left, -rcDlg.top);
		OffsetRect(&rc, -rc.left, -rc.top);
		OffsetRect(&rc, -rcDlg.right, -rcDlg.bottom);

		// The new position is the sum of half the remaining space and the owner's original position. 
		SetWindowPos(hDlg, HWND_TOP, rcOwner.left + (rc.right / 2), rcOwner.top + (rc.bottom / 2), 0, 0, SWP_NOSIZE);

		return (INT_PTR)TRUE;
	}
	case WM_COMMAND:
		if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
		{
			EndDialog(hDlg, LOWORD(wParam));
			return (INT_PTR)TRUE;
		}
		break;
	}
	return (INT_PTR)FALSE;
}

void initToolbar(void)
{
	// Declare and initialize local constants.
	const int ImageListID = 0;
	const int numButtons = 8;

	// Create the image list.
	HIMAGELIST g_hImageList = ImageList_Create(24, 24,   // Dimensions of individual bitmaps.
		ILC_COLOR32 | ILC_MASK,   // Ensures transparent background.
		7, 0);

	ImageList_AddIcon(g_hImageList, (HICON)LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_OPEN)));
	ImageList_AddIcon(g_hImageList, (HICON)LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_PROP)));
	ImageList_AddIcon(g_hImageList, (HICON)LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_RUN)));
	ImageList_AddIcon(g_hImageList, (HICON)LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_PAUSE)));
	ImageList_AddIcon(g_hImageList, (HICON)LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_STOP)));
	ImageList_AddIcon(g_hImageList, (HICON)LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_LOOP)));
	ImageList_AddIcon(g_hImageList, (HICON)LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_CLEAR)));

	// Set the image list.
	SendMessage(hTool, TB_SETIMAGELIST, (WPARAM)ImageListID, (LPARAM)g_hImageList);

	// Initialize button info.
	TBBUTTON tbButtons[numButtons] =
	{
		{ MAKELONG(TBIMG_OPEN, ImageListID),	IDM_OPEN,		TBSTATE_ENABLED, 0,			 { 0 }, 0, (INT_PTR)"Open" },
		{ MAKELONG(TBIMG_PROP, ImageListID),	IDM_PROP,		TBSTATE_ENABLED, 0,			 { 0 }, 0, (INT_PTR)"Properties" },
		{ 0,									0,				TBSTATE_ENABLED, TBSTYLE_SEP,{ 0 }, 0, 0 },
		{ MAKELONG(TBIMG_RUN, ImageListID),		IDM_RUNPAUSE,	TBSTATE_ENABLED, 0,			 { 0 }, 0, (INT_PTR)"Run" },
		{ MAKELONG(TBIMG_STOP, ImageListID),	IDM_STOP,		TBSTATE_ENABLED, 0,			 { 0 }, 0, (INT_PTR)"Stop" },
		{ MAKELONG(TBIMG_LOOP, ImageListID),	IDM_LOOP,		TBSTATE_ENABLED, BTNS_CHECK, { 0 }, 0, (INT_PTR)"Loop" },
		{ 0,									0,				TBSTATE_ENABLED, TBSTYLE_SEP,{ 0 }, 0, 0 },
		{ MAKELONG(TBIMG_CLEAR, ImageListID),	IDM_CLEAR,		TBSTATE_ENABLED, 0,			 { 0 }, 0, (INT_PTR)"Clear Log" }
	};

	// Add buttons.
	SendMessage(hTool, TB_BUTTONSTRUCTSIZE, (WPARAM)sizeof(TBBUTTON), 0);
	SendMessage(hTool, TB_ADDBUTTONS, (WPARAM)numButtons, (LPARAM)&tbButtons);
}


void logAppend(const char * text)
{
	static int lastCR;
	string str(text);
	
	while (str.length())
	{
		int n;
		string line;
		// split into lines
		if ((n = str.find("\n")) == string::npos)
		{
			line = str;
			str = "";
		}
		else
		{
			line = str.substr(0, n + 1);
			str = str.substr(n + 1, string::npos);
		}
		// get position of last char
		int ndx = GetWindowTextLength(hLogEdit);
		int offset = ndx;	
		// prepend \r if previous text ended with it
		if (lastCR)
		{
			line = "\r" + line;
			lastCR = 0;
		}
		// find all \r in line
		n = -1;
		while ((n = line.find("\r", n+1)) != string::npos)
		{
			// if line ends in \r, remove it & prepend it next time
			if (n == (line.length() - 1))
			{
				lastCR = 1;
				line = line.erase(n, 1);
			}
			// if \r is not followed by \r or \n
			else if (line.at(n + 1) != '\r' && line.at(n + 1) != '\n')
			{
				// move caret to beginning of current line
				line = line.substr(n, string::npos);
				n = 0;
				offset = SendMessage(hLogEdit, EM_LINEINDEX, -1, 0);
			}
		}	
		// append/replace the text
		SendMessage(hLogEdit, EM_SETSEL, (WPARAM)offset, (LPARAM)ndx);
		SendMessage(hLogEdit, EM_REPLACESEL, 0, (LPARAM)line.c_str());
	}
}

void logClear(void)
{
	SetWindowText(hLogEdit, NULL);
}

// Message handler for properties dialog
INT_PTR CALLBACK PropDlg(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER(lParam);
	switch (message)
	{
	case WM_INITDIALOG:
	{
		// Initialize the dialog with params
		if (!cmdProc.paramsInit(hDlg))
		{
			EndDialog(hDlg, LOWORD(wParam));
			break;
		}

		// Center the dialog
		HWND hwndOwner;
		RECT rc, rcDlg, rcOwner;
		// Get the owner window and dialog box rectangles. 
		hwndOwner = GetParent(hDlg);

		GetWindowRect(hwndOwner, &rcOwner);
		GetWindowRect(hDlg, &rcDlg);
		CopyRect(&rc, &rcOwner);

		// Offset the owner and dialog box rectangles  
		OffsetRect(&rcDlg, -rcDlg.left, -rcDlg.top);
		OffsetRect(&rc, -rc.left, -rc.top);
		OffsetRect(&rc, -rcDlg.right, -rcDlg.bottom);

		// The new position is the sum of half the remaining space and the owner's original position. 
		SetWindowPos(hDlg, HWND_TOP, rcOwner.left + (rc.right / 2), rcOwner.top + (rc.bottom / 2), 0, 0, SWP_NOSIZE);
	}
	case WM_COMMAND:
	{
		int wmId = LOWORD(wParam);
		// Parse the menu selections:
		switch (wmId)
		{
		case IDCANCEL:
			EndDialog(hDlg, LOWORD(wParam));
			break;
		case IDOK:
			if (cmdProc.paramsOK(hDlg)) EndDialog(hDlg, LOWORD(wParam));
			break;
		case IDB_SAVE:
			if (cmdProc.paramsSave(hDlg)) EndDialog(hDlg, LOWORD(wParam));
			break;
		case IDB_SAVEAS:
		{
			OPENFILENAME ofn;
			CHAR szFileName[MAX_PATH];
			ZeroMemory(&ofn, sizeof(ofn));
			ofn.lStructSize = sizeof(ofn);
			ofn.hwndOwner = hDlg;
			ofn.lpstrFilter = "FWPro2 Files (*.fwpro2)\0*.fwpro2\0\0";
			ofn.lpstrDefExt = "fwpro2";
			ofn.lpstrFile = szFileName;
			ofn.lpstrFile[0] = '\0';
			ofn.nMaxFile = MAX_PATH;
			ofn.Flags = OFN_EXPLORER | OFN_OVERWRITEPROMPT;
			if (!GetSaveFileName(&ofn)) break;
			if (!cmdProc.paramsSaveAs(hDlg, ofn.lpstrFile)) break;
			std::string title = std::string(ofn.lpstrFile) + " - " + szTitle;
			SetWindowText(hWnd, title.c_str());
			EndDialog(hDlg, LOWORD(wParam));
			break;
		}
		default:
			if ((wmId & ~PROPDLG_MASK) == IDB_PROPOPEN || (wmId & ~PROPDLG_MASK) == IDB_PROPSAVE)
			{
				OPENFILENAME ofn;
				CHAR szFileName[MAX_PATH];
				ZeroMemory(&ofn, sizeof(ofn));
				ofn.lStructSize = sizeof(ofn);
				ofn.hwndOwner = hDlg;
				ofn.lpstrFile = szFileName;
				ofn.lpstrFile[0] = '\0';
				ofn.nMaxFile = MAX_PATH;

				if ((wmId & ~PROPDLG_MASK) == IDB_PROPOPEN)
				{
					ofn.Flags = OFN_EXPLORER | OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;
					if (!GetOpenFileName(&ofn)) break;
				}
				else
				{
					ofn.Flags = OFN_EXPLORER | OFN_OVERWRITEPROMPT;
					if (!GetSaveFileName(&ofn)) break;
				}

				// Recover associated edit control from button id
				HWND hEdit = GetDlgItem(hDlg, ((wmId & PROPDLG_MASK) | IDD_PROPDLG));
				SetWindowText(hEdit, ofn.lpstrFile);
				// Move cursor to end
				SendMessage(hEdit, EM_SETSEL, 0, -1);
				SendMessage(hEdit, EM_SETSEL, -1, -1);
			}
			else
				return (INT_PTR)FALSE;
		}
	}
	default:
		return (INT_PTR)FALSE;
	}
	return (INT_PTR)TRUE;
}

void updateButtonStates(void)
{
	TBBUTTONINFO tbButton;
	tbButton.cbSize = sizeof(TBBUTTONINFO);
	tbButton.dwMask = TBIF_TEXT;
	// default enabled
	BOOL buttonState[5] = { TRUE, TRUE, TRUE, TRUE, TRUE };
	tbButton.pszText = "Run";
	int img = TBIMG_RUN;
	
	if (state == FW_LAUNCH)
	{
		// disable everything except open (0)
		for (int i = 1; i < (sizeof(buttonState)/sizeof(BOOL)); i++) buttonState[i] = FALSE;
	}
	else if (state == FW_IDLE)
	{
		buttonState[3] = FALSE;
	}
	else if (state == FW_RUNNING)
	{
		tbButton.pszText = "Pause";
		img = TBIMG_PAUSE;
		buttonState[0] = FALSE; buttonState[1] = FALSE;
	}
	else if (state == FW_PAUSED)
	{
		buttonState[0] = FALSE; buttonState[1] = FALSE;
	}
	else if (state == FW_BUSY)
	{
		for (int i = 0; i < (sizeof(buttonState) / sizeof(BOOL)); i++) buttonState[i] = FALSE;
	}
	SendMessage(hTool, TB_CHANGEBITMAP, IDM_RUNPAUSE, img);
	SendMessage(hTool, TB_SETBUTTONINFO, IDM_RUNPAUSE, (LPARAM)&tbButton);
	SendMessage(hTool, TB_ENABLEBUTTON, IDM_OPEN, (LPARAM)buttonState[0]);
	SendMessage(hTool, TB_ENABLEBUTTON, IDM_PROP, (LPARAM)buttonState[1]);
	SendMessage(hTool, TB_ENABLEBUTTON, IDM_RUNPAUSE, (LPARAM)buttonState[2]);
	SendMessage(hTool, TB_ENABLEBUTTON, IDM_STOP, (LPARAM)buttonState[3]);
	SendMessage(hTool, TB_ENABLEBUTTON, IDM_LOOP, (LPARAM)buttonState[4]);
}

DWORD WINAPI cmdProcThread(LPVOID lpParam)
{
	cmdProc.run();
	state = FW_IDLE;
	updateButtonStates();
	return 0;
}

DWORD WINAPI logThread(LPVOID lpParam)
{
	char buffer[256];
	DWORD bytesRead;
	while (true)
	{
		ReadFile(hstdout, buffer, 255, &bytesRead, NULL);
		buffer[bytesRead] = '\0';
		logAppend(buffer);
	}
	return 0;
}