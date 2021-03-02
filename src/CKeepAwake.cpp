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
            return 0; // ����0������������
            break;
        }

        case WM_POWERBROADCAST:// ��ʾ���ȹ㲥
        {
            if (wParam == PBT_POWERSETTINGCHANGE)
            {
                POWERBROADCAST_SETTING* lvpsSetting = (POWERBROADCAST_SETTING*)lParam;
                DWORD lvStatus = *(DWORD*)(lvpsSetting->Data);
                if (lvStatus == 0) // 0�ر�, 1��
                {
                    // ��������, ����˵û�á�
                    SendMessage(hWnd, WM_SYSCOMMAND, SC_MONITORPOWER, -1); // ����ʾ��
                    CKeepAwake::GetInstance()->RestoreMonitorBrightness();
                    return 0;
                }
                //if (lvStatus != 0) //0�ر�, ��
                //{
                //    LRESULT ret = ::SendMessage(HWND_BROADCAST, WM_SYSCOMMAND, SC_MONITORPOWER, 2);//�ر���ʾ�� 
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

// ������Ļ����
void CKeepAwake::DisableScrSaver()
{
    SystemParametersInfo(SPI_GETSCREENSAVEACTIVE, 0, &m_bScrActive, 0);
    if (m_bScrActive)
    {
        SystemParametersInfo(SPI_SETSCREENSAVEACTIVE, FALSE, nullptr, 0);
    }
}

// �ָ���Ļ����
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

    // ��ֹ����
    m_oldState = SetThreadExecutionState(ES_CONTINUOUS | ES_SYSTEM_REQUIRED | ES_DISPLAY_REQUIRED | ES_AWAYMODE_REQUIRED);

    // ��ֹ�ػ���Ϣ
    m_bRet = ShutdownBlockReasonCreate(m_hWnd, L"���ػ��в�. ( �b- �b)�ĥ�");

    // ���ô˳����ڽ����Ựʱ���ڵ�һλ
    SetProcessShutdownParameters(0x4FF, SHUTDOWN_NORETRY);

    // ע����Ϣ
    HPOWERNOTIFY m_lvhpNotify = RegisterPowerSettingNotification(m_hWnd
        , &GUID_CONSOLE_DISPLAY_STATE, DEVICE_NOTIFY_WINDOW_HANDLE);

    // ȡ��ԭ���Ĵ��ڹ���
    if (nullptr == g_oldWndProc)
    {
        g_oldWndProc = (WNDPROC)::SetWindowLongPtr(m_hWnd, GWLP_WNDPROC, (LONG_PTR)MyWindowProc);
    }

    // ������Ļ����
    DisableScrSaver();

    // ���浱ǰ��Ļ����
    SaveMonitorBrightness();
}

TCHAR* CKeepAwake::GetErrorString()
{
    DWORD dwError = GetLastError();
    TCHAR* buffer = nullptr;
    ::FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM, NULL, dwError, 0, (LPTSTR)&buffer, 0, NULL);
    return buffer;
}
