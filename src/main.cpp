#include "header.h"
#include "Resource.h"
#include <shellapi.h>
#include "CKeepAwake.h"
#include "RegistryConfig.h"
#include "OptimizeService.h"
#include "KeepBalancePower.h"
#include "ShutdownSystem.h"

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

HINSTANCE g_hInst = nullptr;                             // 当前实例
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

    // 功能初始化
    if (RegIsKeepBalancePower())
    {
        LockBalancePower(true);
    }
    InitShutdownSystem();

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
    g_hInst = hInstance; // 将实例句柄存储在全局变量中

    HWND hWnd = CreateWindowW(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, nullptr, nullptr, hInstance, nullptr);

    if (!hWnd)
    {
        return FALSE;
    }

    // 注册电源设置的变更通知


    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    return TRUE;
}


void SetMenuText(HMENU hMenu, UINT itemID, WCHAR* newText)
{
    MENUITEMINFO mii;
    mii.cbSize = sizeof(MENUITEMINFO);
    mii.fMask = MIIM_TYPE;  // 设置菜单项类型
    mii.fType = MFT_STRING; // 指定菜单项为文本类型
    mii.dwTypeData = newText;  // 设置新的文本内容

    // 修改菜单项文本
    SetMenuItemInfo(hMenu, itemID, FALSE, &mii);
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
            IconData.hIcon = LoadIcon(g_hInst, MAKEINTRESOURCE(IDI_FORBIDSHUTDOWN));
            IconData.uCallbackMessage = UM_TRAY_NOTIFY;
            lstrcpy(IconData.szTip, L"避免休眠、禁用屏保、阻止关机、屏蔽更新");
            Shell_NotifyIcon(NIM_ADD, &IconData);

            g_pKeepAwake = CKeepAwake::GetInstance();
            g_pKeepAwake->Init(hWnd);

            // 屏蔽更新
            if (RegIsBlockWindowsUpdate())
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
                HMENU hMenu = LoadMenu(g_hInst, MAKEINTRESOURCE(IDR_TRAY_MENU));
                if (hMenu)
                {
                    HMENU hSubMenu = GetSubMenu(hMenu, 0);
                    if (hSubMenu) 
				    {
					    // 勾选开机启动菜单
					    if (RegIsBootUp())
					    {
						    CheckMenuItem(hMenu, IDM_BOOT_UP, MF_BYCOMMAND | MF_CHECKED);
					    }
                        
                        // 勾选屏蔽更新菜单
                        if (RegIsBlockWindowsUpdate())
                        {
                            CheckMenuItem(hMenu, IDM_BLOCK_WINDOWS_UPDATE, MF_BYCOMMAND | MF_CHECKED);
                        }

                        // 勾选锁定平衡方案
                        if (RegIsKeepBalancePower())
                        {
                            CheckMenuItem(hMenu, IDM_LOCK_BALANCE_POWER, MF_BYCOMMAND | MF_CHECKED);
                        }
                    
                        // 勾选启用自动关机
                        if (IsEnableShutdownSystem())
                        {
                            CheckMenuItem(hMenu, IDM_ENABLE_SHUTDOWN, MF_BYCOMMAND | MF_CHECKED);
                        }

                        // 定时关机时间
                        auto hour = RegGetShutdownSystemHours();
                        auto minutes = RegGetShutdownSystemMinutes();
                        WCHAR menuText[64]; 
                        swprintf(menuText, ARRAYSIZE(menuText), TEXT("定时关机时间: %02d时%02d分"), hour, minutes);
                        SetMenuText(hMenu, IDM_SET_SHUTDOWN, menuText);

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
			    RegSetBootUp(!RegIsBootUp());
		    }
		    else if (LOWORD(wParam) == IDM_LINK)
		    {
			    // system("rundll32 url.dll,FileProtocolHandler https://github.com/LickBag");
                // system("start https://github.com/LickBag");
                ShellExecute(nullptr, TEXT("open"), TEXT("https://github.com/LickBag"), nullptr, nullptr, SW_HIDE);
		    }
            else if (LOWORD(wParam) == IDM_BLOCK_WINDOWS_UPDATE)
            {
                bool block = !RegIsBlockWindowsUpdate();
                RegSetBlockWindowsUpdate(block);
                if (block)
                {
                    OptimizeService();
                }
                else
                {
                    CancelOptimizeService();
                }
            }
            else if (LOWORD(wParam) == IDM_LOCK_BALANCE_POWER)
            {
                LockBalancePower(!RegIsKeepBalancePower());
            }
            else if (LOWORD(wParam) == IDM_ENABLE_SHUTDOWN)
            {
                EnableShutdownSystem(!IsEnableShutdownSystem());
            }
            else if (LOWORD(wParam) == IDM_SET_SHUTDOWN)
            {
                ShowSetShutdownSystemWindow(g_hInst);
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

        case WM_POWERBROADCAST:
        {
            break;
        }

        default:
        {
            return DefWindowProc(hWnd, message, wParam, lParam);
        }
    }
    return 0;
}
