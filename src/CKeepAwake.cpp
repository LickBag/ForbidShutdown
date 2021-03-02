#include "CKeepAwake.h"
#include "highlevelmonitorconfigurationapi.h"
#include <xstring>
using namespace std;

WNDPROC g_oldWndProc = nullptr;

LRESULT CALLBACK MyWindowProc(HWND hWnd,
    UINT Msg,
    WPARAM wParam,
    LPARAM lParam)
{
    switch (Msg)
    {
        case WM_QUERYENDSESSION:
        {
            return 0; // 返回0代表不允许休眠
            break;
        }

        case WM_POWERBROADCAST:// 显示器等广播
        {
            if (wParam == PBT_POWERSETTINGCHANGE)
            {
                POWERBROADCAST_SETTING* lvpsSetting = (POWERBROADCAST_SETTING*)lParam;
                DWORD lvStatus = *(DWORD*)(lvpsSetting->Data);
                if (lvStatus == 0) // 0关闭, 1打开
                {
                    // 并不好用, 或者说没用。
                    SendMessage(hWnd, WM_SYSCOMMAND, SC_MONITORPOWER, -1); // 打开显示器
                    CKeepAwake::GetInstance()->RestoreMonitorBrightness();
                    return 0;
                }
                //if (lvStatus != 0) //0关闭, 打开
                //{
                //    LRESULT ret = ::SendMessage(HWND_BROADCAST, WM_SYSCOMMAND, SC_MONITORPOWER, 2);//关闭显示器 
                //    return 0;
                //}
            }
            break;
        }

        default:
            break;
    }

    return CallWindowProc(g_oldWndProc, hWnd, Msg, wParam, lParam);
}

CKeepAwake::CKeepAwake()
{
}


CKeepAwake::~CKeepAwake()
{


    RestoreScrSaver();

    if (g_oldWndProc != nullptr)
    {
        ::SetWindowLongPtr(m_hWnd, GWLP_WNDPROC, (LONG_PTR)g_oldWndProc);
    }

    UnregisterPowerSettingNotification(m_lvhpNotify);

    if (m_bRet)
    {
        ShutdownBlockReasonDestroy(m_hWnd);
    }

    SetThreadExecutionState(m_oldState);
}

// 禁用屏幕保护
void CKeepAwake::DisableScrSaver()
{
    SystemParametersInfo(SPI_GETSCREENSAVEACTIVE, 0, &m_bScrActive, 0);
    if (m_bScrActive)
    {
        SystemParametersInfo(SPI_SETSCREENSAVEACTIVE, FALSE, nullptr, 0);
    }
}

// 恢复屏幕保护
void CKeepAwake::RestoreScrSaver()
{
    if (m_bScrActive)
    {
        SystemParametersInfo(SPI_SETSCREENSAVEACTIVE, TRUE, nullptr, 0);
    }
}

void CKeepAwake::SaveMonitorBrightness()
{
    HMONITOR hMonitor = MonitorFromWindow(HWND_DESKTOP, MONITOR_DEFAULTTOPRIMARY);
    if (hMonitor)
    {
        wstring strFunc;
        BOOL bRet = TRUE;
        do 
        {
            DWORD dwNumberOfPhysicalMonitors = 0;
            bRet = GetNumberOfPhysicalMonitorsFromHMONITOR(hMonitor, &dwNumberOfPhysicalMonitors);
            if (!bRet) { break; }

            m_pAryPhysicalMonitor = new PHYSICAL_MONITOR[dwNumberOfPhysicalMonitors];
            ZeroMemory(m_pAryPhysicalMonitor, sizeof(PHYSICAL_MONITOR) * dwNumberOfPhysicalMonitors);
            bRet = GetPhysicalMonitorsFromHMONITOR(hMonitor, dwNumberOfPhysicalMonitors, (LPPHYSICAL_MONITOR)m_pAryPhysicalMonitor);
            if (!bRet) { break; }

            DWORD dwMinimumBrightness = 0, dwMaximumBrightness = 0;
            bRet = GetMonitorBrightness(((LPPHYSICAL_MONITOR)m_pAryPhysicalMonitor)[0].hPhysicalMonitor, &dwMinimumBrightness, &m_dwCurrentBrightness, &dwMaximumBrightness);
            if (!bRet) { break; }

        } while (false);

        if (!bRet)
        {
            TCHAR* pStrError = GetErrorString();
			if (m_pAryPhysicalMonitor != nullptr)
			{
				delete []m_pAryPhysicalMonitor;
				m_pAryPhysicalMonitor = nullptr;
			}
        }
    }
}

void CKeepAwake::RestoreMonitorBrightness()
{
    if (m_pAryPhysicalMonitor != nullptr)
    {
        SetMonitorBrightness(((LPPHYSICAL_MONITOR)m_pAryPhysicalMonitor)[0].hPhysicalMonitor, m_dwCurrentBrightness);
		delete []m_pAryPhysicalMonitor;
		m_pAryPhysicalMonitor = nullptr;
    }
}

CKeepAwake* CKeepAwake::m_pInstance = nullptr;
CKeepAwake* CKeepAwake::GetInstance()
{
    if (m_pInstance == nullptr)
    {
        m_pInstance = new CKeepAwake();
    }
    return m_pInstance;
}

void CKeepAwake::DelInstance()
{
    delete m_pInstance;
    m_pInstance = nullptr;
}

void CKeepAwake::Init(HWND hWnd)
{
    m_hWnd = hWnd;

    // 阻止休眠
    m_oldState = SetThreadExecutionState(ES_CONTINUOUS | ES_SYSTEM_REQUIRED | ES_DISPLAY_REQUIRED | ES_AWAYMODE_REQUIRED);

    // 阻止关机信息
    m_bRet = ShutdownBlockReasonCreate(m_hWnd, L"不关机行不. ( ゜- ゜)つロ");

    // 设置此程序在结束会话时排在第一位
    SetProcessShutdownParameters(0x4FF, SHUTDOWN_NORETRY);

    // 注册消息
    HPOWERNOTIFY m_lvhpNotify = RegisterPowerSettingNotification(m_hWnd
        , &GUID_CONSOLE_DISPLAY_STATE, DEVICE_NOTIFY_WINDOW_HANDLE);

    // 取代原来的窗口过程
    if (nullptr == g_oldWndProc)
    {
        g_oldWndProc = (WNDPROC)::SetWindowLongPtr(m_hWnd, GWLP_WNDPROC, (LONG_PTR)MyWindowProc);
    }

    // 禁用屏幕保护
    DisableScrSaver();

    // 保存当前屏幕亮度
    SaveMonitorBrightness();
}

TCHAR* CKeepAwake::GetErrorString()
{
    DWORD dwError = GetLastError();
    TCHAR* buffer = nullptr;
    ::FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM, NULL, dwError, 0, (LPTSTR)&buffer, 0, NULL);
    return buffer;
}
