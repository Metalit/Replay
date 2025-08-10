#pragma once

// source: https://github.com/joyeecheung/md5

/*
 * Reference: https://www.ietf.org/rfc/rfc1321.txt
 */

#include <string>

namespace joyee {
    static constexpr short BLOCK_SIZE = 64;  // 16-word referece RFC 1321 3.4

    class MD5 {
       public:
        MD5();
        MD5& update(uint8_t const* input, uint32_t inputLen);
        MD5& update(char const* input, uint32_t inputLen);
        MD5& update(std::string const& input) { return update(input.c_str(), input.size()); }
        MD5& finalize();
        std::string toString() const;

       private:
        void init();
        void transform(uint8_t const block[BLOCK_SIZE]);

        uint8_t buffer[BLOCK_SIZE];  // buffer of the raw data
        uint8_t digest[16];  // result hash, little endian

        uint32_t state[4];  // state (ABCD)
        uint32_t lo, hi;  // number of bits, modulo 2^64 (lsb first)
        bool finalized;  // if the context has been finalized
    };

    std::string md5(std::string const str);
}
