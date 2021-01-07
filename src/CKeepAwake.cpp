#include "CKeepAwake.h"
#include "highlevelmonitorconfigurationapi.h"
#include <xstring>
using namespace std;

WNDPROC g_oldWndProc = NULL;

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

    if (g_oldWndProc != NULL)
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
        SystemParametersInfo(SPI_SETSCREENSAVEACTIVE, FALSE, NULL, 0);
    }
}

// 恢复屏幕保护
void CKeepAwake::RestoreScrSaver()
{
    if (m_bScrActive)
    {
        SystemParametersInfo(SPI_SETSCREENSAVEACTIVE, TRUE, NULL, 0);
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
            if (!bRet) { strFunc = _T("GetNumberOfPhysicalMonitorsFromHMONITOR"); break; }

            m_pAryPhysicalMonitor = new PHYSICAL_MONITOR[dwNumberOfPhysicalMonitors];
            ZeroMemory(m_pAryPhysicalMonitor, sizeof(PHYSICAL_MONITOR) * dwNumberOfPhysicalMonitors);
            bRet = GetPhysicalMonitorsFromHMONITOR(hMonitor, dwNumberOfPhysicalMonitors, (LPPHYSICAL_MONITOR)m_pAryPhysicalMonitor);
            if (!bRet) { strFunc = _T("GetPhysicalMonitorsFromHMONITOR"); break; }

            DWORD dwMinimumBrightness = 0, dwMaximumBrightness = 0;
            bRet = GetMonitorBrightness(((LPPHYSICAL_MONITOR)m_pAryPhysicalMonitor)[0].hPhysicalMonitor, &dwMinimumBrightness, &m_dwCurrentBrightness, &dwMaximumBrightness);
            if (!bRet) { strFunc = _T("GetMonitorBrightness");break; }

        } while (0);

        if (!bRet)
        {
            TCHAR* pStrError = GetErrorString();
            MessageBox(m_hWnd, pStrError, strFunc.c_str()/* _T("控制屏幕亮度失败")*/, MB_OK);
        }
    }
}

void CKeepAwake::RestoreMonitorBrightness()
{
    if (m_pAryPhysicalMonitor != nullptr)
    {
        BOOL bRet = SetMonitorBrightness(((LPPHYSICAL_MONITOR)m_pAryPhysicalMonitor)[0].hPhysicalMonitor, m_dwCurrentBrightness);
        if (!bRet)
        {
            TCHAR* pStrError = GetErrorString();
            MessageBox(m_hWnd, pStrError, _T("控制屏幕亮度失败"), MB_OK);
        }
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
    if (NULL == g_oldWndProc)
    {
        g_oldWndProc = (WNDPROC)::SetWindowLongPtr(m_hWnd, GWLP_WNDPROC, (LONG_PTR)MyWindowProc);
    }

    // 禁用屏幕保护
    DisableScrSaver();

    // 保存当前屏幕亮度
    SaveMonitorBrightness();

    { //  销毁句柄
        // DestroyPhysicalMonitors(cPhysicalMonitors, pPhysicalMonitors);
        // delete pPhysicalMonitors;
    }
}

TCHAR* CKeepAwake::GetErrorString()
{
    DWORD dwError = GetLastError();
    TCHAR* buffer = NULL;
    ::FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM, NULL, dwError, 0, (LPTSTR)&buffer, 0, NULL);
    return buffer;
}
