#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

struct EmpfPreviewImages {
    std::vector<uint8_t> thumbnailPng;
    std::vector<std::vector<uint8_t>> layerPngs;
};

bool ExtractEmpfPreviewImages(const uint8_t* data, size_t size, EmpfPreviewImages& images);
bool ExtractEmpfPreviewPng(const uint8_t* data, size_t size, std::vector<uint8_t>& pngOut);
bool ExtractEmpfPreviewPngFromFile(const wchar_t* path, std::vector<uint8_t>& pngOut);
