#include <windows.h>

#include <cstdio>
#include <string>
#include <vector>

#include "EmpfReader.h"

int wmain(int argc, wchar_t** argv) {
    if (argc < 3) {
        fwprintf(stderr, L"Usage: empf-extract <input.empf> <output.png>\n");
        return 2;
    }

    std::vector<uint8_t> png;
    if (!ExtractEmpfPreviewPngFromFile(argv[1], png)) {
        fwprintf(stderr, L"Failed to extract a preview PNG from %s\n", argv[1]);
        return 1;
    }

    HANDLE file = CreateFileW(argv[2], GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        fwprintf(stderr, L"Failed to write %s\n", argv[2]);
        return 1;
    }
    DWORD written = 0;
    const BOOL ok = WriteFile(file, png.data(), static_cast<DWORD>(png.size()), &written, nullptr);
    CloseHandle(file);
    if (!ok || written != png.size()) {
        fwprintf(stderr, L"Failed to write %s\n", argv[2]);
        return 1;
    }

    wprintf(L"Wrote %u bytes to %s\n", written, argv[2]);
    return 0;
}
