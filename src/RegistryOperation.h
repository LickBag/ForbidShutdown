#pragma once
#include <Windows.h>

INT64 DelRegKey(_In_ HKEY hBaseKey, _In_ REGSAM regSam, _In_ LPCTSTR lpSubKey);


INT64 GetRegDwordValue(_In_ HKEY hBaseKey, _In_ REGSAM regSam, _In_ LPCTSTR lpSubKey, _In_ LPCTSTR lpValue, _Out_ LPDWORD dwData);
INT64 SetRegDwordValue(_In_ HKEY hBaseKey, _In_ REGSAM regSam, _In_ LPCTSTR lpSubKey, _In_ LPCTSTR lpValue, _In_ DWORD dwSetData);


INT64 SetRegStringValue(_In_ HKEY hBaseKey, _In_ REGSAM regSam, _In_ LPCTSTR lpSubKey, _In_ LPCTSTR lpValue, _In_ LPCTSTR lpSetData);





