#include "base58.h"

#include <openssl/bn.h>

#include <cstring>
#include <memory>
#include <vector>

#include "utils.h"

// Bitcoin Base58 alphabet (no 0, O, I, l to avoid confusion)
static const char BASE58_ALPHABET[] = "123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz";

// RAII type aliases the BIGNUM resources
using BN_ptr = std::unique_ptr<BIGNUM, decltype(&BN_free)>;
using BN_CTX_ptr = std::unique_ptr<BN_CTX, decltype(&BN_CTX_free)>;

std::vector<uint8_t> Base58Encode(const std::vector<uint8_t>& input) {
    std::vector<uint8_t> result;

    BN_ptr x(BN_new(), BN_free);
    BN_ptr base(BN_new(), BN_free);
    BN_ptr zero(BN_new(), BN_free);
    BN_ptr mod(BN_new(), BN_free);
    BN_CTX_ptr ctx(BN_CTX_new(), BN_CTX_free);

    if (!x || !base || !zero || !mod || !ctx) {
        throw std::runtime_error("Failed to allocate BIGNUM resources for Base58 encoding");
    }

    BN_bin2bn(input.data(), input.size(), x.get());
    BN_set_word(base.get(), sizeof(BASE58_ALPHABET) - 1);
    BN_zero(zero.get());

    while (BN_cmp(x.get(), zero.get()) != 0) {
        BN_div(x.get(), mod.get(), x.get(), base.get(), ctx.get());
        result.push_back(BASE58_ALPHABET[BN_get_word(mod.get())]);
    }

    ReverseBytes(result);

    // add a '1' for each leading 0 byte
    for (uint8_t byte : input) {
        if (byte == 0x00) {
            result.insert(result.begin(), BASE58_ALPHABET[0]);
        } else {
            break;
        }
    }

    return result;
}

std::vector<uint8_t> Base58Decode(const std::vector<uint8_t>& input) {
    BN_ptr result(BN_new(), BN_free);
    BN_ptr base(BN_new(), BN_free);
    BN_ptr charValue(BN_new(), BN_free);
    BN_CTX_ptr ctx(BN_CTX_new(), BN_CTX_free);

    if (!result || !base || !charValue || !ctx) {
        throw std::runtime_error("Failed to allocate BIGNUM resources for Base58 decoding");
    }

    BN_zero(result.get());
    BN_set_word(base.get(), sizeof(BASE58_ALPHABET) - 1);

    uint32_t zeroBytes = 0;

    for (uint8_t byte : input) {
        if (byte == BASE58_ALPHABET[0]) {
            zeroBytes++;
        } else {
            break;
        }
    }

    for (size_t i = zeroBytes; i < input.size(); i++) {
        const char* p = strchr(BASE58_ALPHABET, input[i]);
        if (!p) {
            throw std::runtime_error(std::string("Invalid Base58 character '") +
                                     static_cast<char>(input[i]) + "'");
        }

        int charIndex = p - BASE58_ALPHABET;
        BN_mul(result.get(), result.get(), base.get(), ctx.get());
        BN_set_word(charValue.get(), charIndex);
        BN_add(result.get(), result.get(), charValue.get());
    }

    std::vector<uint8_t> decoded(BN_num_bytes(result.get()));
    BN_bn2bin(result.get(), decoded.data());

    std::vector<uint8_t> finalResult(zeroBytes, 0x00);
    finalResult.insert(finalResult.end(), decoded.begin(), decoded.end());

    return finalResult;
}

std::string Base58EncodeStr(const std::vector<uint8_t>& input) {
    std::vector<uint8_t> encoded = Base58Encode(input);
    return BytesToString(encoded);
}

std::vector<uint8_t> Base58DecodeStr(const std::string& input) {
    std::vector<uint8_t> inputBytes = StringToBytes(input);
    return Base58Decode(inputBytes);
}