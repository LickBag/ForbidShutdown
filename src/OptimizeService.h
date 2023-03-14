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

// 建议不要在主程序中调用下面的函数, 因为它会创建线程, 涉及APC调用
// https://docs.microsoft.com/zh-cn/windows/win32/api/winsvc/nf-winsvc-notifyservicestatuschangea
INT64 OptimizeService();

VOID CancelOptimizeService();

//#ifdef __cplusplus
//}
//#endif
