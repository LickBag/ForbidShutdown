#pragma once


// 开机自启动
bool RegIsBootUp();
void RegSetBootUp(bool bSet);


// 阻止windows update
bool RegIsBlockWindowsUpdate();
void RegSetBlockWindowsUpdate(bool block);


// balace saving
bool RegIsKeepBalancePower();
void RegSetKeepBalancePower(bool keep);


// 定时关机
int RegGetShutdownSystemHours();
void RegSetShutdownSystemHours(int hours);

int RegGetShutdownSystemMinutes();
void RegSetShutdownSystemMinutes(int minutes);
