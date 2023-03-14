#include "OptimizeService.h"
#include <tchar.h>
#include "Common.h"
#include "RegistryOperation.h"

// 计划任务
#include <taskschd.h>
#include <Atlbase.h>
#include <comdef.h>
#pragma comment(lib, "taskschd.lib")


static bool g_bInited = false;
static bool g_bRunning = true;
static SC_HANDLE g_schSCManager = nullptr;
static SC_HANDLE g_schService = nullptr;
static HANDLE g_event = nullptr;


// MSDN:不建议在回调函数中调用服务相关函数
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
            // 等待事件触发或者APC Queue
            DWORD dw = WaitForSingleObjectEx(g_event, INFINITE, TRUE);

            // Event触发
            if (dw == WAIT_OBJECT_0)
            {
                return 0;
            }
            // APC触发
            else if (dw == WAIT_IO_COMPLETION)
            {
                // 尝试将服务修改为自启动, 否则ControlService会失败
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

                // 改为停止
                SERVICE_STATUS ss = { 0 };
                ControlService(g_schService, SERVICE_CONTROL_STOP, &ss);

                // 自动更新失败时不再尝试
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

                // WARNING! 还是不要禁用了, 用户可以自己在不运行客户端的时候更新系统
            }
        }

        // 当长时间未更新时, windows会自动强制更新, 会频繁启用windows update. 防止客户端禁用后又被系统启用, 一直循环导致进程CPU飙升, Sleep一下(实际上只会自启动几次而已)
        Sleep(1000);
    }

    return 0;
}

VOID DisableAutoUpdate()
{
    LPCTSTR lpSubKey = _T("SOFTWARE\\Policies\\Microsoft\\Windows\\WindowsUpdate\\AU");

    // 禁用自动更新. 
    // 禁用后, 后面的2个没有实际作用: https://learn.microsoft.com/en-us/windows/deployment/update/waas-wu-settings#configure-automatic-updates
    SetRegDwordValue(HKEY_LOCAL_MACHINE, KEY_WOW64_64KEY, lpSubKey, _T("NoAutoUpdate"), 0x1);

    // 不自动下载更新
    SetRegDwordValue(HKEY_LOCAL_MACHINE, KEY_WOW64_64KEY, lpSubKey, _T("AUOptions"), 0x1);

    // 不允许有用户登陆时自动重启
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


// 非常占用机械硬盘的磁盘利用率  _T("WSearch")
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

    // 不判断是否成功
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

    // 不判断是否成功
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
        // Windows Search 非常占用机械硬盘的磁盘利用率  
        DisableService(_T("WSearch"), SERVICE_DISABLED);

        // 以下为windows update相关
        // 关闭自动更新
        DisableAutoUpdate();

        // 禁用windows update任务
        DisableWindowsUpdateSchedle();

        // Windows Update
        DisableService(_T("Wuauserv"), SERVICE_DISABLED);

        // Windows Update Medic Service
        DisableService(_T("WaaSMedicSvc"), SERVICE_DISABLED);

        // Delivery Optimization 传递优化
        DisableService(_T("dosvc"), SERVICE_DISABLED);

        // Background Intelligent Transfer Service
        DisableService(_T("BITS"), SERVICE_DISABLED);

        // Update Orchestrator Service 必须最后停止这个, 打开windows update设置窗口时该服务会启动, 且停止会失败
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
