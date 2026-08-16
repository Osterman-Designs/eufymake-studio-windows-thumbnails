#pragma once

#include "EmpfReader.h"

#include <windows.h>
#include <cstddef>
#include <cstdint>

bool PngToHBitmap(const uint8_t* png, size_t pngSize, UINT maxEdge, HBITMAP* bitmapOut, bool* hasAlphaOut);
bool EmpfImagesToHBitmap(const EmpfPreviewImages& images, UINT maxEdge, HBITMAP* bitmapOut, bool* hasAlphaOut);
bool SaveHBitmapPng(HBITMAP bitmap, const wchar_t* path);
