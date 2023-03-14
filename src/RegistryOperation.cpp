#include "RegistryOperation.h"
#include <tchar.h>
#include <shlwapi.h>
#include "Common.h"


INT64 DelRegKey(_In_ HKEY hBaseKey, _In_ REGSAM regSam, _In_ LPCTSTR lpSubKey)
{
    if (lpSubKey == nullptr || _tcslen(lpSubKey) <= 0 || regSam == 0)
    {
        return ERROR_INVALID_PARAMETER;
    }

    HKEY hTargetKey;
    LSTATUS status = RegOpenKeyEx(hBaseKey, lpSubKey, REG_OPTION_RESERVED, KEY_READ | KEY_WRITE | regSam, &hTargetKey);
    if (status == ERROR_FILE_NOT_FOUND)
    {
        return ERROR_SUCCESS;
    }
    else
    {
        RegDeleteTree(hTargetKey, nullptr);
        status = RegDeleteKeyEx(hBaseKey, lpSubKey, regSam, 0);
        RegFlushKey(hTargetKey);
        SAFE_CLOSE_HANDLE(hTargetKey);
    }

    return status;
}


INT64 GetRegDwordValue(_In_ HKEY hBaseKey, _In_ REGSAM regSam, _In_ LPCTSTR lpSubKey, _In_ LPCTSTR lpValue, _Inout_ LPDWORD lpData)
{
    if (lpData == nullptr || lpValue == nullptr)
    {
        return ERROR_INVALID_PARAMETER;
    }

    HKEY hTargetKey;
    LSTATUS status = RegOpenKeyEx(hBaseKey, lpSubKey, REG_OPTION_RESERVED, KEY_READ | KEY_WRITE | regSam, &hTargetKey);
    if (status != ERROR_SUCCESS || hTargetKey == INVALID_HANDLE_VALUE)
    {
        return status;
    }

    // value-data
    DWORD dwOldData = 0;
    DWORD dwOldType = 0;
    DWORD dwOldSize = sizeof(dwOldData);
    status = RegQueryValueEx(hTargetKey, lpValue, nullptr, &dwOldType, (LPBYTE)&dwOldData, &dwOldSize);

    // 如果失败或者不是一个整数, 将入参写入到注册表
    DWORD dwZero = *lpData;
    if (status != ERROR_SUCCESS || dwOldType != REG_DWORD)
    {
        status = RegSetValueEx(hTargetKey, lpValue, 0, REG_DWORD, (BYTE*)&dwZero, sizeof(dwZero));
        RegFlushKey(hTargetKey);
    }
    else
    {
        *lpData = dwOldData;
    }

    SAFE_CLOSE_HANDLE(hTargetKey);

    return status;
}

INT64 SetRegDwordValue(_In_ HKEY hBaseKey, _In_ REGSAM regSam, _In_ LPCTSTR lpSubKey, _In_ LPCTSTR lpValue, _In_ DWORD dwSetData)
{
    HKEY hTargetKey;
    LSTATUS status = RegOpenKeyEx(hBaseKey, lpSubKey, REG_OPTION_RESERVED, KEY_READ | KEY_WRITE | regSam, &hTargetKey);
    if (status != ERROR_SUCCESS)
    {
        DWORD dwDisposition;
        status = RegCreateKeyEx(hBaseKey, lpSubKey, 0, nullptr, REG_OPTION_NON_VOLATILE, KEY_READ | KEY_WRITE | regSam,
            nullptr, &hTargetKey, &dwDisposition);
    }

    if (status != ERROR_SUCCESS)
    {
        return status;
    }

    if (lpValue == nullptr)
    {
        RegFlushKey(hTargetKey);
        SAFE_CLOSE_HANDLE(hTargetKey);
        return ERROR_SUCCESS;
    }

    // д value-data
    DWORD dwOldData = 0;
    DWORD dwOldType = 0;
    DWORD dwOldSize = sizeof(dwOldData);
    status = RegQueryValueEx(hTargetKey, lpValue, nullptr, &dwOldType, (LPBYTE)&dwOldData, &dwOldSize);
    if (status != ERROR_SUCCESS || dwOldType != REG_DWORD || dwOldData != dwSetData)
    {
        status = RegSetValueEx(hTargetKey, lpValue, 0, REG_DWORD, (BYTE*)&dwSetData, sizeof(dwSetData));
    }

    RegFlushKey(hTargetKey);
    SAFE_CLOSE_HANDLE(hTargetKey);

    return status;
}


INT64 SetRegStringValue(_In_ HKEY hBaseKey, _In_ REGSAM regSam, _In_ LPCTSTR lpSubKey, _In_ LPCTSTR lpValue, _In_ LPCTSTR lpSetData)
{
    HKEY hTargetKey;
    LSTATUS status = RegOpenKeyEx(hBaseKey, lpSubKey, REG_OPTION_RESERVED, KEY_READ | KEY_WRITE | regSam, &hTargetKey);
    if (status != ERROR_SUCCESS)
    {
        DWORD dwDisposition;
        status = RegCreateKeyEx(hBaseKey, lpSubKey, 0, nullptr, REG_OPTION_NON_VOLATILE, KEY_READ | KEY_WRITE | regSam,
            nullptr, &hTargetKey, &dwDisposition);
    }

    if (status != ERROR_SUCCESS)
    {
        return status;
    }

    if (lpValue == nullptr)
    {
        RegFlushKey(hTargetKey);
        SAFE_CLOSE_HANDLE(hTargetKey);
        return ERROR_SUCCESS;
    }

    // д value-data
    DWORD dwType = REG_SZ;
    TCHAR szOldData[MAX_PATH_NEW] = { 0 };
    DWORD dwOldDataSize = ARRAY_SIZE(szOldData);
    status = RegQueryValueEx(hTargetKey, lpValue, 0, &dwType, (LPBYTE)szOldData, &dwOldDataSize);
    if (status != ERROR_SUCCESS || dwType != REG_SZ || StrCmp(szOldData, lpSetData) != 0)
    {
        status = RegSetValueEx(hTargetKey, lpValue, 0, REG_SZ, (LPBYTE)lpSetData, (DWORD)(_tcslen(lpSetData) + 1) * sizeof(TCHAR));
    }

    RegFlushKey(hTargetKey);
    SAFE_CLOSE_HANDLE(hTargetKey);

    return status;
}

