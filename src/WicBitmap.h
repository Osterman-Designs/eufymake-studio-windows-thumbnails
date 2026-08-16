#pragma once

#include <windows.h>
#include <cstddef>
#include <cstdint>

bool PngToHBitmap(const uint8_t* png, size_t pngSize, UINT maxEdge, HBITMAP* bitmapOut, bool* hasAlphaOut);
