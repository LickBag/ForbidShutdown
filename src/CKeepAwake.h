#pragma once
#include "header.h"

class CKeepAwake
{

public:
    static CKeepAwake* GetInstance();
    static void DelInstance();
    void Init(HWND hWnd);

	void DisableScrSaver();
	void RestoreScrSaver();
    void SaveMonitorBrightness();
    void RestoreMonitorBrightness();
    

private:
    TCHAR* GetErrorString();
    CKeepAwake();
    ~CKeepAwake();

private:
    static CKeepAwake* m_pInstance;
	EXECUTION_STATE m_oldState = NULL;
	HWND			m_hWnd = NULL;
	BOOL			m_bRet = FALSE;
	BOOL			m_bScrActive = FALSE;
	HPOWERNOTIFY    m_lvhpNotify = NULL;
    DWORD           m_dwCurrentBrightness = 50;
    VOID*           m_pAryPhysicalMonitor = nullptr;
};
