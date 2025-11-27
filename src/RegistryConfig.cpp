#include <windows.h>
#include "RegistryConfig.h"
#include "tchar.h"
#include "RegistryOperation.h"


#define REGISTER_ROOT_PATH (_T("SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run"))
#define REGISTER_KEY (_T("ForbidShutDown"))

#define REGISTER_SOFTWARE_PATH (_T("SOFTWARE\\ForbidShutDown"))
#define REGISTER_BLOCK_WINDOWS_UPDATE_VALUE (_T("BlockWindowsUpdate"))
#define REGISTER_KEEP_BALANCE_POWER_VALUE (_T("KeepBalancePower"))
#define REGISTER_SHUTDOWN_SYSTEM_HOURS_VALUE (_T("ShutDownSystemHours"))
#define REGISTER_SHUTDOWN_SYSTEM_MINUTES_VALUE (_T("ShutDownSystemMinutes"))


bool RegIsBootUp()
{
	bool bExist = false;

	// 找到系统的启动项  
	HKEY hKey;
	if (RegOpenKeyEx(HKEY_CURRENT_USER, REGISTER_ROOT_PATH, 0, KEY_ALL_ACCESS, &hKey) == ERROR_SUCCESS)
	{
		// 得到本程序自身的全路径
		TCHAR strExeFullDir[MAX_PATH];
		GetModuleFileName(NULL, strExeFullDir, MAX_PATH);

		// 判断注册表项是否已经存在
		TCHAR strDir[MAX_PATH] = {};
		DWORD nLength = MAX_PATH;
		long result = RegGetValue(hKey, nullptr, REGISTER_KEY, RRF_RT_REG_SZ, 0, strDir, &nLength);

		if (result == ERROR_SUCCESS)
		{
			bExist = true;

			// 如果路径不一致则修正
			if (_tcscmp(strExeFullDir, strDir) != 0)
			{
				LSTATUS ret = RegSetValueEx(hKey, REGISTER_KEY, 0, REG_SZ, (LPBYTE)strExeFullDir, (lstrlen(strExeFullDir) + 1) * sizeof(TCHAR));
				if (ERROR_SUCCESS != ret)
				{
					bExist = false;
				} 
				// 关闭注册表
				RegCloseKey(hKey);
				hKey = NULL;
			}
		}
	}

	return bExist;
}

// 设置当前程序开机自启动
void RegSetBootUp(bool bSet)
{
	// 找到系统的启动项 
	HKEY hKey;
	if (RegOpenKeyEx(HKEY_CURRENT_USER, REGISTER_ROOT_PATH, 0, KEY_ALL_ACCESS, &hKey) == ERROR_SUCCESS) // 打开启动项       
	{
		if (bSet)
		{
			// 得到本程序自身的全路径
			TCHAR strExeFullDir[MAX_PATH];
			GetModuleFileName(NULL, strExeFullDir, MAX_PATH);

			// 判断注册表项是否已经存在
			TCHAR strDir[MAX_PATH] = {};
			DWORD nLength = MAX_PATH;
			long result = RegGetValue(hKey, nullptr, REGISTER_KEY, RRF_RT_REG_SZ, 0, strDir, &nLength);

			// 已经存在
			if (result != ERROR_SUCCESS || _tcscmp(strExeFullDir, strDir) != 0)
			{
				// 添加一个子Key, 并设置值，REGISTER_KEY 是应用程序名字（不加后缀.exe） 
				RegSetValueEx(hKey, REGISTER_KEY, 0, REG_SZ, (LPBYTE)strExeFullDir, (lstrlen(strExeFullDir) + 1) * sizeof(TCHAR));

				// 关闭注册表
				RegCloseKey(hKey);
				hKey = NULL;
			}
		}
		else
		{
			// 删除值
			RegDeleteValue(hKey, REGISTER_KEY);

			// 关闭注册表
			RegCloseKey(hKey);
			hKey = NULL;
		}
	}
}



bool RegIsBlockWindowsUpdate()
{
//#ifdef _WIN64
//    REGSAM regSam = KEY_WOW64_64KEY;
//#else
//    REGSAM regSam = KEY_WOW64_32KEY;
//#endif

    DWORD dwData = 1;
    INT64 ret = GetRegDwordValue(HKEY_CURRENT_USER, 0, REGISTER_SOFTWARE_PATH, REGISTER_BLOCK_WINDOWS_UPDATE_VALUE, &dwData);
    if (ret == ERROR_SUCCESS && dwData != 0)
    {
        return true;
    }
    return false;
}

void RegSetBlockWindowsUpdate(bool block)
{
    DWORD dwData = block ? 1 : 0;
    SetRegDwordValue(HKEY_CURRENT_USER, 0, REGISTER_SOFTWARE_PATH, REGISTER_BLOCK_WINDOWS_UPDATE_VALUE, dwData);
}



bool RegIsKeepBalancePower()
{
	DWORD dwData = 0;
	INT64 ret = GetRegDwordValue(HKEY_CURRENT_USER, 0, REGISTER_SOFTWARE_PATH, REGISTER_KEEP_BALANCE_POWER_VALUE, &dwData);
	if (ret == ERROR_SUCCESS && dwData != 0)
	{
		return true;
	}
	return false;
}

void RegSetKeepBalancePower(bool keep)
{
	DWORD dwData = keep ? 1 : 0;
	SetRegDwordValue(HKEY_CURRENT_USER, 0, REGISTER_SOFTWARE_PATH, REGISTER_KEEP_BALANCE_POWER_VALUE, dwData);
}




int RegGetShutdownSystemHours()
{
	DWORD dwData = 0;
	GetRegDwordValue(HKEY_CURRENT_USER, 0, REGISTER_SOFTWARE_PATH, REGISTER_SHUTDOWN_SYSTEM_HOURS_VALUE, &dwData);
	return dwData;
}

void RegSetShutdownSystemHours(int hours)
{
	DWORD dwData = hours;
	SetRegDwordValue(HKEY_CURRENT_USER, 0, REGISTER_SOFTWARE_PATH, REGISTER_SHUTDOWN_SYSTEM_HOURS_VALUE, dwData);
}

int RegGetShutdownSystemMinutes()
{
	DWORD dwData = 0;
	GetRegDwordValue(HKEY_CURRENT_USER, 0, REGISTER_SOFTWARE_PATH, REGISTER_SHUTDOWN_SYSTEM_MINUTES_VALUE, &dwData);
	return dwData;
}

void RegSetShutdownSystemMinutes(int minutes)
{
	DWORD dwData = minutes;
	SetRegDwordValue(HKEY_CURRENT_USER, 0, REGISTER_SOFTWARE_PATH, REGISTER_SHUTDOWN_SYSTEM_MINUTES_VALUE, dwData);
}
