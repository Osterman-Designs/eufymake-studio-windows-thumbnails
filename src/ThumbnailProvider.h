#pragma once

#include <thumbcache.h>
#include <propsys.h>

#include <cstdint>
#include <vector>

class EmpfThumbnailProvider : public IInitializeWithStream, public IThumbnailProvider {
public:
    EmpfThumbnailProvider();

    // IUnknown
    IFACEMETHODIMP QueryInterface(REFIID riid, void** ppv) override;
    IFACEMETHODIMP_(ULONG) AddRef() override;
    IFACEMETHODIMP_(ULONG) Release() override;

    // IInitializeWithStream
    IFACEMETHODIMP Initialize(IStream* stream, DWORD grfMode) override;

    // IThumbnailProvider
    IFACEMETHODIMP GetThumbnail(UINT cx, HBITMAP* phbmp, WTS_ALPHATYPE* pdwAlpha) override;

private:
    ~EmpfThumbnailProvider();

    long m_ref;
    std::vector<uint8_t> m_bytes;
    bool m_initialized;
};
