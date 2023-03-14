#include <windows.h>
#include "RegistryConfig.h"
#include "tchar.h"
#include "RegistryOperation.h"



#define REGISTER_ROOT_PATH (_T("SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run"))
#define REGISTER_KEY (_T("ForbidShutDown"))

#define REGISTER_SOFTWARE_PATH (_T("SOFTWARE\\ForbidShutDown"))
#define REGISTER_BLOCK_WINDOWS_UPDATE_VALUE (_T("BlockWindowsUpdate"))

bool IsBootUp()
{
	bool bExist = false;

	// �ҵ�ϵͳ��������  
	HKEY hKey;
	if (RegOpenKeyEx(HKEY_CURRENT_USER, REGISTER_ROOT_PATH, 0, KEY_ALL_ACCESS, &hKey) == ERROR_SUCCESS)
	{
		// �õ������������ȫ·��
		TCHAR strExeFullDir[MAX_PATH];
		GetModuleFileName(NULL, strExeFullDir, MAX_PATH);

		// �ж�ע������Ƿ��Ѿ�����
		TCHAR strDir[MAX_PATH] = {};
		DWORD nLength = MAX_PATH;
		long result = RegGetValue(hKey, nullptr, REGISTER_KEY, RRF_RT_REG_SZ, 0, strDir, &nLength);

		if (result == ERROR_SUCCESS)
		{
			bExist = true;

			// ���·����һ��������
			if (_tcscmp(strExeFullDir, strDir) != 0)
			{
				LSTATUS ret = RegSetValueEx(hKey, REGISTER_KEY, 0, REG_SZ, (LPBYTE)strExeFullDir, (lstrlen(strExeFullDir) + 1) * sizeof(TCHAR));
				if (ERROR_SUCCESS != ret)
				{
					bExist = false;
				} 
				// �ر�ע���
				RegCloseKey(hKey);
				hKey = NULL;
			}
		}
	}

	return bExist;
}


// ���õ�ǰ���򿪻�������
void SetBootUp(bool bSet)
{
	// �ҵ�ϵͳ�������� 
	HKEY hKey;
	if (RegOpenKeyEx(HKEY_CURRENT_USER, REGISTER_ROOT_PATH, 0, KEY_ALL_ACCESS, &hKey) == ERROR_SUCCESS) // ��������       
	{
		if (bSet)
		{
			// �õ������������ȫ·��
			TCHAR strExeFullDir[MAX_PATH];
			GetModuleFileName(NULL, strExeFullDir, MAX_PATH);

			// �ж�ע������Ƿ��Ѿ�����
			TCHAR strDir[MAX_PATH] = {};
			DWORD nLength = MAX_PATH;
			long result = RegGetValue(hKey, nullptr, REGISTER_KEY, RRF_RT_REG_SZ, 0, strDir, &nLength);

			// �Ѿ�����
			if (result != ERROR_SUCCESS || _tcscmp(strExeFullDir, strDir) != 0)
			{
				// ���һ����Key, ������ֵ��REGISTER_KEY ��Ӧ�ó������֣����Ӻ�׺.exe�� 
				RegSetValueEx(hKey, REGISTER_KEY, 0, REG_SZ, (LPBYTE)strExeFullDir, (lstrlen(strExeFullDir) + 1) * sizeof(TCHAR));

				// �ر�ע���
				RegCloseKey(hKey);
				hKey = NULL;
			}
		}
		else
		{
			// ɾ��ֵ
			RegDeleteValue(hKey, REGISTER_KEY);

			// �ر�ע���
			RegCloseKey(hKey);
			hKey = NULL;
		}
	}
}




bool IsBlockWindowsUpdate()
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


void SetIsBlockWindowsUpdate(bool block)
{
    DWORD dwData = block ? 1 : 0;
    SetRegDwordValue(HKEY_CURRENT_USER, 0, REGISTER_SOFTWARE_PATH, REGISTER_BLOCK_WINDOWS_UPDATE_VALUE, dwData);
}