#include <stdint.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include "simd_platform.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <time.h>
#endif

uint64_t GetCurrentTimeTicks();
uint64_t TicksToNs(const uint64_t ticks);

constexpr uint32_t TotalSymbolCountBits = 15;
constexpr uint32_t TotalSymbolCount = ((uint32_t)1 << TotalSymbolCountBits);
constexpr size_t DecodeConsumePoint = 1 << 23;
constexpr size_t EncodeEmitPoint = ((DecodeConsumePoint >> TotalSymbolCountBits) << 8);

static_assert(TotalSymbolCountBits < 16);

constexpr size_t RunCount = 10;
static uint64_t _ClocksPerRun[RunCount];
static uint64_t _NsPerRun[RunCount];

void print_perf_info(const size_t fileSize)
{
  uint64_t completeNs = 0;
  uint64_t completeClocks = 0;

  for (size_t i = 0; i < RunCount; i++)
  {
    completeNs += _NsPerRun[i];
    completeClocks += _ClocksPerRun[i];
  }

  const double meanNs = completeNs / (double)RunCount;
  const double meanClocks = completeClocks / (double)RunCount;
  double stdDevNs = 0;
  double stdDevClocks = 0;

  for (size_t i = 0; i < RunCount; i++)
  {
    const double diffNs = _NsPerRun[i] - meanNs;
    const double diffClocks = _ClocksPerRun[i] - meanClocks;

    stdDevNs += diffNs * diffNs;
    stdDevClocks += diffClocks * diffClocks;
  }

  stdDevNs = sqrt(stdDevNs / (double)(RunCount - 1));
  stdDevClocks = sqrt(stdDevClocks / (double)(RunCount - 1));

  printf("\nAverage: \t%5.3f clk/byte\t(std dev: %5.3f ~ %5.3f)\n", meanClocks / fileSize, (meanClocks - stdDevClocks) / fileSize, (meanClocks + stdDevClocks) / fileSize);
  printf("Average: \t%5.3f MiB/s\t(std dev: %5.3f ~ %5.3f)\n\n", (fileSize / (1024.0 * 1024.0)) / (meanNs * 1e-9), (fileSize / (1024.0 * 1024.0)) / ((meanNs + stdDevNs) * 1e-9), (fileSize / (1024.0 * 1024.0)) / ((meanNs - stdDevNs) * 1e-9));
}

struct hist_t
{
  uint16_t symbolCount[256];
  uint16_t cumul[256];
};

typedef uint32_t enc_sym_t;

struct hist_enc_t
{
  enc_sym_t symbols[256];
};

struct hist_dec_t : hist_t
{
  uint8_t cumulInv[TotalSymbolCount];
};

void make_hist(hist_t *pHist, const uint8_t *pData, const size_t size)
{
  uint32_t hist[256];
  memset(hist, 0, sizeof(hist));

  for (size_t i = 0; i < size; i++)
    hist[pData[i]]++;

  uint32_t counter = 0;

  for (size_t i = 0; i < 256; i++)
    counter += hist[i];

  uint16_t capped[256];
  size_t cappedSum = 0;

  constexpr bool FloatingPointHistLimit = true;

  if constexpr (FloatingPointHistLimit)
  {
    const float mul = (float)TotalSymbolCount / counter;

    for (size_t i = 0; i < 256; i++)
    {
      capped[i] = (uint16_t)(hist[i] * mul + 0.5f);

      if (capped[i] == 0 && hist[i])
        capped[i] = 1;

      cappedSum += capped[i];
    }
  }
  else
  {
    const uint32_t div = counter / TotalSymbolCount;

    if (div)
    {
      const uint32_t add = div / 2;

      for (size_t i = 0; i < 256; i++)
      {
        capped[i] = (uint16_t)((hist[i] + add) / div);

        if (capped[i] == 0 && hist[i])
          capped[i] = 1;

        cappedSum += capped[i];
      }
    }
    else
    {
      const uint32_t mul = TotalSymbolCount / counter;

      for (size_t i = 0; i < 256; i++)
      {
        capped[i] = (uint16_t)(hist[i] * mul);
        cappedSum += capped[i];
      }
    }
  }

  printf("make_hist archived sum on try 1: %" PRIu64 " (from %" PRIu64 ", target %" PRIu32 ")\n\n", cappedSum, size, TotalSymbolCount);

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
  if (cappedSum != TotalSymbolCount)
  {
    puts("Invalid Symbol Count.");
    exit(1);
  }

  counter = 0;

  for (size_t i = 0; i < 256; i++)
  {
    pHist->cumul[i] = (uint16_t)counter;
    pHist->symbolCount[i] = capped[i];
    counter += capped[i];
  }
}

void make_enc_hist(hist_enc_t *pHistEnc, const hist_t *pHist)
{
  for (size_t i = 0; i < 256; i++)
    pHistEnc->symbols[i] = pHist->cumul[i] << 16 | pHist->symbolCount[i];
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

inline uint8_t decode_symbol(uint32_t *pState, const hist_dec_t *pHist)
{
  const uint32_t state = *pState;
  const uint32_t slot = state & (TotalSymbolCount - 1);
  const uint8_t symbol = pHist->cumulInv[slot];
  const uint32_t previousState = (state >> TotalSymbolCountBits) * pHist->symbolCount[symbol] + slot - pHist->cumul[symbol];

  *pState = previousState;

  return symbol;
}

inline uint32_t encode_symbol_basic(const uint8_t symbol, const hist_t *pHist, const uint32_t state)
{
  const uint32_t symbolCount = pHist->symbolCount[symbol];

  return ((state / symbolCount) << TotalSymbolCountBits) + pHist->cumul[symbol] + (state % symbolCount);
}

inline uint8_t decode_symbol_basic(uint32_t *pState, const hist_dec_t *pHist)
{
  const uint32_t state = *pState;
  const uint32_t slot = state & (TotalSymbolCount - 1);
  const uint8_t symbol = pHist->cumulInv[slot];
  const uint32_t previousState = (state >> TotalSymbolCountBits) * (uint32_t)pHist->symbolCount[symbol] + slot - (uint32_t)pHist->cumul[symbol];

  *pState = previousState;

  return symbol;
}

#ifdef _MSC_VER
__declspec(noinline)
#endif
size_t encode(const uint8_t *pInData, const size_t length, uint8_t *pOutData, const size_t outCapacity, const hist_enc_t *pHist)
{
  if (outCapacity < length + sizeof(uint32_t) * (256 + 1))
    return 0;

  uint32_t state = DecodeConsumePoint; // technically `n * TotalSymbolCount`.
  uint8_t *pWrite = pOutData + outCapacity - 1;

  for (int64_t i = length - 1; i >= 0; i--)
  {
    const uint8_t in = pInData[i];
    const enc_sym_t pack = pHist->symbols[in];

    const uint32_t freq = pack & 0xFFFF;
    const uint32_t cumul = pack >> 16;
    const uint32_t max = EncodeEmitPoint * freq;
   
    while (state >= max)
    {
      *pWrite = (uint8_t)(state & 0xFF);
      pWrite--;
      state >>= 8;
    }

#ifndef _MSC_VER
    const uint32_t stateDivFreq = state / freq;
    const uint32_t stateModFreq = state % freq;
#else

    uint32_t stateDivFreq, stateModFreq;
    stateDivFreq = _udiv64(state, freq, &stateModFreq);
#endif

    state = (stateDivFreq << TotalSymbolCountBits) + cumul + stateModFreq;
  }

  *reinterpret_cast<uint32_t *>(pOutData) = state;

  const size_t writtenSize = (pOutData + outCapacity - 1) - pWrite;
  memmove(pOutData + sizeof(uint32_t), pWrite + 1, writtenSize);

  return writtenSize + sizeof(uint32_t);
}

#ifdef _MSC_VER
__declspec(noinline)
#endif
size_t decode(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outLength, const hist_dec_t *pHist)
{
  (void)inLength;

  uint32_t state = *reinterpret_cast<const uint32_t *>(pInData);
  size_t inIndex = sizeof(uint32_t);

  for (size_t i = 0; i < outLength; i++)
  {
    const uint32_t slot = state & (TotalSymbolCount - 1);
    const uint8_t symbol = pHist->cumulInv[slot];
    state = (state >> TotalSymbolCountBits) * (uint32_t)pHist->symbolCount[symbol] + slot - (uint32_t)pHist->cumul[symbol];

    pOutData[i] = symbol;

    while (state < DecodeConsumePoint)
      state = state << 8 | pInData[inIndex++];
  }

  return outLength;
}

#ifdef _MSC_VER
__declspec(noinline)
#endif
size_t encode_basic(const uint8_t *pInData, const size_t length, uint8_t *pOutData, const size_t outCapacity, const hist_t *pHist)
{
  if (outCapacity < length + sizeof(uint32_t) * (256 + 1))
    return 0;

  uint32_t state = DecodeConsumePoint; // technically `n * TotalSymbolCount`.
  uint8_t *pWrite = pOutData + outCapacity - 1;

  for (int64_t i = length - 1; i >= 0; i--)
  {
    const uint8_t in = pInData[i];
    const uint32_t symbolCount = pHist->symbolCount[in];
    const uint32_t max = EncodeEmitPoint * symbolCount;

    while (state >= max)
    {
      *pWrite = (uint8_t)(state & 0xFF);
      pWrite--;
      state >>= 8;
    }

    state = ((state / symbolCount) << TotalSymbolCountBits) + (uint32_t)pHist->cumul[in] + (state % symbolCount);
  }

  *reinterpret_cast<uint32_t *>(pOutData) = state;

  const size_t writtenSize = (pOutData + outCapacity - 1) - pWrite;
  memmove(pOutData + sizeof(uint32_t), pWrite + 1, writtenSize);

  return writtenSize + sizeof(uint32_t);
}

#ifdef _MSC_VER
__declspec(noinline)
#endif
size_t decode_basic(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outLength, const hist_dec_t *pHist)
{
  (void)inLength;

  uint32_t state = *reinterpret_cast<const uint32_t *>(pInData);
  size_t inIndex = sizeof(uint32_t);

  for (size_t i = 0; i < outLength; i++)
  {
    pOutData[i] = decode_symbol_basic(&state, pHist);

    while (state < DecodeConsumePoint)
      state = state << 8 | pInData[inIndex++];
  }

  return outLength;
}

int32_t main(const int32_t argc, char **pArgv)
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

  size_t symCount = 0;

  for (size_t i = 0; i < 256; i++)
    symCount += (size_t)!!hist.symbolCount[i];

  if (symCount < 8)
  {
    puts("Hist:");

    for (size_t i = 0; i < 256; i++)
      if (hist.symbolCount[i])
        printf("0x%02" PRIX8 "(%c): %" PRIu16 " (%" PRIu16 ")\n", (uint8_t)i, (char)i, hist.symbolCount[i], hist.cumul[i]);

    puts("");
  }

  hist_enc_t histEnc;
  make_enc_hist(&histEnc, &hist);

  size_t compressedLength = 0;

  {
    compressedLength = encode_basic(pUncompressedData, fileSize, pCompressedData, compressedDataCapacity, &hist);

    for (size_t run = 0; run < RunCount; run++)
    {
      const uint64_t startTick = GetCurrentTimeTicks();
      const uint64_t startClock = __rdtsc();
      compressedLength = encode_basic(pUncompressedData, fileSize, pCompressedData, compressedDataCapacity, &hist);
      const uint64_t endClock = __rdtsc();
      const uint64_t endTick = GetCurrentTimeTicks();

      _mm_mfence();

      _NsPerRun[run] = TicksToNs(endTick - startTick);
      _ClocksPerRun[run] = endClock - startClock;

      printf("encode_basic: \t%" PRIu64 " bytes from %" PRIu64 " bytes. (%5.3f %%, %6.3f clocks/byte, %5.2f MiB/s)\n", compressedLength, fileSize, compressedLength / (double)fileSize * 100.0, (endClock - startClock) / (double)fileSize, (fileSize / (1024.0 * 1024.0)) / (TicksToNs(endTick - startTick) * 1e-9));
    }

    print_perf_info(fileSize);
  }

  puts("");

  {
    compressedLength = encode(pUncompressedData, fileSize, pCompressedData, compressedDataCapacity, &histEnc);

    for (size_t run = 0; run < RunCount; run++)
    {
      const uint64_t startTick = GetCurrentTimeTicks();
      const uint64_t startClock = __rdtsc();
      compressedLength = encode(pUncompressedData, fileSize, pCompressedData, compressedDataCapacity, &histEnc);
      const uint64_t endClock = __rdtsc();
      const uint64_t endTick = GetCurrentTimeTicks();

      _mm_mfence();

      _NsPerRun[run] = TicksToNs(endTick - startTick);
      _ClocksPerRun[run] = endClock - startClock;

      printf("encode: \t%" PRIu64 " bytes from %" PRIu64 " bytes. (%5.3f %%, %6.3f clocks/byte, %5.2f MiB/s)\n", compressedLength, fileSize, compressedLength / (double)fileSize * 100.0, (endClock - startClock) / (double)fileSize, (fileSize / (1024.0 * 1024.0)) / (TicksToNs(endTick - startTick) * 1e-9));
    }

    print_perf_info(fileSize);
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

  {
    decompressedLength = decode_basic(pCompressedData, compressedLength, pDecompressedData, fileSize, &histDec);

    for (size_t run = 0; run < RunCount; run++)
    {
      const uint64_t startTick = GetCurrentTimeTicks();
      const uint64_t startClock = __rdtsc();
      decompressedLength = decode_basic(pCompressedData, compressedLength, pDecompressedData, fileSize, &histDec);
      const uint64_t endClock = __rdtsc();
      const uint64_t endTick = GetCurrentTimeTicks();

      _mm_mfence();

      _NsPerRun[run] = TicksToNs(endTick - startTick);
      _ClocksPerRun[run] = endClock - startClock;

      printf("decode_basic: \tdecompressed to %" PRIu64 " bytes (should be %" PRIu64 "). (%6.3f clocks/byte, %5.2f MiB/s)\n", decompressedLength, fileSize, (endClock - startClock) / (double)fileSize, (fileSize / (1024.0 * 1024.0)) / (TicksToNs(endTick - startTick) * 1e-9));
    }

    print_perf_info(fileSize);
  }

  puts("");

  {
    decompressedLength = decode(pCompressedData, compressedLength, pDecompressedData, fileSize, &histDec);

    for (size_t run = 0; run < RunCount; run++)
    {
      const uint64_t startTick = GetCurrentTimeTicks();
      const uint64_t startClock = __rdtsc();
      decompressedLength = decode(pCompressedData, compressedLength, pDecompressedData, fileSize, &histDec);
      const uint64_t endClock = __rdtsc();
      const uint64_t endTick = GetCurrentTimeTicks();

      _mm_mfence();

      _NsPerRun[run] = TicksToNs(endTick - startTick);
      _ClocksPerRun[run] = endClock - startClock;

      printf("decode: \tdecompressed to %" PRIu64 " bytes (should be %" PRIu64 "). (%6.3f clocks/byte, %5.2f MiB/s)\n", decompressedLength, fileSize, (endClock - startClock) / (double)fileSize, (fileSize / (1024.0 * 1024.0)) / (TicksToNs(endTick - startTick) * 1e-9));
    }

    print_perf_info(fileSize);
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

    puts("");
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
