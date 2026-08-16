#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

bool ExtractEmpfPreviewPng(const uint8_t* data, size_t size, std::vector<uint8_t>& pngOut);
bool ExtractEmpfPreviewPngFromFile(const wchar_t* path, std::vector<uint8_t>& pngOut);
