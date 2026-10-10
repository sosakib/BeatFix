#pragma once

#include <string>
#include <string_view>
#include <filesystem>
#include <algorithm>
#include <cctype>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace bf::io {

inline std::wstring utf8_to_wide(std::string_view utf8) {
    if (utf8.empty()) return L"";
    int count = MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), nullptr, 0);
    if (count <= 0) return L"";
    std::wstring wide(static_cast<size_t>(count), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), wide.data(), count);
    return wide;
}

inline std::string wide_to_utf8(std::wstring_view wide) {
    if (wide.empty()) return "";
    int count = WideCharToMultiByte(CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()), nullptr, 0, nullptr, nullptr);
    if (count <= 0) return "";
    std::string utf8(static_cast<size_t>(count), '\0');
    WideCharToMultiByte(CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()), utf8.data(), count, nullptr, nullptr);
    return utf8;
}

inline std::filesystem::path path_from_utf8(std::string_view utf8) {
    return std::filesystem::path(utf8_to_wide(utf8));
}

inline std::string path_to_utf8(const std::filesystem::path& p) {
    return wide_to_utf8(p.wstring());
}

inline std::string get_extension_lower(std::string_view file_path) {
    std::filesystem::path p = path_from_utf8(file_path);
    std::string ext = wide_to_utf8(p.extension().wstring());
    for (char& c : ext) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return ext;
}

} // namespace bf::io

#else

namespace bf::io {

inline std::filesystem::path path_from_utf8(std::string_view utf8) {
    return std::filesystem::path(std::string(utf8));
}

inline std::string path_to_utf8(const std::filesystem::path& p) {
    return p.string();
}

inline std::string get_extension_lower(std::string_view file_path) {
    std::filesystem::path p(file_path);
    std::string ext = p.extension().string();
    for (char& c : ext) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return ext;
}

} // namespace bf::io

#endif
