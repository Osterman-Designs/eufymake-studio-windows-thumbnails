#include <windows.h>
#include <objbase.h>

#include <cstdio>
#include <vector>

#include "EmpfReader.h"
#include "WicBitmap.h"

bool ReadAllBytes(const wchar_t* path, std::vector<uint8_t>& data) {
    HANDLE file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                              nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        return false;
    }
    LARGE_INTEGER size = {};
    if (!GetFileSizeEx(file, &size) || size.QuadPart <= 0) {
        CloseHandle(file);
        return false;
    }
    data.resize(static_cast<size_t>(size.QuadPart));
    DWORD read = 0;
    const BOOL ok = ReadFile(file, data.data(), static_cast<DWORD>(data.size()), &read, nullptr);
    CloseHandle(file);
    return ok && read == data.size();
}

int wmain(int argc, wchar_t** argv) {
    const bool iconMode = argc >= 4 && wcscmp(argv[1], L"--icon") == 0;
    if ((!iconMode && argc < 3) || (iconMode && argc < 4)) {
        fwprintf(stderr, L"Usage: empf-extract [--icon] <input.empf> <output.png>\n");
        return 2;
    }

    const wchar_t* input = iconMode ? argv[2] : argv[1];
    const wchar_t* output = iconMode ? argv[3] : argv[2];

    if (iconMode) {
        CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
        std::vector<uint8_t> data;
        EmpfPreviewImages images;
        if (!ReadAllBytes(input, data) || !ExtractEmpfPreviewImages(data.data(), data.size(), images)) {
            fwprintf(stderr, L"Failed to parse %s\n", input);
            return 1;
        }
        HBITMAP bitmap = nullptr;
        bool hasAlpha = true;
        if (!EmpfImagesToHBitmap(images, 256, &bitmap, &hasAlpha) || !SaveHBitmapPng(bitmap, output)) {
            fwprintf(stderr, L"Failed to render icon for %s\n", input);
            if (bitmap) {
                DeleteObject(bitmap);
            }
            return 1;
        }
        DeleteObject(bitmap);
        wprintf(L"Wrote icon %s\n", output);
        return 0;
    }

    std::vector<uint8_t> png;
    if (!ExtractEmpfPreviewPngFromFile(input, png)) {
        fwprintf(stderr, L"Failed to extract a preview PNG from %s\n", input);
        return 1;
    }

    HANDLE file = CreateFileW(output, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        fwprintf(stderr, L"Failed to write %s\n", output);
        return 1;
    }
    DWORD written = 0;
    const BOOL ok = WriteFile(file, png.data(), static_cast<DWORD>(png.size()), &written, nullptr);
    CloseHandle(file);
    if (!ok || written != png.size()) {
        fwprintf(stderr, L"Failed to write %s\n", output);
        return 1;
    }

    wprintf(L"Wrote %u bytes to %s\n", written, output);
    return 0;
}
