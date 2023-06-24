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

    printf("| %6.3f clk/byte | %6.3f clk/byte (%6.3f ~ %6.3f) ", minClocks / (double_t)fileSize, meanClocks / fileSize, (meanClocks - stdDevClocks) / fileSize, (meanClocks + stdDevClocks) / fileSize);
    printf("| %8.2f MiB/s | %8.2f MiB/s (%8.2f ~ %8.2f)\n", (fileSize / (1024.0 * 1024.0)) / (minNs * 1e-9), (fileSize / (1024.0 * 1024.0)) / (meanNs * 1e-9), (fileSize / (1024.0 * 1024.0)) / ((meanNs + stdDevNs) * 1e-9), (fileSize / (1024.0 * 1024.0)) / ((meanNs - stdDevNs) * 1e-9));
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

template <typename func_t>
struct func_info_t
{
  const char *name = nullptr;
  func_t func = nullptr;
};

constexpr size_t MaxEncoderCount = 32; // if someone needs more than 32, simply increase this.
constexpr size_t MaxDecoderCount = 32; // if someone needs more than 32, simply increase this.

struct codec_info_t
{
  typedef size_t (*encodeFunc)(const uint8_t *pInData, const size_t length, uint8_t *pOutData, const size_t outCapacity, const hist_t *pHist);
  typedef size_t (*decodeFunc)(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity);

  const char *name = "<UNNAMED CODEC>";
  uint32_t totalSymbolCountBits = 0;
  func_info_t<encodeFunc> encoders[MaxEncoderCount];
  func_info_t<decodeFunc> decoders[MaxDecoderCount];
};

static codec_info_t _Codecs[] =
{
  { "rANS32x32 multi block", 15, {{ "encode_basic", rANS32x32_encode_basic_15 }, {}}, {{ "decode_basic", rANS32x32_decode_basic_15 }, { "decode_avx2 (variant A)", rANS32x32_decode_avx2_varA_15 }, { "decode_avx2 (variant A 2x)", rANS32x32_decode_avx2_varA2_15 }, { "decode_avx2 (variant B)", rANS32x32_decode_avx2_varB_15 }, { "decode_avx2 (variant B 2x)", rANS32x32_decode_avx2_varB2_15 }, {}}},

  { "rANS32x32 multi block", 14, {{ "encode_basic", rANS32x32_encode_basic_14 }, {}}, {{ "decode_basic", rANS32x32_decode_basic_14 }, { "decode_avx2 (variant A)", rANS32x32_decode_avx2_varA_14 }, { "decode_avx2 (variant A 2x)", rANS32x32_decode_avx2_varA2_14 }, { "decode_avx2 (variant B)", rANS32x32_decode_avx2_varB_14 }, { "decode_avx2 (variant B 2x)", rANS32x32_decode_avx2_varB2_14 }, {}}},

  { "rANS32x32 multi block", 13, {{ "encode_basic", rANS32x32_encode_basic_13 }, {}}, {{ "decode_basic", rANS32x32_decode_basic_13 }, { "decode_avx2 (variant A)", rANS32x32_decode_avx2_varA_13 }, { "decode_avx2 (variant A 2x)", rANS32x32_decode_avx2_varA2_13 }, { "decode_avx2 (variant B)", rANS32x32_decode_avx2_varB_13 }, { "decode_avx2 (variant B 2x)", rANS32x32_decode_avx2_varB2_13 }, {}}},

  { "rANS32x32 multi block", 12, {{ "encode_basic", rANS32x32_encode_basic_12 }, {}}, {{ "decode_basic", rANS32x32_decode_basic_12 }, { "decode_avx2 (variant A)", rANS32x32_decode_avx2_varA_12 }, { "decode_avx2 (variant A 2x)", rANS32x32_decode_avx2_varA2_12 }, { "decode_avx2 (variant B)", rANS32x32_decode_avx2_varB_12 }, { "decode_avx2 (variant B 2x)", rANS32x32_decode_avx2_varB2_12 }, {}}},

  { "rANS32x32 multi block", 11, {{ "encode_basic", rANS32x32_encode_basic_11 }, {}}, {{ "decode_basic", rANS32x32_decode_basic_11 }, { "decode_avx2 (variant A)", rANS32x32_decode_avx2_varA_11 }, { "decode_avx2 (variant A 2x)", rANS32x32_decode_avx2_varA2_11 }, { "decode_avx2 (variant B)", rANS32x32_decode_avx2_varB_11 }, { "decode_avx2 (variant B 2x)", rANS32x32_decode_avx2_varB2_11 }, {}}},

  { "rANS32x32 multi block", 10, {{ "encode_basic", rANS32x32_encode_basic_10 }, {}}, {{ "decode_basic", rANS32x32_decode_basic_10 }, { "decode_avx2 (variant A)", rANS32x32_decode_avx2_varA_10 }, { "decode_avx2 (variant A 2x)", rANS32x32_decode_avx2_varA2_10 }, { "decode_avx2 (variant B)", rANS32x32_decode_avx2_varB_10 }, { "decode_avx2 (variant B 2x)", rANS32x32_decode_avx2_varB2_10 }, {}}},
};

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

  puts("Codec Type               Encoder/Decoder Impl           Ratio      Minimum           Average         ( StdDev.       )   Maximum          Average        ( StdDev.           )");
  puts("~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~");

  for (size_t codecId = 0; codecId < sizeof(_Codecs) / sizeof(_Codecs[0]); codecId++)
  {
    static hist_t hist;
    make_hist(&hist, pUncompressedData, fileSize, _Codecs[codecId].totalSymbolCountBits);

    size_t encodedSize = 0;

    for (size_t i = 0; i < MaxEncoderCount; i++)
    {
      if (_Codecs[codecId].encoders[i].name == nullptr)
        break;

      printf("\r(dry run)");
      encodedSize = _Codecs[codecId].encoders[i].func(pUncompressedData, fileSize, pCompressedData, compressedDataCapacity, &hist);

      for (size_t run = 0; run < RunCount; run++)
      {
        const uint64_t startTick = GetCurrentTimeTicks();
        const uint64_t startClock = __rdtsc();
        encodedSize = _Codecs[codecId].encoders[i].func(pUncompressedData, fileSize, pCompressedData, compressedDataCapacity, &hist);
        const uint64_t endClock = __rdtsc();
        const uint64_t endTick = GetCurrentTimeTicks();

        _mm_mfence();

        _NsPerRun[run] = TicksToNs(endTick - startTick);
        _ClocksPerRun[run] = endClock - startClock;

        printf("\rcompressed to %" PRIu64 " bytes (%5.3f %%). (%6.3f clocks/byte, %5.2f MiB/s)", encodedSize, encodedSize / (double)fileSize * 100.0, (endClock - startClock) / (double)fileSize, (fileSize / (1024.0 * 1024.0)) / (TicksToNs(endTick - startTick) * 1e-9));
      }

      printf("\r%-21s %2" PRIu32 " %-28s | %6.2f %% ", _Codecs[codecId].name, _Codecs[codecId].totalSymbolCountBits, _Codecs[codecId].encoders[i].name, encodedSize / (double)fileSize * 100.0);
      print_perf_info(fileSize);

      const size_t decodedSize = _Codecs[codecId].decoders[0].func(pCompressedData, encodedSize, pDecompressedData, fileSize);

      if (decodedSize != fileSize || !Validate(pDecompressedData, pUncompressedData, fileSize))
        puts("Failed to validate.");
    }

    size_t decodedSize = 0;

    for (size_t i = 0; i < MaxDecoderCount; i++)
    {
      if (_Codecs[codecId].decoders[i].name == nullptr)
        break;

      printf("\r(dry run)");
      decodedSize = _Codecs[codecId].decoders[i].func(pCompressedData, encodedSize, pDecompressedData, fileSize);

      for (size_t run = 0; run < RunCount; run++)
      {
        const uint64_t startTick = GetCurrentTimeTicks();
        const uint64_t startClock = __rdtsc();
        decodedSize = _Codecs[codecId].decoders[i].func(pCompressedData, encodedSize, pDecompressedData, fileSize);
        const uint64_t endClock = __rdtsc();
        const uint64_t endTick = GetCurrentTimeTicks();

        _mm_mfence();

        _NsPerRun[run] = TicksToNs(endTick - startTick);
        _ClocksPerRun[run] = endClock - startClock;

        printf("\rdecompressed to %" PRIu64 " bytes. (%6.3f clocks/byte, %5.2f MiB/s)", decodedSize, (endClock - startClock) / (double)fileSize, (fileSize / (1024.0 * 1024.0)) / (TicksToNs(endTick - startTick) * 1e-9));
      }

      printf("\r%-21s %2" PRIu32 " %-28s |          ", _Codecs[codecId].name, _Codecs[codecId].totalSymbolCountBits, _Codecs[codecId].decoders[i].name);
      print_perf_info(fileSize);

      if (decodedSize != fileSize || !Validate(pDecompressedData, pUncompressedData, fileSize))
        puts("\nFailed to validate.");
    }
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

        const int64_t start = rans_max(0, (int64_t)(i - 64) & ~(int64_t)31);
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
