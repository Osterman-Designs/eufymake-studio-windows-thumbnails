#include <initguid.h>

#include "ClassFactory.h"
#include "Guids.h"
#include "Module.h"

#include <new>
#include <strsafe.h>

HINSTANCE g_hInst = nullptr;
long g_moduleLocks = 0;

void ModuleAddRef() {
    InterlockedIncrement(&g_moduleLocks);
}

void ModuleRelease() {
    InterlockedDecrement(&g_moduleLocks);
}

HRESULT GetModulePath(wchar_t* path, DWORD pathChars) {
    const DWORD copied = GetModuleFileNameW(g_hInst, path, pathChars);
    if (copied == 0 || copied >= pathChars) {
        return E_FAIL;
    }
    return S_OK;
}

BOOL APIENTRY DllMain(HINSTANCE instance, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        g_hInst = instance;
        DisableThreadLibraryCalls(instance);
    }
    return TRUE;
}

STDAPI DllCanUnloadNow() {
    return g_moduleLocks == 0 ? S_OK : S_FALSE;
}

STDAPI DllGetClassObject(REFCLSID clsid, REFIID riid, void** ppv) {
    if (clsid != CLSID_EmpfThumbnailProvider && clsid != CLSID_EmpfPreviewHandler) {
        return CLASS_E_CLASSNOTAVAILABLE;
    }
    EmpfClassFactory* factory = new (std::nothrow) EmpfClassFactory(clsid);
    if (!factory) {
        return E_OUTOFMEMORY;
    }
    const HRESULT hr = factory->QueryInterface(riid, ppv);
    factory->Release();
    return hr;
}
