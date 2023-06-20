#include <stdint.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include "simd_platform.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

uint64_t GetCurrentTimeTicks();
uint64_t TicksToNs(const uint64_t ticks);

constexpr uint32_t TotalSymbolCountBits = 12;
constexpr uint32_t TotalSymbolCount = ((uint32_t)1 << TotalSymbolCountBits);

struct hist_t
{
  uint32_t symbolCount[256];
  uint32_t cumul[256];
  uint32_t total;
};

struct hist_dec_t : hist_t
{
  uint8_t cumulInv[TotalSymbolCount];
};

void make_hist(hist_t *pHist, const uint8_t *pData, const size_t size)
{
  memset(pHist, 0, sizeof(hist_t));

  for (size_t i = 0; i < size; i++)
    pHist->symbolCount[pData[i]]++;

  uint32_t counter = 0;

  for (size_t i = 0; i < 256; i++)
  {
    pHist->cumul[i] = counter;
    counter += pHist->symbolCount[i];
  }

  pHist->total = counter;

  const uint32_t div = counter / TotalSymbolCount;

  uint32_t capped[256];
  uint32_t cappedSum = 0;

  if (div)
  {
    const uint32_t add = div / 2;

    for (size_t i = 0; i < 256; i++)
    {
      capped[i] = ((pHist->symbolCount[i] + add) / div) | (uint32_t)!!(pHist->symbolCount[i]);
      cappedSum += capped[i];
    }
  }
  else
  {
    const uint32_t mul = TotalSymbolCount / counter;

    for (size_t i = 0; i < 256; i++)
    {
      capped[i] = pHist->symbolCount[i] * mul;
      cappedSum += capped[i];
    }
  }

  printf("make_hist archived sum on try 1: %" PRIu32 " (from %" PRIu64 ", target %" PRIu32 ")\n\n", cappedSum, size, TotalSymbolCount);

  if (cappedSum != TotalSymbolCount)
  {
    while (cappedSum > TotalSymbolCount) // Start stealing.
    {
      size_t target = 2;

      while (true)
      {
        size_t found = TotalSymbolCount;

        for (size_t i = 0; i < 256; i++)
          if (capped[i] > target && capped[i] < found)
            found = capped[i];

        if (found == TotalSymbolCount)
          break;

        for (size_t i = 0; i < 256; i++)
        {
          if (capped[i] == found)
          {
            capped[i]--;
            cappedSum--;

            if (cappedSum == TotalSymbolCount)
              goto hist_ready;
          }
        }

        target = found + 1;
      }
    }
    
    while (cappedSum < TotalSymbolCount) // Start a charity.
    {
      size_t target = TotalSymbolCount;

      while (true)
      {
        size_t found = 1;

        for (size_t i = 0; i < 256; i++)
          if (capped[i] < target && capped[i] > found)
            found = capped[i];

        if (found == 1)
          break;

        for (size_t i = 0; i < 256; i++)
        {
          if (capped[i] == found)
          {
            capped[i]++;
            cappedSum++;

            if (cappedSum == TotalSymbolCount)
              goto hist_ready;
          }
        }

        target = found - 1;
      }
    }
  }

hist_ready:

  counter = 0;

  for (size_t i = 0; i < 256; i++)
  {
    pHist->cumul[i] = counter;
    pHist->symbolCount[i] = capped[i];
    counter += capped[i];
  }

  pHist->total = counter;
}

void make_dec_hist(hist_dec_t *pHistDec, const hist_t *pHist)
{
  memcpy(pHistDec, pHist, sizeof(hist_t));

  uint8_t sym = 0;

  for (size_t i = 0; i < TotalSymbolCount; i++)
  {
    while (sym != 0xFF && (!pHist->symbolCount[sym] || pHist->cumul[sym + 1] <= i))
      sym++;

    pHistDec->cumulInv[i] = sym;
  }
}

uint32_t encode_symbol(const uint8_t symbol, const hist_t *pHist, const uint32_t state)
{
  const uint32_t symbolCount = pHist->symbolCount[symbol];

  return (state / symbolCount) * TotalSymbolCount + pHist->cumul[symbol] + (state % symbolCount);
}

uint8_t decode_symbol(uint32_t *pState, const hist_dec_t *pHist)
{
  const uint32_t slot = *pState % TotalSymbolCount;
  const uint8_t symbol = pHist->cumulInv[slot];
  const uint32_t previousState = (*pState / TotalSymbolCount) * pHist->symbolCount[symbol] + slot - pHist->cumul[symbol];

  *pState = previousState;

  return symbol;
}

constexpr size_t LowLevel = 1 << 23;
constexpr size_t RangeFactor = ((LowLevel >> TotalSymbolCountBits) << 8);

size_t encode(const uint8_t *pInData, const size_t length, uint8_t *pOutData, const size_t outCapacity, const hist_t *pHist)
{
  if (outCapacity < length + sizeof(uint32_t) * (256 + 1))
    return 0;

  uint32_t state = LowLevel; // technically `n * TotalSymbolCount`.
  uint8_t *pWrite = pOutData + outCapacity - 1;

  for (int64_t i = length - 1; i >= 0; i--)
  {
    const uint8_t in = pInData[i];
    const uint32_t max = RangeFactor * pHist->symbolCount[in];
   
    while (state >= max)
    {
      *pWrite = (uint8_t)(state & 0xFF);
      pWrite--;
      state >>= 8;
    }

    state = encode_symbol(in, pHist, state);
  }

  *reinterpret_cast<uint32_t *>(pOutData) = state;

  const size_t writtenSize = (pOutData + outCapacity - 1) - pWrite;
  memmove(pOutData + sizeof(uint32_t), pWrite + 1, writtenSize);

  return writtenSize + sizeof(uint32_t);
}

size_t decode(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outLength, const hist_dec_t *pHist)
{
  (void)inLength;

  uint32_t state = *reinterpret_cast<const uint32_t *>(pInData);
  size_t inIndex = sizeof(uint32_t);

  for (size_t i = 0; i < outLength; i++)
  {
    pOutData[i] = decode_symbol(&state, pHist);

    while (state < LowLevel)
      state = state << 8 | pInData[inIndex++];
  }

  return outLength;
}

int32_t main(const int64_t argc, char **pArgv)
{
  if (argc == 0)
  {
    puts("Invalid Parameter.");
    return 1;
  }

  size_t fileSize = 0;
  size_t compressedDataCapacity = 0;

  uint8_t *pUncompressedData = nullptr;
  uint8_t *pCompressedData = nullptr;
  uint8_t *pDecompressedData = nullptr;

  // Read File.
  {
    FILE *pFile = fopen(pArgv[1], "rb");

    if (!pFile)
    {
      puts("Failed to read file.");
      return 1;
    }

    fseek(pFile, 0, SEEK_END);
    fileSize = ftell(pFile);

    if (fileSize <= 0)
    {
      puts("Invalid File size / failed to read file.");
      fclose(pFile);
      return 1;
    }

    fseek(pFile, 0, SEEK_SET);

    pUncompressedData = (uint8_t *)malloc(fileSize);
    pDecompressedData = (uint8_t *)malloc(fileSize);

    compressedDataCapacity = fileSize + sizeof(uint32_t) * (256 + 1);
    pCompressedData = (uint8_t *)malloc(compressedDataCapacity);

    if (pUncompressedData == nullptr || pDecompressedData == nullptr || pCompressedData == nullptr)
    {
      puts("Memory allocation failure.");
      fclose(pFile);
      return 1;
    }

    if (fileSize != fread(pUncompressedData, 1, fileSize, pFile))
    {
      puts("Failed to read file.");
      fclose(pFile);
      return 1;
    }

    fclose(pFile);
  }

  hist_t hist;
  make_hist(&hist, pUncompressedData, fileSize);

  if (hist.total != TotalSymbolCount)
  {
    puts("Ahhhh! Not `TotalSymbolCount`!");
    return 1;
  }

  size_t symCount = 0;

  for (size_t i = 0; i < 256; i++)
    symCount += (size_t)!!hist.symbolCount[i];

  if (symCount < 8)
  {
    puts("Hist:");

    for (size_t i = 0; i < 256; i++)
      if (hist.symbolCount[i])
        printf("0x%02" PRIX8 "(%c): %" PRIu32 " (%" PRIu32 ")\n", (uint8_t)i, (char)i, hist.symbolCount[i], hist.cumul[i]);

    puts("");
  }

  size_t compressedLength = 0;

  for (size_t run = 0; run < 10; run++)
  {
    const uint64_t startTick = GetCurrentTimeTicks();
    const uint64_t startClock = __rdtsc();
    compressedLength = encode(pUncompressedData, fileSize, pCompressedData, compressedDataCapacity, &hist);
    const uint64_t endClock = __rdtsc();
    const uint64_t endTick = GetCurrentTimeTicks();

    _mm_mfence();

    printf("%" PRIu64 " bytes from %" PRIu64 " bytes. (%6.3f clocks/byte, %5.2f MiB/s)\n", compressedLength, fileSize, (endClock - startClock) / (double)fileSize, (fileSize / (1024.0 * 1024.0)) / (TicksToNs(endTick - startTick) * 1e-9));
  }

  puts("");

  if (compressedLength < 32 * 8)
  {
    printf("Compressed: (state: %" PRIu32 ")\n\n", *reinterpret_cast<const uint32_t *>(pCompressedData));

    for (size_t i = 0; i < compressedLength; i += 32)
    {
      const size_t max = i + 32 > compressedLength ? compressedLength : i + 32;
      size_t j;

      for (j = i; j < max; j++)
        printf("%02" PRIX8 " ", pCompressedData[j]);

      puts("");
    }

    puts("");
  }

  hist_dec_t histDec;
  make_dec_hist(&histDec, &hist);

  if (symCount < 12)
  {
    puts("DecHist:");

    for (size_t i = 0; i < TotalSymbolCount;)
    {
      const uint8_t sym = histDec.cumulInv[i];

      size_t j = i + 1;

      for (; j < TotalSymbolCount; j++)
        if (histDec.cumulInv[j] != sym)
          break;

      printf("%02" PRIX8 "(%c): %" PRIu64 " ~ %" PRIu64 "\n", sym, (char)sym, i, j - 1);

      i = j;
    }

    puts("");
  }


  size_t decompressedLength = 0;

  for (size_t run = 0; run < 10; run++)
  {
    const uint64_t startTick = GetCurrentTimeTicks();
    const uint64_t startClock = __rdtsc();
    decompressedLength = decode(pCompressedData, compressedLength, pDecompressedData, fileSize, &histDec);
    const uint64_t endClock = __rdtsc();
    const uint64_t endTick = GetCurrentTimeTicks();

    _mm_mfence();

    printf("decompressed to %" PRIu64 " bytes (should be %" PRIu64 "). (%6.3f clocks/byte, %5.2f MiB/s)\n", decompressedLength, fileSize, (endClock - startClock) / (double)fileSize, (fileSize / (1024.0 * 1024.0)) / (TicksToNs(endTick - startTick) * 1e-9));
  }

  puts("");

  if (decompressedLength != fileSize)
  {
    puts("Decompressed Length differs from initial file size.");
    return 1;
  }

  if (fileSize < 32 * 8)
  {
    puts("Input:                                              Output:\n");

    for (size_t i = 0; i < fileSize; i += 16)
    {
      const size_t max = i + 16 > fileSize ? fileSize : i + 16;
      size_t j;

      for (j = i; j < max; j++)
        printf("%02" PRIX8 " ", pUncompressedData[j]);

      for (; j < i + 16; j++)
        printf("   ");

      printf(" |  ");

      for (j = i; j < max; j++)
        printf("%02" PRIX8 " ", pDecompressedData[j]);

      for (; j < i + 16; j++)
        printf("   ");

      puts("");

      if (memcmp(pUncompressedData + i, pUncompressedData + i, max - i) != 0)
      {
        for (j = i; j < max; j++)
          printf(pUncompressedData[j] != pDecompressedData[j] ? "~~ " : "   ");

        for (; j < i + 16; j++)
          printf("   ");

        printf(" |  ");

        for (j = i; j < max; j++)
          printf(pUncompressedData[j] != pDecompressedData[j] ? "~~ " : "   ");

        for (; j < i + 16; j++)
          printf("   ");

        puts("");
      }
    }
  }

  if (memcmp(pDecompressedData, pUncompressedData, fileSize) != 0)
  {
    puts("Failed to decompress correctly.");
    return 1;
  }
  else
  {
    puts("Success!");
  }

  return 0;
}

uint64_t GetCurrentTimeTicks()
{
#ifdef WIN32
  LARGE_INTEGER now;
  QueryPerformanceCounter(&now);

  return now.QuadPart;
#else
  struct timespec time;
  clock_gettime(CLOCK_REALTIME, &time);

  return (uint64_t)time.tv_sec * 1000000000 + (uint64_t)time.tv_nsec;
#endif
}

uint64_t TicksToNs(const uint64_t ticks)
{
#ifdef WIN32
  LARGE_INTEGER freq;
  QueryPerformanceFrequency(&freq);

  return (ticks * 1000 * 1000 * 1000) / freq.QuadPart;
#else
  return ticks;
#endif
}
