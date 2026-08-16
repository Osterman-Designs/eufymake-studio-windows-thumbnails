#pragma once

#include <guiddef.h>

// {7C3E9A12-4B8D-4F21-A6C3-91E04D2B7F11}
DEFINE_GUID(CLSID_EmpfThumbnailProvider,
    0x7c3e9a12, 0x4b8d, 0x4f21, 0xa6, 0xc3, 0x91, 0xe0, 0x4d, 0x2b, 0x7f, 0x11);

// {7C3E9A12-4B8D-4F21-A6C3-91E04D2B7F12}
DEFINE_GUID(CLSID_EmpfPreviewHandler,
    0x7c3e9a12, 0x4b8d, 0x4f21, 0xa6, 0xc3, 0x91, 0xe0, 0x4d, 0x2b, 0x7f, 0x12);

// {7C3E9A12-4B8D-4F21-A6C3-91E04D2B7F13}
DEFINE_GUID(CLSID_EmpfThumbsAppId,
    0x7c3e9a12, 0x4b8d, 0x4f21, 0xa6, 0xc3, 0x91, 0xe0, 0x4d, 0x2b, 0x7f, 0x13);

inline constexpr wchar_t kThumbnailClsid[] = L"{7C3E9A12-4B8D-4F21-A6C3-91E04D2B7F11}";
inline constexpr wchar_t kPreviewClsid[] = L"{7C3E9A12-4B8D-4F21-A6C3-91E04D2B7F12}";
inline constexpr wchar_t kAppIdClsid[] = L"{7C3E9A12-4B8D-4F21-A6C3-91E04D2B7F13}";
inline constexpr wchar_t kThumbnailHandlerShellEx[] = L"{E357FCCD-A995-4576-B01F-234630154E96}";
inline constexpr wchar_t kPreviewHandlerShellEx[] = L"{8895B1C6-B41F-4C1C-A562-0D564250836F}";
