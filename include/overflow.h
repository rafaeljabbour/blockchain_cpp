#ifndef OVERFLOW_H
#define OVERFLOW_H

#include <cstdint>
#include <limits>

// this is used to check overflow, it uses the builtin overflow on linux and arithmetics for others
inline bool CheckedAdd(int64_t a, int64_t b, int64_t& result) {
#if defined(__GNUC__) || defined(__clang__)
    return __builtin_add_overflow(a, b, &result);
#else
    if ((b > 0 && a > std::numeric_limits<int64_t>::max() - b) ||
        (b < 0 && a < std::numeric_limits<int64_t>::min() - b)) {
        return true;
    }
    result = a + b;
    return false;
#endif
}

#endif
