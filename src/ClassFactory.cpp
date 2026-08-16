#include "ClassFactory.h"

#include "Guids.h"
#include "Module.h"
#include "PreviewHandler.h"
#include "ThumbnailProvider.h"

#include <new>

EmpfClassFactory::EmpfClassFactory(REFCLSID clsid) : m_ref(1), m_clsid(clsid) {
    ModuleAddRef();
}

EmpfClassFactory::~EmpfClassFactory() {
    ModuleRelease();
}

IFACEMETHODIMP EmpfClassFactory::QueryInterface(REFIID riid, void** ppv) {
    if (!ppv) {
        return E_POINTER;
    }
    *ppv = nullptr;
    if (riid == IID_IUnknown || riid == IID_IClassFactory) {
        *ppv = static_cast<IClassFactory*>(this);
        AddRef();
        return S_OK;
    }
    return E_NOINTERFACE;
}

IFACEMETHODIMP_(ULONG) EmpfClassFactory::AddRef() {
    return InterlockedIncrement(&m_ref);
}

IFACEMETHODIMP_(ULONG) EmpfClassFactory::Release() {
    const long ref = InterlockedDecrement(&m_ref);
    if (ref == 0) {
        delete this;
    }
    return ref;
}

IFACEMETHODIMP EmpfClassFactory::CreateInstance(IUnknown* outer, REFIID riid, void** ppv) {
    if (outer) {
        return CLASS_E_NOAGGREGATION;
    }
    if (!ppv) {
        return E_POINTER;
    }
    *ppv = nullptr;

    IUnknown* obj = nullptr;
    if (m_clsid == CLSID_EmpfThumbnailProvider) {
        obj = static_cast<IInitializeWithStream*>(new (std::nothrow) EmpfThumbnailProvider());
    } else if (m_clsid == CLSID_EmpfPreviewHandler) {
        obj = static_cast<IInitializeWithStream*>(new (std::nothrow) EmpfPreviewHandler());
    } else {
        return CLASS_E_CLASSNOTAVAILABLE;
    }

    if (!obj) {
        return E_OUTOFMEMORY;
    }
    const HRESULT hr = obj->QueryInterface(riid, ppv);
    obj->Release();
    return hr;
}

IFACEMETHODIMP EmpfClassFactory::LockServer(BOOL lock) {
    if (lock) {
        ModuleAddRef();
    } else {
        ModuleRelease();
    }
    return S_OK;
}
