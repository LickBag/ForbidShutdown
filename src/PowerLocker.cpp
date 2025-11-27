#include "RegistryOperation.h"
#include <tchar.h>
#include <shlwapi.h>
#include "Common.h"
#include <powrprof.h>

#pragma comment(lib, "Powrprof.lib")


static HANDLE g_hNotificationRegister = nullptr;


ULONG CALLBACK HandlePowerNotifications(PVOID Context, ULONG Type, PVOID Setting)
{
    PowerSetActiveScheme(nullptr, &GUID_TYPICAL_POWER_SAVINGS);

    // PPOWERBROADCAST_SETTING PowerSettings = (PPOWERBROADCAST_SETTING)Setting;
    //if (Type == PBT_POWERSETTINGCHANGE && PowerSettings->PowerSetting == GUID_CONSOLE_DISPLAY_STATE)
    //{
    //    switch (*PowerSettings->Data)
    //    {
    //    case 0x0:
    //    case 0x1:
    //        break;

    //    case 0x2:
    //    {
    //        //USER PAYLOAD HERE
    //        break;
    //    }

    //    default:
    //        break;
    //    }
    //}

    return ERROR_SUCCESS;
}


// 注册电源设置的变更通知
void LockBalancePower(bool bLock)
{
    if (bLock)
    {
        if (g_hNotificationRegister == nullptr)
        {
            DEVICE_NOTIFY_SUBSCRIBE_PARAMETERS NotificationsParameters;
            NotificationsParameters.Callback = HandlePowerNotifications;
            NotificationsParameters.Context = NULL;

            PowerSettingRegisterNotification(&GUID_ACTIVE_POWERSCHEME, DEVICE_NOTIFY_CALLBACK, (HANDLE)&NotificationsParameters, &g_hNotificationRegister);
        }
    }
    else
    {
        UnregisterPowerSettingNotification(g_hNotificationRegister);
        g_hNotificationRegister = nullptr;
    }
}


bool IsLockBalancePower()
{
    return g_hNotificationRegister != nullptr;
}