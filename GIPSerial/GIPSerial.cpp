// GIPSerial.cpp : Defines the entry point for the application.OpenEventW
//

#include "framework.h"
#include "GIPSerial.h"

#define MAX_LOADSTRING 100
#define PI_VID L"0525"
#define PI_PID L"a4a7"
#define MB_WAIT_TIMEOUT 30000 // 30 seconds
#define IDM_EXIT 105
#define IDM_SYNC 106
#define IDM_CLEAR 107
#define IDM_CLEAR_SINGLE 108
#define IDM_DEVICE_NOT_FOUND 109
#define IDM_DEVICE_NOT_FOUND_EXIT 110
#define NBOFEVENTS 6

#define APPWM_ICONNOTIFY (WM_APP + 1)

// GIP Commands
#define RASPBERRY_PI_GIP_POLL 0xaf
#define RASPBERRY_PI_GIP_SYNC 0xb0
#define RASPBERRY_PI_GIP_CLEAR 0xb1
#define RASPBERRY_PI_GIP_LOCK 0xb2
#define RASPBERRY_PI_CLEAR_NEXT_SYNCED_CONTROLLER 0xb4

// Global Variables:
HINSTANCE hInst;                                // current instance
WCHAR szTitle[MAX_LOADSTRING];                  // The title bar text
WCHAR szWindowClass[MAX_LOADSTRING];            // the main window class name

HDEVNOTIFY hDeviceSerial = NULL;
HPOWERNOTIFY hPowerNotify = NULL;

std::wstring devicePath;
std::wstring comPath;
std::wstring devicePort;
static DWORD32 controllerCount = -1;
static UINT WM_TaskBarCreated = 0;
std::wstring controllerCountWStr;
// Forward declarations of functions included in this code module:
ATOM                MyRegisterClass(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);
BOOL                FindAllDevices(const GUID* ClassGuid, std::vector<std::wstring>& DevicePaths, std::vector<std::wstring>* DeviceNames);
void                ScanForSerialDevices();
BOOL                AddNotificationIcon(HWND hwnd);

BOOL ReadADoubleWord32(HANDLE hComm, DWORD32* lpDW32)
{
	OVERLAPPED osRead = { 0 };
	DWORD dwRead = 0;
	DWORD dwRes = 0;
	BOOL fRes = FALSE;
	HANDLE lpHandles[2] = { NULL };
	if (hComm)
	{
		// Create this read operation's OVERLAPPED structure's hEvent.
		osRead.hEvent = OpenEvent(SYNCHRONIZE | EVENT_MODIFY_STATE, FALSE, L"SerialReadEvent");
		if (osRead.hEvent == NULL)
		{
			return FALSE;
		 }
		lpHandles[0] = osRead.hEvent;
		lpHandles[1] = OpenEvent(SYNCHRONIZE | EVENT_MODIFY_STATE, FALSE, L"ShutdownEvent");

		if ( lpHandles[1] == NULL)
			// error creating overlapped event handle
			return FALSE;


		// Issue read.
		if (!ReadFile(hComm, lpDW32, sizeof(DWORD32), &dwRead, &osRead)) {
			if (GetLastError() != ERROR_IO_PENDING) {
				// ReadFile failed, but isn't delayed. Report error and abort.
				fRes = FALSE;
			}
			else
				// Read is pending.
				dwRes = WaitForMultipleObjects(ARRAYSIZE(lpHandles), lpHandles, FALSE, INFINITE);
			switch (dwRes)
			{
				// OVERLAPPED structure's event has been signaled. 
			case 0:
				if (!GetOverlappedResult(hComm, &osRead, &dwRead, FALSE))
					fRes = FALSE;
				else
					// Read operation completed successfully.
					fRes = TRUE;
				break;

			default:
				// An error has occurred in WaitForSingleObject.
				// This usually indicates a problem with the
			   // OVERLAPPED structure's event handle.
				fRes = FALSE;
				break;
			}
		}
		else {
			// ReadFile completed immediately.
			fRes = TRUE;
		}
		CloseHandle(osRead.hEvent);
		CloseHandle(lpHandles[1]);
	}
			 return fRes;
}

BOOL WriteADoubleWord32(HANDLE hComm, DWORD32* lpDW32)
{
	OVERLAPPED osWrite = { 0 };
	DWORD dwWrite = 0;
	DWORD dwRes = 0;
	BOOL fRes = FALSE;
	HANDLE lpHandles[2] = { NULL };
	if (hComm)
	{
		// Create this write operation's OVERLAPPED structure's hEvent.
		osWrite.hEvent = OpenEvent(SYNCHRONIZE | EVENT_MODIFY_STATE, FALSE, L"SerialWriteEvent");

		if (osWrite.hEvent == NULL )
			// error creating overlapped event handle
			return FALSE;

		lpHandles[0] = osWrite.hEvent;
		lpHandles[1] = OpenEvent(SYNCHRONIZE | EVENT_MODIFY_STATE, FALSE, L"ShutdownEvent");
		if (lpHandles[1] == NULL)
			// error creating overlapped event handle
			return FALSE;
		// Issue write.
		if (!WriteFile(hComm, lpDW32, sizeof(DWORD32), &dwWrite, &osWrite)) {
			if (GetLastError() != ERROR_IO_PENDING) {
				// WriteFile failed, but isn't delayed. Report error and abort.
				fRes = FALSE;
			}
			else
				// Write is pending.
				dwRes = WaitForMultipleObjects(ARRAYSIZE(lpHandles), lpHandles, FALSE, INFINITE);
			switch (dwRes)
			{
				// OVERLAPPED structure's event has been signaled. 
			case 0:
				if (!GetOverlappedResult(hComm, &osWrite, &dwWrite, FALSE))
					fRes = FALSE;
				else
					// Write operation completed successfully.
					fRes = TRUE;
				break;
			default:
				// An error has occurred in WaitForSingleObject.
				// This usually indicates a problem with the
			   // OVERLAPPED structure's event handle.
				fRes = FALSE;
				break;
			}
		}
		else {
			// WriteFile completed immediately.
			fRes = TRUE;
		}
		CloseHandle(osWrite.hEvent);
		CloseHandle(lpHandles[1]);

	}
	return fRes;
}

DWORD WINAPI SerialThread(LPVOID lpParam) {

	UNREFERENCED_PARAMETER(lpParam);
	ScanForSerialDevices();
	HANDLE lpHandles[NBOFEVENTS] = { NULL };

	HANDLE hSerial = CreateFile(comPath.c_str(), GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, NULL);
	if (hSerial == INVALID_HANDLE_VALUE)
	{
		hSerial = NULL;
	}
	else
	{
		DCB dcb = { 0 };
		dcb.DCBlength = sizeof(dcb);
		GetCommState(hSerial, &dcb);
		dcb.BaudRate = CBR_9600;
		SetCommState(hSerial, &dcb);
	}

	lpHandles[0] = OpenEvent( SYNCHRONIZE | EVENT_MODIFY_STATE, FALSE, L"ShutdownEvent");
	if (lpHandles[0] == NULL && GetLastError() == ERROR_FILE_NOT_FOUND)
	{
		lpHandles[0] = CreateEvent(NULL, FALSE, FALSE, L"ShutdownEvent");
	}

	lpHandles[1] = OpenEvent( SYNCHRONIZE | EVENT_MODIFY_STATE, FALSE, L"SyncEvent");
	if (lpHandles[1] == NULL && GetLastError() == ERROR_FILE_NOT_FOUND)
	{
		lpHandles[1] = CreateEvent(NULL, FALSE, FALSE, L"SyncEvent");
	}

	HANDLE hReadEvent = OpenEvent(SYNCHRONIZE | EVENT_MODIFY_STATE, FALSE, L"SerialReadEvent");
	if (hReadEvent == NULL && GetLastError() == ERROR_FILE_NOT_FOUND)
	{
		hReadEvent = CreateEvent(NULL, FALSE, FALSE, L"SerialReadEvent");
	}
	if (hReadEvent)
	{
		ResetEvent(hReadEvent);
	}

	HANDLE hWriteEvent = OpenEvent(SYNCHRONIZE | EVENT_MODIFY_STATE, FALSE, L"SerialWriteEvent");
	if (hWriteEvent == NULL && GetLastError() == ERROR_FILE_NOT_FOUND)
	{
		hWriteEvent = CreateEvent(NULL, FALSE, FALSE, L"SerialWriteEvent");
	}
	if (hWriteEvent)
	{
		ResetEvent(hWriteEvent);
	}

	HANDLE hFinshedSyncEvent = OpenEvent(SYNCHRONIZE | EVENT_MODIFY_STATE, FALSE, L"FinshedSyncEvent");
	if (hFinshedSyncEvent == NULL && GetLastError() == ERROR_FILE_NOT_FOUND)
	{
		hFinshedSyncEvent = CreateEvent(NULL, FALSE, FALSE, L"FinshedSyncEvent");
	}
	if (hFinshedSyncEvent)
	{
		ResetEvent(hFinshedSyncEvent);
	}

	lpHandles[2] = OpenEvent( SYNCHRONIZE | EVENT_MODIFY_STATE, FALSE, L"ClearSingleEvent");
	if (lpHandles[2] == NULL && GetLastError() == ERROR_FILE_NOT_FOUND)
	{
		lpHandles[2] = CreateEvent(NULL, FALSE, FALSE, L"ClearSingleEvent");
	}

	HANDLE hFinshedClearSingleEvent = OpenEvent(EVENT_MODIFY_STATE, FALSE, L"FinshedClearSingleEvent");
	if (hFinshedClearSingleEvent == NULL && GetLastError() == ERROR_FILE_NOT_FOUND)
	{
		hFinshedClearSingleEvent = CreateEvent(NULL, FALSE, FALSE, L"FinshedClearSingleEvent");
	}

	lpHandles[3] = OpenEvent( SYNCHRONIZE | EVENT_MODIFY_STATE, FALSE, L"ClearAllEvent");
	if (lpHandles[3] == NULL && GetLastError() == ERROR_FILE_NOT_FOUND)
	{
		lpHandles[3] = CreateEventW(NULL, FALSE, FALSE, L"ClearAllEvent");
	}

	HANDLE hFinshedClearAllEvent = OpenEvent(SYNCHRONIZE | EVENT_MODIFY_STATE, FALSE, L"FinshedClearAllEvent");
	if (hFinshedClearAllEvent == NULL && GetLastError() == ERROR_FILE_NOT_FOUND)
	{
		hFinshedClearAllEvent = CreateEvent(NULL, FALSE, FALSE, L"FinshedClearAllEvent");
	}

	lpHandles[4] = OpenEvent( SYNCHRONIZE | EVENT_MODIFY_STATE, FALSE, L"LockDeviceEvent");
	if (lpHandles[4] == NULL && GetLastError() == ERROR_FILE_NOT_FOUND)
	{
		lpHandles[4] = CreateEvent(NULL, FALSE, FALSE, L"LockDeviceEvent");
	}

	HANDLE hFinshedLockDeviceEvent = OpenEvent(SYNCHRONIZE | EVENT_MODIFY_STATE, FALSE, L"FinshedLockDeviceEvent");
	if (hFinshedLockDeviceEvent == NULL && GetLastError() == ERROR_FILE_NOT_FOUND)
	{
		hFinshedLockDeviceEvent = CreateEvent(NULL, FALSE, FALSE, L"FinshedLockDeviceEvent");
	}

	lpHandles[5] = OpenEvent( SYNCHRONIZE | EVENT_MODIFY_STATE, FALSE, L"NewDeviceEvent");
	if (lpHandles[5] == NULL && GetLastError() == ERROR_FILE_NOT_FOUND)
	{
		lpHandles[5] = CreateEvent(NULL, FALSE, FALSE, L"NewDeviceEvent");
	}

	DWORD32 lastControllerCount = 0;
	DWORD32 cmd = RASPBERRY_PI_GIP_POLL;
	DWORD32 pwrStatus = 0xaf;
	DWORD dwWaitResult = 0;
	DWORD currentTime = 0;
	DWORD endTime = 0;
	dwWaitResult = WAIT_TIMEOUT;

	while (dwWaitResult == WAIT_TIMEOUT) {
		dwWaitResult = WaitForMultipleObjects(ARRAYSIZE(lpHandles), lpHandles, FALSE, 50);
		currentTime = timeGetTime();
		switch (dwWaitResult)
		{
		case 1: // hSyncEvent
		{
			if (lpHandles[1] && dwWaitResult == 1)
			{
				ResetEvent(lpHandles[1]);
				cmd = RASPBERRY_PI_GIP_SYNC;
			}
		}
		case 2: // hClearSingleEvent
		{
			if (lpHandles[2] && dwWaitResult == 2)
			{
				ResetEvent(lpHandles[2]);
				cmd = RASPBERRY_PI_CLEAR_NEXT_SYNCED_CONTROLLER;
			}
		}
		case 3: // hClearAllEvent
		{
			if (lpHandles[3] && dwWaitResult == 3)
			{
				ResetEvent(lpHandles[3]);
				cmd = RASPBERRY_PI_GIP_CLEAR;
			}
		}
		case 4: // hLockDeviceEvent
		{
			if (lpHandles[4] && dwWaitResult == 4)
			{
				ResetEvent(lpHandles[4]);
				cmd = RASPBERRY_PI_GIP_LOCK;
			}
		}
		case WAIT_TIMEOUT: {
			if (hSerial)
			{
				if (dwWaitResult == 1 ||
					dwWaitResult == 2 ||
					dwWaitResult == 3)
				{
					lastControllerCount = controllerCount;
					endTime = timeGetTime();
					endTime = endTime + 30000;
					dwWaitResult = WAIT_TIMEOUT;
				}

				if (endTime < currentTime && endTime != 0)
				{
					cmd = RASPBERRY_PI_GIP_POLL;
					endTime = 0;
				}

				if (!WriteADoubleWord32(hSerial, &cmd) ||
					!ReadADoubleWord32(hSerial, &controllerCount))
				{
					CloseHandle(hSerial);
					controllerCount = -1;
					hSerial = NULL;
					break;
				}

				if (controllerCount > lastControllerCount && cmd == RASPBERRY_PI_GIP_SYNC)
				{
					if (hFinshedSyncEvent)
					{
						SetEvent(hFinshedSyncEvent);
					}
					cmd = RASPBERRY_PI_GIP_POLL;
				}
				if (controllerCount < lastControllerCount && cmd == RASPBERRY_PI_CLEAR_NEXT_SYNCED_CONTROLLER)
				{
					if (hFinshedClearSingleEvent)
					{
						SetEvent(hFinshedClearSingleEvent);
					}
					cmd = RASPBERRY_PI_GIP_POLL;
				}
				else if (controllerCount == 0 && cmd == RASPBERRY_PI_GIP_CLEAR)
				{
					if (hFinshedClearAllEvent)
					{
						SetEvent(hFinshedClearAllEvent);
					}
					cmd = RASPBERRY_PI_GIP_POLL;
				}

				if (cmd == RASPBERRY_PI_GIP_LOCK)
				{
					if (hFinshedLockDeviceEvent)
					{
						SetEvent(hFinshedLockDeviceEvent);

					}
					if (lpHandles[0])
					{
						SetEvent(lpHandles[0]);
					}
				}
			}
			else
			{
				controllerCount = -1;
				Sleep(100);
				ScanForSerialDevices();
				if (comPath != L"")
				{
					hSerial = CreateFile(comPath.c_str(), GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, NULL);
					if (hSerial == INVALID_HANDLE_VALUE)
					{
						hSerial = NULL;
					}
					else
					{
						DCB dcb = { 0 };
						dcb.DCBlength = sizeof(dcb);
						GetCommState(hSerial, &dcb);
						dcb.BaudRate = CBR_9600;
						SetCommState(hSerial, &dcb);
					}
				}
			}
			break;
		}
		case 0: // hShutdownEvent
		{
			for (size_t i = 0; i < ARRAYSIZE(lpHandles); i++)
			{
				if (lpHandles[i] != NULL)
				{
					CloseHandle(lpHandles[i]);
				}
			}
			if (hSerial)
			{
				CloseHandle(hSerial);
				hSerial = NULL;
			}
			if (hWriteEvent)
			{
				CloseHandle(hWriteEvent);
				hWriteEvent = NULL;
			}
			if (hReadEvent)
			{
				CloseHandle(hReadEvent);
				hReadEvent = NULL;
			}
			return 0;
		}
		case 5: // hNewDeviceEvent
		{
			if (hSerial)
			{
				CloseHandle(hSerial);
			}
			hSerial = NULL;
			if (lpHandles[5])
			{
				ResetEvent(lpHandles[5]);
			}
			ScanForSerialDevices();
			if (comPath != L"")
			{
				hSerial = CreateFile(comPath.c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, NULL);
				if (hSerial == INVALID_HANDLE_VALUE)
				{
					hSerial = NULL;
				}
				else
				{
					DCB dcb = { 0 };
					dcb.DCBlength = sizeof(dcb);
					GetCommState(hSerial, &dcb);
					dcb.BaudRate = CBR_9600;
					SetCommState(hSerial, &dcb);
				}
			}
			break;
		}
		}
		dwWaitResult = WAIT_TIMEOUT;
	}
	return 1;
}

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPWSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    // TODO: Place code here.
	HANDLE mutex = CreateMutex(0, 0, L"SteamSwitchMutex");
	MSG msg = {};
	switch (GetLastError())
	{
	case ERROR_ALREADY_EXISTS:
		// app already running
		break;

	case ERROR_SUCCESS:
	{
		MSG msg = { 0 };

		HANDLE hSerialThread = CreateThread(NULL, 0, SerialThread, NULL, 0, NULL);
		// Initialize global strings
		LoadString(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
		LoadString(hInstance, IDC_GIPSERIAL, szWindowClass, MAX_LOADSTRING);
		MyRegisterClass(hInstance);

		// Perform application initialization:
		if (!InitInstance(hInstance, nCmdShow))
		{
			return FALSE;
		}

		HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_GIPSERIAL));

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
	}
	return FALSE;
}



//
//  FUNCTION: MyRegisterClass()
//
//  PURPOSE: Registers the window class.
//
ATOM MyRegisterClass(HINSTANCE hInstance)
{
    WNDCLASSEXW wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style          = 0;
    wcex.lpfnWndProc    = WndProc;
    wcex.cbClsExtra     = 0;
    wcex.cbWndExtra     = 0;
    wcex.hInstance      = hInstance;
    wcex.hIcon			= NULL;
    wcex.hCursor        = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground  = (HBRUSH)(COLOR_WINDOW+1);
    wcex.lpszMenuName   = MAKEINTRESOURCE(IDC_GIPSERIAL);
    wcex.lpszClassName  = szWindowClass;
    wcex.hIconSm        = NULL;

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

   HWND hWnd = CreateWindow(szWindowClass, L"", 0,
	   0, 0, 0, 0, nullptr, nullptr, hInstance, nullptr);

   if (!hWnd)
   {
      return FALSE;
   }

   ShowWindow(hWnd, SW_HIDE);
   UpdateWindow(hWnd);
   if (hDeviceSerial == NULL)
   {
	   DEV_BROADCAST_DEVICEINTERFACE serialFilter = { };
	   serialFilter.dbcc_size = sizeof(DEV_BROADCAST_DEVICEINTERFACE);
	   serialFilter.dbcc_classguid = GUID_DEVINTERFACE_COMPORT;
	   serialFilter.dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE;
	   hDeviceSerial = RegisterDeviceNotification(hWnd, &serialFilter, DEVICE_NOTIFY_WINDOW_HANDLE);
   }
   if (hPowerNotify == NULL)
   {
	   hPowerNotify = RegisterSuspendResumeNotification(hWnd, DEVICE_NOTIFY_WINDOW_HANDLE);
   }
   WM_TaskBarCreated = RegisterWindowMessage(L"TaskbarCreated");
   AddNotificationIcon(hWnd);

   return TRUE;
}

//
//  FUNCTION: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  PURPOSE: Processes messages for the main window.
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
	case WM_POWERBROADCAST:
	{
		if (wParam == PBT_APMSUSPEND)
		{
			HANDLE hNewDeviceEvent = OpenEvent(SYNCHRONIZE | EVENT_MODIFY_STATE, FALSE, L"NewDeviceEvent");
			if (hNewDeviceEvent)
			{
				SetEvent(hNewDeviceEvent);
				CloseHandle(hNewDeviceEvent);
			}
		}
		break;
	}
	case APPWM_ICONNOTIFY:
	{
		switch (lParam)
		{
		case WM_LBUTTONUP:
		case WM_RBUTTONUP:
			HMENU Hmenu = CreatePopupMenu();
			if (controllerCount != -1 )
			{
				if (controllerCount == 1)
				{
					controllerCountWStr = L"Enable Pairing Mode: (1) controller paired.";
				}
				else
				{
					controllerCountWStr = L"Enable Pairing Mode: (" + std::to_wstring(controllerCount) + L") controllers paired.";
				}

				if (controllerCount > 0)
				{
					AppendMenu(Hmenu, MF_STRING, IDM_CLEAR, L"Clear All Paired Controllers");
					AppendMenu(Hmenu, MF_STRING, IDM_CLEAR_SINGLE, L"Clear a Single Paired Controller");
				}

				AppendMenu(Hmenu, MF_STRING, IDM_SYNC, controllerCountWStr.c_str());
				AppendMenu(Hmenu, MF_STRING, IDM_EXIT, L"Close GIPSerial");
			}
			else
			{
				AppendMenu(Hmenu, MF_STRING | MF_DISABLED, IDM_DEVICE_NOT_FOUND, L"The Raspberry Pi ZeroW2 device was not found.");
				AppendMenu(Hmenu, MF_STRING, IDM_DEVICE_NOT_FOUND_EXIT, L"Close GIPSerial");
			}
			POINT p;
			GetCursorPos(&p);
			SetForegroundWindow(hWnd);
			TrackPopupMenu(Hmenu, TPM_LEFTBUTTON, p.x, p.y, 0, hWnd, 0);
			PostMessage(hWnd, WM_NULL, 0, 0);
			break;
		}
		break;
	}
	case WM_COMMAND:
	{
		int wmId = LOWORD(wParam);
		// Parse the menu selections:
		switch (wmId)
		{
		case IDM_DEVICE_NOT_FOUND:
		{
			MessageBox(hWnd, L"Do not run GIPSerial unless you have the required Raspberry Pi ZeroW2 serial device connected to your computer.", L"GIPSerial Error", MB_OK | MB_ICONERROR);
			break;
		}
		case IDM_DEVICE_NOT_FOUND_EXIT:
		{
			DestroyWindow(hWnd);
			PostQuitMessage(0);
			break;
		}
		case IDM_EXIT:
		{
			HANDLE hLockDeviceEvent = OpenEvent(EVENT_MODIFY_STATE, FALSE, L"LockDeviceEvent");
			HANDLE hShutdownEvent = OpenEvent(EVENT_MODIFY_STATE | SYNCHRONIZE, FALSE, L"ShutdownEvent");
			HANDLE hNewDeviceEvent = OpenEvent(EVENT_MODIFY_STATE, FALSE, L"NewDeviceEvent");
			if (hNewDeviceEvent && WaitForSingleObject(hNewDeviceEvent, 1) != WAIT_OBJECT_0)
			{
				SetEvent(hNewDeviceEvent);
			}
			if (hLockDeviceEvent)
			{
				SetEvent(hLockDeviceEvent);
				if (hShutdownEvent && WaitForSingleObject(hShutdownEvent, MB_WAIT_TIMEOUT) == WAIT_OBJECT_0)
				{
					MessageBox(hWnd, L"The device is now locked and can be re enabled by running GIPSerial again.\nThis is to prevent accidentally shutdowns from happening.", L"GIPSerial Important Information", MB_OK | MB_ICONINFORMATION);
				}
				else
				{
					MessageBox(hWnd, L"The device may be unlocked and could power down the current computer unexpectedly when a paired controlled is used.\n\nRun GIPSerial again to fix this.\n\nDo not run GIPSerial unless you have the required Raspberry Pi ZeroW2 serial device connected to your computer.", L"GIPSerial Error", MB_OK | MB_ICONERROR);
				}
			}
			if (hLockDeviceEvent)
			{
				CloseHandle(hLockDeviceEvent);
			}
			if (hShutdownEvent)
			{
				CloseHandle(hShutdownEvent);
			}
			DestroyWindow(hWnd);
			PostQuitMessage(0);
			break;
		}
		case IDM_SYNC:
		{
			int selection = MessageBox(hWnd, L"Enable paring mode for the Raspberry Pi ZeroW2 device?\nAnother message box window will appear to indicate if a controller was paired or not.\nClick ok to continue or click cancel to exit.", L"GIPSerial", MB_OKCANCEL | MB_ICONQUESTION);
			switch (selection)
			{
			case IDCANCEL:
				return DefWindowProc(hWnd, message, wParam, lParam);
			case IDOK:
			{
				HANDLE hSyncEvent = OpenEvent(EVENT_MODIFY_STATE, FALSE, L"SyncEvent");
				HANDLE hFinshedSyncEvent = OpenEvent(EVENT_MODIFY_STATE | SYNCHRONIZE, FALSE, L"FinshedSyncEvent");
				HANDLE hNewDeviceEvent = OpenEvent(EVENT_MODIFY_STATE, FALSE, L"NewDeviceEvent");

				if (hNewDeviceEvent && WaitForSingleObject(hNewDeviceEvent, 1) != WAIT_OBJECT_0)
				{
					SetEvent(hNewDeviceEvent);
				}
				if (hSyncEvent && hFinshedSyncEvent)
				{
					SetEvent(hSyncEvent);
					if (WaitForSingleObject(hFinshedSyncEvent, MB_WAIT_TIMEOUT) == WAIT_OBJECT_0)
					{
						MessageBox(hWnd, L"The controller is now paired with the Raspberry Pi ZeroW2 device. To unpair this controller select the option \"Clear All Paired Controllers\" or \"Clear a Single Paired Controller\"", L"GIPSerial Important Information", MB_OK | MB_ICONINFORMATION);
					}
					else
					{
						MessageBox(hWnd, L"The Raspberry Pi ZeroW2 device did not find any new controllers to pair.", L"GIPSerial Error", MB_OK | MB_ICONERROR);
					}
					ResetEvent(hFinshedSyncEvent);
				}
				if (hSyncEvent)
				{
					CloseHandle(hSyncEvent);
				}
				if (hFinshedSyncEvent)
				{
					CloseHandle(hFinshedSyncEvent);
				}
			}
			}
			break;
		}
		case IDM_CLEAR_SINGLE:
		{
			int selection = MessageBox(hWnd, L"Warning: This option will attempt to unpair the next controller that tries to connect to the device.\nClick ok to continue or click cancel to exit.", L"GIPSerial Warning", MB_OKCANCEL | MB_ICONWARNING);
			switch (selection)
			{
			case IDCANCEL:
				return DefWindowProc(hWnd, message, wParam, lParam);
			case IDOK:
			{
				HANDLE hClearSingleEvent = OpenEvent(EVENT_MODIFY_STATE, FALSE, L"ClearSingleEvent");
				HANDLE hFinshedClearSingleEvent = OpenEvent(EVENT_MODIFY_STATE | SYNCHRONIZE, FALSE, L"FinshedClearSingleEvent");
				HANDLE hNewDeviceEvent = OpenEvent(EVENT_MODIFY_STATE, FALSE, L"NewDeviceEvent");

				if (hNewDeviceEvent && WaitForSingleObject(hNewDeviceEvent, 1) != WAIT_OBJECT_0)
				{
					SetEvent(hNewDeviceEvent);
				}
				if (hClearSingleEvent && hFinshedClearSingleEvent)
				{
					SetEvent(hClearSingleEvent);
					if (WaitForSingleObject(hFinshedClearSingleEvent, MB_WAIT_TIMEOUT) == WAIT_OBJECT_0)
					{
						MessageBox(hWnd, L"The controller was unpaired successfully.\nYou can now safety pair this controller to another device.\nTo pair again click the option \"Enable Pairing Mode\".", L"GIPSerial Important Information", MB_OK | MB_ICONINFORMATION);
					}
					else
					{
						MessageBox(hWnd, L"The Raspberry Pi ZeroW2 device was unable to clear a single paired controller.\nTry restarting before trying again.", L"GIPSerial Error", MB_OK | MB_ICONERROR);
					}
					ResetEvent(hFinshedClearSingleEvent);
				}
				if (hClearSingleEvent)
				{
					CloseHandle(hClearSingleEvent);
				}
				if (hFinshedClearSingleEvent)
				{
					CloseHandle(hFinshedClearSingleEvent);
				}
			}
			}
			break;
		}
		case IDM_CLEAR:
		{
			int selection = MessageBox(hWnd, L"Warning: This option will attempt to unpair all synced controllers.\nClick ok to continue or click cancel to exit.", L"GIPSerial Warning", MB_OKCANCEL | MB_ICONWARNING);
			switch (selection)
			{
			case IDCANCEL:
				return DefWindowProc(hWnd, message, wParam, lParam);
			case IDOK:
			{
				HANDLE hClearAllEvent = OpenEvent(EVENT_MODIFY_STATE, FALSE, L"ClearAllEvent");
				HANDLE hFinshedClearAllEvent = OpenEvent(EVENT_MODIFY_STATE | SYNCHRONIZE, FALSE, L"FinshedClearAllEvent");
				HANDLE hNewDeviceEvent = OpenEvent(EVENT_MODIFY_STATE, FALSE, L"NewDeviceEvent");

				if (hNewDeviceEvent && WaitForSingleObject(hNewDeviceEvent, 1) != WAIT_OBJECT_0)
				{
					SetEvent(hNewDeviceEvent);
				}
				if (hClearAllEvent && hFinshedClearAllEvent)
				{
					ResetEvent(hClearAllEvent);
					SetEvent(hClearAllEvent);
					if (WaitForSingleObject(hFinshedClearAllEvent, MB_WAIT_TIMEOUT) == WAIT_OBJECT_0)
					{
						MessageBox(hWnd, L"All controllers were unpaired successfully.\nYou can now safety pair this controller to another device.\nTo pair again click the option \"Enable Pairing Mode\".", L"GIPSerial Important Information", MB_OK | MB_ICONINFORMATION);
					}
					else
					{
						MessageBox(hWnd, L"The Raspberry Pi ZeroW2 device was unable to clear all paired controller.\nTry restarting before trying again.", L"GIPSerial Error", MB_OK | MB_ICONWARNING);
					}
					ResetEvent(hFinshedClearAllEvent);
				}
				if (hClearAllEvent)
				{
					CloseHandle(hClearAllEvent);
				}
				if (hFinshedClearAllEvent)
				{
					CloseHandle(hFinshedClearAllEvent);
				}
			}
			}
			break;
		}
		}
		break;
	}
	case WM_DEVICECHANGE:
	{
		HANDLE hNewDeviceEvent = OpenEvent(EVENT_MODIFY_STATE, FALSE, L"NewDeviceEvent");

		if (hNewDeviceEvent && wParam == DBT_DEVNODES_CHANGED && WaitForSingleObject(hNewDeviceEvent, 1) != WAIT_OBJECT_0)
		{
			SetEvent(hNewDeviceEvent);
		}
		break;
	}
	case WM_CREATE:
	{
		AddNotificationIcon(hWnd);
		break;
	}
    case WM_DESTROY:
	{
		if (hDeviceSerial)
		{
			UnregisterDeviceNotification(hDeviceSerial);
			hDeviceSerial = NULL;
		}
		if (hPowerNotify)
		{
			UnregisterSuspendResumeNotification(hPowerNotify);
			hPowerNotify = NULL;
		}
		PostQuitMessage(0);
		break;
	}
    default:
	{
		if(message == WM_TaskBarCreated)
		{
			AddNotificationIcon(hWnd);
		}
		return DefWindowProc(hWnd, message, wParam, lParam);
	}

    }
    return 0;
}
void ScanForSerialDevices()
{
	std::vector<std::wstring> DevicePaths;
	std::vector<std::wstring> DeviceNames;
	FindAllDevices((LPGUID)&GUID_DEVINTERFACE_COMPORT, DevicePaths, &DeviceNames);
	for (size_t i = 0; i < DevicePaths.size(); i++)
	{
		if (DevicePaths[i].find(L"vid_" + std::wstring(PI_VID) + L"&pid_" + PI_PID) != std::wstring::npos)
		{
			devicePath = DevicePaths[i];
			if (DeviceNames[i].find(L"COM") != std::wstring::npos)
			{
				devicePort = DeviceNames[i].substr(DeviceNames[i].find(L"COM"), DeviceNames[i].find(L")"));
				devicePort = devicePort.substr(0, 5);
				comPath = L"\\\\.\\" + devicePort;
				DWORD exitCode = 0;

			}
			break;
		}
	}
}
BOOL FindAllDevices(const GUID* ClassGuid, std::vector<std::wstring>& DevicePaths, std::vector<std::wstring>* DeviceNames)
{
	if (DevicePaths.size())
	{
		DevicePaths.clear();
	}
	PSP_DEVICE_INTERFACE_DETAIL_DATA_W deviceDetails = nullptr;
	WCHAR* deviceName = nullptr;
	HDEVINFO hdevInfo = SetupDiGetClassDevs(ClassGuid, NULL, NULL, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
	if (hdevInfo == INVALID_HANDLE_VALUE)
	{
		return FALSE;
	}
	for (DWORD i = 0; ; i++)
	{
		deviceName = nullptr;
		SP_DEVICE_INTERFACE_DATA deviceInterfaceData = { 0 };
		SP_DEVINFO_DATA deviceData = { 0 };
		ULONG DataSize = 0;
		deviceInterfaceData.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);
		deviceData.cbSize = sizeof(SP_DEVINFO_DATA);
		if (SetupDiEnumDeviceInterfaces(hdevInfo, NULL, ClassGuid, i, &deviceInterfaceData) == FALSE ||
			SetupDiEnumDeviceInfo(hdevInfo, i, &deviceData) == FALSE)
		{
			if (GetLastError() != ERROR_NO_MORE_ITEMS) {
				SetupDiDestroyDeviceInfoList(hdevInfo);
				return FALSE;
			}
			else
			{
				break;
			}
		}
		if (DeviceNames != nullptr) {
			if (CM_Get_DevNode_Registry_Property(deviceData.DevInst, CM_DRP_FRIENDLYNAME, NULL, NULL, &DataSize, 0) != CR_NO_SUCH_VALUE)
			{
				if (DataSize) {
					deviceName = (WCHAR*)HeapAlloc(GetProcessHeap(), 0, DataSize * sizeof(WCHAR) + 2);
					if (CM_Get_DevNode_Registry_Property(deviceData.DevInst, CM_DRP_FRIENDLYNAME, NULL, deviceName, &DataSize, 0) == CR_SUCCESS)
					{
						if (deviceName != 0) {
							DeviceNames->push_back(deviceName);
						}
					}
					HeapFree(GetProcessHeap(), 0, deviceName);
				}
			}

		}
		deviceDetails = (PSP_DEVICE_INTERFACE_DETAIL_DATA_W)HeapAlloc(GetProcessHeap(), 0, MAX_PATH * sizeof(WCHAR) + sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W));
		if (deviceDetails == NULL)
		{
			SetupDiDestroyDeviceInfoList(hdevInfo);
			return FALSE;
		}
		deviceDetails->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W);
		if (SetupDiGetDeviceInterfaceDetail(hdevInfo, &deviceInterfaceData, deviceDetails, MAX_PATH * sizeof(WCHAR) + sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W), NULL, NULL) == TRUE)
		{
			std::wstring DevicePath = deviceDetails->DevicePath;
			HeapFree(GetProcessHeap(), 0, deviceDetails);
			DevicePaths.push_back(DevicePath);
		}
	}

	SetupDiDestroyDeviceInfoList(hdevInfo);
	return TRUE;
}
BOOL AddNotificationIcon(HWND hwnd)
{
	NOTIFYICONDATAW nid;
	nid.hWnd = hwnd;
	nid.cbSize = sizeof(NOTIFYICONDATAW_V3_SIZE);
	nid.uTimeout = 500;
	nid.uID = 1;
	nid.uFlags = NIF_TIP | NIF_ICON | NIF_MESSAGE | NIF_INFO | 0x00000080;
	nid.uCallbackMessage = WM_USER + 200;
	nid.hIcon = LoadIcon(hInst, MAKEINTRESOURCE(IDI_SMALL));
	std::copy(L"Click here to open GIP Serial options", L"Click here to open GIP Serial options" + 38, nid.szTip);
	std::copy(L"Click here to open GIP Serial options", L"Click here to open GIP Serial options" + 26, nid.szTip);
	nid.uCallbackMessage = APPWM_ICONNOTIFY;
	return Shell_NotifyIcon(NIM_ADD, &nid);
}