#include "EmpfReader.h"

#include <windows.h>
#include <bcrypt.h>

#include <algorithm>
#include <cstring>
#include <string>

#include "miniz.h"

#pragma comment(lib, "bcrypt.lib")

namespace {

constexpr uint32_t ReadBe32(const uint8_t* p) {
    return (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) | (uint32_t(p[2]) << 8) | uint32_t(p[3]);
}

bool IsZip(const uint8_t* data, size_t size) {
    return size >= 4 && data[0] == 0x50 && data[1] == 0x4B &&
           (data[2] == 0x03 || data[2] == 0x05 || data[2] == 0x07) &&
           (data[3] == 0x04 || data[3] == 0x06 || data[3] == 0x08);
}

bool LooksLikePng(const uint8_t* data, size_t size) {
    static const uint8_t kPng[] = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};
    return size >= sizeof(kPng) && memcmp(data, kPng, sizeof(kPng)) == 0;
}

bool EndsWithInsensitive(const std::string& value, const char* suffix) {
    const size_t suffixLen = strlen(suffix);
    if (value.size() < suffixLen) {
        return false;
    }
    return _stricmp(value.c_str() + (value.size() - suffixLen), suffix) == 0;
}

bool DecryptStudioPayload(const uint8_t* data, size_t size, uint32_t headerLength, std::vector<uint8_t>& zipOut) {
    constexpr size_t kNonceLen = 12;
    constexpr size_t kTagLen = 16;
    // Published Studio AES-256-GCM content key from MIT-licensed empf-web-preview.
    static const uint8_t kKey[] = "ab24ba760a896cd89eb9e15a9caec7fa";

    if (headerLength < 12 || size < headerLength + kNonceLen + kTagLen) {
        return false;
    }

    const uint8_t* nonce = data + headerLength;
    const uint8_t* body = data + headerLength + kNonceLen;
    const size_t bodyLen = size - headerLength - kNonceLen;
    if (bodyLen <= kTagLen) {
        return false;
    }

    const size_t cipherLen = bodyLen - kTagLen;
    const uint8_t* tag = body + cipherLen;

    BCRYPT_ALG_HANDLE alg = nullptr;
    BCRYPT_KEY_HANDLE key = nullptr;
    NTSTATUS status = BCryptOpenAlgorithmProvider(&alg, BCRYPT_AES_ALGORITHM, nullptr, 0);
    if (status < 0) {
        return false;
    }

    status = BCryptSetProperty(
        alg,
        BCRYPT_CHAINING_MODE,
        reinterpret_cast<PUCHAR>(const_cast<wchar_t*>(BCRYPT_CHAIN_MODE_GCM)),
        sizeof(BCRYPT_CHAIN_MODE_GCM),
        0);
    if (status < 0) {
        BCryptCloseAlgorithmProvider(alg, 0);
        return false;
    }

    status = BCryptGenerateSymmetricKey(alg, &key, nullptr, 0, const_cast<PUCHAR>(kKey), 32, 0);
    if (status < 0) {
        BCryptCloseAlgorithmProvider(alg, 0);
        return false;
    }

    BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO authInfo;
    BCRYPT_INIT_AUTH_MODE_INFO(authInfo);
    authInfo.pbNonce = const_cast<PUCHAR>(nonce);
    authInfo.cbNonce = kNonceLen;
    authInfo.pbTag = const_cast<PUCHAR>(tag);
    authInfo.cbTag = kTagLen;

    ULONG plainLen = 0;
    zipOut.assign(cipherLen, 0);
    status = BCryptDecrypt(
        key,
        const_cast<PUCHAR>(body),
        static_cast<ULONG>(cipherLen),
        &authInfo,
        nullptr,
        0,
        zipOut.data(),
        static_cast<ULONG>(zipOut.size()),
        &plainLen,
        0);

    BCryptDestroyKey(key);
    BCryptCloseAlgorithmProvider(alg, 0);

    if (status < 0) {
        zipOut.clear();
        return false;
    }
    zipOut.resize(plainLen);
    return IsZip(zipOut.data(), zipOut.size());
}

bool UnwrapToZip(const uint8_t* data, size_t size, std::vector<uint8_t>& zipStorage, const uint8_t*& zipData, size_t& zipSize) {
    zipData = nullptr;
    zipSize = 0;

    if (IsZip(data, size)) {
        zipData = data;
        zipSize = size;
        return true;
    }

    if (size >= 12 && memcmp(data, "eufyMake", 8) == 0) {
        const uint32_t headerLength = ReadBe32(data + 8);
        if (DecryptStudioPayload(data, size, headerLength, zipStorage)) {
            zipData = zipStorage.data();
            zipSize = zipStorage.size();
            return true;
        }
    }
    return false;
}

bool ExtractBestPng(const uint8_t* zipData, size_t zipSize, std::vector<uint8_t>& pngOut) {
    mz_zip_archive zip = {};
    if (!mz_zip_reader_init_mem(&zip, zipData, zipSize, 0)) {
        return false;
    }

    const mz_uint fileCount = mz_zip_reader_get_num_files(&zip);
    int preferred = -1;
    int fallback = -1;
    mz_uint64 fallbackSize = 0;

    for (mz_uint i = 0; i < fileCount; ++i) {
        mz_zip_archive_file_stat stat = {};
        if (!mz_zip_reader_file_stat(&zip, i, &stat) || stat.m_is_directory) {
            continue;
        }

        const std::string name = stat.m_filename;
        if (EndsWithInsensitive(name, "thumbnail.png") ||
            _stricmp(name.c_str(), "Asset/images/thumbnail.png") == 0) {
            preferred = static_cast<int>(i);
            break;
        }
        if (EndsWithInsensitive(name, ".png") && stat.m_uncomp_size > fallbackSize) {
            fallback = static_cast<int>(i);
            fallbackSize = stat.m_uncomp_size;
        }
    }

    const int chosen = preferred >= 0 ? preferred : fallback;
    if (chosen < 0) {
        mz_zip_reader_end(&zip);
        return false;
    }

    size_t outSize = 0;
    void* bytes = mz_zip_reader_extract_to_heap(&zip, static_cast<mz_uint>(chosen), &outSize, 0);
    mz_zip_reader_end(&zip);
    if (!bytes || outSize == 0) {
        return false;
    }

    const bool ok = LooksLikePng(static_cast<const uint8_t*>(bytes), outSize);
    if (ok) {
        pngOut.assign(static_cast<const uint8_t*>(bytes), static_cast<const uint8_t*>(bytes) + outSize);
    }
    mz_free(bytes);
    return ok;
}

}  // namespace

bool ExtractEmpfPreviewPng(const uint8_t* data, size_t size, std::vector<uint8_t>& pngOut) {
    pngOut.clear();
    if (!data || size < 4) {
        return false;
    }

    std::vector<uint8_t> zipStorage;
    const uint8_t* zipData = nullptr;
    size_t zipSize = 0;
    if (!UnwrapToZip(data, size, zipStorage, zipData, zipSize)) {
        return false;
    }
    return ExtractBestPng(zipData, zipSize, pngOut);
}

bool ExtractEmpfPreviewPngFromFile(const wchar_t* path, std::vector<uint8_t>& pngOut) {
    HANDLE file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                              nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        return false;
    }

    LARGE_INTEGER fileSize = {};
    if (!GetFileSizeEx(file, &fileSize) || fileSize.QuadPart <= 0 || fileSize.QuadPart > 512ll * 1024 * 1024) {
        CloseHandle(file);
        return false;
    }

    std::vector<uint8_t> data(static_cast<size_t>(fileSize.QuadPart));
    DWORD read = 0;
    const BOOL ok = ReadFile(file, data.data(), static_cast<DWORD>(data.size()), &read, nullptr);
    CloseHandle(file);
    if (!ok || read != data.size()) {
        return false;
    }
    return ExtractEmpfPreviewPng(data.data(), data.size(), pngOut);
}
