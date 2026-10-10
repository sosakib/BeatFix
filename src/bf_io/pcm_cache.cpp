#include "pcm_cache.h"
#include <windows.h>
#include <bcrypt.h>
#include <fstream>
#include <iomanip>
#include <sstream>

#pragma comment(lib, "bcrypt.lib")

namespace bf::io {

namespace {

std::string bytes_to_hex(const unsigned char* data, size_t len) {
    std::ostringstream ss;
    for (size_t i = 0; i < len; ++i) {
        ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(data[i]);
    }
    return ss.str();
}

} // namespace

Result<std::string> compute_file_sha256(const std::string& file_path) {
    std::ifstream file(file_path, std::ios::binary);
    if (!file) {
        return Result<std::string>::error("Failed to open file for hashing: " + file_path);
    }

    BCRYPT_ALG_HANDLE hAlg = nullptr;
    BCRYPT_HASH_HANDLE hHash = nullptr;

    NTSTATUS status = BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_SHA256_ALGORITHM, nullptr, 0);
    if (!BCRYPT_SUCCESS(status)) {
        return Result<std::string>::error("BCryptOpenAlgorithmProvider failed");
    }

    status = BCryptCreateHash(hAlg, &hHash, nullptr, 0, nullptr, 0, 0);
    if (!BCRYPT_SUCCESS(status)) {
        BCryptCloseAlgorithmProvider(hAlg, 0);
        return Result<std::string>::error("BCryptCreateHash failed");
    }

    std::vector<char> buffer(65536);
    while (file.read(buffer.data(), buffer.size()) || file.gcount() > 0) {
        status = BCryptHashData(hHash, reinterpret_cast<PUCHAR>(buffer.data()), static_cast<ULONG>(file.gcount()), 0);
        if (!BCRYPT_SUCCESS(status)) {
            BCryptDestroyHash(hHash);
            BCryptCloseAlgorithmProvider(hAlg, 0);
            return Result<std::string>::error("BCryptHashData failed");
        }
    }

    unsigned char hash_bytes[32];
    status = BCryptFinishHash(hHash, hash_bytes, sizeof(hash_bytes), 0);
    BCryptDestroyHash(hHash);
    BCryptCloseAlgorithmProvider(hAlg, 0);

    if (!BCRYPT_SUCCESS(status)) {
        return Result<std::string>::error("BCryptFinishHash failed");
    }

    return Result<std::string>::ok(bytes_to_hex(hash_bytes, sizeof(hash_bytes)));
}

std::string compute_pcm_sha256(
    std::span<const float> left,
    std::span<const float> right,
    double sample_rate
) {
    BCRYPT_ALG_HANDLE hAlg = nullptr;
    BCRYPT_HASH_HANDLE hHash = nullptr;

    if (!BCRYPT_SUCCESS(BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_SHA256_ALGORITHM, nullptr, 0))) {
        return "";
    }
    if (!BCRYPT_SUCCESS(BCryptCreateHash(hAlg, &hHash, nullptr, 0, nullptr, 0, 0))) {
        BCryptCloseAlgorithmProvider(hAlg, 0);
        return "";
    }

    // Hash sample rate
    BCryptHashData(hHash, reinterpret_cast<PUCHAR>(&sample_rate), sizeof(sample_rate), 0);

    // Hash left channel
    if (!left.empty()) {
        BCryptHashData(hHash, reinterpret_cast<PUCHAR>(const_cast<float*>(left.data())), static_cast<ULONG>(left.size_bytes()), 0);
    }
    // Hash right channel
    if (!right.empty()) {
        BCryptHashData(hHash, reinterpret_cast<PUCHAR>(const_cast<float*>(right.data())), static_cast<ULONG>(right.size_bytes()), 0);
    }

    unsigned char hash_bytes[32];
    BCryptFinishHash(hHash, hash_bytes, sizeof(hash_bytes), 0);
    BCryptDestroyHash(hHash);
    BCryptCloseAlgorithmProvider(hAlg, 0);

    return bytes_to_hex(hash_bytes, sizeof(hash_bytes));
}

} // namespace bf::io
