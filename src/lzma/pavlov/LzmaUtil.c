/* LzmaUtil.c -- Test application for LZMA compression
2021-11-01 : Igor Pavlov : Public domain */

// modified for our purposes to directly encode/decode files

#include "lzma/pavlov/Precomp.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lzma/pavlov/CpuArch.h"

#include "lzma/pavlov/Alloc.h"
#include "lzma/pavlov/7zFile.h"
#include "lzma/pavlov/7zVersion.h"
#include "lzma/pavlov/LzFind.h"
#include "lzma/pavlov/LzmaDec.h"
#include "lzma/pavlov/LzmaEnc.h"


#include "lzma/pavlov/LzmaUtil.h"


#define IN_BUF_SIZE (1 << 16)
#define OUT_BUF_SIZE (1 << 16)


static SRes Decode2(CLzmaDec *state, ISeqOutStream *outStream, ISeqInStream *inStream,
    UInt64 unpackSize)
{
  int thereIsSize = (unpackSize != (UInt64)(Int64)-1);
  Byte inBuf[IN_BUF_SIZE];
  Byte outBuf[OUT_BUF_SIZE];
  size_t inPos = 0, inSize = 0, outPos = 0;
  LzmaDec_Init(state);
  for (;;)
  {
    if (inPos == inSize)
    {
      inSize = IN_BUF_SIZE;
      RINOK(inStream->Read(inStream, inBuf, &inSize));
      inPos = 0;
    }
    {
      SRes res;
      SizeT inProcessed = inSize - inPos;
      SizeT outProcessed = OUT_BUF_SIZE - outPos;
      ELzmaFinishMode finishMode = LZMA_FINISH_ANY;
      ELzmaStatus status;
      if (thereIsSize && outProcessed > unpackSize)
      {
        outProcessed = (SizeT)unpackSize;
        finishMode = LZMA_FINISH_END;
      }

      res = LzmaDec_DecodeToBuf(state, outBuf + outPos, &outProcessed,
        inBuf + inPos, &inProcessed, finishMode, &status);
      inPos += inProcessed;
      outPos += outProcessed;
      unpackSize -= outProcessed;

      if (outStream)
        if (outStream->Write(outStream, outBuf, outPos) != outPos)
          return SZ_ERROR_WRITE;

      outPos = 0;

      if (res != SZ_OK || (thereIsSize && unpackSize == 0))
        return res;

      if (inProcessed == 0 && outProcessed == 0)
      {
        if (thereIsSize || status != LZMA_STATUS_FINISHED_WITH_MARK)
          return SZ_ERROR_DATA;
        return res;
      }
    }
  }
}


SRes Decode(ISeqOutStream *outStream, ISeqInStream *inStream)
{
  UInt64 unpackSize;
  int i;
  SRes res = 0;

  CLzmaDec state;

  /* header: 5 bytes of LZMA properties and 8 bytes of uncompressed size */
  unsigned char header[LZMA_PROPS_SIZE + 8];

  /* Read and parse header */

  RINOK(SeqInStream_Read(inStream, header, sizeof(header)));

  unpackSize = 0;
  for (i = 0; i < 8; i++)
    unpackSize += (UInt64)header[LZMA_PROPS_SIZE + i] << (i * 8);

  LzmaDec_Construct(&state);
  RINOK(LzmaDec_Allocate(&state, header, LZMA_PROPS_SIZE, &g_Alloc));
  res = Decode2(&state, outStream, inStream, unpackSize);
  LzmaDec_Free(&state, &g_Alloc);
  return res;
}

SRes Encode(ISeqOutStream *outStream, ISeqInStream *inStream, UInt64 fileSize)
{
  CLzmaEncHandle enc;
  SRes res;
  CLzmaEncProps props;

  enc = LzmaEnc_Create(&g_Alloc);
  if (enc == 0)
    return SZ_ERROR_MEM;

  LzmaEncProps_Init(&props);
  res = LzmaEnc_SetProps(enc, &props);

  if (res == SZ_OK)
  {
    Byte header[LZMA_PROPS_SIZE + 8];
    size_t headerSize = LZMA_PROPS_SIZE;
    int i;

    res = LzmaEnc_WriteProperties(enc, header, &headerSize);
    for (i = 0; i < 8; i++)
      header[headerSize++] = (Byte)(fileSize >> (8 * i));
    if (outStream->Write(outStream, header, headerSize) != headerSize)
      res = SZ_ERROR_WRITE;
    else
    {
      if (res == SZ_OK)
        res = LzmaEnc_Encode(enc, outStream, inStream, NULL, &g_Alloc, &g_Alloc);
    }
  }
  LzmaEnc_Destroy(enc, &g_Alloc, &g_Alloc);
  return res;
}


int lzmaEncode(const char* infile_name, const char* outfile_name)
{
  CFileSeqInStream inStream;
  CFileOutStream outStream;
  char c;
  int res;

  LzFindPrepare();

  FileSeqInStream_CreateVTable(&inStream);
  File_Construct(&inStream.file);
  inStream.wres = 0;

  FileOutStream_CreateVTable(&outStream);
  File_Construct(&outStream.file);
  outStream.wres = 0;

  WRes wres = InFile_Open(&inStream.file, infile_name);
  if (wres != 0)
    return 0;
  wres = OutFile_Open(&outStream.file, outfile_name);
  if (wres != 0)
    return 0;

  UInt64 fileSize;
  wres = File_GetLength(&inStream.file, &fileSize);
  if (wres != 0)
    return 0;
  res = Encode(&outStream.vt, &inStream.vt, fileSize);

  File_Close(&outStream.file);
  File_Close(&inStream.file);

  return res == SZ_OK;
}

int lzmaDecodeF(SRes (*Read)(void *buf, size_t *size), size_t (*Write)(const void *buf, size_t size))
{

}


int lzmaDecode(const char* infile_name, const char* outfile_name)
{
  CFileSeqInStream inStream;
  CFileOutStream outStream;
  char c;
  int res;

  LzFindPrepare();

  FileSeqInStream_CreateVTable(&inStream);
  File_Construct(&inStream.file);
  inStream.wres = 0;

  FileOutStream_CreateVTable(&outStream);
  File_Construct(&outStream.file);
  outStream.wres = 0;

  WRes wres = InFile_Open(&inStream.file, infile_name);
  if (wres != 0)
    return 0;
  wres = OutFile_Open(&outStream.file, outfile_name);
  if (wres != 0)
    return 0;

  res = Decode(&outStream.vt, &inStream.vt);

  File_Close(&outStream.file);
  File_Close(&inStream.file);

  return res == SZ_OK;
}
