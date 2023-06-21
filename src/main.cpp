#include <stdint.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include "simd_platform.h"
#include "rans32x1.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <time.h>
#endif

//////////////////////////////////////////////////////////////////////////

uint64_t GetCurrentTimeTicks();
uint64_t TicksToNs(const uint64_t ticks);

//////////////////////////////////////////////////////////////////////////

constexpr size_t RunCount = 10;
static uint64_t _ClocksPerRun[RunCount];
static uint64_t _NsPerRun[RunCount];

//////////////////////////////////////////////////////////////////////////

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

//////////////////////////////////////////////////////////////////////////

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

    compressedDataCapacity = rANS32x1_capacity(fileSize);
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
    compressedLength = rANS32x1_encode_basic(pUncompressedData, fileSize, pCompressedData, compressedDataCapacity, &hist);

    for (size_t run = 0; run < RunCount; run++)
    {
      const uint64_t startTick = GetCurrentTimeTicks();
      const uint64_t startClock = __rdtsc();
      compressedLength = rANS32x1_encode_basic(pUncompressedData, fileSize, pCompressedData, compressedDataCapacity, &hist);
      const uint64_t endClock = __rdtsc();
      const uint64_t endTick = GetCurrentTimeTicks();

      _mm_mfence();

      _NsPerRun[run] = TicksToNs(endTick - startTick);
      _ClocksPerRun[run] = endClock - startClock;

      printf("encode_basic: \t%" PRIu64 " bytes from %" PRIu64 " bytes. (%5.3f %%, %6.3f clocks/byte, %5.2f MiB/s)\n", compressedLength, fileSize, compressedLength / (double)fileSize * 100.0, (endClock - startClock) / (double)fileSize, (fileSize / (1024.0 * 1024.0)) / (TicksToNs(endTick - startTick) * 1e-9));
    }

    print_perf_info(fileSize);
  }

  {
    compressedLength = rANS32x1_encode(pUncompressedData, fileSize, pCompressedData, compressedDataCapacity, &histEnc);

    for (size_t run = 0; run < RunCount; run++)
    {
      const uint64_t startTick = GetCurrentTimeTicks();
      const uint64_t startClock = __rdtsc();
      compressedLength = rANS32x1_encode(pUncompressedData, fileSize, pCompressedData, compressedDataCapacity, &histEnc);
      const uint64_t endClock = __rdtsc();
      const uint64_t endTick = GetCurrentTimeTicks();

      _mm_mfence();

      _NsPerRun[run] = TicksToNs(endTick - startTick);
      _ClocksPerRun[run] = endClock - startClock;

      printf("encode: \t%" PRIu64 " bytes from %" PRIu64 " bytes. (%5.3f %%, %6.3f clocks/byte, %5.2f MiB/s)\n", compressedLength, fileSize, compressedLength / (double)fileSize * 100.0, (endClock - startClock) / (double)fileSize, (fileSize / (1024.0 * 1024.0)) / (TicksToNs(endTick - startTick) * 1e-9));
    }

    print_perf_info(fileSize);
  }

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

  hist_dec2_t histDec2;
  make_dec2_hist(&histDec2, &hist);

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
    decompressedLength = rANS32x1_decode_basic(pCompressedData, compressedLength, pDecompressedData, fileSize, &histDec);

    for (size_t run = 0; run < RunCount; run++)
    {
      const uint64_t startTick = GetCurrentTimeTicks();
      const uint64_t startClock = __rdtsc();
      decompressedLength = rANS32x1_decode_basic(pCompressedData, compressedLength, pDecompressedData, fileSize, &histDec);
      const uint64_t endClock = __rdtsc();
      const uint64_t endTick = GetCurrentTimeTicks();

      _mm_mfence();

      _NsPerRun[run] = TicksToNs(endTick - startTick);
      _ClocksPerRun[run] = endClock - startClock;

      printf("decode_basic: \tdecompressed to %" PRIu64 " bytes (should be %" PRIu64 "). (%6.3f clocks/byte, %5.2f MiB/s)\n", decompressedLength, fileSize, (endClock - startClock) / (double)fileSize, (fileSize / (1024.0 * 1024.0)) / (TicksToNs(endTick - startTick) * 1e-9));
    }

    print_perf_info(fileSize);
  }

  {
    decompressedLength = rANS32x1_decode(pCompressedData, compressedLength, pDecompressedData, fileSize, &histDec2);

    for (size_t run = 0; run < RunCount; run++)
    {
      const uint64_t startTick = GetCurrentTimeTicks();
      const uint64_t startClock = __rdtsc();
      decompressedLength = rANS32x1_decode(pCompressedData, compressedLength, pDecompressedData, fileSize, &histDec2);
      const uint64_t endClock = __rdtsc();
      const uint64_t endTick = GetCurrentTimeTicks();

      _mm_mfence();

      _NsPerRun[run] = TicksToNs(endTick - startTick);
      _ClocksPerRun[run] = endClock - startClock;

      printf("decode: \tdecompressed to %" PRIu64 " bytes (should be %" PRIu64 "). (%6.3f clocks/byte, %5.2f MiB/s)\n", decompressedLength, fileSize, (endClock - startClock) / (double)fileSize, (fileSize / (1024.0 * 1024.0)) / (TicksToNs(endTick - startTick) * 1e-9));
    }

    print_perf_info(fileSize);
  }

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

//////////////////////////////////////////////////////////////////////////

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
