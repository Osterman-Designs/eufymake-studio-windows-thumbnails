#include "WicBitmap.h"

#include <wincodec.h>
#include <shlwapi.h>

#include <algorithm>

#pragma comment(lib, "windowscodecs.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "ole32.lib")

bool PngToHBitmap(const uint8_t* png, size_t pngSize, UINT maxEdge, HBITMAP* bitmapOut, bool* hasAlphaOut) {
    if (!png || !pngSize || !bitmapOut) {
        return false;
    }
    *bitmapOut = nullptr;
    if (hasAlphaOut) {
        *hasAlphaOut = true;
    }

    IStream* stream = SHCreateMemStream(png, static_cast<UINT>(pngSize));
    if (!stream) {
        return false;
    }

    IWICImagingFactory* factory = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                                  IID_PPV_ARGS(&factory));
    if (FAILED(hr)) {
        stream->Release();
        return false;
    }

    IWICBitmapDecoder* decoder = nullptr;
    hr = factory->CreateDecoderFromStream(stream, nullptr, WICDecodeMetadataCacheOnLoad, &decoder);
    stream->Release();
    if (FAILED(hr)) {
        factory->Release();
        return false;
    }

    IWICBitmapFrameDecode* frame = nullptr;
    hr = decoder->GetFrame(0, &frame);
    decoder->Release();
    if (FAILED(hr)) {
        factory->Release();
        return false;
    }

    UINT width = 0;
    UINT height = 0;
    frame->GetSize(&width, &height);
    if (width == 0 || height == 0) {
        frame->Release();
        factory->Release();
        return false;
    }

    UINT destWidth = width;
    UINT destHeight = height;
    if (maxEdge > 0 && (width > maxEdge || height > maxEdge)) {
        if (width >= height) {
            destWidth = maxEdge;
            destHeight = std::max(1u, height * maxEdge / width);
        } else {
            destHeight = maxEdge;
            destWidth = std::max(1u, width * maxEdge / height);
        }
    }

    IWICBitmapScaler* scaler = nullptr;
    IWICBitmapSource* source = frame;
    if (destWidth != width || destHeight != height) {
        hr = factory->CreateBitmapScaler(&scaler);
        if (SUCCEEDED(hr)) {
            hr = scaler->Initialize(frame, destWidth, destHeight, WICBitmapInterpolationModeFant);
        }
        if (FAILED(hr)) {
            if (scaler) {
                scaler->Release();
            }
            frame->Release();
            factory->Release();
            return false;
        }
        source = scaler;
    }

    IWICFormatConverter* converter = nullptr;
    hr = factory->CreateFormatConverter(&converter);
    if (SUCCEEDED(hr)) {
        hr = converter->Initialize(source, GUID_WICPixelFormat32bppPBGRA, WICBitmapDitherTypeNone,
                                   nullptr, 0.0, WICBitmapPaletteTypeCustom);
    }
    if (scaler) {
        scaler->Release();
    }
    frame->Release();
    if (FAILED(hr)) {
        if (converter) {
            converter->Release();
        }
        factory->Release();
        return false;
    }

    BITMAPINFO info = {};
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = static_cast<LONG>(destWidth);
    info.bmiHeader.biHeight = -static_cast<LONG>(destHeight);
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;

    void* bits = nullptr;
    HBITMAP bitmap = CreateDIBSection(nullptr, &info, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (!bitmap || !bits) {
        converter->Release();
        factory->Release();
        return false;
    }

    const UINT stride = destWidth * 4;
    hr = converter->CopyPixels(nullptr, stride, stride * destHeight, static_cast<BYTE*>(bits));
    converter->Release();
    factory->Release();
    if (FAILED(hr)) {
        DeleteObject(bitmap);
        return false;
    }

    *bitmapOut = bitmap;
    return true;
}
