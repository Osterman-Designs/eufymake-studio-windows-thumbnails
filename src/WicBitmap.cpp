#include "WicBitmap.h"

#include <wincodec.h>
#include <shlwapi.h>
#include <webp/decode.h>

#include <algorithm>
#include <cstring>
#include <vector>

#pragma comment(lib, "windowscodecs.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "ole32.lib")

namespace {

bool IsEmptyPixel(const BYTE* bgra) {
    const BYTE b = bgra[0];
    const BYTE g = bgra[1];
    const BYTE r = bgra[2];
    const BYTE a = bgra[3];
    return a < 16 || (r >= 248 && g >= 248 && b >= 248);
}

bool LooksLikeWebP(const uint8_t* data, size_t size) {
    return size >= 12 && memcmp(data, "RIFF", 4) == 0 && memcmp(data + 8, "WEBP", 4) == 0;
}

bool DecodeWebPSource(const uint8_t* data, size_t size, IWICImagingFactory* factory,
                      IWICBitmapSource** sourceOut) {
    int width = 0;
    int height = 0;
    uint8_t* bgra = WebPDecodeBGRA(data, size, &width, &height);
    if (!bgra || width <= 0 || height <= 0) {
        if (bgra) {
            WebPFree(bgra);
        }
        return false;
    }

    IWICBitmap* bitmap = nullptr;
    const UINT stride = static_cast<UINT>(width) * 4;
    const HRESULT hr = factory->CreateBitmapFromMemory(
        static_cast<UINT>(width), static_cast<UINT>(height), GUID_WICPixelFormat32bppBGRA, stride,
        stride * static_cast<UINT>(height), bgra, &bitmap);
    WebPFree(bgra);
    if (FAILED(hr)) {
        return false;
    }
    *sourceOut = bitmap;
    return true;
}

bool DecodeSource(const uint8_t* bytes, size_t size, IWICImagingFactory** factoryOut,
                  IWICBitmapSource** sourceOut) {
    *factoryOut = nullptr;
    *sourceOut = nullptr;

    IWICImagingFactory* factory = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                                  IID_PPV_ARGS(&factory));
    if (FAILED(hr)) {
        return false;
    }

    if (LooksLikeWebP(bytes, size)) {
        if (DecodeWebPSource(bytes, size, factory, sourceOut)) {
            *factoryOut = factory;
            return true;
        }
        factory->Release();
        return false;
    }

    IStream* stream = SHCreateMemStream(bytes, static_cast<UINT>(size));
    if (!stream) {
        factory->Release();
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

    *factoryOut = factory;
    *sourceOut = frame;
    return true;
}

bool ContentBounds(IWICImagingFactory* factory, IWICBitmapSource* source, WICRect* bounds) {
    UINT width = 0;
    UINT height = 0;
    source->GetSize(&width, &height);
    if (width == 0 || height == 0) {
        return false;
    }

    IWICFormatConverter* converter = nullptr;
    HRESULT hr = factory->CreateFormatConverter(&converter);
    if (FAILED(hr) || FAILED(converter->Initialize(source, GUID_WICPixelFormat32bppPBGRA,
                                                   WICBitmapDitherTypeNone, nullptr, 0.0,
                                                   WICBitmapPaletteTypeCustom))) {
        if (converter) {
            converter->Release();
        }
        return false;
    }

    std::vector<BYTE> pixels(static_cast<size_t>(width) * height * 4);
    hr = converter->CopyPixels(nullptr, width * 4, static_cast<UINT>(pixels.size()), pixels.data());
    converter->Release();
    if (FAILED(hr)) {
        return false;
    }

    UINT left = width;
    UINT top = height;
    UINT right = 0;
    UINT bottom = 0;
    for (UINT y = 0; y < height; ++y) {
        const BYTE* row = pixels.data() + static_cast<size_t>(y) * width * 4;
        for (UINT x = 0; x < width; ++x) {
            if (!IsEmptyPixel(row + x * 4)) {
                left = std::min(left, x);
                top = std::min(top, y);
                right = std::max(right, x);
                bottom = std::max(bottom, y);
            }
        }
    }
    if (right < left || bottom < top) {
        bounds->X = 0;
        bounds->Y = 0;
        bounds->Width = static_cast<INT>(width);
        bounds->Height = static_cast<INT>(height);
        return true;
    }

    const UINT padX = std::max(2u, (right - left + 1) / 20);
    const UINT padY = std::max(2u, (bottom - top + 1) / 20);
    const UINT x0 = left > padX ? left - padX : 0;
    const UINT y0 = top > padY ? top - padY : 0;
    const UINT x1 = std::min(width - 1, right + padX);
    const UINT y1 = std::min(height - 1, bottom + padY);
    bounds->X = static_cast<INT>(x0);
    bounds->Y = static_cast<INT>(y0);
    bounds->Width = static_cast<INT>(x1 - x0 + 1);
    bounds->Height = static_cast<INT>(y1 - y0 + 1);

    // Wide bed strips often have two coins with a white gap. A center square
    // lands in that gap; pick the largest content island instead.
    if (bounds->Width * 5 > bounds->Height * 8) {
        std::vector<char> colHas(width, 0);
        for (UINT y = y0; y <= y1; ++y) {
            const BYTE* row = pixels.data() + static_cast<size_t>(y) * width * 4;
            for (UINT x = x0; x <= x1; ++x) {
                if (!IsEmptyPixel(row + x * 4)) {
                    colHas[x] = 1;
                }
            }
        }

        UINT bestL = x0;
        UINT bestR = x1;
        UINT bestW = 0;
        INT runL = -1;
        for (UINT x = x0; x <= x1 + 1; ++x) {
            const bool filled = x <= x1 && colHas[x];
            if (filled && runL < 0) {
                runL = static_cast<INT>(x);
            }
            if (!filled && runL >= 0) {
                const UINT runW = x - static_cast<UINT>(runL);
                if (runW > bestW) {
                    bestW = runW;
                    bestL = static_cast<UINT>(runL);
                    bestR = x - 1;
                }
                runL = -1;
            }
        }

        if (bestW > 0) {
            UINT iLeft = width;
            UINT iTop = height;
            UINT iRight = 0;
            UINT iBottom = 0;
            for (UINT y = y0; y <= y1; ++y) {
                const BYTE* row = pixels.data() + static_cast<size_t>(y) * width * 4;
                for (UINT x = bestL; x <= bestR; ++x) {
                    if (!IsEmptyPixel(row + x * 4)) {
                        iLeft = std::min(iLeft, x);
                        iTop = std::min(iTop, y);
                        iRight = std::max(iRight, x);
                        iBottom = std::max(iBottom, y);
                    }
                }
            }
            if (iRight >= iLeft && iBottom >= iTop) {
                const UINT iPadX = std::max(2u, (iRight - iLeft + 1) / 20);
                const UINT iPadY = std::max(2u, (iBottom - iTop + 1) / 20);
                const UINT ix0 = iLeft > iPadX ? iLeft - iPadX : 0;
                const UINT iy0 = iTop > iPadY ? iTop - iPadY : 0;
                const UINT ix1 = std::min(width - 1, iRight + iPadX);
                const UINT iy1 = std::min(height - 1, iBottom + iPadY);
                bounds->X = static_cast<INT>(ix0);
                bounds->Y = static_cast<INT>(iy0);
                bounds->Width = static_cast<INT>(ix1 - ix0 + 1);
                bounds->Height = static_cast<INT>(iy1 - iy0 + 1);
            }
        }
    }
    return true;
}

bool IsWideRect(const WICRect& rect) {
    return rect.Width > 0 && rect.Height > 0 &&
           rect.Width * 5 > rect.Height * 8;
}

void SquareAroundContent(UINT imageWidth, UINT imageHeight, WICRect* bounds) {
    const INT contentW = std::max(1, bounds->Width);
    const INT contentH = std::max(1, bounds->Height);
    // Wide bed previews have multiple coins; a center square hits the gap
    // between them. Keep the full cluster in that case.
    if (IsWideRect(*bounds)) {
        return;
    }
    const INT centerX = bounds->X + contentW / 2;
    const INT centerY = bounds->Y + contentH / 2;
    INT side = contentW > contentH ? contentW : contentH;

    INT x = centerX - side / 2;
    INT y = centerY - side / 2;
    if (x < 0) {
        x = 0;
    }
    if (y < 0) {
        y = 0;
    }
    if (x + side > static_cast<INT>(imageWidth)) {
        x = static_cast<INT>(imageWidth) - side;
    }
    if (y + side > static_cast<INT>(imageHeight)) {
        y = static_cast<INT>(imageHeight) - side;
    }
    if (x < 0) {
        x = 0;
        side = static_cast<INT>(imageWidth);
    }
    if (y < 0) {
        y = 0;
        side = static_cast<INT>(imageHeight);
    }
    bounds->X = x;
    bounds->Y = y;
    bounds->Width = side;
    bounds->Height = side;
}

bool RenderSource(IWICImagingFactory* factory, IWICBitmapSource* source, const WICRect& crop,
                  UINT maxEdge, HBITMAP* bitmapOut) {
    IWICBitmapClipper* clipper = nullptr;
    HRESULT hr = factory->CreateBitmapClipper(&clipper);
    if (FAILED(hr) || FAILED(clipper->Initialize(source, &crop))) {
        if (clipper) {
            clipper->Release();
        }
        return false;
    }

    const UINT cropW = crop.Width > 0 ? static_cast<UINT>(crop.Width) : 1u;
    const UINT cropH = crop.Height > 0 ? static_cast<UINT>(crop.Height) : 1u;
    const UINT box = maxEdge ? maxEdge : 256;
    const bool contain = cropW * 5 > cropH * 8 || cropH * 5 > cropW * 8;
    const UINT scaledW = contain
                             ? (cropW >= cropH ? box : std::max(1u, box * cropW / cropH))
                             : (cropW >= cropH ? std::max(box, box * cropW / cropH) : box);
    const UINT scaledH = contain
                             ? (cropH > cropW ? box : std::max(1u, box * cropH / cropW))
                             : (cropH > cropW ? std::max(box, box * cropH / cropW) : box);

    IWICBitmapScaler* scaler = nullptr;
    IWICBitmapSource* scaled = clipper;
    if (scaledW != cropW || scaledH != cropH) {
        hr = factory->CreateBitmapScaler(&scaler);
        if (SUCCEEDED(hr)) {
            hr = scaler->Initialize(clipper, scaledW, scaledH, WICBitmapInterpolationModeHighQualityCubic);
        }
        if (FAILED(hr)) {
            if (scaler) {
                scaler->Release();
            }
            clipper->Release();
            return false;
        }
        scaled = scaler;
    }

    IWICFormatConverter* converter = nullptr;
    hr = factory->CreateFormatConverter(&converter);
    if (SUCCEEDED(hr)) {
        hr = converter->Initialize(scaled, GUID_WICPixelFormat32bppPBGRA, WICBitmapDitherTypeNone,
                                   nullptr, 0.0, WICBitmapPaletteTypeCustom);
    }
    if (scaler) {
        scaler->Release();
    }
    clipper->Release();
    if (FAILED(hr)) {
        if (converter) {
            converter->Release();
        }
        return false;
    }

    BITMAPINFO info = {};
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = static_cast<LONG>(box);
    info.bmiHeader.biHeight = -static_cast<LONG>(box);
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;

    void* bits = nullptr;
    HBITMAP bitmap = CreateDIBSection(nullptr, &info, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (!bitmap || !bits) {
        converter->Release();
        return false;
    }

    memset(bits, 0, static_cast<size_t>(box) * box * 4);
    const UINT destX = scaledW < box ? (box - scaledW) / 2 : 0;
    const UINT destY = scaledH < box ? (box - scaledH) / 2 : 0;
    const UINT copyW = scaledW < box ? scaledW : box;
    const UINT copyH = scaledH < box ? scaledH : box;
    WICRect srcRect = {
        static_cast<INT>(scaledW > box ? (scaledW - box) / 2 : 0),
        static_cast<INT>(scaledH > box ? (scaledH - box) / 2 : 0),
        static_cast<INT>(copyW),
        static_cast<INT>(copyH)};
    const UINT stride = box * 4;
    BYTE* dest = static_cast<BYTE*>(bits) + static_cast<size_t>(destY) * stride + destX * 4;
    hr = converter->CopyPixels(&srcRect, stride, stride * box - static_cast<UINT>(dest - static_cast<BYTE*>(bits)),
                               dest);
    converter->Release();
    if (FAILED(hr)) {
        DeleteObject(bitmap);
        return false;
    }

    *bitmapOut = bitmap;
    return true;
}

bool RenderPng(const uint8_t* png, size_t pngSize, UINT maxEdge, HBITMAP* bitmapOut, WICRect* cropOut) {
    IWICImagingFactory* factory = nullptr;
    IWICBitmapSource* source = nullptr;
    if (!DecodeSource(png, pngSize, &factory, &source)) {
        return false;
    }

    UINT width = 0;
    UINT height = 0;
    source->GetSize(&width, &height);

    WICRect crop = {};
    if (!ContentBounds(factory, source, &crop)) {
        source->Release();
        factory->Release();
        return false;
    }
    SquareAroundContent(width, height, &crop);
    if (cropOut) {
        *cropOut = crop;
    }

    const bool ok = RenderSource(factory, source, crop, maxEdge, bitmapOut);
    source->Release();
    factory->Release();
    return ok;
}

}  // namespace

bool PngToHBitmap(const uint8_t* png, size_t pngSize, UINT maxEdge, HBITMAP* bitmapOut, bool* hasAlphaOut) {
    if (!png || !pngSize || !bitmapOut) {
        return false;
    }
    *bitmapOut = nullptr;
    if (hasAlphaOut) {
        *hasAlphaOut = true;
    }
    return RenderPng(png, pngSize, maxEdge, bitmapOut, nullptr);
}

bool ImageMinEdge(const std::vector<uint8_t>& bytes, UINT* minEdge) {
    IWICImagingFactory* factory = nullptr;
    IWICBitmapSource* source = nullptr;
    if (!DecodeSource(bytes.data(), bytes.size(), &factory, &source)) {
        return false;
    }
    UINT width = 0;
    UINT height = 0;
    source->GetSize(&width, &height);
    source->Release();
    factory->Release();
    if (width == 0 || height == 0) {
        return false;
    }
    *minEdge = width < height ? width : height;
    return true;
}

bool EmpfImagesToHBitmap(const EmpfPreviewImages& images, UINT maxEdge, HBITMAP* bitmapOut, bool* hasAlphaOut) {
    if (!bitmapOut) {
        return false;
    }
    *bitmapOut = nullptr;
    if (hasAlphaOut) {
        *hasAlphaOut = true;
    }

    const std::vector<uint8_t>* best = nullptr;
    UINT bestEdge = 0;
    auto consider = [&](const std::vector<uint8_t>& bytes) {
        UINT edge = 0;
        if (ImageMinEdge(bytes, &edge) && edge > bestEdge) {
            bestEdge = edge;
            best = &bytes;
        }
    };
    for (const auto& layer : images.layerPngs) {
        consider(layer);
    }
    if (!best && !images.thumbnailPng.empty()) {
        consider(images.thumbnailPng);
    }

    if (best) {
        return RenderPng(best->data(), best->size(), maxEdge, bitmapOut, nullptr);
    }
    if (!images.thumbnailPng.empty()) {
        return RenderPng(images.thumbnailPng.data(), images.thumbnailPng.size(), maxEdge, bitmapOut, nullptr);
    }
    return false;
}

bool SaveHBitmapPng(HBITMAP bitmap, const wchar_t* path) {
    if (!bitmap || !path) {
        return false;
    }

    IWICImagingFactory* factory = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                                  IID_PPV_ARGS(&factory));
    if (FAILED(hr)) {
        return false;
    }

    IWICBitmap* wicBitmap = nullptr;
    hr = factory->CreateBitmapFromHBITMAP(bitmap, nullptr, WICBitmapUsePremultipliedAlpha, &wicBitmap);
    if (FAILED(hr)) {
        factory->Release();
        return false;
    }

    IWICStream* stream = nullptr;
    IWICBitmapEncoder* encoder = nullptr;
    IWICBitmapFrameEncode* frame = nullptr;
    hr = factory->CreateStream(&stream);
    if (SUCCEEDED(hr)) {
        hr = stream->InitializeFromFilename(path, GENERIC_WRITE);
    }
    if (SUCCEEDED(hr)) {
        hr = factory->CreateEncoder(GUID_ContainerFormatPng, nullptr, &encoder);
    }
    if (SUCCEEDED(hr)) {
        hr = encoder->Initialize(stream, WICBitmapEncoderNoCache);
    }
    if (SUCCEEDED(hr)) {
        hr = encoder->CreateNewFrame(&frame, nullptr);
    }
    if (SUCCEEDED(hr)) {
        hr = frame->Initialize(nullptr);
    }
    if (SUCCEEDED(hr)) {
        hr = frame->WriteSource(wicBitmap, nullptr);
    }
    if (SUCCEEDED(hr)) {
        hr = frame->Commit();
    }
    if (SUCCEEDED(hr)) {
        hr = encoder->Commit();
    }

    if (frame) {
        frame->Release();
    }
    if (encoder) {
        encoder->Release();
    }
    if (stream) {
        stream->Release();
    }
    wicBitmap->Release();
    factory->Release();
    return SUCCEEDED(hr);
}
