#include "base58.h"

#include <openssl/bn.h>

#include <cstdint>
#include <iostream>
#include <vector>

#include "utils.h"

// Bitcoin Base58 alphabet (no 0, O, I, l to avoid confusion)
static const char BASE58_ALPHABET[] = "123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz";

std::vector<uint8_t> Base58Encode(const std::vector<uint8_t>& input) {
    std::vector<uint8_t> result;

    BIGNUM* x = BN_new();
    BN_bin2bn(input.data(), input.size(), x);

    BIGNUM* base = BN_new();
    BN_set_word(base, sizeof(BASE58_ALPHABET) - 1);

    BIGNUM* zero = BN_new();
    BN_zero(zero);

    BIGNUM* mod = BN_new();
    BN_CTX* ctx = BN_CTX_new();

    while (BN_cmp(x, zero) != 0) {
        BN_div(x, mod, x, base, ctx);
        result.push_back(BASE58_ALPHABET[BN_get_word(mod)]);
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

    BN_free(x);
    BN_free(base);
    BN_free(zero);
    BN_free(mod);
    BN_CTX_free(ctx);

    return result;
}
std::vector<uint8_t> Base58Decode(const std::string& input) {
    BIGNUM* result = BN_new();
    BN_zero(result);

    BIGNUM* base = BN_new();
    BN_set_word(base, sizeof(BASE58_ALPHABET) - 1);

    BIGNUM* charValue = BN_new();
    BN_CTX* ctx = BN_CTX_new();

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
            std::cerr << "Error: Invalid Base58 character '" << input[i] << "'" << std::endl;
            BN_free(result);
            BN_free(base);
            BN_free(charValue);
            BN_CTX_free(ctx);
            exit(1);
        }

        int charIndex = p - BASE58_ALPHABET;
        BN_mul(result, result, base, ctx);
        BN_set_word(charValue, charIndex);
        BN_add(result, result, charValue);
    }

    std::vector<uint8_t> decoded(BN_num_bytes(result));
    BN_bn2bin(result, decoded.data());

    std::vector<uint8_t> finalResult(zeroBytes, 0x00);
    finalResult.insert(finalResult.end(), decoded.begin(), decoded.end());

    BN_free(result);
    BN_free(base);
    BN_free(charValue);
    BN_CTX_free(ctx);

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