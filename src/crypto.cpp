#include "crypto.h"

#include <openssl/evp.h>

#include <memory>
#include <stdexcept>

static std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)> makeCtx() {
    auto ctx =
        std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)>(EVP_MD_CTX_new(), EVP_MD_CTX_free);
    if (!ctx) throw std::runtime_error("Failed to allocate EVP_MD_CTX");
    return ctx;
}

static std::vector<uint8_t> evpDigest(const EVP_MD* algo, const std::vector<uint8_t>& data) {
    auto ctx = makeCtx();

    unsigned char buf[EVP_MAX_MD_SIZE];
    unsigned int len = 0;

    if (EVP_DigestInit_ex(ctx.get(), algo, nullptr) <= 0 ||
        EVP_DigestUpdate(ctx.get(), data.data(), data.size()) <= 0 ||
        EVP_DigestFinal_ex(ctx.get(), buf, &len) <= 0) {
        throw std::runtime_error("EVP digest operation failed");
    }

    return {buf, buf + len};
}

std::vector<uint8_t> SHA256Hash(const std::vector<uint8_t>& data) {
    return evpDigest(EVP_sha256(), data);
}

std::vector<uint8_t> SHA256DoubleHash(const std::vector<uint8_t>& data) {
    return SHA256Hash(SHA256Hash(data));
}

std::vector<uint8_t> RIPEMD160Hash(const std::vector<uint8_t>& data) {
    return evpDigest(EVP_ripemd160(), data);
}
