#ifndef __LZMA_UTIL_H
#define __LZMA_UTIL_H

#include "Precomp.h"
#include "7zFile.h"

SRes Encode(ISeqOutStream *outStream, ISeqInStream *inStream, UInt64 fileSize);
SRes Decode(ISeqOutStream *outStream, ISeqInStream *inStream);

#endif