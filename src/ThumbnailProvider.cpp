#include "ThumbnailProvider.h"

#include "EmpfReader.h"
#include "Module.h"
#include "StreamUtil.h"
#include "WicBitmap.h"

#include <shlwapi.h>

EmpfThumbnailProvider::EmpfThumbnailProvider()
    : m_ref(1), m_initialized(false) {
    ModuleAddRef();
}

EmpfThumbnailProvider::~EmpfThumbnailProvider() {
    ModuleRelease();
}

IFACEMETHODIMP EmpfThumbnailProvider::QueryInterface(REFIID riid, void** ppv) {
    if (!ppv) {
        return E_POINTER;
    }
    *ppv = nullptr;
    if (riid == IID_IUnknown || riid == IID_IInitializeWithStream) {
        *ppv = static_cast<IInitializeWithStream*>(this);
    } else if (riid == IID_IThumbnailProvider) {
        *ppv = static_cast<IThumbnailProvider*>(this);
    } else {
        return E_NOINTERFACE;
    }
    AddRef();
    return S_OK;
}

IFACEMETHODIMP_(ULONG) EmpfThumbnailProvider::AddRef() {
    return InterlockedIncrement(&m_ref);
}

IFACEMETHODIMP_(ULONG) EmpfThumbnailProvider::Release() {
    const long ref = InterlockedDecrement(&m_ref);
    if (ref == 0) {
        delete this;
    }
    return ref;
}

IFACEMETHODIMP EmpfThumbnailProvider::Initialize(IStream* stream, DWORD) {
    if (m_initialized) {
        return HRESULT_FROM_WIN32(ERROR_ALREADY_INITIALIZED);
    }
    if (!ReadStreamToMemory(stream, m_bytes)) {
        return E_FAIL;
    }
    m_initialized = true;
    return S_OK;
}

IFACEMETHODIMP EmpfThumbnailProvider::GetThumbnail(UINT cx, HBITMAP* phbmp, WTS_ALPHATYPE* pdwAlpha) {
    if (!phbmp) {
        return E_POINTER;
    }
    *phbmp = nullptr;
    if (pdwAlpha) {
        *pdwAlpha = WTSAT_ARGB;
    }
    if (!m_initialized) {
        return E_UNEXPECTED;
    }

    EmpfPreviewImages images;
    if (!ExtractEmpfPreviewImages(m_bytes.data(), m_bytes.size(), images)) {
        return E_FAIL;
    }

    bool hasAlpha = true;
    if (!EmpfImagesToHBitmap(images, cx ? cx : 256, phbmp, &hasAlpha)) {
        return E_FAIL;
    }
    if (pdwAlpha) {
        *pdwAlpha = hasAlpha ? WTSAT_ARGB : WTSAT_RGB;
    }
    return S_OK;
}
