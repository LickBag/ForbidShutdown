#include "header.h"
#include "Resource.h"
#include <shellapi.h>
#include "CKeepAwake.h"
#include "RegistryConfig.h"
#include "OptimizeService.h"



// 添加MessageBoxTimeout支持
extern "C"
{
    int WINAPI MessageBoxTimeoutA(IN HWND hWnd, IN LPCSTR lpText, IN LPCSTR lpCaption, IN UINT uType, IN WORD wLanguageId, IN DWORD dwMilliseconds);
    int WINAPI MessageBoxTimeoutW(IN HWND hWnd, IN LPCWSTR lpText, IN LPCWSTR lpCaption, IN UINT uType, IN WORD wLanguageId, IN DWORD dwMilliseconds);
};
#ifdef UNICODE
#define MessageBoxTimeout MessageBoxTimeoutW
#else
#define MessageBoxTimeout MessageBoxTimeoutA
#endif


#define MAX_LOADSTRING 100
#define UM_TRAY_NOTIFY (WM_USER + WM_USER)
#define UM_END 0xBFFE

HINSTANCE hInst = nullptr;                             // 当前实例
WCHAR szTitle[MAX_LOADSTRING] = { L"ForbidShutDown" }; // 标题栏文本
WCHAR szWindowClass[MAX_LOADSTRING] = { L"ForbidShutDown" }; // 主窗口类名
CKeepAwake* g_pKeepAwake = nullptr;

BOOL             InitInstance(HINSTANCE, int);
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
ATOM             MyRegisterClass(HINSTANCE hInstance);

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPWSTR    lpCmdLine,
    _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    // 保证单例
    auto h = ::CreateEvent(nullptr, FALSE, TRUE, _T("52E6F1E6-26CB-4182-BC0F-60EA520B0EA3"));
    auto err = GetLastError();
    if (err == ERROR_ALREADY_EXISTS)
    {
        if (MessageBoxTimeout == nullptr)
        {
            MessageBox(nullptr, TEXT("已有进程启动"), nullptr, MB_OK | MB_ICONSTOP);
        }
        else
        {
            MessageBoxTimeout(nullptr, TEXT("已有进程启动"), nullptr, MB_OK | MB_ICONSTOP, 0, 1000);
        }
        return 0;
    }

    MyRegisterClass(hInstance);
    if (!InitInstance(hInstance, SW_HIDE))
    {
        return FALSE;
    }

    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return (int)msg.wParam;
}

ATOM MyRegisterClass(HINSTANCE hInstance)
{
    WNDCLASSEXW wcex;
    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = 0;
    wcex.hInstance = hInstance;
    wcex.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_FORBIDSHUTDOWN));
    wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszMenuName = nullptr;// MAKEINTRESOURCEW(IDC_FORBIDSHUTDOWN);
    wcex.lpszClassName = szWindowClass;
    wcex.hIconSm = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_FORBIDSHUTDOWN));

    return RegisterClassExW(&wcex);
}

BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
    hInst = hInstance; // 将实例句柄存储在全局变量中

    HWND hWnd = CreateWindowW(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, nullptr, nullptr, hInstance, nullptr);

    if (!hWnd)
    {
        return FALSE;
    }

    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    return TRUE;
}


LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    POINT lpClickPoint;

    switch (message)
    {
        case WM_CREATE:
        {
            NOTIFYICONDATA IconData = { 0 };
            IconData.cbSize = sizeof(NOTIFYICONDATA);
            IconData.hWnd = (HWND)hWnd;
            IconData.uID = 0;
            IconData.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
            IconData.hIcon = LoadIcon(hInst, MAKEINTRESOURCE(IDI_FORBIDSHUTDOWN));
            IconData.uCallbackMessage = UM_TRAY_NOTIFY;
            lstrcpy(IconData.szTip, L"避免休眠、禁用屏保、阻止关机、屏蔽更新");
            Shell_NotifyIcon(NIM_ADD, &IconData);

            g_pKeepAwake = CKeepAwake::GetInstance();
            g_pKeepAwake->Init(hWnd);

            // 屏蔽更新
            if (IsBlockWindowsUpdate())
            {
                OptimizeService();
            }
            break;
        }

        case UM_TRAY_NOTIFY:
        {
            if (lParam == WM_RBUTTONDOWN)
            {
                GetCursorPos(&lpClickPoint);
                HMENU hMenu = LoadMenu(hInst, MAKEINTRESOURCE(IDR_TRAY_MENU));
                if (hMenu)
                {
                    HMENU hSubMenu = GetSubMenu(hMenu, 0);
                    if (hSubMenu) 
				    {
					    // 勾选开机启动菜单
					    if (IsBootUp())
					    {
						    CheckMenuItem(hMenu, IDM_BOOT_UP, MF_BYCOMMAND | MF_CHECKED);
					    }
                        
                        // 勾选屏蔽更新菜单
                        if (IsBlockWindowsUpdate())
                        {
                            CheckMenuItem(hMenu, IDM_BLOCK_WINDOWS_UPDATE, MF_BYCOMMAND | MF_CHECKED);
                        }
                    
					    SetForegroundWindow(hWnd); // 避免菜单弹出后, 如果不点击则不消失的问题。
                        TrackPopupMenu(hSubMenu, TPM_LEFTALIGN | TPM_LEFTBUTTON | TPM_BOTTOMALIGN, lpClickPoint.x, lpClickPoint.y, 0, hWnd, NULL);
                    }
                    DestroyMenu(hMenu);
				    hMenu = nullptr;
                }
            }

            if (lParam == WM_LBUTTONDOWN)
            {
                NOTIFYICONDATA IconData = { 0 };
                IconData.cbSize = sizeof(NOTIFYICONDATA);
                IconData.hWnd = (HWND)hWnd;
                IconData.uFlags = NIF_INFO;
                lstrcpy(IconData.szInfo, _T("别点我, 好痛~"));
                lstrcpy(IconData.szInfoTitle,  _T("嘤嘤嘤~"));
                Shell_NotifyIcon(NIM_MODIFY, &IconData);
                return 0;
            }
            break;
        }

        case WM_COMMAND:
        {
            if (LOWORD(wParam) == IDM_EXIT)
            {
                PostMessage(hWnd, WM_DESTROY, 0, 0);
            }
		    else if (LOWORD(wParam) == IDM_BOOT_UP)
		    {
			    bool bBookUp = !IsBootUp();
			    SetBootUp(bBookUp);
		    }
		    else if (LOWORD(wParam) == IDM_LINK)
		    {
			    // system("rundll32 url.dll,FileProtocolHandler https://github.com/LickBag");
                // system("start https://github.com/LickBag");
                ShellExecute(nullptr, TEXT("open"), TEXT("https://github.com/LickBag"), nullptr, nullptr, SW_HIDE);
		    }
            else if (LOWORD(wParam) == IDM_BLOCK_WINDOWS_UPDATE)
            {
                bool block = !IsBlockWindowsUpdate();
                SetIsBlockWindowsUpdate(block);
                if (block)
                {
                    OptimizeService();
                }
                else
                {
                    CancelOptimizeService();
                }
            }
            break;
        }

        case WM_DESTROY:
        {
            CKeepAwake::DelInstance();

            NOTIFYICONDATA IconData = { 0 };
            IconData.cbSize = sizeof(NOTIFYICONDATA);
            IconData.hWnd = (HWND)hWnd;

            Shell_NotifyIcon(NIM_DELETE, &IconData);

            PostQuitMessage(0);
            break;
        }

        default:
        {
            return DefWindowProc(hWnd, message, wParam, lParam);
        }
    }
    return 0;
}
