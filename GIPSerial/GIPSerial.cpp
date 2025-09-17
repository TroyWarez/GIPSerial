// GIPSerial.cpp : Defines the entry point for the application.OpenEventW
//

#include "framework.h"
#include "GIPSerial.h"

#define MAX_LOADSTRING 100
#define PI_VID L"0525"
#define PI_PID L"a4a7"

#define IDM_EXIT 105
#define IDM_SYNC 106
#define IDM_CLEAR 107
#define IDM_CLEAR_SINGLE 108


#define APPWM_ICONNOTIFY (WM_APP + 1)

// GIP Commands
#define RASPBERRY_PI_GIP_POLL 0x00af
#define RASPBERRY_PI_GIP_SYNC 0x00b0
#define RASPBERRY_PI_GIP_CLEAR 0x00b1
#define RASPBERRY_PI_GIP_LOCK 0x00b2
#define RASPBERRY_PI_SYNCED_CONTROLLER_COUNT 0x00b3
#define RASPBERRY_PI_CLEAR_NEXT_SYNCED_CONTROLLER 0x00b4

// Global Variables:
HINSTANCE hInst;                                // current instance
WCHAR szTitle[MAX_LOADSTRING];                  // The title bar text
WCHAR szWindowClass[MAX_LOADSTRING];            // the main window class name
std::wstring devicePath;
std::wstring comPath;
std::wstring devicePort;

static HANDLE hShutdownEvent = NULL;
static HANDLE hSyncEvent = NULL;
static HANDLE hClearSingleEvent = NULL;
static HANDLE hClearAllEvent = NULL;
static HANDLE hLockDeviceEvent = NULL;
static HANDLE hNewDeviceEvent = NULL;

// Forward declarations of functions included in this code module:
ATOM                MyRegisterClass(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);
BOOL                FindAllDevices(const GUID* ClassGuid, std::vector<std::wstring>& DevicePaths, std::vector<std::wstring>* DeviceNames);
void                ScanForSerialDevices();
BOOL                AddNotificationIcon(HWND hwnd);

DWORD WINAPI SerialThread(LPVOID lpParam) {

	UNREFERENCED_PARAMETER(lpParam);

	HANDLE hSerial = CreateFile(comPath.c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, FILE_FLAG_WRITE_THROUGH, NULL);
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

	hShutdownEvent = OpenEvent(SYNCHRONIZE, FALSE, L"ShutdownEvent");
	if (hShutdownEvent == NULL && GetLastError() == ERROR_FILE_NOT_FOUND)
	{
		hShutdownEvent = CreateEvent(NULL, FALSE, FALSE, L"ShutdownEvent");
	}
	if (hShutdownEvent)
	{
		ResetEvent(hShutdownEvent);
	}

	hSyncEvent = OpenEvent(SYNCHRONIZE, FALSE, L"SyncEvent");
	if (hSyncEvent == NULL && GetLastError() == ERROR_FILE_NOT_FOUND)
	{
		hSyncEvent = CreateEvent(NULL, FALSE, FALSE, L"SyncEvent");
	}
	if (hSyncEvent)
	{
		ResetEvent(hSyncEvent);
	}

	hClearSingleEvent = OpenEvent(SYNCHRONIZE, FALSE, L"ClearSingleEvent");
	if (hClearSingleEvent == NULL && GetLastError() == ERROR_FILE_NOT_FOUND)
	{
		hClearSingleEvent = CreateEvent(NULL, FALSE, FALSE, L"ClearSingleEvent");
	}
	if (hClearSingleEvent)
	{
		ResetEvent(hClearSingleEvent);
	}

	hClearAllEvent = OpenEvent(SYNCHRONIZE, FALSE, L"ClearAllEvent");
	if (hClearAllEvent == NULL && GetLastError() == ERROR_FILE_NOT_FOUND)
	{
		hClearAllEvent = CreateEvent(NULL, FALSE, FALSE, L"ClearAllEvent");
	}
	if (hClearAllEvent)
	{
		ResetEvent(hClearAllEvent);
	}

	hLockDeviceEvent = OpenEvent(SYNCHRONIZE, FALSE, L"LockDeviceEvent");
	if (hLockDeviceEvent == NULL && GetLastError() == ERROR_FILE_NOT_FOUND)
	{
		hLockDeviceEvent = CreateEvent(NULL, FALSE, FALSE, L"LockDeviceEvent");
	}
	if (hLockDeviceEvent)
	{
		ResetEvent(hLockDeviceEvent);
	}

	hNewDeviceEvent = OpenEvent(SYNCHRONIZE, FALSE, L"NewDeviceEvent");
	if (hNewDeviceEvent == NULL && GetLastError() == ERROR_FILE_NOT_FOUND)
	{
		hNewDeviceEvent = CreateEvent(NULL, FALSE, FALSE, L"NewDeviceEvent");
	}
	if (hNewDeviceEvent)
	{
		ResetEvent(hNewDeviceEvent);
	}

	std::vector<HANDLE> hEvents;

	hEvents.push_back(hShutdownEvent);
	hEvents.push_back(hSyncEvent);
	hEvents.push_back(hClearSingleEvent);
	hEvents.push_back(hClearAllEvent);
	hEvents.push_back(hLockDeviceEvent);
	hEvents.push_back(hNewDeviceEvent);

	UCHAR pwrStatus = 0;
	USHORT cmd = RASPBERRY_PI_GIP_POLL;
	DWORD dwWaitResult = 0;

	dwWaitResult = WAIT_TIMEOUT;
	while (dwWaitResult == WAIT_TIMEOUT) {
		dwWaitResult = WaitForMultipleObjects((DWORD)hEvents.size(), hEvents.data(), FALSE, 1);
		switch (dwWaitResult)
		{
		case 1: // hSyncEvent
		{
			if (hSyncEvent && dwWaitResult == 1)
			{
				ResetEvent(hSyncEvent);
				cmd = RASPBERRY_PI_GIP_SYNC;
			}
		}
		case 2: // hClearSingleEvent
		{
			if (hClearSingleEvent && dwWaitResult == 2)
			{
				ResetEvent(hClearSingleEvent);
				cmd = RASPBERRY_PI_CLEAR_NEXT_SYNCED_CONTROLLER;
			}
		}
		case 3: // hClearAllEvent
		{
			if (hClearAllEvent && dwWaitResult == 3)
			{
				ResetEvent(hClearAllEvent);
				cmd = RASPBERRY_PI_GIP_CLEAR;
			}
		}
		case WAIT_TIMEOUT: {

			if (hSerial)
			{
				if (!ReadFile(hSerial, &pwrStatus, sizeof(pwrStatus), NULL, NULL))
				{
					CloseHandle(hSerial);
					hSerial = NULL;
				}
				else if (!WriteFile(hSerial, &cmd, sizeof(cmd), NULL, NULL))
				{
					CloseHandle(hSerial);
					hSerial = NULL;
				}
			}
			break;
		}
		case 4: // hLockDeviceEvent
		{
			if (hLockDeviceEvent)
			{
				ResetEvent(hLockDeviceEvent);
			}
			cmd = RASPBERRY_PI_GIP_LOCK;
			if (hSerial)
			{
				if (!WriteFile(hSerial, &cmd, sizeof(cmd), NULL, NULL))
				{
					CloseHandle(hSerial);
					hSerial = NULL;
				}
				else
				{
					if (hShutdownEvent)
					{
						SetEvent(hShutdownEvent);
					}
				}
			}
		}
		case 0: // hShutdownEvent
		{
			for (size_t i = 0; i < hEvents.size(); i++)
			{
				if (hEvents[i] != NULL)
				{
					CloseHandle(hEvents[i]);
				}
			}
			if (hSerial)
			{
				CloseHandle(hSerial);
				hSerial = NULL;
			}
			return 0;
		}
		case 5: // hNewDeviceEvent
		{
			if (hNewDeviceEvent)
			{
				ResetEvent(hNewDeviceEvent);
			}
			ScanForSerialDevices();
			if (comPath != L"")
			{
				hSerial = CreateFile(comPath.c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, FILE_FLAG_WRITE_THROUGH, NULL);
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
		}
	}
	return 0;
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
		hShutdownEvent = CreateEvent(NULL, FALSE, FALSE, L"ShutdownEvent");
		hSyncEvent = CreateEvent(NULL, FALSE, FALSE, L"SyncEvent");
		hClearSingleEvent = CreateEvent(NULL, FALSE, FALSE, L"ClearSingleEvent");
		hClearAllEvent = CreateEvent(NULL, FALSE, FALSE, L"ClearAllEvent");
		hLockDeviceEvent = CreateEvent(NULL, FALSE, FALSE, L"LockDeviceEvent");

		ScanForSerialDevices();

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
		while (true)
		{
			if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
			{
				if (msg.message == WM_QUIT)
				{
					break;
				}

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

   DEV_BROADCAST_DEVICEINTERFACE hidFilter = { };
   hidFilter.dbcc_size = sizeof(DEV_BROADCAST_DEVICEINTERFACE);
   hidFilter.dbcc_classguid = GUID_DEVINTERFACE_COMPORT;
   hidFilter.dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE;
   HDEVNOTIFY hDeviceHID = RegisterDeviceNotification(hWnd, &hidFilter, DEVICE_NOTIFY_WINDOW_HANDLE);


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
	case APPWM_ICONNOTIFY:
	{
		switch (lParam)
		{
		case WM_LBUTTONUP:
			HMENU Hmenu = CreatePopupMenu();

			AppendMenu(Hmenu, MF_STRING, IDM_CLEAR, L"Clear All Paired Controllers");
			AppendMenu(Hmenu, MF_STRING, IDM_CLEAR_SINGLE, L"Clear a Single Paired Controller");
			AppendMenu(Hmenu, MF_STRING, IDM_SYNC, L"Enable Pairing Mode");
			AppendMenu(Hmenu, MF_STRING, IDM_EXIT, L"Close GIPSerial");
			POINT p;
			GetCursorPos(&p);
			TrackPopupMenu(Hmenu, TPM_LEFTBUTTON, p.x, p.y, 0, hWnd, 0);
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
		case IDM_EXIT:
			if (hLockDeviceEvent)
			{
				SetEvent(hLockDeviceEvent);
				if (hShutdownEvent && WaitForSingleObject(hShutdownEvent, 5000) == WAIT_OBJECT_0)
				{
					MessageBox(hWnd, L"The device is now locked and can be re enabled by running GIPSerial again.\nThis is to prevent accidentally shutdowns from happening.", L"GIPSerial Important Information", MB_OK | MB_ICONINFORMATION);
				}
				else
				{
					MessageBox(hWnd, L"The device may be unlocked and could power down the current computer unexpectedly when a paired controlled is used.\n\nRun GIPSerial again to fix this.\n\nDo not run GIPSerial unless you have the required Raspberry Pi ZeroW2 serial device connected to your computer.", L"GIPSerial Error", MB_OK | MB_ICONERROR);
				}
			}
			DestroyWindow(hWnd);
			PostQuitMessage(0);
			break;
		case IDM_SYNC:
		{
			int selection = MessageBox(hWnd, L"Enable paring mode for the Raspberry Pi ZeroW2 device?Another message box window will appear to indicate if a controller was paired or not.\nClick ok to continue or click cancel to exit.", L"GIPSerial", MB_OKCANCEL | MB_ICONQUESTION);
			switch (selection)
			{
			case IDCANCEL:
				return DefWindowProc(hWnd, message, wParam, lParam);
			case IDOK:
			{
				if (hSyncEvent) // get current time
				{
					ResetEvent(hSyncEvent);
					SetEvent(hSyncEvent);
					if (WaitForSingleObject(hShutdownEvent, 5000) == WAIT_OBJECT_0)
					{
						MessageBox(hWnd, L"The device is now locked and can be re enabled by running GIPSerial again.\nThis is to prevent accidentally shutdowns from happening.", L"GIPSerial Important Information", MB_OK | MB_ICONINFORMATION);
					}
					else
					{
						MessageBox(hWnd, L"The device may be unlocked and could power down the current computer unexpectedly when a paired controlled is used.\n\nRun GIPSerial again to fix this.\n\nDo not run GIPSerial unless you have the required Raspberry Pi ZeroW2 serial device connected to your computer.", L"GIPSerial Error", MB_OK | MB_ICONERROR);
					}
					CloseHandle(hShutdownEvent);
				}
				ScanForSerialDevices();
				if (comPath != L"")
				{
					HANDLE hSerial = CreateFile(comPath.c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, FILE_FLAG_WRITE_THROUGH, NULL);
					if (hSerial != INVALID_HANDLE_VALUE)
					{
						USHORT cmd = RASPBERRY_PI_GIP_SYNC;
						WriteFile(hSerial, &cmd, sizeof(cmd), NULL, NULL);
						CloseHandle(hSerial);
					}
				}
				else
				{
					MessageBox(hWnd, L"No Raspberry Pi ZeroW2 device found.\nPlease ensure the device is connected and try again.\nThe controller failed to pair with the device.", L"GIPSerial Error", MB_OK | MB_ICONERROR);
				}
			}
			}
			break;
		}
		case IDM_CLEAR_SINGLE:
		case IDM_CLEAR:
		{
			int selection = MessageBox(hWnd, L"Warning: This option will attempt to unpair all synced controllers.\nClick ok to continue or click cancel to exit.", L"GIPSerial Warning", MB_OKCANCEL | MB_ICONWARNING);
			switch (selection)
			{
			case IDCANCEL:
				return DefWindowProc(hWnd, message, wParam, lParam);
			case IDOK:
			{
				ScanForSerialDevices();
				if (comPath != L"")
				{
					HANDLE hSerial = CreateFile(comPath.c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, FILE_FLAG_WRITE_THROUGH, NULL);
					if (hSerial != INVALID_HANDLE_VALUE)
					{
						USHORT cmd = RASPBERRY_PI_GIP_CLEAR;
						WriteFile(hSerial, &cmd, sizeof(cmd), NULL, NULL);
						CloseHandle(hSerial);
						MessageBox(hWnd, L"All controllers are now unpaired.\nTo pair again click the option \"Enable Pairing Mode\".", L"GIPSerial Important Information", MB_OK | MB_ICONINFORMATION);
					}
				}
				else
				{
					MessageBox(hWnd, L"No Raspberry Pi ZeroW2 device found.\nPlease ensure the device is connected and try again.", L"GIPSerial Error", MB_OK | MB_ICONERROR);
				}
				break;
			}
			}
			break;
		}
		default:
			return DefWindowProc(hWnd, message, wParam, lParam);
		}
	}
	break;
	case WM_DEVICECHANGE:
	{

		if ( wParam == DBT_DEVNODES_CHANGED && WaitForSingleObject(hNewDeviceEvent, 1) != WAIT_OBJECT_0)
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
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
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