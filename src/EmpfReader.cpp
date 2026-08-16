#include "EmpfReader.h"

#include <windows.h>
#include <bcrypt.h>

#include <algorithm>
#include <cstring>
#include <string>
#include <wincrypt.h>

#include "miniz.h"

#pragma comment(lib, "bcrypt.lib")
#pragma comment(lib, "crypt32.lib")

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

bool LooksLikeImage(const uint8_t* data, size_t size) {
    if (LooksLikePng(data, size)) {
        return true;
    }
    if (size >= 3 && data[0] == 0xFF && data[1] == 0xD8 && data[2] == 0xFF) {
        return true;
    }
    return size >= 12 && memcmp(data, "RIFF", 4) == 0 && memcmp(data + 8, "WEBP", 4) == 0;
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

bool ExtractZipIndex(mz_zip_archive& zip, mz_uint index, std::vector<uint8_t>& out) {
    size_t outSize = 0;
    void* bytes = mz_zip_reader_extract_to_heap(&zip, index, &outSize, 0);
    if (!bytes || outSize == 0) {
        return false;
    }
    out.assign(static_cast<const uint8_t*>(bytes), static_cast<const uint8_t*>(bytes) + outSize);
    mz_free(bytes);
    return true;
}

bool DecodeDataUriImage(const std::vector<uint8_t>& uri, std::vector<uint8_t>& imageOut) {
    if (uri.size() < 16 || memcmp(uri.data(), "data:image/", 11) != 0) {
        return false;
    }
    const auto comma = std::find(uri.begin(), uri.end(), static_cast<uint8_t>(','));
    if (comma == uri.end()) {
        return false;
    }
    const DWORD b64Len = static_cast<DWORD>(uri.end() - (comma + 1));
    if (b64Len == 0) {
        return false;
    }
    DWORD decodedLen = 0;
    if (!CryptStringToBinaryA(reinterpret_cast<const char*>(&*(comma + 1)), b64Len, CRYPT_STRING_BASE64,
                              nullptr, &decodedLen, nullptr, nullptr) ||
        decodedLen == 0) {
        return false;
    }
    imageOut.resize(decodedLen);
    if (!CryptStringToBinaryA(reinterpret_cast<const char*>(&*(comma + 1)), b64Len, CRYPT_STRING_BASE64,
                              imageOut.data(), &decodedLen, nullptr, nullptr)) {
        imageOut.clear();
        return false;
    }
    imageOut.resize(decodedLen);
    return LooksLikeImage(imageOut.data(), imageOut.size());
}

void CollectDataUriImages(const std::vector<uint8_t>& text, std::vector<std::vector<uint8_t>>& images) {
    const char marker[] = "data:image/";
    size_t pos = 0;
    while (pos + 11 < text.size()) {
        const auto it = std::search(text.begin() + static_cast<std::ptrdiff_t>(pos), text.end(),
                                    marker, marker + 11);
        if (it == text.end()) {
            break;
        }
        const size_t start = static_cast<size_t>(it - text.begin());
        size_t end = start;
        while (end < text.size() && text[end] != '"' && text[end] != '\'' && text[end] != '<' &&
               text[end] != ' ') {
            ++end;
        }
        std::vector<uint8_t> uri(text.begin() + static_cast<std::ptrdiff_t>(start),
                                 text.begin() + static_cast<std::ptrdiff_t>(end));
        pos = end + 1;

        std::vector<uint8_t> decoded;
        if (DecodeDataUriImage(uri, decoded) && decoded.size() > 16 * 1024) {
            images.push_back(std::move(decoded));
        }
    }
}

bool ExtractPreviewImagesFromZip(const uint8_t* zipData, size_t zipSize, EmpfPreviewImages& images) {
    images = {};
    mz_zip_archive zip = {};
    if (!mz_zip_reader_init_mem(&zip, zipData, zipSize, 0)) {
        return false;
    }

    const mz_uint fileCount = mz_zip_reader_get_num_files(&zip);
    for (mz_uint i = 0; i < fileCount; ++i) {
        mz_zip_archive_file_stat stat = {};
        if (!mz_zip_reader_file_stat(&zip, i, &stat) || stat.m_is_directory) {
            continue;
        }

        const std::string name = stat.m_filename;
        std::vector<uint8_t> bytes;
        if (!ExtractZipIndex(zip, i, bytes)) {
            continue;
        }

        if (EndsWithInsensitive(name, "thumbnail.png") || EndsWithInsensitive(name, ".png")) {
            if (LooksLikePng(bytes.data(), bytes.size())) {
                if (EndsWithInsensitive(name, "thumbnail.png") || images.thumbnailPng.empty()) {
                    images.thumbnailPng = std::move(bytes);
                }
            }
            continue;
        }

        if (EndsWithInsensitive(name, ".dat")) {
            std::vector<uint8_t> decoded;
            if (DecodeDataUriImage(bytes, decoded)) {
                images.layerPngs.push_back(std::move(decoded));
            }
            continue;
        }

        if (EndsWithInsensitive(name, ".json") && name.find("canvas") != std::string::npos) {
            CollectDataUriImages(bytes, images.layerPngs);
        }
    }

    mz_zip_reader_end(&zip);
    std::sort(images.layerPngs.begin(), images.layerPngs.end(),
              [](const std::vector<uint8_t>& a, const std::vector<uint8_t>& b) { return a.size() > b.size(); });
    return !images.thumbnailPng.empty() || !images.layerPngs.empty();
}

}  // namespace

bool ExtractEmpfPreviewImages(const uint8_t* data, size_t size, EmpfPreviewImages& images) {
    images = {};
    if (!data || size < 4) {
        return false;
    }

    std::vector<uint8_t> zipStorage;
    const uint8_t* zipData = nullptr;
    size_t zipSize = 0;
    if (!UnwrapToZip(data, size, zipStorage, zipData, zipSize)) {
        return false;
    }
    return ExtractPreviewImagesFromZip(zipData, zipSize, images);
}

bool ExtractEmpfPreviewPng(const uint8_t* data, size_t size, std::vector<uint8_t>& pngOut) {
    pngOut.clear();
    EmpfPreviewImages images;
    if (!ExtractEmpfPreviewImages(data, size, images)) {
        return false;
    }
    if (!images.layerPngs.empty() &&
        (images.thumbnailPng.empty() || images.layerPngs.front().size() > images.thumbnailPng.size() * 4)) {
        pngOut = std::move(images.layerPngs.front());
        return true;
    }
    if (!images.thumbnailPng.empty()) {
        pngOut = std::move(images.thumbnailPng);
        return true;
    }
    if (!images.layerPngs.empty()) {
        pngOut = std::move(images.layerPngs.front());
        return true;
    }
    return false;
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
