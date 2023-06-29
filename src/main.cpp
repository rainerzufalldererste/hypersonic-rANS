#include <stdint.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include "simd_platform.h"
#include "rans32x1.h"
#include "rans32x32_32blk_8w.h"
#include "rans32x32_32blk_16w.h"
#include "rANS32x32_16w.h"
#include "rANS32x16_16w.h"
#include "rANS32x64_16w.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <time.h>
#include <unistd.h>

#include <sched.h>
#include <pthread.h>
#endif

//////////////////////////////////////////////////////////////////////////

uint64_t GetCurrentTimeTicks();
uint64_t TicksToNs(const uint64_t ticks);
void SleepNs(const uint64_t sleepNs);
bool Validate(const uint8_t *pUncompressedData, const uint8_t *pDecompressedData, const size_t size);

template <typename T>
inline size_t rans_max(const T a, const T b) { return a > b ? a : b; }

template <typename T>
inline size_t rans_min(const T a, const T b) { return a < b ? a : b; }

//////////////////////////////////////////////////////////////////////////

constexpr bool DisableSleep = false;
constexpr size_t RunCount = 8;
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

    printf("| %7.2f clk/byte | %7.2f clk/byte (%7.2f ~ %7.2f) ", minClocks / (double_t)fileSize, meanClocks / fileSize, (meanClocks - stdDevClocks) / fileSize, (meanClocks + stdDevClocks) / fileSize);
    printf("| %8.2f MiB/s | %8.2f MiB/s (%8.2f ~ %8.2f)\n", (fileSize / (1024.0 * 1024.0)) / (minNs * 1e-9), (fileSize / (1024.0 * 1024.0)) / (meanNs * 1e-9), (fileSize / (1024.0 * 1024.0)) / ((meanNs + stdDevNs) * 1e-9), (fileSize / (1024.0 * 1024.0)) / ((meanNs - stdDevNs) * 1e-9));
  }
  else
  {
    printf("| %7.2f clk/byte | %7.2f clk/byte (%7.2f ~ %7.2f) ", _ClocksPerRun[0] / (double_t)fileSize, _ClocksPerRun[0] / (double)fileSize, (_ClocksPerRun[0]) / (double)fileSize, (_ClocksPerRun[0]) / (double)fileSize);
    printf("| %8.2f MiB/s | %8.2f MiB/s (%8.2f ~ %8.2f)\n", (fileSize / (1024.0 * 1024.0)) / (_NsPerRun[0] * 1e-9), (fileSize / (1024.0 * 1024.0)) / (_NsPerRun[0] * 1e-9), (fileSize / (1024.0 * 1024.0)) / ((_NsPerRun[0]) * 1e-9), (fileSize / (1024.0 * 1024.0)) / ((_NsPerRun[0]) * 1e-9));
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
  { "rANS32x32 16w", 15, {{ "enc scalar", rANS32x32_16w_encode_scalar_15 }, {}}, {{ "dec scalar", rANS32x32_16w_decode_scalar_15 }, { "dec avx2 (xmm shfl, sym dpndt)", rANS32x32_xmmShfl_16w_decode_avx2_varA_15 }, { "dec avx2 (xmm shfl, sym indpt)", rANS32x32_xmmShfl_16w_decode_avx2_varB_15 }, { "dec avx2 (xmm shfl2, sym dpndt)", rANS32x32_xmmShfl2_16w_decode_avx2_varA_15 }, { "dec avx2 (xmm shfl2, sym indpt)", rANS32x32_xmmShfl2_16w_decode_avx2_varB_15 }, { "dec avx2 (ymm perm, sym dpndt)", rANS32x32_ymmPerm_16w_decode_avx2_varA_15 }, { "dec avx2 (ymm perm, sym indpt)", rANS32x32_ymmPerm_16w_decode_avx2_varB_15 }, {}}},
  { "rANS32x32 16w", 14, {{ "enc scalar", rANS32x32_16w_encode_scalar_14 }, {}}, {{ "dec scalar", rANS32x32_16w_decode_scalar_14 }, { "dec avx2 (xmm shfl, sym dpndt)", rANS32x32_xmmShfl_16w_decode_avx2_varA_14 }, { "dec avx2 (xmm shfl, sym indpt)", rANS32x32_xmmShfl_16w_decode_avx2_varB_14 }, { "dec avx2 (xmm shfl2, sym dpndt)", rANS32x32_xmmShfl2_16w_decode_avx2_varA_14 }, { "dec avx2 (xmm shfl2, sym indpt)", rANS32x32_xmmShfl2_16w_decode_avx2_varB_14 }, { "dec avx2 (ymm perm, sym dpndt)", rANS32x32_ymmPerm_16w_decode_avx2_varA_14 }, { "dec avx2 (ymm perm, sym indpt)", rANS32x32_ymmPerm_16w_decode_avx2_varB_14 }, {}}},
  { "rANS32x32 16w", 13, {{ "enc scalar", rANS32x32_16w_encode_scalar_13 }, {}}, {{ "dec scalar", rANS32x32_16w_decode_scalar_13 }, { "dec avx2 (xmm shfl, sym dpndt)", rANS32x32_xmmShfl_16w_decode_avx2_varA_13 }, { "dec avx2 (xmm shfl, sym indpt)", rANS32x32_xmmShfl_16w_decode_avx2_varB_13 }, { "dec avx2 (xmm shfl2, sym dpndt)", rANS32x32_xmmShfl2_16w_decode_avx2_varA_13 }, { "dec avx2 (xmm shfl2, sym indpt)", rANS32x32_xmmShfl2_16w_decode_avx2_varB_13 }, { "dec avx2 (ymm perm, sym dpndt)", rANS32x32_ymmPerm_16w_decode_avx2_varA_13 }, { "dec avx2 (ymm perm, sym indpt)", rANS32x32_ymmPerm_16w_decode_avx2_varB_13 }, {}}},
  { "rANS32x32 16w", 12, {{ "enc scalar", rANS32x32_16w_encode_scalar_12 }, {}}, {{ "dec scalar", rANS32x32_16w_decode_scalar_12 }, { "dec avx2 (xmm shfl, sym dpndt)", rANS32x32_xmmShfl_16w_decode_avx2_varA_12 }, { "dec avx2 (xmm shfl, sym indpt)", rANS32x32_xmmShfl_16w_decode_avx2_varB_12 }, { "dec avx2 (xmm shfl, sngl gthr)", rANS32x32_xmmShfl_16w_decode_avx2_varC_12 }, { "dec avx2 (xmm shfl2, sym dpndt)", rANS32x32_xmmShfl2_16w_decode_avx2_varA_12 }, { "dec avx2 (xmm shfl2, sym indpt)", rANS32x32_xmmShfl2_16w_decode_avx2_varB_12 }, { "dec avx2 (xmm shfl2, sngl gthr)", rANS32x32_xmmShfl2_16w_decode_avx2_varC_12 }, { "dec avx2 (ymm perm, sym dpndt)", rANS32x32_ymmPerm_16w_decode_avx2_varA_12 }, { "dec avx2 (ymm perm, sym indpt)", rANS32x32_ymmPerm_16w_decode_avx2_varB_12 }, { "dec avx2 (ymm perm, sngl gthr)", rANS32x32_ymmPerm_16w_decode_avx2_varC_12 }, { "dec avx512 (xmm shfl, sngl gthr)", rANS32x32_xmmShfl_16w_decode_avx512_varC_12 }, { "dec avx512 (xmm shfl2, sngl gthr)", rANS32x32_xmmShfl2_16w_decode_avx512_varC_12 }, { "dec avx512 (zmm perm, sngl gthr)", rANS32x32_zmmPerm_16w_decode_avx512_varC_12 }, {}}},
  { "rANS32x32 16w", 11, {{ "enc scalar", rANS32x32_16w_encode_scalar_11 }, {}}, {{ "dec scalar", rANS32x32_16w_decode_scalar_11 }, { "dec avx2 (xmm shfl, sym dpndt)", rANS32x32_xmmShfl_16w_decode_avx2_varA_11 }, { "dec avx2 (xmm shfl, sym indpt)", rANS32x32_xmmShfl_16w_decode_avx2_varB_11 }, { "dec avx2 (xmm shfl, sngl gthr)", rANS32x32_xmmShfl_16w_decode_avx2_varC_11 }, { "dec avx2 (xmm shfl2, sym dpndt)", rANS32x32_xmmShfl2_16w_decode_avx2_varA_11 }, { "dec avx2 (xmm shfl2, sym indpt)", rANS32x32_xmmShfl2_16w_decode_avx2_varB_11 }, { "dec avx2 (xmm shfl2, sngl gthr)", rANS32x32_xmmShfl2_16w_decode_avx2_varC_11 }, { "dec avx2 (ymm perm, sym dpndt)", rANS32x32_ymmPerm_16w_decode_avx2_varA_11 }, { "dec avx2 (ymm perm, sym indpt)", rANS32x32_ymmPerm_16w_decode_avx2_varB_11 }, { "dec avx2 (ymm perm, sngl gthr)", rANS32x32_ymmPerm_16w_decode_avx2_varC_11 }, { "dec avx512 (xmm shfl, sngl gthr)", rANS32x32_xmmShfl_16w_decode_avx512_varC_11 }, { "dec avx512 (xmm shfl2, sngl gthr)", rANS32x32_xmmShfl2_16w_decode_avx512_varC_11 }, { "dec avx512 (zmm perm, sngl gthr)", rANS32x32_zmmPerm_16w_decode_avx512_varC_11 }, {}}},
  { "rANS32x32 16w", 10, {{ "enc scalar", rANS32x32_16w_encode_scalar_10 }, {}}, {{ "dec scalar", rANS32x32_16w_decode_scalar_10 }, { "dec avx2 (xmm shfl, sym dpndt)", rANS32x32_xmmShfl_16w_decode_avx2_varA_10 }, { "dec avx2 (xmm shfl, sym indpt)", rANS32x32_xmmShfl_16w_decode_avx2_varB_10 }, { "dec avx2 (xmm shfl, sngl gthr)", rANS32x32_xmmShfl_16w_decode_avx2_varC_10 }, { "dec avx2 (xmm shfl2, sym dpndt)", rANS32x32_xmmShfl2_16w_decode_avx2_varA_10 }, { "dec avx2 (xmm shfl2, sym indpt)", rANS32x32_xmmShfl2_16w_decode_avx2_varB_10 }, { "dec avx2 (xmm shfl2, sngl gthr)", rANS32x32_xmmShfl2_16w_decode_avx2_varC_10 }, { "dec avx2 (ymm perm, sym dpndt)", rANS32x32_ymmPerm_16w_decode_avx2_varA_10 }, { "dec avx2 (ymm perm, sym indpt)", rANS32x32_ymmPerm_16w_decode_avx2_varB_10 }, { "dec avx2 (ymm perm, sngl gthr)", rANS32x32_ymmPerm_16w_decode_avx2_varC_10 }, { "dec avx512 (xmm shfl, sngl gthr)", rANS32x32_xmmShfl_16w_decode_avx512_varC_10 }, { "dec avx512 (xmm shfl2, sngl gthr)", rANS32x32_xmmShfl2_16w_decode_avx512_varC_10 }, { "dec avx512 (zmm perm, sngl gthr)", rANS32x32_zmmPerm_16w_decode_avx512_varC_10 }, {}}},

  { "rANS32x64 16w", 15, {{ "enc scalar", rANS32x64_16w_encode_scalar_15 }, {}}, {{ "dec scalar", rANS32x64_16w_decode_scalar_15 }, { "dec avx2 (xmm shfl, sym dpndt)", rANS32x64_xmmShfl_16w_decode_avx2_varA_15 }, { "dec avx2 (xmm shfl, sym indpt)", rANS32x64_xmmShfl_16w_decode_avx2_varB_15 }, { "dec avx2 (xmm shfl2, sym dpndt)", rANS32x64_xmmShfl2_16w_decode_avx2_varA_15 }, { "dec avx2 (xmm shfl2, sym indpt)", rANS32x64_xmmShfl2_16w_decode_avx2_varB_15 }, { "dec avx2 (ymm perm, sym dpndt)", rANS32x64_ymmPerm_16w_decode_avx2_varA_15 }, { "dec avx2 (ymm perm, sym indpt)", rANS32x64_ymmPerm_16w_decode_avx2_varB_15 }, { "dec avx512 (xmm shfl, sym dpndt)", rANS32x64_xmmShfl_16w_decode_avx512_varA_15 }, { "dec avx512 (xmm shfl, sym indpt)", rANS32x64_xmmShfl_16w_decode_avx512_varB_15 }, { "dec avx512 (xmm shfl2, sym dpndt)", rANS32x64_xmmShfl2_16w_decode_avx512_varA_15 }, { "dec avx512 (xmm shfl2, sym indpt)", rANS32x64_xmmShfl2_16w_decode_avx512_varB_15 }, { "dec avx512 (zmm perm, sym dpndt)", rANS32x64_zmmPerm_16w_decode_avx512_varA_15 }, { "dec avx512 (zmm perm, sym indpt)", rANS32x64_zmmPerm_16w_decode_avx512_varB_15 }, {}}},
  { "rANS32x64 16w", 14, {{ "enc scalar", rANS32x64_16w_encode_scalar_14 }, {}}, {{ "dec scalar", rANS32x64_16w_decode_scalar_14 }, { "dec avx2 (xmm shfl, sym dpndt)", rANS32x64_xmmShfl_16w_decode_avx2_varA_14 }, { "dec avx2 (xmm shfl, sym indpt)", rANS32x64_xmmShfl_16w_decode_avx2_varB_14 }, { "dec avx2 (xmm shfl2, sym dpndt)", rANS32x64_xmmShfl2_16w_decode_avx2_varA_14 }, { "dec avx2 (xmm shfl2, sym indpt)", rANS32x64_xmmShfl2_16w_decode_avx2_varB_14 }, { "dec avx2 (ymm perm, sym dpndt)", rANS32x64_ymmPerm_16w_decode_avx2_varA_14 }, { "dec avx2 (ymm perm, sym indpt)", rANS32x64_ymmPerm_16w_decode_avx2_varB_14 }, { "dec avx512 (xmm shfl, sym dpndt)", rANS32x64_xmmShfl_16w_decode_avx512_varA_14 }, { "dec avx512 (xmm shfl, sym indpt)", rANS32x64_xmmShfl_16w_decode_avx512_varB_14 }, { "dec avx512 (xmm shfl2, sym dpndt)", rANS32x64_xmmShfl2_16w_decode_avx512_varA_14 }, { "dec avx512 (xmm shfl2, sym indpt)", rANS32x64_xmmShfl2_16w_decode_avx512_varB_14 }, { "dec avx512 (zmm perm, sym dpndt)", rANS32x64_zmmPerm_16w_decode_avx512_varA_14 }, { "dec avx512 (zmm perm, sym indpt)", rANS32x64_zmmPerm_16w_decode_avx512_varB_14 }, {}}},
  { "rANS32x64 16w", 13, {{ "enc scalar", rANS32x64_16w_encode_scalar_13 }, {}}, {{ "dec scalar", rANS32x64_16w_decode_scalar_13 }, { "dec avx2 (xmm shfl, sym dpndt)", rANS32x64_xmmShfl_16w_decode_avx2_varA_13 }, { "dec avx2 (xmm shfl, sym indpt)", rANS32x64_xmmShfl_16w_decode_avx2_varB_13 }, { "dec avx2 (xmm shfl2, sym dpndt)", rANS32x64_xmmShfl2_16w_decode_avx2_varA_13 }, { "dec avx2 (xmm shfl2, sym indpt)", rANS32x64_xmmShfl2_16w_decode_avx2_varB_13 }, { "dec avx2 (ymm perm, sym dpndt)", rANS32x64_ymmPerm_16w_decode_avx2_varA_13 }, { "dec avx2 (ymm perm, sym indpt)", rANS32x64_ymmPerm_16w_decode_avx2_varB_13 }, { "dec avx512 (xmm shfl, sym dpndt)", rANS32x64_xmmShfl_16w_decode_avx512_varA_13 }, { "dec avx512 (xmm shfl, sym indpt)", rANS32x64_xmmShfl_16w_decode_avx512_varB_13 }, { "dec avx512 (xmm shfl2, sym dpndt)", rANS32x64_xmmShfl2_16w_decode_avx512_varA_13 }, { "dec avx512 (xmm shfl2, sym indpt)", rANS32x64_xmmShfl2_16w_decode_avx512_varB_13 }, { "dec avx512 (zmm perm, sym dpndt)", rANS32x64_zmmPerm_16w_decode_avx512_varA_13 }, { "dec avx512 (zmm perm, sym indpt)", rANS32x64_zmmPerm_16w_decode_avx512_varB_13 }, {}}},
  { "rANS32x64 16w", 12, {{ "enc scalar", rANS32x64_16w_encode_scalar_12 }, {}}, {{ "dec scalar", rANS32x64_16w_decode_scalar_12 }, { "dec avx2 (xmm shfl, sym dpndt)", rANS32x64_xmmShfl_16w_decode_avx2_varA_12 }, { "dec avx2 (xmm shfl, sym indpt)", rANS32x64_xmmShfl_16w_decode_avx2_varB_12 }, { "dec avx2 (xmm shfl, sngl gthr)", rANS32x64_xmmShfl_16w_decode_avx2_varC_12 }, { "dec avx2 (xmm shfl2, sym dpndt)", rANS32x64_xmmShfl2_16w_decode_avx2_varA_12 }, { "dec avx2 (xmm shfl2, sym indpt)", rANS32x64_xmmShfl2_16w_decode_avx2_varB_12 }, { "dec avx2 (xmm shfl2, sngl gthr)", rANS32x64_xmmShfl2_16w_decode_avx2_varC_12 }, { "dec avx2 (ymm perm, sym dpndt)", rANS32x64_ymmPerm_16w_decode_avx2_varA_12 }, { "dec avx2 (ymm perm, sym indpt)", rANS32x64_ymmPerm_16w_decode_avx2_varB_12 }, { "dec avx2 (ymm perm, sngl gthr)", rANS32x64_ymmPerm_16w_decode_avx2_varC_12 }, { "dec avx512 (xmm shfl, sym dpndt)", rANS32x64_xmmShfl_16w_decode_avx512_varA_12 }, { "dec avx512 (xmm shfl, sym indpt)", rANS32x64_xmmShfl_16w_decode_avx512_varB_12 }, { "dec avx512 (xmm shfl, sngl gthr)", rANS32x64_xmmShfl_16w_decode_avx512_varC_12 }, { "dec avx512 (xmm shfl2, sym dpndt)", rANS32x64_xmmShfl2_16w_decode_avx512_varA_12 }, { "dec avx512 (xmm shfl2, sym indpt)", rANS32x64_xmmShfl2_16w_decode_avx512_varB_12 }, { "dec avx512 (xmm shfl2, sngl gthr)", rANS32x64_xmmShfl2_16w_decode_avx512_varC_12 }, { "dec avx512 (zmm perm, sym dpndt)", rANS32x64_zmmPerm_16w_decode_avx512_varA_12 }, { "dec avx512 (zmm perm, sym indpt)", rANS32x64_zmmPerm_16w_decode_avx512_varB_12 }, { "dec avx512 (zmm perm, sngl gthr)", rANS32x64_zmmPerm_16w_decode_avx512_varC_12 }, {}}},
  { "rANS32x64 16w", 11, {{ "enc scalar", rANS32x64_16w_encode_scalar_11 }, {}}, {{ "dec scalar", rANS32x64_16w_decode_scalar_11 }, { "dec avx2 (xmm shfl, sym dpndt)", rANS32x64_xmmShfl_16w_decode_avx2_varA_11 }, { "dec avx2 (xmm shfl, sym indpt)", rANS32x64_xmmShfl_16w_decode_avx2_varB_11 }, { "dec avx2 (xmm shfl, sngl gthr)", rANS32x64_xmmShfl_16w_decode_avx2_varC_11 }, { "dec avx2 (xmm shfl2, sym dpndt)", rANS32x64_xmmShfl2_16w_decode_avx2_varA_11 }, { "dec avx2 (xmm shfl2, sym indpt)", rANS32x64_xmmShfl2_16w_decode_avx2_varB_11 }, { "dec avx2 (xmm shfl2, sngl gthr)", rANS32x64_xmmShfl2_16w_decode_avx2_varC_11 }, { "dec avx2 (ymm perm, sym dpndt)", rANS32x64_ymmPerm_16w_decode_avx2_varA_11 }, { "dec avx2 (ymm perm, sym indpt)", rANS32x64_ymmPerm_16w_decode_avx2_varB_11 }, { "dec avx2 (ymm perm, sngl gthr)", rANS32x64_ymmPerm_16w_decode_avx2_varC_11 }, { "dec avx512 (xmm shfl, sym dpndt)", rANS32x64_xmmShfl_16w_decode_avx512_varA_11 }, { "dec avx512 (xmm shfl, sym indpt)", rANS32x64_xmmShfl_16w_decode_avx512_varB_11 }, { "dec avx512 (xmm shfl, sngl gthr)", rANS32x64_xmmShfl_16w_decode_avx512_varC_11 }, { "dec avx512 (xmm shfl2, sym dpndt)", rANS32x64_xmmShfl2_16w_decode_avx512_varA_11 }, { "dec avx512 (xmm shfl2, sym indpt)", rANS32x64_xmmShfl2_16w_decode_avx512_varB_11 }, { "dec avx512 (xmm shfl2, sngl gthr)", rANS32x64_xmmShfl2_16w_decode_avx512_varC_11 }, { "dec avx512 (zmm perm, sym dpndt)", rANS32x64_zmmPerm_16w_decode_avx512_varA_11 }, { "dec avx512 (zmm perm, sym indpt)", rANS32x64_zmmPerm_16w_decode_avx512_varB_11 }, { "dec avx512 (zmm perm, sngl gthr)", rANS32x64_zmmPerm_16w_decode_avx512_varC_11 }, {}}},
  { "rANS32x64 16w", 10, {{ "enc scalar", rANS32x64_16w_encode_scalar_10 }, {}}, {{ "dec scalar", rANS32x64_16w_decode_scalar_10 }, { "dec avx2 (xmm shfl, sym dpndt)", rANS32x64_xmmShfl_16w_decode_avx2_varA_10 }, { "dec avx2 (xmm shfl, sym indpt)", rANS32x64_xmmShfl_16w_decode_avx2_varB_10 }, { "dec avx2 (xmm shfl, sngl gthr)", rANS32x64_xmmShfl_16w_decode_avx2_varC_10 }, { "dec avx2 (xmm shfl2, sym dpndt)", rANS32x64_xmmShfl2_16w_decode_avx2_varA_10 }, { "dec avx2 (xmm shfl2, sym indpt)", rANS32x64_xmmShfl2_16w_decode_avx2_varB_10 }, { "dec avx2 (xmm shfl2, sngl gthr)", rANS32x64_xmmShfl2_16w_decode_avx2_varC_10 }, { "dec avx2 (ymm perm, sym dpndt)", rANS32x64_ymmPerm_16w_decode_avx2_varA_10 }, { "dec avx2 (ymm perm, sym indpt)", rANS32x64_ymmPerm_16w_decode_avx2_varB_10 }, { "dec avx2 (ymm perm, sngl gthr)", rANS32x64_ymmPerm_16w_decode_avx2_varC_10 }, { "dec avx512 (xmm shfl, sym dpndt)", rANS32x64_xmmShfl_16w_decode_avx512_varA_10 }, { "dec avx512 (xmm shfl, sym indpt)", rANS32x64_xmmShfl_16w_decode_avx512_varB_10 }, { "dec avx512 (xmm shfl, sngl gthr)", rANS32x64_xmmShfl_16w_decode_avx512_varC_10 }, { "dec avx512 (xmm shfl2, sym dpndt)", rANS32x64_xmmShfl2_16w_decode_avx512_varA_10 }, { "dec avx512 (xmm shfl2, sym indpt)", rANS32x64_xmmShfl2_16w_decode_avx512_varB_10 }, { "dec avx512 (xmm shfl2, sngl gthr)", rANS32x64_xmmShfl2_16w_decode_avx512_varC_10 }, { "dec avx512 (zmm perm, sym dpndt)", rANS32x64_zmmPerm_16w_decode_avx512_varA_10 }, { "dec avx512 (zmm perm, sym indpt)", rANS32x64_zmmPerm_16w_decode_avx512_varB_10 }, { "dec avx512 (zmm perm, sngl gthr)", rANS32x64_zmmPerm_16w_decode_avx512_varC_10 }, {}}},

  { "rANS32x32 32blk 16w", 15, {{ "enc scalar", rANS32x32_32blk_16w_encode_scalar_15 }, {}}, {{ "dec scalar", rANS32x32_32blk_16w_decode_scalar_15 }, { "dec avx2 (sym dpndt)", rANS32x32_32blk_16w_decode_avx2_varA_15 }, { "dec avx2 (sym dpndt 2x)", rANS32x32_32blk_16w_decode_avx2_varA2_15 }, { "dec avx2 (sym indpt)", rANS32x32_32blk_16w_decode_avx2_varB_15 }, { "dec avx2 (sym indpt 2x)", rANS32x32_32blk_16w_decode_avx2_varB2_15 }, {}}},
  { "rANS32x32 32blk 16w", 14, {{ "enc scalar", rANS32x32_32blk_16w_encode_scalar_14 }, {}}, {{ "dec scalar", rANS32x32_32blk_16w_decode_scalar_14 }, { "dec avx2 (sym dpndt)", rANS32x32_32blk_16w_decode_avx2_varA_14 }, { "dec avx2 (sym dpndt 2x)", rANS32x32_32blk_16w_decode_avx2_varA2_14 }, { "dec avx2 (sym indpt)", rANS32x32_32blk_16w_decode_avx2_varB_14 }, { "dec avx2 (sym indpt 2x)", rANS32x32_32blk_16w_decode_avx2_varB2_14 }, {}}},
  { "rANS32x32 32blk 16w", 13, {{ "enc scalar", rANS32x32_32blk_16w_encode_scalar_13 }, {}}, {{ "dec scalar", rANS32x32_32blk_16w_decode_scalar_13 }, { "dec avx2 (sym dpndt)", rANS32x32_32blk_16w_decode_avx2_varA_13 }, { "dec avx2 (sym dpndt 2x)", rANS32x32_32blk_16w_decode_avx2_varA2_13 }, { "dec avx2 (sym indpt)", rANS32x32_32blk_16w_decode_avx2_varB_13 }, { "dec avx2 (sym indpt 2x)", rANS32x32_32blk_16w_decode_avx2_varB2_13 }, {}}},
  { "rANS32x32 32blk 16w", 12, {{ "enc scalar", rANS32x32_32blk_16w_encode_scalar_12 }, {}}, {{ "dec scalar", rANS32x32_32blk_16w_decode_scalar_12 }, { "dec avx2 (sym dpndt)", rANS32x32_32blk_16w_decode_avx2_varA_12 }, { "dec avx2 (sym dpndt 2x)", rANS32x32_32blk_16w_decode_avx2_varA2_12 }, { "dec avx2 (sym indpt)", rANS32x32_32blk_16w_decode_avx2_varB_12 }, { "dec avx2 (sym indpt 2x)", rANS32x32_32blk_16w_decode_avx2_varB2_12 }, { "dec avx2 (sngl gthr)", rANS32x32_32blk_16w_decode_avx2_varC_12 }, { "dec avx2 (sngl gthr 2x)", rANS32x32_32blk_16w_decode_avx2_varC2_12 }, {}}},
  { "rANS32x32 32blk 16w", 11, {{ "enc scalar", rANS32x32_32blk_16w_encode_scalar_11 }, {}}, {{ "dec scalar", rANS32x32_32blk_16w_decode_scalar_11 }, { "dec avx2 (sym dpndt)", rANS32x32_32blk_16w_decode_avx2_varA_11 }, { "dec avx2 (sym dpndt 2x)", rANS32x32_32blk_16w_decode_avx2_varA2_11 }, { "dec avx2 (sym indpt)", rANS32x32_32blk_16w_decode_avx2_varB_11 }, { "dec avx2 (sym indpt 2x)", rANS32x32_32blk_16w_decode_avx2_varB2_11 }, { "dec avx2 (sngl gthr)", rANS32x32_32blk_16w_decode_avx2_varC_11 }, { "dec avx2 (sngl gthr 2x)", rANS32x32_32blk_16w_decode_avx2_varC2_11 }, {}}},
  { "rANS32x32 32blk 16w", 10, {{ "enc scalar", rANS32x32_32blk_16w_encode_scalar_10 }, {}}, {{ "dec scalar", rANS32x32_32blk_16w_decode_scalar_10 }, { "dec avx2 (sym dpndt)", rANS32x32_32blk_16w_decode_avx2_varA_10 }, { "dec avx2 (sym dpndt 2x)", rANS32x32_32blk_16w_decode_avx2_varA2_10 }, { "dec avx2 (sym indpt)", rANS32x32_32blk_16w_decode_avx2_varB_10 }, { "dec avx2 (sym indpt 2x)", rANS32x32_32blk_16w_decode_avx2_varB2_10 }, { "dec avx2 (sngl gthr)", rANS32x32_32blk_16w_decode_avx2_varC_10 }, { "dec avx2 (sngl gthr 2x)", rANS32x32_32blk_16w_decode_avx2_varC2_10 }, {}}},

  { "rANS32x32 32blk 8w", 15, {{ "enc scalar", rANS32x32_32blk_8w_encode_scalar_15 }, {}}, {{ "dec scalar", rANS32x32_32blk_8w_decode_scalar_15 }, { "dec avx2 (sym dpndt)", rANS32x32_32blk_8w_decode_avx2_varA_15 }, { "dec avx2 (sym dpndt 2x)", rANS32x32_32blk_8w_decode_avx2_varA2_15 }, { "dec avx2 (sym indpt)", rANS32x32_32blk_8w_decode_avx2_varB_15 }, { "dec avx2 (sym indpt 2x)", rANS32x32_32blk_8w_decode_avx2_varB2_15 }, {}}},
  { "rANS32x32 32blk 8w", 14, {{ "enc scalar", rANS32x32_32blk_8w_encode_scalar_14 }, {}}, {{ "dec scalar", rANS32x32_32blk_8w_decode_scalar_14 }, { "dec avx2 (sym dpndt)", rANS32x32_32blk_8w_decode_avx2_varA_14 }, { "dec avx2 (sym dpndt 2x)", rANS32x32_32blk_8w_decode_avx2_varA2_14 }, { "dec avx2 (sym indpt)", rANS32x32_32blk_8w_decode_avx2_varB_14 }, { "dec avx2 (sym indpt 2x)", rANS32x32_32blk_8w_decode_avx2_varB2_14 }, {}}},
  { "rANS32x32 32blk 8w", 13, {{ "enc scalar", rANS32x32_32blk_8w_encode_scalar_13 }, {}}, {{ "dec scalar", rANS32x32_32blk_8w_decode_scalar_13 }, { "dec avx2 (sym dpndt)", rANS32x32_32blk_8w_decode_avx2_varA_13 }, { "dec avx2 (sym dpndt 2x)", rANS32x32_32blk_8w_decode_avx2_varA2_13 }, { "dec avx2 (sym indpt)", rANS32x32_32blk_8w_decode_avx2_varB_13 }, { "dec avx2 (sym indpt 2x)", rANS32x32_32blk_8w_decode_avx2_varB2_13 }, {}}},
  { "rANS32x32 32blk 8w", 12, {{ "enc scalar", rANS32x32_32blk_8w_encode_scalar_12 }, {}}, {{ "dec scalar", rANS32x32_32blk_8w_decode_scalar_12 }, { "dec avx2 (sym dpndt)", rANS32x32_32blk_8w_decode_avx2_varA_12 }, { "dec avx2 (sym dpndt 2x)", rANS32x32_32blk_8w_decode_avx2_varA2_12 }, { "dec avx2 (sym indpt)", rANS32x32_32blk_8w_decode_avx2_varB_12 }, { "dec avx2 (sym indpt 2x)", rANS32x32_32blk_8w_decode_avx2_varB2_12 }, { "dec avx2 (sngl gthr)", rANS32x32_32blk_8w_decode_avx2_varC_12 }, { "dec avx2 (sngl gthr 2x)", rANS32x32_32blk_8w_decode_avx2_varC2_12 }, {}}},
  { "rANS32x32 32blk 8w", 11, {{ "enc scalar", rANS32x32_32blk_8w_encode_scalar_11 }, {}}, {{ "dec scalar", rANS32x32_32blk_8w_decode_scalar_11 }, { "dec avx2 (sym dpndt)", rANS32x32_32blk_8w_decode_avx2_varA_11 }, { "dec avx2 (sym dpndt 2x)", rANS32x32_32blk_8w_decode_avx2_varA2_11 }, { "dec avx2 (sym indpt)", rANS32x32_32blk_8w_decode_avx2_varB_11 }, { "dec avx2 (sym indpt 2x)", rANS32x32_32blk_8w_decode_avx2_varB2_11 }, { "dec avx2 (sngl gthr)", rANS32x32_32blk_8w_decode_avx2_varC_11 }, { "dec avx2 (sngl gthr 2x)", rANS32x32_32blk_8w_decode_avx2_varC2_11 }, {}}},
  { "rANS32x32 32blk 8w", 10, {{ "enc scalar", rANS32x32_32blk_8w_encode_scalar_10 }, {}}, {{ "dec scalar", rANS32x32_32blk_8w_decode_scalar_10 }, { "dec avx2 (sym dpndt)", rANS32x32_32blk_8w_decode_avx2_varA_10 }, { "dec avx2 (sym dpndt 2x)", rANS32x32_32blk_8w_decode_avx2_varA2_10 }, { "dec avx2 (sym indpt)", rANS32x32_32blk_8w_decode_avx2_varB_10 }, { "dec avx2 (sym indpt 2x)", rANS32x32_32blk_8w_decode_avx2_varB2_10 }, { "dec avx2 (sngl gthr)", rANS32x32_32blk_8w_decode_avx2_varC_10 }, { "dec avx2 (sngl gthr 2x)", rANS32x32_32blk_8w_decode_avx2_varC2_10 }, {}}},
  
  { "rANS32x16 16w", 15, {{ "enc scalar", rANS32x16_16w_encode_scalar_15 }, {}}, {{ "dec scalar", rANS32x16_16w_decode_scalar_15 }, { "dec avx2 (xmm shfl, sym dpndt)", rANS32x16_xmmShfl_16w_decode_avx2_varA_15 }, { "dec avx2 (xmm shfl, sym indpt)", rANS32x16_xmmShfl_16w_decode_avx2_varB_15 }, { "dec avx2 (ymm perm, sym dpndt)", rANS32x16_ymmPerm_16w_decode_avx2_varA_15 }, { "dec avx2 (ymm perm, sym indpt)", rANS32x16_ymmPerm_16w_decode_avx2_varB_15 }, {}}},
  { "rANS32x16 16w", 14, {{ "enc scalar", rANS32x16_16w_encode_scalar_14 }, {}}, {{ "dec scalar", rANS32x16_16w_decode_scalar_14 }, { "dec avx2 (xmm shfl, sym dpndt)", rANS32x16_xmmShfl_16w_decode_avx2_varA_14 }, { "dec avx2 (xmm shfl, sym indpt)", rANS32x16_xmmShfl_16w_decode_avx2_varB_14 }, { "dec avx2 (ymm perm, sym dpndt)", rANS32x16_ymmPerm_16w_decode_avx2_varA_14 }, { "dec avx2 (ymm perm, sym indpt)", rANS32x16_ymmPerm_16w_decode_avx2_varB_14 }, {}}},
  { "rANS32x16 16w", 13, {{ "enc scalar", rANS32x16_16w_encode_scalar_13 }, {}}, {{ "dec scalar", rANS32x16_16w_decode_scalar_13 }, { "dec avx2 (xmm shfl, sym dpndt)", rANS32x16_xmmShfl_16w_decode_avx2_varA_13 }, { "dec avx2 (xmm shfl, sym indpt)", rANS32x16_xmmShfl_16w_decode_avx2_varB_13 }, { "dec avx2 (ymm perm, sym dpndt)", rANS32x16_ymmPerm_16w_decode_avx2_varA_13 }, { "dec avx2 (ymm perm, sym indpt)", rANS32x16_ymmPerm_16w_decode_avx2_varB_13 }, {}}},
  { "rANS32x16 16w", 12, {{ "enc scalar", rANS32x16_16w_encode_scalar_12 }, {}}, {{ "dec scalar", rANS32x16_16w_decode_scalar_12 }, { "dec avx2 (xmm shfl, sym dpndt)", rANS32x16_xmmShfl_16w_decode_avx2_varA_12 }, { "dec avx2 (xmm shfl, sym indpt)", rANS32x16_xmmShfl_16w_decode_avx2_varB_12 }, { "dec avx2 (xmm shfl, sngl gthr)", rANS32x16_xmmShfl_16w_decode_avx2_varC_12 }, { "dec avx2 (ymm perm, sym dpndt)", rANS32x16_ymmPerm_16w_decode_avx2_varA_12 }, { "dec avx2 (ymm perm, sym indpt)", rANS32x16_ymmPerm_16w_decode_avx2_varB_12 }, { "dec avx2 (ymm perm, sngl gthr)", rANS32x16_ymmPerm_16w_decode_avx2_varC_12 }, {}}},
  { "rANS32x16 16w", 11, {{ "enc scalar", rANS32x16_16w_encode_scalar_11 }, {}}, {{ "dec scalar", rANS32x16_16w_decode_scalar_11 }, { "dec avx2 (xmm shfl, sym dpndt)", rANS32x16_xmmShfl_16w_decode_avx2_varA_11 }, { "dec avx2 (xmm shfl, sym indpt)", rANS32x16_xmmShfl_16w_decode_avx2_varB_11 }, { "dec avx2 (xmm shfl, sngl gthr)", rANS32x16_xmmShfl_16w_decode_avx2_varC_11 }, { "dec avx2 (ymm perm, sym dpndt)", rANS32x16_ymmPerm_16w_decode_avx2_varA_11 }, { "dec avx2 (ymm perm, sym indpt)", rANS32x16_ymmPerm_16w_decode_avx2_varB_11 }, { "dec avx2 (ymm perm, sngl gthr)", rANS32x16_ymmPerm_16w_decode_avx2_varC_11 }, {}}},
  { "rANS32x16 16w", 10, {{ "enc scalar", rANS32x16_16w_encode_scalar_10 }, {}}, {{ "dec scalar", rANS32x16_16w_decode_scalar_10 }, { "dec avx2 (xmm shfl, sym dpndt)", rANS32x16_xmmShfl_16w_decode_avx2_varA_10 }, { "dec avx2 (xmm shfl, sym indpt)", rANS32x16_xmmShfl_16w_decode_avx2_varB_10 }, { "dec avx2 (xmm shfl, sngl gthr)", rANS32x16_xmmShfl_16w_decode_avx2_varC_10 }, { "dec avx2 (ymm perm, sym dpndt)", rANS32x16_ymmPerm_16w_decode_avx2_varA_10 }, { "dec avx2 (ymm perm, sym indpt)", rANS32x16_ymmPerm_16w_decode_avx2_varB_10 }, { "dec avx2 (ymm perm, sngl gthr)", rANS32x16_ymmPerm_16w_decode_avx2_varC_10 }, {}}},
};

//////////////////////////////////////////////////////////////////////////

int32_t main(const int32_t argc, char **pArgv)
{
  if (argc == 1)
  {
    puts("Invalid Parameter.");
    return 1;
  }

  if (argc == 3)
  {
    // For more consistent benchmarking results.
    const size_t cpuCoreIndex = strtoull(pArgv[2], nullptr, 10);

#ifdef _WIN32
    HANDLE thread = GetCurrentThread();
    SetThreadPriority(thread, THREAD_PRIORITY_HIGHEST);
    SetThreadAffinityMask(thread, (uint64_t)1 << cpuCoreIndex);
#else
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET((int32_t)cpuCoreIndex, &cpuset);

    pthread_t current_thread = pthread_self();
    pthread_setaffinity_np(current_thread, sizeof(cpu_set_t), &cpuset);
#endif
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

    compressedDataCapacity = rans_max(rans_max(rans_max(rANS32x64_16w_capacity(fileSize), rANS32x16_16w_capacity(fileSize)), rANS32x32_16w_capacity(fileSize)), rans_max(rANS32x32_32blk_16w_capacity(fileSize), rANS32x32_32blk_8w_capacity(fileSize)));
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

  puts("Codec Type (Enc/Dec Impl)       Hist  Ratio      Minimum            Average          ( StdDev.         )   Maximum          Average        ( StdDev.           )");
  puts("~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~");

  for (size_t codecId = 0; codecId < sizeof(_Codecs) / sizeof(_Codecs[0]); codecId++)
  {
    static hist_t hist;
    make_hist(&hist, pUncompressedData, fileSize, _Codecs[codecId].totalSymbolCountBits);

    size_t encodedSize = 0;

    for (size_t i = 0; i < MaxEncoderCount; i++)
    {
      if (_Codecs[codecId].encoders[i].name == nullptr)
        break;

      printf("%-32s %2" PRIu32 " | -------- | ---------------- | ------------------------------------ | -------------- | ------------------------------------\n", _Codecs[codecId].name, _Codecs[codecId].totalSymbolCountBits);

      memset(pCompressedData, 0xCC, compressedDataCapacity);

      if constexpr (RunCount > 1)
      {
        printf("\r  (dry run)");
        encodedSize = _Codecs[codecId].encoders[i].func(pUncompressedData, fileSize, pCompressedData, compressedDataCapacity, &hist);
      }

      SleepNs(1500 * 1000 * 1000);

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

        printf("\r  %-33s | %6.2f %% | compressed to %" PRIu64 " bytes (%6.3f clocks/byte, %5.2f MiB/s)", _Codecs[codecId].encoders[i].name, encodedSize / (double)fileSize * 100.0, encodedSize, (endClock - startClock) / (double)fileSize, (fileSize / (1024.0 * 1024.0)) / (TicksToNs(endTick - startTick) * 1e-9));

        SleepNs(250 * 1000 * 1000);
      }

      printf("\r  %-33s | %6.2f %% ", _Codecs[codecId].encoders[i].name, encodedSize / (double)fileSize * 100.0);
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

      memset(pDecompressedData, 0xCC, fileSize);

      if constexpr (RunCount > 1)
      {
        printf("\r(dry run)");
        decodedSize = _Codecs[codecId].decoders[i].func(pCompressedData, encodedSize, pDecompressedData, fileSize);
      }

      SleepNs(1500 * 1000 * 1000);

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

        printf("\r  %-33s |          | decompressed to %" PRIu64 " bytes. (%6.3f clocks/byte, %5.2f MiB/s)", _Codecs[codecId].decoders[i].name, decodedSize, (endClock - startClock) / (double)fileSize, (fileSize / (1024.0 * 1024.0)) / (TicksToNs(endTick - startTick) * 1e-9));

        SleepNs(250 * 1000 * 1000);
      }

      printf("\r  %-33s |          ", _Codecs[codecId].decoders[i].name);
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

void SleepNs(const uint64_t sleepNs)
{
  if constexpr (!DisableSleep && RunCount > 1)
  {
#ifdef _WIN32
    Sleep((DWORD)((sleepNs + 500 * 1000) / (1000 * 1000)));
#else
    usleep((uint32_t)((sleepNs + 500) / (1000)));
#endif
  }
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

        const int64_t start = rans_max((int64_t)0, (int64_t)(i - 64) & ~(int64_t)31);
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
            printf("%02" PRIX8 " ", pExpected[j]);

          for (int64_t j = context_end; j < context + 16; j++)
            fputs("   ", stdout);

          fputs(" |  ", stdout);

          for (int64_t j = context; j < context_end; j++)
            printf("%02" PRIX8 " ", pReceived[j]);

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
