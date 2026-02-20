#ifndef UTILS_H
#define UTILS_H

#include <leveldb/slice.h>

#include <cstdint>
#include <stdexcept>
#include <vector>

// access to all utilities.
#include "crypto.h"
#include "serialization.h"

// converts vector to LevelDB's internal Slice format
leveldb::Slice ByteArrayToSlice(const std::vector<uint8_t>& bytes);

#endif