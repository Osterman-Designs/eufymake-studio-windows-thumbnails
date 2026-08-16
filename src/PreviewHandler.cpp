#include "PreviewHandler.h"

#include "EmpfReader.h"
#include "Module.h"
#include "StreamUtil.h"
#include "WicBitmap.h"

#include <algorithm>

namespace {
constexpr wchar_t kPreviewClass[] = L"EmpfThumbsPreviewHost";

void RegisterPreviewClass() {
    WNDCLASSW wc = {};
    if (GetClassInfoW(g_hInst, kPreviewClass, &wc)) {
        return;
    }
    wc.lpfnWndProc = EmpfPreviewHandler::WndProc;
    wc.hInstance = g_hInst;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
    wc.lpszClassName = kPreviewClass;
    RegisterClassW(&wc);
}
}  // namespace

EmpfPreviewHandler::EmpfPreviewHandler()
    : m_ref(1),
      m_site(nullptr),
      m_hwndParent(nullptr),
      m_hwndPreview(nullptr),
      m_bitmap(nullptr),
      m_initialized(false) {
    SetRectEmpty(&m_rect);
    ModuleAddRef();
}

EmpfPreviewHandler::~EmpfPreviewHandler() {
    Unload();
    if (m_site) {
        m_site->Release();
        m_site = nullptr;
    }
    ModuleRelease();
}

IFACEMETHODIMP EmpfPreviewHandler::QueryInterface(REFIID riid, void** ppv) {
    if (!ppv) {
        return E_POINTER;
    }
    *ppv = nullptr;
    if (riid == IID_IUnknown || riid == IID_IInitializeWithStream) {
        *ppv = static_cast<IInitializeWithStream*>(this);
    } else if (riid == IID_IObjectWithSite) {
        *ppv = static_cast<IObjectWithSite*>(this);
    } else if (riid == IID_IOleWindow) {
        *ppv = static_cast<IOleWindow*>(this);
    } else if (riid == IID_IPreviewHandler) {
        *ppv = static_cast<IPreviewHandler*>(this);
    } else {
        return E_NOINTERFACE;
    }
    AddRef();
    return S_OK;
}

IFACEMETHODIMP_(ULONG) EmpfPreviewHandler::AddRef() {
    return InterlockedIncrement(&m_ref);
}

IFACEMETHODIMP_(ULONG) EmpfPreviewHandler::Release() {
    const long ref = InterlockedDecrement(&m_ref);
    if (ref == 0) {
        delete this;
    }
    return ref;
}

IFACEMETHODIMP EmpfPreviewHandler::Initialize(IStream* stream, DWORD) {
    if (m_initialized) {
        return HRESULT_FROM_WIN32(ERROR_ALREADY_INITIALIZED);
    }
    if (!ReadStreamToMemory(stream, m_bytes)) {
        return E_FAIL;
    }
    m_initialized = true;
    return S_OK;
}

IFACEMETHODIMP EmpfPreviewHandler::SetSite(IUnknown* site) {
    if (m_site) {
        m_site->Release();
        m_site = nullptr;
    }
    m_site = site;
    if (m_site) {
        m_site->AddRef();
    }
    return S_OK;
}

IFACEMETHODIMP EmpfPreviewHandler::GetSite(REFIID riid, void** ppv) {
    if (!m_site) {
        if (ppv) {
            *ppv = nullptr;
        }
        return E_FAIL;
    }
    return m_site->QueryInterface(riid, ppv);
}

IFACEMETHODIMP EmpfPreviewHandler::GetWindow(HWND* phwnd) {
    if (!phwnd) {
        return E_POINTER;
    }
    *phwnd = m_hwndPreview;
    return m_hwndPreview ? S_OK : E_FAIL;
}

IFACEMETHODIMP EmpfPreviewHandler::ContextSensitiveHelp(BOOL) {
    return E_NOTIMPL;
}

IFACEMETHODIMP EmpfPreviewHandler::SetWindow(HWND hwnd, const RECT* prc) {
    m_hwndParent = hwnd;
    if (prc) {
        m_rect = *prc;
    }
    if (m_hwndPreview) {
        SetParent(m_hwndPreview, m_hwndParent);
        SetWindowPos(m_hwndPreview, nullptr, m_rect.left, m_rect.top,
                     m_rect.right - m_rect.left, m_rect.bottom - m_rect.top,
                     SWP_NOZORDER | SWP_NOACTIVATE);
    }
    return S_OK;
}

IFACEMETHODIMP EmpfPreviewHandler::SetRect(const RECT* prc) {
    if (!prc) {
        return E_INVALIDARG;
    }
    m_rect = *prc;
    if (m_hwndPreview) {
        SetWindowPos(m_hwndPreview, nullptr, m_rect.left, m_rect.top,
                     m_rect.right - m_rect.left, m_rect.bottom - m_rect.top,
                     SWP_NOZORDER | SWP_NOACTIVATE);
        InvalidateRect(m_hwndPreview, nullptr, TRUE);
    }
    return S_OK;
}

IFACEMETHODIMP EmpfPreviewHandler::DoPreview() {
    if (!m_initialized || !m_hwndParent) {
        return E_UNEXPECTED;
    }

    if (!m_bitmap) {
        EmpfPreviewImages images;
        if (!ExtractEmpfPreviewImages(m_bytes.data(), m_bytes.size(), images)) {
            return E_FAIL;
        }
        bool hasAlpha = true;
        if (!EmpfImagesToHBitmap(images, 1024, &m_bitmap, &hasAlpha)) {
            return E_FAIL;
        }
    }

    RegisterPreviewClass();
    if (!m_hwndPreview) {
        m_hwndPreview = CreateWindowExW(
            0, kPreviewClass, L"",
            WS_CHILD | WS_VISIBLE,
            m_rect.left, m_rect.top,
            std::max(0L, m_rect.right - m_rect.left),
            std::max(0L, m_rect.bottom - m_rect.top),
            m_hwndParent, nullptr, g_hInst, this);
        if (!m_hwndPreview) {
            return E_FAIL;
        }
    }
    ShowWindow(m_hwndPreview, SW_SHOW);
    return S_OK;
}

IFACEMETHODIMP EmpfPreviewHandler::Unload() {
    DestroyPreviewWindow();
    if (m_bitmap) {
        DeleteObject(m_bitmap);
        m_bitmap = nullptr;
    }
    m_bytes.clear();
    m_initialized = false;
    return S_OK;
}

IFACEMETHODIMP EmpfPreviewHandler::SetFocus() {
    if (m_hwndPreview) {
        ::SetFocus(m_hwndPreview);
        return S_OK;
    }
    return S_FALSE;
}

IFACEMETHODIMP EmpfPreviewHandler::QueryFocus(HWND* phwnd) {
    if (!phwnd) {
        return E_POINTER;
    }
    *phwnd = GetFocus();
    return *phwnd ? S_OK : S_FALSE;
}

IFACEMETHODIMP EmpfPreviewHandler::TranslateAccelerator(MSG* pmsg) {
    IPreviewHandlerFrame* frame = nullptr;
    if (m_site && SUCCEEDED(m_site->QueryInterface(IID_PPV_ARGS(&frame)))) {
        const HRESULT hr = frame->TranslateAccelerator(pmsg);
        frame->Release();
        return hr;
    }
    return S_FALSE;
}

void EmpfPreviewHandler::DestroyPreviewWindow() {
    if (m_hwndPreview) {
        DestroyWindow(m_hwndPreview);
        m_hwndPreview = nullptr;
    }
}

LRESULT CALLBACK EmpfPreviewHandler::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    EmpfPreviewHandler* self = nullptr;
    if (msg == WM_NCCREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        self = static_cast<EmpfPreviewHandler*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    } else {
        self = reinterpret_cast<EmpfPreviewHandler*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }

    if (msg == WM_PAINT && self) {
        self->OnPaint(hwnd);
        return 0;
    }
    if (msg == WM_ERASEBKGND) {
        return 1;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

void EmpfPreviewHandler::OnPaint(HWND hwnd) {
    PAINTSTRUCT ps = {};
    HDC hdc = BeginPaint(hwnd, &ps);

    RECT client = {};
    GetClientRect(hwnd, &client);
    FillRect(hdc, &client, static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)));

    if (m_bitmap) {
        BITMAP bm = {};
        GetObject(m_bitmap, sizeof(bm), &bm);
        if (bm.bmWidth > 0 && bm.bmHeight > 0) {
            const int boxW = client.right - client.left;
            const int boxH = client.bottom - client.top;
            const double scale = std::min(double(boxW) / bm.bmWidth, double(boxH) / bm.bmHeight);
            const int destW = std::max(1, static_cast<int>(bm.bmWidth * scale));
            const int destH = std::max(1, static_cast<int>(bm.bmHeight * scale));
            const int destX = client.left + (boxW - destW) / 2;
            const int destY = client.top + (boxH - destH) / 2;

            HDC mem = CreateCompatibleDC(hdc);
            HGDIOBJ old = SelectObject(mem, m_bitmap);
            SetStretchBltMode(hdc, HALFTONE);
            SetBrushOrgEx(hdc, 0, 0, nullptr);
            StretchBlt(hdc, destX, destY, destW, destH, mem, 0, 0, bm.bmWidth, bm.bmHeight, SRCCOPY);
            SelectObject(mem, old);
            DeleteDC(mem);
        }
    }

    EndPaint(hwnd, &ps);
}
