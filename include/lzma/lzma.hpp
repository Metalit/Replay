#pragma once

extern "C" {
    #include "lzma/pavlov/LzmaUtil.h"
}
#include <vector>

namespace LZMA {
    // both of these are highly non-threadsafe, if two of these calls (even different ones) run at the same time it *will* mess things up.
    bool lzmaDecompress(const std::vector<char>& in, std::vector<char>& out);
    bool lzmaCompress(const std::vector<char>& in, std::vector<char>& out);
}
