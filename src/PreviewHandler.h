#pragma once

#include <shobjidl.h>
#include <propsys.h>

#include <cstdint>
#include <vector>

class EmpfPreviewHandler : public IInitializeWithStream,
                           public IObjectWithSite,
                           public IOleWindow,
                           public IPreviewHandler {
public:
    EmpfPreviewHandler();

    IFACEMETHODIMP QueryInterface(REFIID riid, void** ppv) override;
    IFACEMETHODIMP_(ULONG) AddRef() override;
    IFACEMETHODIMP_(ULONG) Release() override;

    IFACEMETHODIMP Initialize(IStream* stream, DWORD grfMode) override;

    IFACEMETHODIMP SetSite(IUnknown* site) override;
    IFACEMETHODIMP GetSite(REFIID riid, void** ppv) override;

    IFACEMETHODIMP GetWindow(HWND* phwnd) override;
    IFACEMETHODIMP ContextSensitiveHelp(BOOL fEnterMode) override;

    IFACEMETHODIMP SetWindow(HWND hwnd, const RECT* prc) override;
    IFACEMETHODIMP SetRect(const RECT* prc) override;
    IFACEMETHODIMP DoPreview() override;
    IFACEMETHODIMP Unload() override;
    IFACEMETHODIMP SetFocus() override;
    IFACEMETHODIMP QueryFocus(HWND* phwnd) override;
    IFACEMETHODIMP TranslateAccelerator(MSG* pmsg) override;
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

private:
    ~EmpfPreviewHandler();
    void DestroyPreviewWindow();
    void OnPaint(HWND hwnd);

    long m_ref;
    IUnknown* m_site;
    HWND m_hwndParent;
    HWND m_hwndPreview;
    RECT m_rect;
    HBITMAP m_bitmap;
    std::vector<uint8_t> m_bytes;
    bool m_initialized;
};
