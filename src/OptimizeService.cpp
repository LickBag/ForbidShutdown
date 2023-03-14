#include "OptimizeService.h"
#include <tchar.h>
#include "Common.h"
#include "RegistryOperation.h"

// �ƻ�����
#include <taskschd.h>
#include <Atlbase.h>
#include <comdef.h>
#pragma comment(lib, "taskschd.lib")


static bool g_bInited = false;
static bool g_bRunning = true;
static SC_HANDLE g_schSCManager = nullptr;
static SC_HANDLE g_schService = nullptr;
static HANDLE g_event = nullptr;


// MSDN:�������ڻص������е��÷�����غ���
VOID CALLBACK NotifyWAUAUStatusChanged(_In_ PVOID pParameter)
{
    return;
}

DWORD WINAPI WatchWindowsUpdateThreadProc(PVOID)
{
    while (g_bRunning)
    {
        SERVICE_NOTIFY serviceNotify = { SERVICE_NOTIFY_STATUS_CHANGE, NotifyWAUAUStatusChanged };
        DWORD dwError = NotifyServiceStatusChange(g_schService, SERVICE_NOTIFY_RUNNING, &serviceNotify);
        if (dwError == ERROR_SUCCESS)
        {
            // �ȴ��¼���������APC Queue
            DWORD dw = WaitForSingleObjectEx(g_event, INFINITE, TRUE);

            // Event����
            if (dw == WAIT_OBJECT_0)
            {
                return 0;
            }
            // APC����
            else if (dw == WAIT_IO_COMPLETION)
            {
                // ���Խ������޸�Ϊ������, ����ControlService��ʧ��
                DWORD dwBytesNeeded;
                QueryServiceConfig(g_schService, nullptr, 0, &dwBytesNeeded);
                LPQUERY_SERVICE_CONFIG lpsc = (LPQUERY_SERVICE_CONFIG)LocalAlloc(LPTR, dwBytesNeeded);
                if (lpsc != nullptr)
                {
                    if (QueryServiceConfig(g_schService, lpsc, dwBytesNeeded, &dwBytesNeeded))
                    {
                        if (lpsc->dwStartType != SERVICE_AUTO_START)
                        {
                            ChangeServiceConfig(g_schService, SERVICE_NO_CHANGE, SERVICE_AUTO_START,
                                SERVICE_NO_CHANGE, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
                        }
                    }

                    LocalFree(lpsc);
                    lpsc = nullptr;
                }

                // ��Ϊֹͣ
                SERVICE_STATUS ss = { 0 };
                ControlService(g_schService, SERVICE_CONTROL_STOP, &ss);

                // �Զ�����ʧ��ʱ���ٳ���
                SC_ACTION actions[3];
                ZeroMemory(&actions, sizeof(actions));

                actions[0].Type = SC_ACTION_NONE;
                actions[0].Delay = INFINITE;
                actions[1].Type = SC_ACTION_NONE;
                actions[1].Delay = INFINITE;
                actions[2].Type = SC_ACTION_NONE;
                actions[2].Delay = INFINITE;
                
                SERVICE_FAILURE_ACTIONS failAction = { 0 };
                failAction.dwResetPeriod = INFINITE;
                failAction.cActions = ARRAY_SIZE(actions);
                failAction.lpsaActions = actions;
                ChangeServiceConfig2(g_schService, SERVICE_CONFIG_FAILURE_ACTIONS, &failAction);

                // WARNING! ���ǲ�Ҫ������, �û������Լ��ڲ����пͻ��˵�ʱ�����ϵͳ
            }
        }

        // ����ʱ��δ����ʱ, windows���Զ�ǿ�Ƹ���, ��Ƶ������windows update. ��ֹ�ͻ��˽��ú��ֱ�ϵͳ����, һֱѭ�����½���CPU���, Sleepһ��(ʵ����ֻ�����������ζ���)
        Sleep(1000);
    }

    return 0;
}

VOID DisableAutoUpdate()
{
    LPCTSTR lpSubKey = _T("SOFTWARE\\Policies\\Microsoft\\Windows\\WindowsUpdate\\AU");

    // �����Զ�����. 
    // ���ú�, �����2��û��ʵ������: https://learn.microsoft.com/en-us/windows/deployment/update/waas-wu-settings#configure-automatic-updates
    SetRegDwordValue(HKEY_LOCAL_MACHINE, KEY_WOW64_64KEY, lpSubKey, _T("NoAutoUpdate"), 0x1);

    // ���Զ����ظ���
    SetRegDwordValue(HKEY_LOCAL_MACHINE, KEY_WOW64_64KEY, lpSubKey, _T("AUOptions"), 0x1);

    // ���������û���½ʱ�Զ�����
    SetRegDwordValue(HKEY_LOCAL_MACHINE, KEY_WOW64_64KEY, lpSubKey, _T("NoAutoRebootWithLoggedOnUsers"), 0x1);
}

INT64 WatchWindowsUpdateService()
{
    g_schSCManager = OpenSCManager(nullptr, nullptr, SC_MANAGER_ALL_ACCESS);
    if (g_schSCManager == nullptr)
    {
        return GetLastError();
    }

    g_schService = OpenService(g_schSCManager, _T("wuauserv"), SERVICE_ALL_ACCESS);
    if (g_schService == nullptr)
    {
        SAFE_CLOSE_SERVICE_HANDLE(g_schSCManager);
        return GetLastError();
    }

    g_event = CreateEvent(nullptr, TRUE, FALSE, TEXT("AutobioWuauservEvent"));

    g_bRunning = true;
    DWORD dwThreadID = 0;
    HANDLE hThread = CreateThread(nullptr, 0, WatchWindowsUpdateThreadProc, nullptr, 0, &dwThreadID);
    SAFE_CLOSE_HANDLE(hThread);

    return ERROR_SUCCESS;
}

VOID DisableWindowsUpdateSchedle()
{
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(hr))
    {
        return;
    }

    ITaskService *m_lpITS = nullptr;
    ITaskFolder *m_lpRootFolder = nullptr;
    IRegisteredTask *pRegisteredTask = nullptr;

    do
    {
        hr = ::CoCreateInstance(CLSID_TaskScheduler, nullptr, CLSCTX_INPROC_SERVER, IID_ITaskService, (LPVOID *)(&m_lpITS));
        CHECK_TRUE_BREAK(FAILED(hr));

        hr = m_lpITS->Connect(_variant_t(), _variant_t(), _variant_t(), _variant_t());
        CHECK_TRUE_BREAK(FAILED(hr));

        hr = m_lpITS->GetFolder(_bstr_t("\\Microsoft\\Windows\\WindowsUpdate"), &m_lpRootFolder);
        CHECK_TRUE_BREAK((FAILED(hr) || m_lpRootFolder == nullptr));

        CComVariant variantTaskName(NULL);
        variantTaskName = "Scheduled Start";

        hr = m_lpRootFolder->GetTask(variantTaskName.bstrVal, &pRegisteredTask);
        CHECK_TRUE_BREAK(FAILED(hr) || (pRegisteredTask == nullptr));

        CComVariant variantEnable(NULL);
        hr = pRegisteredTask->get_Enabled(&variantEnable.boolVal);
        CHECK_TRUE_BREAK(FAILED(hr) || (variantEnable.boolVal == VARIANT_FALSE))

        variantEnable = VARIANT_FALSE;
        pRegisteredTask->put_Enabled(variantEnable.boolVal);

    } while (false);

    if (pRegisteredTask)
    {
        pRegisteredTask->Release();
        pRegisteredTask = nullptr;
    }
    if (m_lpRootFolder)
    {
        m_lpRootFolder->Release();
        m_lpRootFolder = nullptr;
    }
    if (m_lpITS)
    {
        m_lpITS->Release();
        m_lpITS = nullptr;
    }

    CoUninitialize();
}


// �ǳ�ռ�û�еӲ�̵Ĵ���������  _T("WSearch")
INT64 DisableService(LPCTSTR serviceName, DWORD startType)
{
    SC_HANDLE hSCManager = OpenSCManager(nullptr, nullptr, SC_MANAGER_ALL_ACCESS);
    if (hSCManager == nullptr)
    {
        return GetLastError();
    }

    SC_HANDLE hService = OpenService(hSCManager, serviceName, SERVICE_ALL_ACCESS);
    if (hService == nullptr)
    {
        SAFE_CLOSE_SERVICE_HANDLE(hSCManager);
        return GetLastError();
    }

    // ���ж��Ƿ�ɹ�
    SERVICE_STATUS ss = { 0 };
    ControlService(hService, SERVICE_CONTROL_STOP, &ss);

    DWORD dwBytesNeeded = 0;
    QueryServiceConfig(hService, nullptr, 0, &dwBytesNeeded);
    if (dwBytesNeeded > 0)
    {
        auto lpsc = (LPQUERY_SERVICE_CONFIG)LocalAlloc(LPTR, dwBytesNeeded);
        if (lpsc != nullptr)
        {
            if (QueryServiceConfig(hService, lpsc, dwBytesNeeded, &dwBytesNeeded))
            {
                if (lpsc->dwStartType != startType)
                {
                    ChangeServiceConfig(hService, SERVICE_NO_CHANGE, SERVICE_DEMAND_START,
                        SERVICE_NO_CHANGE, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
                }
            }

            LocalFree(lpsc);
            lpsc = nullptr;
        }
    }

    // ���ж��Ƿ�ɹ�
    ControlService(hService, SERVICE_CONTROL_STOP, &ss);
    
    SAFE_CLOSE_SERVICE_HANDLE(hService);
    SAFE_CLOSE_SERVICE_HANDLE(hSCManager);

    return ERROR_SUCCESS;
}


INT64 OptimizeService()
{
    INT64 dwRet = ERROR_SUCCESS;
    INT64 dwTemp = ERROR_SUCCESS;
    if (!g_bInited)
    {
        // Windows Search �ǳ�ռ�û�еӲ�̵Ĵ���������  
        DisableService(_T("WSearch"), SERVICE_DISABLED);

        // ����Ϊwindows update���
        // �ر��Զ�����
        DisableAutoUpdate();

        // ����windows update����
        DisableWindowsUpdateSchedle();

        // Windows Update
        DisableService(_T("Wuauserv"), SERVICE_DISABLED);

        // Windows Update Medic Service
        DisableService(_T("WaaSMedicSvc"), SERVICE_DISABLED);

        // Delivery Optimization �����Ż�
        DisableService(_T("dosvc"), SERVICE_DISABLED);

        // Background Intelligent Transfer Service
        DisableService(_T("BITS"), SERVICE_DISABLED);

        // Update Orchestrator Service �������ֹͣ���, ��windows update���ô���ʱ�÷��������, ��ֹͣ��ʧ��
        DisableService(_T("UsoSvc"), SERVICE_DEMAND_START);

        dwTemp = WatchWindowsUpdateService();
        IF_TRUE_EXECUTE(dwTemp != ERROR_SUCCESS, dwRet = dwTemp);
        
        g_bInited = true;
    }

    return dwRet;
}


VOID CancelOptimizeService()
{
    if (g_bInited)
    {
        g_bRunning = false;
        SetEvent(g_event);
        SAFE_CLOSE_HANDLE(g_event);
        SAFE_CLOSE_SERVICE_HANDLE(g_schService);
        SAFE_CLOSE_SERVICE_HANDLE(g_schSCManager);

        g_bInited = false;
    }
}
