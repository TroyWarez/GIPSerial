// GIPSerial.cpp : Defines the entry point for the application.
//

#include "framework.h"
#include "GIPSerial.h"

#define MAX_LOADSTRING 100
#define PI_VID L"0525"
#define PI_PID L"a4a7"

#define IDM_EXIT 105
#define IDM_SYNC 106
#define IDM_CLEAR 107


#define APPWM_ICONNOTIFY (WM_APP + 1)

// GIP Commands
#define RASPBERRY_PI_GIP_POLL 0x00af
#define RASPBERRY_PI_GIP_SYNC 0x00b0
#define RASPBERRY_PI_GIP_CLEAR 0x00b1
#define RASPBERRY_PI_GIP_LOCK 0x00b2

// Global Variables:
HINSTANCE hInst;                                // current instance
WCHAR szTitle[MAX_LOADSTRING];                  // The title bar text
WCHAR szWindowClass[MAX_LOADSTRING];            // the main window class name
std::wstring devicePath;
std::wstring comPath;
std::wstring devicePort;


// Forward declarations of functions included in this code module:
ATOM                MyRegisterClass(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);
BOOL                FindAllDevices(const GUID* ClassGuid, std::vector<std::wstring>& DevicePaths, std::vector<std::wstring>* DeviceNames);
void                ScanForSerialDevices();
BOOL                AddNotificationIcon(HWND hwnd);

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
		// Initialize global strings
		LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
		LoadStringW(hInstance, IDC_GIPSERIAL, szWindowClass, MAX_LOADSTRING);
		MyRegisterClass(hInstance);

		// Perform application initialization:
		if (!InitInstance(hInstance, nCmdShow))
		{
			return FALSE;
		}

		HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_GIPSERIAL));

		ScanForSerialDevices();
		MSG msg;
		HANDLE hSerial = NULL;
		USHORT cmd = 0x00af;
		if (comPath != L"")
		{
			hSerial = CreateFile(comPath.c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, FILE_FLAG_WRITE_THROUGH, NULL);
			if (hSerial == INVALID_HANDLE_VALUE)
			{
				hSerial = NULL;
			}
		}

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
			else
			{
				if (hSerial)
				{
					if (!WriteFile(hSerial, &cmd, sizeof(cmd), NULL, NULL))
					{
						CloseHandle(hSerial);
					}
					Sleep(1);
				}
			}
		}
		if (hSerial)
		{
			CloseHandle(hSerial);
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
    wcex.lpszMenuName   = MAKEINTRESOURCEW(IDC_GIPSERIAL);
    wcex.lpszClassName  = szWindowClass;
    wcex.hIconSm        = NULL;

    return RegisterClassExW(&wcex);
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

   HWND hWnd = CreateWindowW(szWindowClass, L"", 0,
	   0, 0, 0, 0, nullptr, nullptr, hInstance, nullptr);

   if (!hWnd)
   {
      return FALSE;
   }

   ShowWindow(hWnd, SW_HIDE);
   UpdateWindow(hWnd);

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
			DestroyWindow(hWnd);
			break;
		case IDM_SYNC:
		{
			int selection = MessageBoxW(hWnd, L"Enable paring mode for the Raspberry Pi ZeroW2 device? Another message box window will appear to indicate if a controller was paired or not. \nClick ok to continue or click cancel to exit.", L"GIPSerial", MB_OKCANCEL | MB_ICONQUESTION);
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
						USHORT cmd = RASPBERRY_PI_GIP_SYNC;
						WriteFile(hSerial, &cmd, sizeof(cmd), NULL, NULL);
						CloseHandle(hSerial);
					}
				}
				else
				{
					MessageBox(hWnd, L"No Raspberry Pi ZeroW2 device found. Please ensure the device is connected and try again. The controller failed to pair with the device.", L"GIPSerial", MB_OK | MB_ICONERROR);
				}
			}
			}
			break;
		}
		case IDM_CLEAR:
		{
			int selection = MessageBoxW(hWnd, L"Warning: This option will attempt to unpair all synced controllers.\nClick ok to continue or click cancel to exit.", L"GIPSerial", MB_OKCANCEL | MB_ICONWARNING);
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
						MessageBox(hWnd, L"All controllers are now unpaired. To pair again click the option \"Enable Pairing Mode\".", L"GIPSerial", MB_OK | MB_ICONINFORMATION);
					}
				}
				else
				{
					MessageBox(hWnd, L"No Raspberry Pi ZeroW2 device found. Please ensure the device is connected and try again.", L"GIPSerial", MB_OK | MB_ICONERROR);
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
			if (CM_Get_DevNode_Registry_PropertyW(deviceData.DevInst, CM_DRP_FRIENDLYNAME, NULL, NULL, &DataSize, 0) != CR_NO_SUCH_VALUE)
			{
				if (DataSize) {
					deviceName = (WCHAR*)HeapAlloc(GetProcessHeap(), 0, DataSize * sizeof(WCHAR) + 2);
					if (CM_Get_DevNode_Registry_PropertyW(deviceData.DevInst, CM_DRP_FRIENDLYNAME, NULL, deviceName, &DataSize, 0) == CR_SUCCESS)
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
		if (SetupDiGetDeviceInterfaceDetailW(hdevInfo, &deviceInterfaceData, deviceDetails, MAX_PATH * sizeof(WCHAR) + sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W), NULL, NULL) == TRUE)
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
	nid.hIcon = LoadIconW(hInst, MAKEINTRESOURCEW(IDI_SMALL));
	std::copy(L"Click here to open GIP Serial options", L"Click here to open GIP Serial options" + 38, nid.szTip);
	std::copy(L"Click here to open GIP Serial options", L"Click here to open GIP Serial options" + 26, nid.szTip);
	nid.uCallbackMessage = APPWM_ICONNOTIFY;
	return Shell_NotifyIconW(NIM_ADD, &nid);
}