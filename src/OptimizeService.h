#pragma once

#include <windows.h>

//#ifndef CRASH_DUMP_HELPER_EXPORTS
//#define CRASH_DUMP_HELPER_API  _declspec(dllimport)
//#else
//#define CRASH_DUMP_HELPER_API  _declspec(dllexport)
//#endif
//
//
//#ifdef __cplusplus 
//extern "C" {
//#endif

// ���鲻Ҫ���������е�������ĺ���, ��Ϊ���ᴴ���߳�, �漰APC����
// https://docs.microsoft.com/zh-cn/windows/win32/api/winsvc/nf-winsvc-notifyservicestatuschangea
INT64 OptimizeService();

VOID CancelOptimizeService();

//#ifdef __cplusplus
//}
//#endif
