#include "RegistryOperation.h"
#include "RegistryConfig.h"
#include <tchar.h>
#include "Common.h"
#include "ShutdownSystem.h"
#include "resource.h"
#include <time.h>

#include <ShellScalingApi.h>
#pragma comment(lib, "shcore.lib")

#include <CommCtrl.h>
#pragma comment(lib,"comctl32.lib")

#pragma comment(linker,"\"/manifestdependency:type='win32' \
name='Microsoft.Windows.Common-Controls' version='6.0.0.0' \
processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")


#define CLS_NAME     L"LickBagShutdownSystemApp"        // 类名
#define WIN_TITLE    L"设定关机时间"        // 窗口标题
#define TIMER_INTERVAL  5000
#define BACKGROUND_RGB  RGB(248, 195, 191)


constexpr auto WINDOW_WIDTH = 600;
constexpr auto WINDOW_HEIGHT = 200;
constexpr auto TRACKBAR_WIDTH = 30;
constexpr auto TRACKBAR_WINDOW_HEIGHT = 55;
constexpr auto TRACKBAR_WINDOW_WIDTH = WINDOW_WIDTH - 16;


int g_Hour = 0;
int g_Minutes = 0;
int g_ConfigHour = 0;
int g_ConfigMinutes = 0;

HWND g_hHoursTrackBar = nullptr;
HWND g_hMinutesTrackBar = nullptr;
RECT g_textRect = { 0 };
bool g_IsEnableShutdownSystem = false;
bool g_IsNeedShutdown = true; // 当前是否真的需要关机
UINT_PTR g_TimerID = 0;


void InitShutdownSystem()
{
    g_Hour = RegGetShutdownSystemHours();
    g_Minutes = RegGetShutdownSystemMinutes();

    g_ConfigHour = g_Hour;
    g_ConfigMinutes = g_Minutes;
}


BOOL EnablePrivilege(LPCTSTR lpszPrivilege)
{
    HANDLE hToken;
    TOKEN_PRIVILEGES tkp;

    // 打开当前进程令牌
    if (!OpenProcessToken(GetCurrentProcess(),
        TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken))
    {
        return FALSE;
    }

    // 查找特权 LUID
    if (!LookupPrivilegeValue(NULL, lpszPrivilege,
        &tkp.Privileges[0].Luid))
    {
        CloseHandle(hToken);
        return FALSE;
    }

    tkp.PrivilegeCount = 1;
    tkp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

    // 调整令牌特权
    if (!AdjustTokenPrivileges(hToken, FALSE, &tkp, 0, NULL, NULL))
    {
        CloseHandle(hToken);
        return FALSE;
    }

    if (GetLastError() == ERROR_NOT_ALL_ASSIGNED)
    {
        CloseHandle(hToken);
        return FALSE;
    }

    CloseHandle(hToken);
    return TRUE;
}

void CALLBACK TimerProc(HWND hwnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime) 
{
    time_t t;
    struct tm tm_info;
    time(&t);
    if (localtime_s(&tm_info, &t) != 0) {
        return;
    }
    int hour = tm_info.tm_hour;
    int minute = tm_info.tm_min;

    if (g_Hour == hour && g_Minutes == minute)
    {
        if (g_IsNeedShutdown)
        {
            if (!EnablePrivilege(SE_SHUTDOWN_NAME))
            {
                return;
            }

            // 2. 调用 InitiateSystemShutdownEx 进行关机
            WCHAR msg[] = _T("系统将在 1 分钟后关闭；如需取消，请右键托盘图标，取消勾选“启用定时关机”");
            InitiateSystemShutdownEx(NULL, msg, 60, TRUE, FALSE, SHTDN_REASON_MAJOR_OTHER | SHTDN_REASON_MINOR_OTHER | SHTDN_REASON_FLAG_PLANNED);
            g_IsNeedShutdown = false;
        }
    }
    else
    {
        g_IsNeedShutdown = true;
    }
}


void EnableShutdownSystem(bool bEnable)
{
    g_IsEnableShutdownSystem = bEnable;

    if (bEnable)
    {
        g_TimerID = SetTimer(NULL, 0, TIMER_INTERVAL, (TIMERPROC)TimerProc);
    }
    else
    {
        KillTimer(NULL, g_TimerID);
        AbortSystemShutdown(NULL);
        g_IsNeedShutdown = true;
    }
}


bool IsEnableShutdownSystem()
{
    return g_IsEnableShutdownSystem;
}



HWND CreateHoursTrack(HWND hWnd, HINSTANCE hInst)
{
    DWORD dwMin = 0, dwMax = 23, dwCur = g_ConfigHour;
    
    HWND hTrack = CreateWindowEx(0, TRACKBAR_CLASS, TEXT("Trackbar Control"),
        WS_CHILD | WS_VISIBLE
        | TBS_HORZ  // 水平的
        | TBS_BOTH  // 把手指两边
        | TBS_AUTOTICKS // 显示刻度
        | TBS_FIXEDLENGTH  // 处理 TBM_SETTHUMBLENGTH 消息
        | TBS_TOOLTIPS     // 显示当前的数值
        | TBS_DOWNISLEFT
        , 0, 0, TRACKBAR_WINDOW_WIDTH, TRACKBAR_WINDOW_HEIGHT, hWnd, nullptr, hInst, nullptr);

    SendMessage(hTrack, TBM_SETRANGE, (WPARAM)TRUE, (LPARAM)MAKELONG(dwMin, dwMax));  // 最小最大刻度
    SendMessage(hTrack, TBM_SETTHUMBLENGTH, TRACKBAR_WIDTH, 0); // 把手大小
    SendMessage(hTrack, TBM_SETTIPSIDE, TBTS_TOP, 0); // tooltip显示位置
    SendMessage(hTrack, TBM_SETPAGESIZE, 0, 1); // 鼠标点击的调整大小
    SendMessage(hTrack, TBM_SETPOS, TRUE, dwCur); // 当前位置0
    SendMessage(hTrack, TBM_SETTICFREQ, 3, 0); // 设置显示的刻度的频率, 每隔10个画一个刻度线

    return hTrack;
}


HWND CreateMinutesTrack(HWND hWnd, HINSTANCE hInst)
{
    DWORD dwMin = 0, dwMax = 59, dwCur = g_ConfigMinutes;

    HWND hTrack = CreateWindowEx(0, TRACKBAR_CLASS, TEXT("Trackbar Control"),
        WS_CHILD | WS_VISIBLE
        | TBS_HORZ  // 水平的
        | TBS_BOTH  // 把手指两边
        | TBS_AUTOTICKS // 显示刻度
        | TBS_FIXEDLENGTH  // 处理 TBM_SETTHUMBLENGTH 消息
        | TBS_TOOLTIPS     // 显示当前的数值
        | TBS_DOWNISLEFT
        , 0, TRACKBAR_WINDOW_HEIGHT, TRACKBAR_WINDOW_WIDTH, TRACKBAR_WINDOW_HEIGHT, hWnd, nullptr, hInst, nullptr);

    SendMessage(hTrack, TBM_SETRANGE, (WPARAM)TRUE, (LPARAM)MAKELONG(dwMin, dwMax));  // 最小最大刻度
    SendMessage(hTrack, TBM_SETTHUMBLENGTH, TRACKBAR_WIDTH, 0); // 把手大小
    SendMessage(hTrack, TBM_SETTIPSIDE, TBTS_TOP, 0); // tooltip显示位置
    SendMessage(hTrack, TBM_SETPAGESIZE, 0, 1); // 鼠标点击的调整大小
    SendMessage(hTrack, TBM_SETPOS, TRUE, dwCur); // 当前位置0
    SendMessage(hTrack, TBM_SETTICFREQ, 10, 0); // 设置显示的刻度的频率, 每隔10个画一个刻度线

    return hTrack;
}


// 窗口函数
LRESULT _stdcall WinProc(HWND hWnd, unsigned int msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
        case WM_CREATE:
        {
            LPCREATESTRUCT lpCreateStruct = (LPCREATESTRUCT)lParam;
            g_hHoursTrackBar = CreateHoursTrack(hWnd, lpCreateStruct->hInstance);
            g_hMinutesTrackBar = CreateMinutesTrack(hWnd, lpCreateStruct->hInstance);

            RECT clientRect = { 0 };
            GetClientRect(hWnd, &clientRect);
            g_textRect = { 0, WINDOW_HEIGHT - TRACKBAR_WINDOW_HEIGHT * 2 + 20, WINDOW_WIDTH,  clientRect.bottom };

            return DefWindowProc(hWnd, msg, wParam, lParam);
        }

        // 处理滑条值变消息
        case WM_HSCROLL:
        {
            INT32 nCurHours = (BYTE)SendMessage(g_hHoursTrackBar, TBM_GETPOS, 0, 0);
            g_ConfigHour = (DWORD)nCurHours;

            INT32 nCurMinutes = (BYTE)SendMessage(g_hMinutesTrackBar, TBM_GETPOS, 0, 0);
            g_ConfigMinutes = (DWORD)nCurMinutes;

            InvalidateRect(hWnd, NULL, FALSE);

            break;
        }

        case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);

            // 创建内存DC
            HDC hMemDC = CreateCompatibleDC(hdc);
            HBITMAP hBitmap = CreateCompatibleBitmap(hdc, g_textRect.right, g_textRect.bottom - g_textRect.top);
            HBITMAP hOldBitmap = (HBITMAP)SelectObject(hMemDC, hBitmap);

            // 抹除背景
            RECT rect = { 0, 0, g_textRect.right, g_textRect.bottom - g_textRect.top };
            FillRect(hMemDC, &rect, (HBRUSH)CreateSolidBrush(BACKGROUND_RGB));

            // 画文字
            HFONT hFont = CreateFont(
                32,               // 字体高度（像素）
                0,                // 字体宽度（0 表示自动设置宽度）
                0,                // 字体的倾斜角度
                0,                // 字体的纵向倾斜角度
                FW_NORMAL,        // 字体的粗细（正常）
                0,                // 是否使用斜体
                0,                // 是否下划线
                0,                // 是否删除线
                DEFAULT_CHARSET,  // 字符集
                OUT_DEFAULT_PRECIS,  // 输出精度
                CLIP_DEFAULT_PRECIS, // 剪裁精度
                DEFAULT_QUALITY,     // 字体质量
                DEFAULT_PITCH,       // 字体间距
                L"Arial"             // 字体名称
            );

            HFONT hOldFont = (HFONT)SelectObject(hMemDC, hFont);

            SetTextColor(hMemDC, RGB(0, 0, 0));
            SetBkColor(hMemDC, BACKGROUND_RGB);

            WCHAR timeStr[16];
            wsprintf(timeStr, L"%02d时%02d分", g_ConfigHour, g_ConfigMinutes);
            DrawText(hMemDC, timeStr, -1, &rect, DT_SINGLELINE | DT_CENTER | DT_VCENTER | DT_WORDBREAK);

            // 恢复原字体
            SelectObject(hMemDC, hOldFont);
            DeleteObject(hFont);

            // 将内存DC的内容复制到窗口的DC
            BitBlt(hdc, g_textRect.left, g_textRect.top, g_textRect.right - g_textRect.left, g_textRect.bottom - g_textRect.top, hMemDC, 0, 0, SRCCOPY);

            // 清理资源
            SelectObject(hMemDC, hOldBitmap);
            DeleteObject(hBitmap);
            DeleteDC(hMemDC);

            EndPaint(hWnd, &ps);
            break;
        }

        case WM_DESTROY:
        {
            PostQuitMessage(0);
            break;
        }

        default:
        {
            return DefWindowProc(hWnd, msg, wParam, lParam);
        }
    }
    return 0;
}


// 创建设置窗口
void CreateSetShutdownSystemWindow(HINSTANCE hInstance)
{
    POINT ptWindow;
    ptWindow.x = (GetSystemMetrics(SM_CXSCREEN) - WINDOW_WIDTH) / 2;
    ptWindow.y = (GetSystemMetrics(SM_CYSCREEN) - WINDOW_HEIGHT) / 2;

    // 注册窗口类
    WNDCLASS wc;
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WinProc;
    wc.cbClsExtra = 0;
    wc.cbWndExtra = 0;
    wc.hInstance = hInstance;
    wc.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_ICON1));
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BACKGROUND + 1);
    wc.lpszMenuName = nullptr;
    wc.lpszClassName = CLS_NAME;
    RegisterClass(&wc);

    HWND hWnd = CreateWindow(CLS_NAME, WIN_TITLE, WS_SYSMENU | WS_CAPTION /* | WS_MINIMIZEBOX*/,
        ptWindow.x, ptWindow.y, WINDOW_WIDTH, WINDOW_HEIGHT, nullptr, nullptr, hInstance, nullptr);

    HMENU hMenu = GetSystemMenu(hWnd, FALSE);
    RemoveMenu(hMenu, SC_RESTORE, MF_BYCOMMAND);
    RemoveMenu(hMenu, SC_MAXIMIZE, MF_BYCOMMAND);
    RemoveMenu(hMenu, SC_MINIMIZE, MF_BYCOMMAND);
    RemoveMenu(hMenu, SC_SIZE, MF_BYCOMMAND);
    RemoveMenu(hMenu, SC_SEPARATOR, MF_BYCOMMAND);

    SetProcessDPIAware();
    ShowWindow(hWnd, SW_SHOWNORMAL);
    UpdateWindow(hWnd);

    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
}


// 显示设置窗口
void ShowSetShutdownSystemWindow(HINSTANCE hInstance)
{
    CreateSetShutdownSystemWindow(hInstance);

    g_Hour = g_ConfigHour;
    g_Minutes = g_ConfigMinutes;

    RegSetShutdownSystemHours(g_Hour);
    RegSetShutdownSystemMinutes(g_Minutes);
}
