#include <stdint.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include "simd_platform.h"
#include "rans32x1.h"
#include "rans32x32.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <time.h>
#endif

//////////////////////////////////////////////////////////////////////////

uint64_t GetCurrentTimeTicks();
uint64_t TicksToNs(const uint64_t ticks);
bool Validate(const uint8_t *pUncompressedData, const uint8_t *pDecompressedData, const size_t size);

inline size_t rans_max(const size_t a, const size_t b) { return a > b ? a : b; }
inline size_t rans_min(const size_t a, const size_t b) { return a < b ? a : b; }

//////////////////////////////////////////////////////////////////////////

constexpr size_t RunCount = 16;
static uint64_t _ClocksPerRun[RunCount];
static uint64_t _NsPerRun[RunCount];

//////////////////////////////////////////////////////////////////////////

void print_perf_info(const size_t fileSize)
{
  if constexpr (RunCount > 1)
  {
    uint64_t completeNs = 0;
    uint64_t completeClocks = 0;
    uint64_t minNs = (uint64_t)-1;
    uint64_t minClocks = (uint64_t)-1;

    for (size_t i = 0; i < RunCount; i++)
    {
      completeNs += _NsPerRun[i];
      completeClocks += _ClocksPerRun[i];

      if (_NsPerRun[i] < minNs)
        minNs = _NsPerRun[i];

      if (_ClocksPerRun[i] < minClocks)
        minClocks = _ClocksPerRun[i];
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

    printf("\nMin: \t%5.3f clk/byte\tAverage: \t%5.3f clk/byte\t(std dev: %5.3f ~ %5.3f)\n", minClocks / (double_t)fileSize, meanClocks / fileSize, (meanClocks - stdDevClocks) / fileSize, (meanClocks + stdDevClocks) / fileSize);
    printf("Max: \t%5.3f MiB/s\tAverage: \t%5.3f MiB/s\t(std dev: %5.3f ~ %5.3f)\n\n", (fileSize / (1024.0 * 1024.0)) / (minNs * 1e-9), (fileSize / (1024.0 * 1024.0)) / (meanNs * 1e-9), (fileSize / (1024.0 * 1024.0)) / ((meanNs + stdDevNs) * 1e-9), (fileSize / (1024.0 * 1024.0)) / ((meanNs - stdDevNs) * 1e-9));
  }
  else
  {
    puts("");
  }
}

//////////////////////////////////////////////////////////////////////////

size_t fileSize = 0;
size_t compressedDataCapacity = 0;

uint8_t *pUncompressedData = nullptr;
uint8_t *pCompressedData = nullptr;
uint8_t *pDecompressedData = nullptr;

size_t compressedLength = 0;

//////////////////////////////////////////////////////////////////////////

void profile_rans32x1_encode(hist_t *pHist)
{
  static hist_enc_t histEnc;
  make_enc_hist(&histEnc, pHist);

  {
    if constexpr (RunCount > 1)
      compressedLength = rANS32x1_encode_basic(pUncompressedData, fileSize, pCompressedData, compressedDataCapacity, pHist);

    for (size_t run = 0; run < RunCount; run++)
    {
      const uint64_t startTick = GetCurrentTimeTicks();
      const uint64_t startClock = __rdtsc();
      compressedLength = rANS32x1_encode_basic(pUncompressedData, fileSize, pCompressedData, compressedDataCapacity, pHist);
      const uint64_t endClock = __rdtsc();
      const uint64_t endTick = GetCurrentTimeTicks();

      _mm_mfence();

      _NsPerRun[run] = TicksToNs(endTick - startTick);
      _ClocksPerRun[run] = endClock - startClock;

      printf("\rrANS32x1_encode_basic: \t%" PRIu64 " bytes from %" PRIu64 " bytes. (%5.3f %%, %6.3f clocks/byte, %5.2f MiB/s)", compressedLength, fileSize, compressedLength / (double)fileSize * 100.0, (endClock - startClock) / (double)fileSize, (fileSize / (1024.0 * 1024.0)) / (TicksToNs(endTick - startTick) * 1e-9));
    }

    print_perf_info(fileSize);
  }

  {
    if constexpr (RunCount > 1)
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

      printf("\rrANS32x1_encode: \t%" PRIu64 " bytes from %" PRIu64 " bytes. (%5.3f %%, %6.3f clocks/byte, %5.2f MiB/s)", compressedLength, fileSize, compressedLength / (double)fileSize * 100.0, (endClock - startClock) / (double)fileSize, (fileSize / (1024.0 * 1024.0)) / (TicksToNs(endTick - startTick) * 1e-9));
    }

    print_perf_info(fileSize);
  }
}

void profile_rans32x32_encode(hist_t *pHist)
{
  {
    if constexpr (RunCount > 1)
      compressedLength = rANS32x32_encode_basic(pUncompressedData, fileSize, pCompressedData, compressedDataCapacity, pHist);

    for (size_t run = 0; run < RunCount; run++)
    {
      const uint64_t startTick = GetCurrentTimeTicks();
      const uint64_t startClock = __rdtsc();
      compressedLength = rANS32x32_encode_basic(pUncompressedData, fileSize, pCompressedData, compressedDataCapacity, pHist);
      const uint64_t endClock = __rdtsc();
      const uint64_t endTick = GetCurrentTimeTicks();

      _mm_mfence();

      _NsPerRun[run] = TicksToNs(endTick - startTick);
      _ClocksPerRun[run] = endClock - startClock;

      printf("\rrANS32x32_encode_basic: \t%" PRIu64 " bytes from %" PRIu64 " bytes. (%5.3f %%, %6.3f clocks/byte, %5.2f MiB/s)", compressedLength, fileSize, compressedLength / (double)fileSize * 100.0, (endClock - startClock) / (double)fileSize, (fileSize / (1024.0 * 1024.0)) / (TicksToNs(endTick - startTick) * 1e-9));
    }

    print_perf_info(fileSize);
  }
}

void profile_rans32x1_decode(hist_t *pHist)
{
  static hist_dec_t histDec;
  make_dec_hist(&histDec, pHist);

  static hist_dec2_t histDec2;
  make_dec2_hist(&histDec2, pHist);

  size_t decompressedLength = 0;

  {
    if constexpr (RunCount > 1)
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

      printf("\rrANS32x1_decode_basic: \tdecompressed to %" PRIu64 " bytes (should be %" PRIu64 "). (%6.3f clocks/byte, %5.2f MiB/s)", decompressedLength, fileSize, (endClock - startClock) / (double)fileSize, (fileSize / (1024.0 * 1024.0)) / (TicksToNs(endTick - startTick) * 1e-9));
    }

    print_perf_info(fileSize);
  }

  {
    if constexpr (RunCount > 1)
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

      printf("\rrANS32x1_decode: \tdecompressed to %" PRIu64 " bytes (should be %" PRIu64 "). (%6.3f clocks/byte, %5.2f MiB/s)", decompressedLength, fileSize, (endClock - startClock) / (double)fileSize, (fileSize / (1024.0 * 1024.0)) / (TicksToNs(endTick - startTick) * 1e-9));
    }

    print_perf_info(fileSize);
  }

  if (decompressedLength != fileSize)
  {
    puts("Decompressed Length differs from initial file size.");
    exit(1);
  }
}

void profile_rans32x32_decode(hist_t *pHist)
{
  static hist_dec_t histDec;
  make_dec_hist(&histDec, pHist);

  static hist_dec2_t histDec2;
  make_dec2_hist(&histDec2, pHist);

  size_t decompressedLength = 0;

  {
    if constexpr (RunCount > 1)
      decompressedLength = rANS32x32_decode_basic(pCompressedData, compressedLength, pDecompressedData, fileSize);

    for (size_t run = 0; run < RunCount; run++)
    {
      const uint64_t startTick = GetCurrentTimeTicks();
      const uint64_t startClock = __rdtsc();
      decompressedLength = rANS32x32_decode_basic(pCompressedData, compressedLength, pDecompressedData, fileSize);
      const uint64_t endClock = __rdtsc();
      const uint64_t endTick = GetCurrentTimeTicks();

      _mm_mfence();

      _NsPerRun[run] = TicksToNs(endTick - startTick);
      _ClocksPerRun[run] = endClock - startClock;

      printf("\rrANS32x32_decode_basic: \tdecompressed to %" PRIu64 " bytes (should be %" PRIu64 "). (%6.3f clocks/byte, %5.2f MiB/s)", decompressedLength, fileSize, (endClock - startClock) / (double)fileSize, (fileSize / (1024.0 * 1024.0)) / (TicksToNs(endTick - startTick) * 1e-9));
    }

    print_perf_info(fileSize);
  }

  {
    if constexpr (RunCount > 1)
      decompressedLength = rANS32x32_decode_avx2_varA(pCompressedData, compressedLength, pDecompressedData, fileSize);
  
    for (size_t run = 0; run < RunCount; run++)
    {
      const uint64_t startTick = GetCurrentTimeTicks();
      const uint64_t startClock = __rdtsc();
      decompressedLength = rANS32x32_decode_avx2_varA(pCompressedData, compressedLength, pDecompressedData, fileSize);
      const uint64_t endClock = __rdtsc();
      const uint64_t endTick = GetCurrentTimeTicks();
  
      _mm_mfence();
  
      _NsPerRun[run] = TicksToNs(endTick - startTick);
      _ClocksPerRun[run] = endClock - startClock;
  
      printf("\rrANS32x32_decode_avx2_varA: \tdecompressed to %" PRIu64 " bytes (should be %" PRIu64 "). (%6.3f clocks/byte, %5.2f MiB/s)", decompressedLength, fileSize, (endClock - startClock) / (double)fileSize, (fileSize / (1024.0 * 1024.0)) / (TicksToNs(endTick - startTick) * 1e-9));
    }
  
    print_perf_info(fileSize);
  }
  
  {
    if constexpr (RunCount > 1)
      decompressedLength = rANS32x32_decode_avx2_varA2(pCompressedData, compressedLength, pDecompressedData, fileSize);
  
    for (size_t run = 0; run < RunCount; run++)
    {
      const uint64_t startTick = GetCurrentTimeTicks();
      const uint64_t startClock = __rdtsc();
      decompressedLength = rANS32x32_decode_avx2_varA2(pCompressedData, compressedLength, pDecompressedData, fileSize);
      const uint64_t endClock = __rdtsc();
      const uint64_t endTick = GetCurrentTimeTicks();
  
      _mm_mfence();
  
      _NsPerRun[run] = TicksToNs(endTick - startTick);
      _ClocksPerRun[run] = endClock - startClock;
  
      printf("\rrANS32x32_decode_avx2_varA2: \tdecompressed to %" PRIu64 " bytes (should be %" PRIu64 "). (%6.3f clocks/byte, %5.2f MiB/s)", decompressedLength, fileSize, (endClock - startClock) / (double)fileSize, (fileSize / (1024.0 * 1024.0)) / (TicksToNs(endTick - startTick) * 1e-9));
    }
  
    print_perf_info(fileSize);
  }
  
  {
    if constexpr (RunCount > 1)
      decompressedLength = rANS32x32_decode_avx2_varB(pCompressedData, compressedLength, pDecompressedData, fileSize);
  
    for (size_t run = 0; run < RunCount; run++)
    {
      const uint64_t startTick = GetCurrentTimeTicks();
      const uint64_t startClock = __rdtsc();
      decompressedLength = rANS32x32_decode_avx2_varB(pCompressedData, compressedLength, pDecompressedData, fileSize);
      const uint64_t endClock = __rdtsc();
      const uint64_t endTick = GetCurrentTimeTicks();
  
      _mm_mfence();
  
      _NsPerRun[run] = TicksToNs(endTick - startTick);
      _ClocksPerRun[run] = endClock - startClock;
  
      printf("\rrANS32x32_decode_avx2_varB: \tdecompressed to %" PRIu64 " bytes (should be %" PRIu64 "). (%6.3f clocks/byte, %5.2f MiB/s)", decompressedLength, fileSize, (endClock - startClock) / (double)fileSize, (fileSize / (1024.0 * 1024.0)) / (TicksToNs(endTick - startTick) * 1e-9));
    }
  
    print_perf_info(fileSize);
  }
  
  {
    if constexpr (RunCount > 1)
      decompressedLength = rANS32x32_decode_avx2_varB2(pCompressedData, compressedLength, pDecompressedData, fileSize);
  
    for (size_t run = 0; run < RunCount; run++)
    {
      const uint64_t startTick = GetCurrentTimeTicks();
      const uint64_t startClock = __rdtsc();
      decompressedLength = rANS32x32_decode_avx2_varB2(pCompressedData, compressedLength, pDecompressedData, fileSize);
      const uint64_t endClock = __rdtsc();
      const uint64_t endTick = GetCurrentTimeTicks();
  
      _mm_mfence();
  
      _NsPerRun[run] = TicksToNs(endTick - startTick);
      _ClocksPerRun[run] = endClock - startClock;
  
      printf("\rrANS32x32_decode_avx2_varB2: \tdecompressed to %" PRIu64 " bytes (should be %" PRIu64 "). (%6.3f clocks/byte, %5.2f MiB/s)", decompressedLength, fileSize, (endClock - startClock) / (double)fileSize, (fileSize / (1024.0 * 1024.0)) / (TicksToNs(endTick - startTick) * 1e-9));
    }
  
    print_perf_info(fileSize);
  }

  if (decompressedLength != fileSize)
  {
    puts("Decompressed Length differs from initial file size.");
    exit(1);
  }
}

//////////////////////////////////////////////////////////////////////////

int32_t main(const int32_t argc, char **pArgv)
{
  if (argc == 0)
  {
    puts("Invalid Parameter.");
    return 1;
  }

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

    compressedDataCapacity = rans_max(rANS32x1_capacity(fileSize), rANS32x32_capacity(fileSize));
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

  static hist_t hist;
  make_hist(&hist, pUncompressedData, fileSize);

  size_t symCount = 0;

  for (size_t i = 0; i < 256; i++)
    symCount += (size_t)!!hist.symbolCount[i];

  if (symCount < 12)
  {
    puts("Hist:");

    for (size_t i = 0; i < 256; i++)
      if (hist.symbolCount[i])
        printf("0x%02" PRIX8 "(%c): %5" PRIu16 " (%5" PRIu16 ")\n", (uint8_t)i, (char)i, hist.symbolCount[i], hist.cumul[i]);

    puts("");
  }

  static hist_dec_t histDec;
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

      printf("%02" PRIX8 "(%c): %5" PRIu64 " ~ %5" PRIu64 "\n", sym, (char)sym, i, j - 1);

      i = j;
    }

    puts("");
  }

  for (size_t codec = 1; codec < 2; codec++)
  {
    switch (codec)
    {
    default:
    case 0: profile_rans32x1_encode(&hist); break;
    case 1: profile_rans32x32_encode(&hist); break;
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

    switch (codec)
    {
    default:
    case 0: profile_rans32x1_decode(&hist); break;
    case 1: profile_rans32x32_decode(&hist); break;
    }

    if (!Validate(pDecompressedData, pUncompressedData, fileSize))
      return 1;
    else
      puts("Success!");
  }

  free(pUncompressedData);
  free(pCompressedData);
  free(pDecompressedData);

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

bool Validate(const uint8_t *pReceived, const uint8_t *pExpected, const size_t size)
{
  if (memcmp(pReceived, pExpected, (size_t)size) != 0)
  {
    puts("Validation Failed.");

    for (size_t i = 0; i < size; i++)
    {
      if (pReceived[i] != pExpected[i])
      {
        printf("First invalid char at %" PRIu64 " [0x%" PRIX64 "] (0x%" PRIX8 " != 0x%" PRIX8 ").\n", i, i, pReceived[i], pExpected[i]);

        const int64_t start = rans_max(0, (int64_t)i - 64) & ~(int64_t)31;
        int64_t end = rans_min((int64_t)size, (int64_t)(i + 96));

        if (end != (int64_t)size)
          end &= ~(int64_t)31;

        printf("\nContext: (%" PRIi64 " to %" PRIi64 ")\n\n   Expected:                                        |  Actual Output:\n\n", start, end);

        for (int64_t context = start; context < end; context += 16)
        {
          const int64_t context_end = rans_min(end, context + 16);

          bool different = false;

          for (int64_t j = context; j < context_end; j++)
          {
            if (pReceived[j] != pExpected[j])
            {
              different = true;
              break;
            }
          }

          if (different)
            fputs("!! ", stdout);
          else
            fputs("   ", stdout);

          for (int64_t j = context; j < context_end; j++)
            printf("%02" PRIX8 " ", pReceived[j]);

          for (int64_t j = context_end; j < context + 16; j++)
            fputs("   ", stdout);

          fputs(" |  ", stdout);

          for (int64_t j = context; j < context_end; j++)
            printf("%02" PRIX8 " ", pExpected[j]);

          puts("");

          if (different)
          {
            fputs("   ", stdout);

            for (int64_t j = context; j < context_end; j++)
            {
              if (pReceived[j] != pExpected[j])
                fputs("~~ ", stdout);
              else
                fputs("   ", stdout);
            }

            for (int64_t j = context_end; j < context + 16; j++)
              fputs("   ", stdout);

            fputs("    ", stdout);

            for (int64_t j = context; j < context_end; j++)
            {
              if (pReceived[j] != pExpected[j])
                fputs("~~ ", stdout);
              else
                fputs("   ", stdout);
            }
          }

          puts("");
        }

        break;
      }
    }

    return false;
  }

  return true;
}
