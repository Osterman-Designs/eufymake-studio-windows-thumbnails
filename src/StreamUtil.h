#pragma once

#include <objidl.h>
#include <cstdint>
#include <vector>

inline bool ReadStreamToMemory(IStream* stream, std::vector<uint8_t>& bytes) {
    bytes.clear();
    if (!stream) {
        return false;
    }

    STATSTG stat = {};
    if (SUCCEEDED(stream->Stat(&stat, STATFLAG_NONAME)) && stat.cbSize.QuadPart > 0 &&
        stat.cbSize.QuadPart <= 512ll * 1024 * 1024) {
        bytes.resize(static_cast<size_t>(stat.cbSize.QuadPart));
        ULONG read = 0;
        const HRESULT hr = stream->Read(bytes.data(), static_cast<ULONG>(bytes.size()), &read);
        if (SUCCEEDED(hr) && read == bytes.size()) {
            return true;
        }
        bytes.clear();
    }

    constexpr ULONG kChunk = 64 * 1024;
    uint8_t buffer[kChunk];
    for (;;) {
        ULONG read = 0;
        const HRESULT hr = stream->Read(buffer, kChunk, &read);
        if (FAILED(hr)) {
            bytes.clear();
            return false;
        }
        if (read == 0) {
            break;
        }
        bytes.insert(bytes.end(), buffer, buffer + read);
        if (bytes.size() > 512u * 1024u * 1024u) {
            bytes.clear();
            return false;
        }
    }
    return !bytes.empty();
}
