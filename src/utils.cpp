#include "utils.h"

leveldb::Slice ByteArrayToSlice(const std::vector<uint8_t>& bytes) {
    return leveldb::Slice(reinterpret_cast<const char*>(bytes.data()), bytes.size());
}
