#include "Guids.h"
#include "Module.h"

#include <windows.h>
#include <shlobj.h>
#include <stdlib.h>
#include <strsafe.h>

namespace {

HRESULT SetSz(HKEY root, const wchar_t* path, const wchar_t* name, const wchar_t* value) {
    HKEY key = nullptr;
    const LSTATUS status = RegCreateKeyExW(root, path, 0, nullptr, 0, KEY_SET_VALUE, nullptr, &key, nullptr);
    if (status != ERROR_SUCCESS) {
        return HRESULT_FROM_WIN32(status);
    }
    const HRESULT hr = HRESULT_FROM_WIN32(RegSetValueExW(
        key, name, 0, REG_SZ,
        reinterpret_cast<const BYTE*>(value),
        static_cast<DWORD>((wcslen(value) + 1) * sizeof(wchar_t))));
    RegCloseKey(key);
    return hr;
}

HRESULT DeleteKeyTree(HKEY root, const wchar_t* path) {
    const LSTATUS status = RegDeleteTreeW(root, path);
    if (status == ERROR_FILE_NOT_FOUND || status == ERROR_PATH_NOT_FOUND) {
        return S_OK;
    }
    return HRESULT_FROM_WIN32(status);
}

HRESULT RegisterHandler(const wchar_t* clsid, const wchar_t* title, const wchar_t* dllPath) {
    wchar_t clsidPath[128] = {};
    wchar_t inprocPath[160] = {};
    StringCchPrintfW(clsidPath, _countof(clsidPath), L"Software\\Classes\\CLSID\\%s", clsid);
    StringCchPrintfW(inprocPath, _countof(inprocPath), L"%s\\InprocServer32", clsidPath);

    HRESULT hr = SetSz(HKEY_CURRENT_USER, clsidPath, nullptr, title);
    if (SUCCEEDED(hr)) {
        hr = SetSz(HKEY_CURRENT_USER, clsidPath, L"AppID", kAppIdClsid);
    }
    if (SUCCEEDED(hr)) {
        hr = SetSz(HKEY_CURRENT_USER, inprocPath, nullptr, dllPath);
    }
    if (SUCCEEDED(hr)) {
        hr = SetSz(HKEY_CURRENT_USER, inprocPath, L"ThreadingModel", L"Apartment");
    }
    return hr;
}

HRESULT RegisterAssociation(const wchar_t* progId) {
    wchar_t thumbPath[256] = {};
    wchar_t previewPath[256] = {};
    StringCchPrintfW(thumbPath, _countof(thumbPath), L"Software\\Classes\\%s\\ShellEx\\%s", progId, kThumbnailHandlerShellEx);
    StringCchPrintfW(previewPath, _countof(previewPath), L"Software\\Classes\\%s\\ShellEx\\%s", progId, kPreviewHandlerShellEx);

    HRESULT hr = SetSz(HKEY_CURRENT_USER, thumbPath, nullptr, kThumbnailClsid);
    if (SUCCEEDED(hr)) {
        hr = SetSz(HKEY_CURRENT_USER, previewPath, nullptr, kPreviewClsid);
    }
    return hr;
}

}  // namespace

STDAPI DllRegisterServer() {
    wchar_t dllPath[MAX_PATH] = {};
    const HRESULT pathHr = GetModulePath(dllPath, MAX_PATH);
    if (FAILED(pathHr)) {
        return pathHr;
    }

    HRESULT hr = RegisterHandler(kThumbnailClsid, L"EMPF Thumbnail Provider", dllPath);
    if (SUCCEEDED(hr)) {
        hr = RegisterHandler(kPreviewClsid, L"EMPF Preview Handler", dllPath);
    }
    if (SUCCEEDED(hr)) {
        wchar_t appIdPath[128] = {};
        StringCchPrintfW(appIdPath, _countof(appIdPath), L"Software\\Classes\\AppID\\%s", kAppIdClsid);
        hr = SetSz(HKEY_CURRENT_USER, appIdPath, nullptr, L"EmpfThumbs");
        if (SUCCEEDED(hr)) {
            hr = SetSz(HKEY_CURRENT_USER, appIdPath, L"DllSurrogate", L"");
        }
    }
    if (SUCCEEDED(hr)) {
        hr = RegisterAssociation(L".empf");
    }
    if (SUCCEEDED(hr)) {
        hr = RegisterAssociation(L"eufy.Studio.1");
    }
    if (SUCCEEDED(hr)) {
        hr = SetSz(HKEY_CURRENT_USER,
                   L"Software\\Microsoft\\Windows\\CurrentVersion\\PreviewHandlers",
                   kPreviewClsid, L"EMPF Preview");
    }
    if (SUCCEEDED(hr)) {
        SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
    }
    return hr;
}

STDAPI DllUnregisterServer() {
    wchar_t thumbClsidPath[128] = {};
    wchar_t previewClsidPath[128] = {};
    wchar_t appIdPath[128] = {};
    StringCchPrintfW(thumbClsidPath, _countof(thumbClsidPath), L"Software\\Classes\\CLSID\\%s", kThumbnailClsid);
    StringCchPrintfW(previewClsidPath, _countof(previewClsidPath), L"Software\\Classes\\CLSID\\%s", kPreviewClsid);
    StringCchPrintfW(appIdPath, _countof(appIdPath), L"Software\\Classes\\AppID\\%s", kAppIdClsid);

    DeleteKeyTree(HKEY_CURRENT_USER, thumbClsidPath);
    DeleteKeyTree(HKEY_CURRENT_USER, previewClsidPath);
    DeleteKeyTree(HKEY_CURRENT_USER, appIdPath);
    wchar_t empfThumb[256] = {};
    wchar_t empfPreview[256] = {};
    wchar_t studioThumb[256] = {};
    wchar_t studioPreview[256] = {};
    StringCchPrintfW(empfThumb, _countof(empfThumb), L"Software\\Classes\\.empf\\ShellEx\\%s", kThumbnailHandlerShellEx);
    StringCchPrintfW(empfPreview, _countof(empfPreview), L"Software\\Classes\\.empf\\ShellEx\\%s", kPreviewHandlerShellEx);
    StringCchPrintfW(studioThumb, _countof(studioThumb), L"Software\\Classes\\eufy.Studio.1\\ShellEx\\%s", kThumbnailHandlerShellEx);
    StringCchPrintfW(studioPreview, _countof(studioPreview), L"Software\\Classes\\eufy.Studio.1\\ShellEx\\%s", kPreviewHandlerShellEx);
    DeleteKeyTree(HKEY_CURRENT_USER, empfThumb);
    DeleteKeyTree(HKEY_CURRENT_USER, empfPreview);
    DeleteKeyTree(HKEY_CURRENT_USER, studioThumb);
    DeleteKeyTree(HKEY_CURRENT_USER, studioPreview);

    HKEY key = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\PreviewHandlers",
                      0, KEY_SET_VALUE, &key) == ERROR_SUCCESS) {
        RegDeleteValueW(key, kPreviewClsid);
        RegCloseKey(key);
    }
    return S_OK;
}
