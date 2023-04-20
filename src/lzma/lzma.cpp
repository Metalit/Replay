#include "lzma/lzma.hpp"
#include <mutex>

namespace LZMA
{
    const std::vector<char> *input_data;
    int input_index;

    std::vector<char> *output_data;

    std::mutex lock; // lazy threadsafety

    SRes Read(const ISeqInStream *pp, void *buf, size_t *size)
    {
        size_t orig_size = *size;
        for(*size = 0; *size < orig_size && input_index < input_data->size(); ++*size) {
            reinterpret_cast<char*>(buf)[*size] = (*input_data)[input_index++];
        }
        return SZ_OK;
    }

    size_t Write(const ISeqOutStream *pp, const void *data, size_t size)
    {
        for(size_t i = 0; i < size; ++i)
            output_data->push_back(reinterpret_cast<const char*>(data)[i]);
        return size;
    }

    void initialize_input(const std::vector<char> &in, ISeqInStream &stream) {
        input_data = &in;
        input_index = 0;
        stream.Read = Read;
    }

    void initialize_output(std::vector<char> &out, ISeqOutStream &stream) {
        output_data = &out;
        stream.Write = Write;
    }

    bool lzmaDecompress(const std::vector<char> &in, std::vector<char> &out) {
        std::lock_guard<std::mutex> guard(lock);

        ISeqInStream instream;
        ISeqOutStream outstream;
        initialize_input(in, instream);
        initialize_output(out, outstream);

        return Decode(&outstream, &instream) == SZ_OK;
    }

    bool lzmaCompress(const std::vector<char> &in, std::vector<char> &out) {
        std::lock_guard<std::mutex> guard(lock);

        ISeqInStream instream;
        ISeqOutStream outstream;
        initialize_input(in, instream);
        initialize_output(out, outstream);

        return Encode(&outstream, &instream, in.size()) == SZ_OK;
    }
}
