#pragma once

#include <windows.h>

extern HINSTANCE g_hInst;
extern long g_moduleLocks;

void ModuleAddRef();
void ModuleRelease();
HRESULT GetModulePath(wchar_t* path, DWORD pathChars);
