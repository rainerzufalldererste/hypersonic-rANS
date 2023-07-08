#include <stdint.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include "simd_platform.h"
#include "rans32x32_32blk_8w.h"
#include "rans32x32_32blk_16w.h"
#include "rANS32x32_16w.h"
#include "rANS32x16_16w.h"
#include "rANS32x64_16w.h"
#include "block_rANS32x32_16w.h"
#include "block_rANS32x64_16w.h"
#include "mt_rANS32x32_16w.h"
#include "mt_rANS32x64_16w.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <time.h>
#include <unistd.h>
#include <sched.h>
#include <pthread.h>
#endif

#if defined(_MSC_VER)
#define ALIGNED_ALLOC(b, a) _aligned_malloc(a, b)
#define ALIGNED_FREE(a) _aligned_free(a)
#else
#define ALIGNED_ALLOC(b, a) aligned_alloc(b, a)
#define ALIGNED_FREE(a) free(a)
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

static bool _DisableSleep = false;
static bool _OnlyRelevantCodecs = true;
static size_t _HistMax = 15;
static size_t _HistMin = 10;
static bool _Include32Block = false;
static bool _IncludeRaw = false;
static bool _IncludeMT = false;
static bool _ExcludeBlock = false;
static bool _Exclude32x16 = false;
static bool _Exclude32x32 = false;
static bool _Exclude32x64 = false;
static bool _IsTest = false;
static size_t _RunCount = 8;
static size_t _EncodeRunCount = 2;
static size_t _DecodeRunCount = 16;

constexpr size_t MaxRunCount = 256;
static uint64_t _ClocksPerRun[MaxRunCount];
static uint64_t _NsPerRun[MaxRunCount];

//////////////////////////////////////////////////////////////////////////

void print_perf_info(const size_t fileSize)
{
  if (_RunCount > 1)
  {
    uint64_t completeNs = 0;
    uint64_t completeClocks = 0;
    uint64_t minNs = (uint64_t)-1;
    uint64_t minClocks = (uint64_t)-1;

    for (size_t i = 0; i < _RunCount; i++)
    {
      completeNs += _NsPerRun[i];
      completeClocks += _ClocksPerRun[i];

      if (_NsPerRun[i] < minNs)
        minNs = _NsPerRun[i];

      if (_ClocksPerRun[i] < minClocks)
        minClocks = _ClocksPerRun[i];
    }

    const double meanNs = completeNs / (double)_RunCount;
    const double meanClocks = completeClocks / (double)_RunCount;
    double stdDevNs = 0;
    double stdDevClocks = 0;

    for (size_t i = 0; i < _RunCount; i++)
    {
      const double diffNs = _NsPerRun[i] - meanNs;
      const double diffClocks = _ClocksPerRun[i] - meanClocks;

      stdDevNs += diffNs * diffNs;
      stdDevClocks += diffClocks * diffClocks;
    }

    stdDevNs = sqrt(stdDevNs / (double)(_RunCount - 1));
    stdDevClocks = sqrt(stdDevClocks / (double)(_RunCount - 1));

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

thread_pool *_pGlobalThreadPool = nullptr;

//////////////////////////////////////////////////////////////////////////

template <typename func_t>
struct func_info_t
{
  const char *name = nullptr;
  func_t func = nullptr;
  bool candidateForFastest = false;
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

template <size_t (*func)(const uint8_t *, const size_t, uint8_t *, const size_t)>
size_t encode_no_hist_wrapper(const uint8_t *pInData, const size_t length, uint8_t *pOutData, const size_t outCapacity, const hist_t *)
{
  return func(pInData, length, pOutData, outCapacity);
}

template <size_t(*func)(const uint8_t *, const size_t, uint8_t *, const size_t, thread_pool *)>
size_t decode_with_thread_pool_wrapper(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity)
{
  if (_pGlobalThreadPool == nullptr)
    _pGlobalThreadPool = thread_pool_new((size_t)rans_max((int64_t)1, (int64_t)thread_pool_max_threads() - 1));

  return func(pInData, inLength, pOutData, outCapacity, _pGlobalThreadPool);
}

static const codec_info_t _Codecs[] =
{
  { "rANS32x32 16w (variable block size)", 15, {{ "encode", encode_no_hist_wrapper<block_rANS32x32_16w_encode_15>, true }, {}}, {{ "decode", block_rANS32x32_16w_decode_15, true }, {}}},
  { "rANS32x32 16w (variable block size)", 14, {{ "encode", encode_no_hist_wrapper<block_rANS32x32_16w_encode_14>, true }, {}}, {{ "decode", block_rANS32x32_16w_decode_14, true }, {}}},
  { "rANS32x32 16w (variable block size)", 13, {{ "encode", encode_no_hist_wrapper<block_rANS32x32_16w_encode_13>, true }, {}}, {{ "decode", block_rANS32x32_16w_decode_13, true }, {}}},
  { "rANS32x32 16w (variable block size)", 12, {{ "encode", encode_no_hist_wrapper<block_rANS32x32_16w_encode_12>, true }, {}}, {{ "decode", block_rANS32x32_16w_decode_12, true }, {}}},
  { "rANS32x32 16w (variable block size)", 11, {{ "encode", encode_no_hist_wrapper<block_rANS32x32_16w_encode_11>, true }, {}}, {{ "decode", block_rANS32x32_16w_decode_11, true }, {}}},
  { "rANS32x32 16w (variable block size)", 10, {{ "encode", encode_no_hist_wrapper<block_rANS32x32_16w_encode_10>, true }, {}}, {{ "decode", block_rANS32x32_16w_decode_10, true }, {}}},
  
  { "rANS32x64 16w (variable block size)", 15, {{ "encode", encode_no_hist_wrapper<block_rANS32x64_16w_encode_15>, true }, {}}, {{ "decode", block_rANS32x64_16w_decode_15, true }, {}}},
  { "rANS32x64 16w (variable block size)", 14, {{ "encode", encode_no_hist_wrapper<block_rANS32x64_16w_encode_14>, true }, {}}, {{ "decode", block_rANS32x64_16w_decode_14, true }, {}}},
  { "rANS32x64 16w (variable block size)", 13, {{ "encode", encode_no_hist_wrapper<block_rANS32x64_16w_encode_13>, true }, {}}, {{ "decode", block_rANS32x64_16w_decode_13, true }, {}}},
  { "rANS32x64 16w (variable block size)", 12, {{ "encode", encode_no_hist_wrapper<block_rANS32x64_16w_encode_12>, true }, {}}, {{ "decode", block_rANS32x64_16w_decode_12, true }, {}}},
  { "rANS32x64 16w (variable block size)", 11, {{ "encode", encode_no_hist_wrapper<block_rANS32x64_16w_encode_11>, true }, {}}, {{ "decode", block_rANS32x64_16w_decode_11, true }, {}}},
  { "rANS32x64 16w (variable block size)", 10, {{ "encode", encode_no_hist_wrapper<block_rANS32x64_16w_encode_10>, true }, {}}, {{ "decode", block_rANS32x64_16w_decode_10, true }, {}}},
  
  { "rANS32x32 16w (independent blocks)", 15, {{ "encode", encode_no_hist_wrapper<mt_rANS32x32_16w_encode_15>, true }, {}}, {{ "decode (single thread)", mt_rANS32x32_16w_decode_15, true }, { "decode (multi threaded)", decode_with_thread_pool_wrapper<mt_rANS32x32_16w_decode_mt_15>, true }, {}}},
  { "rANS32x32 16w (independent blocks)", 14, {{ "encode", encode_no_hist_wrapper<mt_rANS32x32_16w_encode_14>, true }, {}}, {{ "decode (single thread)", mt_rANS32x32_16w_decode_14, true }, { "decode (multi threaded)", decode_with_thread_pool_wrapper<mt_rANS32x32_16w_decode_mt_14>, true }, {}}},
  { "rANS32x32 16w (independent blocks)", 13, {{ "encode", encode_no_hist_wrapper<mt_rANS32x32_16w_encode_13>, true }, {}}, {{ "decode (single thread)", mt_rANS32x32_16w_decode_13, true }, { "decode (multi threaded)", decode_with_thread_pool_wrapper<mt_rANS32x32_16w_decode_mt_13>, true }, {}}},
  { "rANS32x32 16w (independent blocks)", 12, {{ "encode", encode_no_hist_wrapper<mt_rANS32x32_16w_encode_12>, true }, {}}, {{ "decode (single thread)", mt_rANS32x32_16w_decode_12, true }, { "decode (multi threaded)", decode_with_thread_pool_wrapper<mt_rANS32x32_16w_decode_mt_12>, true }, {}}},
  { "rANS32x32 16w (independent blocks)", 11, {{ "encode", encode_no_hist_wrapper<mt_rANS32x32_16w_encode_11>, true }, {}}, {{ "decode (single thread)", mt_rANS32x32_16w_decode_11, true }, { "decode (multi threaded)", decode_with_thread_pool_wrapper<mt_rANS32x32_16w_decode_mt_11>, true }, {}}},
  { "rANS32x32 16w (independent blocks)", 10, {{ "encode", encode_no_hist_wrapper<mt_rANS32x32_16w_encode_10>, true }, {}}, {{ "decode (single thread)", mt_rANS32x32_16w_decode_10, true }, { "decode (multi threaded)", decode_with_thread_pool_wrapper<mt_rANS32x32_16w_decode_mt_10>, true }, {}}},
  
  { "rANS32x64 16w (independent blocks)", 15, {{ "encode", encode_no_hist_wrapper<mt_rANS32x64_16w_encode_15>, true }, {}}, {{ "decode (single thread)", mt_rANS32x64_16w_decode_15, true }, { "decode (multi threaded)", decode_with_thread_pool_wrapper<mt_rANS32x64_16w_decode_mt_15>, true }, {}}},
  { "rANS32x64 16w (independent blocks)", 14, {{ "encode", encode_no_hist_wrapper<mt_rANS32x64_16w_encode_14>, true }, {}}, {{ "decode (single thread)", mt_rANS32x64_16w_decode_14, true }, { "decode (multi threaded)", decode_with_thread_pool_wrapper<mt_rANS32x64_16w_decode_mt_14>, true }, {}}},
  { "rANS32x64 16w (independent blocks)", 13, {{ "encode", encode_no_hist_wrapper<mt_rANS32x64_16w_encode_13>, true }, {}}, {{ "decode (single thread)", mt_rANS32x64_16w_decode_13, true }, { "decode (multi threaded)", decode_with_thread_pool_wrapper<mt_rANS32x64_16w_decode_mt_13>, true }, {}}},
  { "rANS32x64 16w (independent blocks)", 12, {{ "encode", encode_no_hist_wrapper<mt_rANS32x64_16w_encode_12>, true }, {}}, {{ "decode (single thread)", mt_rANS32x64_16w_decode_12, true }, { "decode (multi threaded)", decode_with_thread_pool_wrapper<mt_rANS32x64_16w_decode_mt_12>, true }, {}}},
  { "rANS32x64 16w (independent blocks)", 11, {{ "encode", encode_no_hist_wrapper<mt_rANS32x64_16w_encode_11>, true }, {}}, {{ "decode (single thread)", mt_rANS32x64_16w_decode_11, true }, { "decode (multi threaded)", decode_with_thread_pool_wrapper<mt_rANS32x64_16w_decode_mt_11>, true }, {}}},
  { "rANS32x64 16w (independent blocks)", 10, {{ "encode", encode_no_hist_wrapper<mt_rANS32x64_16w_encode_10>, true }, {}}, {{ "decode (single thread)", mt_rANS32x64_16w_decode_10, true }, { "decode (multi threaded)", decode_with_thread_pool_wrapper<mt_rANS32x64_16w_decode_mt_10>, true }, {}}},
  
  { "rANS32x32 16w (raw)", 15, {{ "enc scalar", rANS32x32_16w_encode_scalar_15, true }, {}}, {{ "dec scalar", rANS32x32_16w_decode_scalar_15 }, { "dec avx2 (xmm shfl, sym dep gthr)", rANS32x32_xmmShfl_16w_decode_avx2_varA_15 }, { "dec avx2 (xmm shfl, sym idp gthr)", rANS32x32_xmmShfl_16w_decode_avx2_varB_15 }, { "dec avx2 (xmm shfl2, sym dep gthr)", rANS32x32_xmmShfl2_16w_decode_avx2_varA_15, true }, { "dec avx2 (xmm shfl2, sym idp gthr)", rANS32x32_xmmShfl2_16w_decode_avx2_varB_15 }, { "dec avx2 (ymm perm, sym dep gthr)", rANS32x32_ymmPerm_16w_decode_avx2_varA_15 }, { "dec avx2 (ymm perm, sym idp gthr)", rANS32x32_ymmPerm_16w_decode_avx2_varB_15 }, {}}},
  { "rANS32x32 16w (raw)", 14, {{ "enc scalar", rANS32x32_16w_encode_scalar_14, true }, {}}, {{ "dec scalar", rANS32x32_16w_decode_scalar_14 }, { "dec avx2 (xmm shfl, sym dep gthr)", rANS32x32_xmmShfl_16w_decode_avx2_varA_14 }, { "dec avx2 (xmm shfl, sym idp gthr)", rANS32x32_xmmShfl_16w_decode_avx2_varB_14 }, { "dec avx2 (xmm shfl2, sym dep gthr)", rANS32x32_xmmShfl2_16w_decode_avx2_varA_14, true }, { "dec avx2 (xmm shfl2, sym idp gthr)", rANS32x32_xmmShfl2_16w_decode_avx2_varB_14 }, { "dec avx2 (ymm perm, sym dep gthr)", rANS32x32_ymmPerm_16w_decode_avx2_varA_14 }, { "dec avx2 (ymm perm, sym idp gthr)", rANS32x32_ymmPerm_16w_decode_avx2_varB_14 }, {}}},
  { "rANS32x32 16w (raw)", 13, {{ "enc scalar", rANS32x32_16w_encode_scalar_13, true }, {}}, {{ "dec scalar", rANS32x32_16w_decode_scalar_13 }, { "dec avx2 (xmm shfl, sym dep gthr)", rANS32x32_xmmShfl_16w_decode_avx2_varA_13 }, { "dec avx2 (xmm shfl, sym idp gthr)", rANS32x32_xmmShfl_16w_decode_avx2_varB_13 }, { "dec avx2 (xmm shfl2, sym dep gthr)", rANS32x32_xmmShfl2_16w_decode_avx2_varA_13, true }, { "dec avx2 (xmm shfl2, sym idp gthr)", rANS32x32_xmmShfl2_16w_decode_avx2_varB_13 }, { "dec avx2 (ymm perm, sym dep gthr)", rANS32x32_ymmPerm_16w_decode_avx2_varA_13 }, { "dec avx2 (ymm perm, sym idp gthr)", rANS32x32_ymmPerm_16w_decode_avx2_varB_13 }, {}}},
  { "rANS32x32 16w (raw)", 12, {{ "enc scalar", rANS32x32_16w_encode_scalar_12, true }, {}}, {{ "dec scalar", rANS32x32_16w_decode_scalar_12 }, { "dec avx2 (xmm shfl, sym dep gthr)", rANS32x32_xmmShfl_16w_decode_avx2_varA_12 }, { "dec avx2 (xmm shfl, sym idp gthr)", rANS32x32_xmmShfl_16w_decode_avx2_varB_12 }, { "dec avx2 (xmm shfl, 1x gthr)", rANS32x32_xmmShfl_16w_decode_avx2_varC_12 }, { "dec avx2 (xmm shfl2, sym dep gthr)", rANS32x32_xmmShfl2_16w_decode_avx2_varA_12 }, { "dec avx2 (xmm shfl2, sym idp gthr)", rANS32x32_xmmShfl2_16w_decode_avx2_varB_12 }, { "dec avx2 (xmm shfl2, 1x gthr)", rANS32x32_xmmShfl2_16w_decode_avx2_varC_12, true }, { "dec avx2 (ymm perm, sym dep gthr)", rANS32x32_ymmPerm_16w_decode_avx2_varA_12 }, { "dec avx2 (ymm perm, sym idp gthr)", rANS32x32_ymmPerm_16w_decode_avx2_varB_12 }, { "dec avx2 (ymm perm, 1x gthr)", rANS32x32_ymmPerm_16w_decode_avx2_varC_12 }, { "dec avx2 (xmm shfl, 1x pre-gthr)", rANS32x32_xmmShfl_16w_decode_avx2_pregather_varC_12 }, { "dec avx2 (xmm shfl2, 1x pre-gthr)", rANS32x32_xmmShfl2_16w_decode_avx2_pregather_varC_12 }, { "dec avx2 (ymm perm, 1x pre-gthr)", rANS32x32_ymmPerm_16w_decode_avx2_pregather_varC_12 }, { "dec avx2 (xmm shfl, 1x s pre-gthr)", rANS32x32_xmmShfl_16w_decode_avx2_scalarpregather_varC_12 }, { "dec avx2 (xmm shfl2, 1x s pre-gthr)", rANS32x32_xmmShfl2_16w_decode_avx2_scalarpregather_varC_12 }, { "dec avx2 (ymm perm, 1x s pre-gthr)", rANS32x32_ymmPerm_16w_decode_avx2_scalarpregather_varC_12 }, { "dec avx2 (xmm shfl, 1x erly gthr)", rANS32x32_xmmShfl_16w_decode_avx2_earlygather_varC_12 }, { "dec avx2 (xmm shfl2, 1x erly gthr)", rANS32x32_xmmShfl2_16w_decode_avx2_earlygather_varC_12 }, { "dec avx2 (ymm perm, 1x erly gthr)", rANS32x32_ymmPerm_16w_decode_avx2_earlygather_varC_12 }, { "dec avx2 (xmm shfl, 1x pref gthr)", rANS32x32_xmmShfl_16w_decode_avx2_prefetch_varC_12 }, { "dec avx2 (xmm shfl2, 1x pref gthr)", rANS32x32_xmmShfl2_16w_decode_avx2_prefetch_varC_12 }, { "dec avx2 (ymm perm, 1x pref gthr)", rANS32x32_ymmPerm_16w_decode_avx2_prefetch_varC_12 }, { "dec avx512 (xmm shfl, 1x gthr)", rANS32x32_xmmShfl_16w_decode_avx512_varC_12 }, { "dec avx512 (xmm shfl2, 1x gthr)", rANS32x32_xmmShfl2_16w_decode_avx512_varC_12 }, { "dec avx512 (ymm shfl, 1x gthr)", rANS32x32_ymmShfl_16w_decode_avx512_varC_12 }, { "dec avx512 (ymm shfl2, 1x gthr)", rANS32x32_ymmShfl2_16w_decode_avx512_varC_12, true }, { "dec avx512 (zmm perm, 1x gthr)", rANS32x32_zmmPerm_16w_decode_avx512_varC_12 }, { "dec avx512 (xmm shfl, 1x ymm gthr)", rANS32x32_xmmShfl_16w_decode_avx512_ymmGthr_varC_12 }, { "dec avx512 (xmm shfl2, 1x ymm gthr)", rANS32x32_xmmShfl2_16w_decode_avx512_ymmGthr_varC_12, true }, { "dec avx512 (ymm shfl, 1x ymm gthr)", rANS32x32_ymmShfl_16w_decode_avx512_ymmGthr_varC_12 }, { "dec avx512 (ymm shfl2, 1x ymm gthr)", rANS32x32_ymmShfl2_16w_decode_avx512_ymmGthr_varC_12, true }, {}}},
  { "rANS32x32 16w (raw)", 11, {{ "enc scalar", rANS32x32_16w_encode_scalar_11, true }, {}}, {{ "dec scalar", rANS32x32_16w_decode_scalar_11 }, { "dec avx2 (xmm shfl, sym dep gthr)", rANS32x32_xmmShfl_16w_decode_avx2_varA_11 }, { "dec avx2 (xmm shfl, sym idp gthr)", rANS32x32_xmmShfl_16w_decode_avx2_varB_11 }, { "dec avx2 (xmm shfl, 1x gthr)", rANS32x32_xmmShfl_16w_decode_avx2_varC_11 }, { "dec avx2 (xmm shfl2, sym dep gthr)", rANS32x32_xmmShfl2_16w_decode_avx2_varA_11 }, { "dec avx2 (xmm shfl2, sym idp gthr)", rANS32x32_xmmShfl2_16w_decode_avx2_varB_11 }, { "dec avx2 (xmm shfl2, 1x gthr)", rANS32x32_xmmShfl2_16w_decode_avx2_varC_11, true }, { "dec avx2 (ymm perm, sym dep gthr)", rANS32x32_ymmPerm_16w_decode_avx2_varA_11 }, { "dec avx2 (ymm perm, sym idp gthr)", rANS32x32_ymmPerm_16w_decode_avx2_varB_11 }, { "dec avx2 (ymm perm, 1x gthr)", rANS32x32_ymmPerm_16w_decode_avx2_varC_11 }, { "dec avx2 (xmm shfl, 1x pre-gthr)", rANS32x32_xmmShfl_16w_decode_avx2_pregather_varC_11 }, { "dec avx2 (xmm shfl2, 1x pre-gthr)", rANS32x32_xmmShfl2_16w_decode_avx2_pregather_varC_11 }, { "dec avx2 (ymm perm, 1x pre-gthr)", rANS32x32_ymmPerm_16w_decode_avx2_pregather_varC_11 }, { "dec avx2 (xmm shfl, 1x s pre-gthr)", rANS32x32_xmmShfl_16w_decode_avx2_scalarpregather_varC_11 }, { "dec avx2 (xmm shfl2, 1x s pre-gthr)", rANS32x32_xmmShfl2_16w_decode_avx2_scalarpregather_varC_11 }, { "dec avx2 (ymm perm, 1x s pre-gthr)", rANS32x32_ymmPerm_16w_decode_avx2_scalarpregather_varC_11 }, { "dec avx2 (xmm shfl, 1x erly gthr)", rANS32x32_xmmShfl_16w_decode_avx2_earlygather_varC_11 }, { "dec avx2 (xmm shfl2, 1x erly gthr)", rANS32x32_xmmShfl2_16w_decode_avx2_earlygather_varC_11 }, { "dec avx2 (ymm perm, 1x erly gthr)", rANS32x32_ymmPerm_16w_decode_avx2_earlygather_varC_11 }, { "dec avx2 (xmm shfl, 1x pref gthr)", rANS32x32_xmmShfl_16w_decode_avx2_prefetch_varC_11 }, { "dec avx2 (xmm shfl2, 1x pref gthr)", rANS32x32_xmmShfl2_16w_decode_avx2_prefetch_varC_11 }, { "dec avx2 (ymm perm, 1x pref gthr)", rANS32x32_ymmPerm_16w_decode_avx2_prefetch_varC_11 }, { "dec avx512 (xmm shfl, 1x gthr)", rANS32x32_xmmShfl_16w_decode_avx512_varC_11 }, { "dec avx512 (xmm shfl2, 1x gthr)", rANS32x32_xmmShfl2_16w_decode_avx512_varC_11 }, { "dec avx512 (ymm shfl, 1x gthr)", rANS32x32_ymmShfl_16w_decode_avx512_varC_11 }, { "dec avx512 (ymm shfl2, 1x gthr)", rANS32x32_ymmShfl2_16w_decode_avx512_varC_11, true }, { "dec avx512 (zmm perm, 1x gthr)", rANS32x32_zmmPerm_16w_decode_avx512_varC_11 }, { "dec avx512 (xmm shfl, 1x ymm gthr)", rANS32x32_xmmShfl_16w_decode_avx512_ymmGthr_varC_11 }, { "dec avx512 (xmm shfl2, 1x ymm gthr)", rANS32x32_xmmShfl2_16w_decode_avx512_ymmGthr_varC_11, true }, { "dec avx512 (ymm shfl, 1x ymm gthr)", rANS32x32_ymmShfl_16w_decode_avx512_ymmGthr_varC_11 }, { "dec avx512 (ymm shfl2, 1x ymm gthr)", rANS32x32_ymmShfl2_16w_decode_avx512_ymmGthr_varC_11, true }, {}}},
  { "rANS32x32 16w (raw)", 10, {{ "enc scalar", rANS32x32_16w_encode_scalar_10, true }, {}}, {{ "dec scalar", rANS32x32_16w_decode_scalar_10 }, { "dec avx2 (xmm shfl, sym dep gthr)", rANS32x32_xmmShfl_16w_decode_avx2_varA_10 }, { "dec avx2 (xmm shfl, sym idp gthr)", rANS32x32_xmmShfl_16w_decode_avx2_varB_10 }, { "dec avx2 (xmm shfl, 1x gthr)", rANS32x32_xmmShfl_16w_decode_avx2_varC_10 }, { "dec avx2 (xmm shfl2, sym dep gthr)", rANS32x32_xmmShfl2_16w_decode_avx2_varA_10 }, { "dec avx2 (xmm shfl2, sym idp gthr)", rANS32x32_xmmShfl2_16w_decode_avx2_varB_10 }, { "dec avx2 (xmm shfl2, 1x gthr)", rANS32x32_xmmShfl2_16w_decode_avx2_varC_10, true }, { "dec avx2 (ymm perm, sym dep gthr)", rANS32x32_ymmPerm_16w_decode_avx2_varA_10 }, { "dec avx2 (ymm perm, sym idp gthr)", rANS32x32_ymmPerm_16w_decode_avx2_varB_10 }, { "dec avx2 (ymm perm, 1x gthr)", rANS32x32_ymmPerm_16w_decode_avx2_varC_10 }, { "dec avx2 (xmm shfl, 1x pre-gthr)", rANS32x32_xmmShfl_16w_decode_avx2_pregather_varC_10 }, { "dec avx2 (xmm shfl2, 1x pre-gthr)", rANS32x32_xmmShfl2_16w_decode_avx2_pregather_varC_10 }, { "dec avx2 (ymm perm, 1x pre-gthr)", rANS32x32_ymmPerm_16w_decode_avx2_pregather_varC_10 }, { "dec avx2 (xmm shfl, 1x s pre-gthr)", rANS32x32_xmmShfl_16w_decode_avx2_scalarpregather_varC_10 }, { "dec avx2 (xmm shfl2, 1x s pre-gthr)", rANS32x32_xmmShfl2_16w_decode_avx2_scalarpregather_varC_10 }, { "dec avx2 (ymm perm, 1x s pre-gthr)", rANS32x32_ymmPerm_16w_decode_avx2_scalarpregather_varC_10 }, { "dec avx2 (xmm shfl, 1x erly gthr)", rANS32x32_xmmShfl_16w_decode_avx2_earlygather_varC_10 }, { "dec avx2 (xmm shfl2, 1x erly gthr)", rANS32x32_xmmShfl2_16w_decode_avx2_earlygather_varC_10 }, { "dec avx2 (ymm perm, 1x erly gthr)", rANS32x32_ymmPerm_16w_decode_avx2_earlygather_varC_10 }, { "dec avx2 (xmm shfl, 1x pref gthr)", rANS32x32_xmmShfl_16w_decode_avx2_prefetch_varC_10 }, { "dec avx2 (xmm shfl2, 1x pref gthr)", rANS32x32_xmmShfl2_16w_decode_avx2_prefetch_varC_10 }, { "dec avx2 (ymm perm, 1x pref gthr)", rANS32x32_ymmPerm_16w_decode_avx2_prefetch_varC_10 }, { "dec avx512 (xmm shfl, 1x gthr)", rANS32x32_xmmShfl_16w_decode_avx512_varC_10 }, { "dec avx512 (xmm shfl2, 1x gthr)", rANS32x32_xmmShfl2_16w_decode_avx512_varC_10 }, { "dec avx512 (ymm shfl, 1x gthr)", rANS32x32_ymmShfl_16w_decode_avx512_varC_10 }, { "dec avx512 (ymm shfl2, 1x gthr)", rANS32x32_ymmShfl2_16w_decode_avx512_varC_10, true }, { "dec avx512 (zmm perm, 1x gthr)", rANS32x32_zmmPerm_16w_decode_avx512_varC_10 }, { "dec avx512 (xmm shfl, 1x ymm gthr)", rANS32x32_xmmShfl_16w_decode_avx512_ymmGthr_varC_10 }, { "dec avx512 (xmm shfl2, 1x ymm gthr)", rANS32x32_xmmShfl2_16w_decode_avx512_ymmGthr_varC_10, true }, { "dec avx512 (ymm shfl, 1x ymm gthr)", rANS32x32_ymmShfl_16w_decode_avx512_ymmGthr_varC_10 }, { "dec avx512 (ymm shfl2, 1x ymm gthr)", rANS32x32_ymmShfl2_16w_decode_avx512_ymmGthr_varC_10, true }, {}}},

  { "rANS32x64 16w (raw)", 15, {{ "enc scalar", rANS32x64_16w_encode_scalar_15, true }, {}}, {{ "dec scalar", rANS32x64_16w_decode_scalar_15 }, { "dec avx2 (xmm shfl, sym dep gthr)", rANS32x64_xmmShfl_16w_decode_avx2_varA_15 }, { "dec avx2 (xmm shfl, sym idp gthr)", rANS32x64_xmmShfl_16w_decode_avx2_varB_15 }, { "dec avx2 (xmm shfl2, sym dep gthr)", rANS32x64_xmmShfl2_16w_decode_avx2_varA_15, true }, { "dec avx2 (xmm shfl2, sym idp gthr)", rANS32x64_xmmShfl2_16w_decode_avx2_varB_15 }, { "dec avx2 (ymm perm, sym dep gthr)", rANS32x64_ymmPerm_16w_decode_avx2_varA_15 }, { "dec avx2 (ymm perm, sym idp gthr)", rANS32x64_ymmPerm_16w_decode_avx2_varB_15 }, { "dec avx512 (xmm shfl, sym dep gthr)", rANS32x64_xmmShfl_16w_decode_avx512_varA_15 }, { "dec avx512 (xmm shfl, sym idp gthr)", rANS32x64_xmmShfl_16w_decode_avx512_varB_15 }, { "dec avx512 (xmm shfl2, sym dep gthr)", rANS32x64_xmmShfl2_16w_decode_avx512_varA_15, true }, { "dec avx512 (xmm shfl2, sym idp gthr)", rANS32x64_xmmShfl2_16w_decode_avx512_varB_15 }, { "dec avx512 (ymm shfl, sym dep gthr)", rANS32x64_ymmShfl_16w_decode_avx512_varA_15 }, { "dec avx512 (ymm shfl, sym idp gthr)", rANS32x64_ymmShfl_16w_decode_avx512_varB_15 }, { "dec avx512 (ymm shfl2, sym dep gthr)", rANS32x64_ymmShfl2_16w_decode_avx512_varA_15, true }, { "dec avx512 (ymm shfl2, sym idp gthr)", rANS32x64_ymmShfl2_16w_decode_avx512_varB_15 }, { "dec avx512 (zmm perm, sym dep gthr)", rANS32x64_zmmPerm_16w_decode_avx512_varA_15 }, { "dec avx512 (zmm perm, sym idp gthr)", rANS32x64_zmmPerm_16w_decode_avx512_varB_15 }, {}}},
  { "rANS32x64 16w (raw)", 14, {{ "enc scalar", rANS32x64_16w_encode_scalar_14, true }, {}}, {{ "dec scalar", rANS32x64_16w_decode_scalar_14 }, { "dec avx2 (xmm shfl, sym dep gthr)", rANS32x64_xmmShfl_16w_decode_avx2_varA_14 }, { "dec avx2 (xmm shfl, sym idp gthr)", rANS32x64_xmmShfl_16w_decode_avx2_varB_14 }, { "dec avx2 (xmm shfl2, sym dep gthr)", rANS32x64_xmmShfl2_16w_decode_avx2_varA_14, true }, { "dec avx2 (xmm shfl2, sym idp gthr)", rANS32x64_xmmShfl2_16w_decode_avx2_varB_14 }, { "dec avx2 (ymm perm, sym dep gthr)", rANS32x64_ymmPerm_16w_decode_avx2_varA_14 }, { "dec avx2 (ymm perm, sym idp gthr)", rANS32x64_ymmPerm_16w_decode_avx2_varB_14 }, { "dec avx512 (xmm shfl, sym dep gthr)", rANS32x64_xmmShfl_16w_decode_avx512_varA_14 }, { "dec avx512 (xmm shfl, sym idp gthr)", rANS32x64_xmmShfl_16w_decode_avx512_varB_14 }, { "dec avx512 (xmm shfl2, sym dep gthr)", rANS32x64_xmmShfl2_16w_decode_avx512_varA_14, true }, { "dec avx512 (xmm shfl2, sym idp gthr)", rANS32x64_xmmShfl2_16w_decode_avx512_varB_14 }, { "dec avx512 (ymm shfl, sym dep gthr)", rANS32x64_ymmShfl_16w_decode_avx512_varA_14 }, { "dec avx512 (ymm shfl, sym idp gthr)", rANS32x64_ymmShfl_16w_decode_avx512_varB_14 }, { "dec avx512 (ymm shfl2, sym dep gthr)", rANS32x64_ymmShfl2_16w_decode_avx512_varA_14, true }, { "dec avx512 (ymm shfl2, sym idp gthr)", rANS32x64_ymmShfl2_16w_decode_avx512_varB_14 }, { "dec avx512 (zmm perm, sym dep gthr)", rANS32x64_zmmPerm_16w_decode_avx512_varA_14 }, { "dec avx512 (zmm perm, sym idp gthr)", rANS32x64_zmmPerm_16w_decode_avx512_varB_14 }, {}}},
  { "rANS32x64 16w (raw)", 13, {{ "enc scalar", rANS32x64_16w_encode_scalar_13, true }, {}}, {{ "dec scalar", rANS32x64_16w_decode_scalar_13 }, { "dec avx2 (xmm shfl, sym dep gthr)", rANS32x64_xmmShfl_16w_decode_avx2_varA_13 }, { "dec avx2 (xmm shfl, sym idp gthr)", rANS32x64_xmmShfl_16w_decode_avx2_varB_13 }, { "dec avx2 (xmm shfl2, sym dep gthr)", rANS32x64_xmmShfl2_16w_decode_avx2_varA_13, true }, { "dec avx2 (xmm shfl2, sym idp gthr)", rANS32x64_xmmShfl2_16w_decode_avx2_varB_13 }, { "dec avx2 (ymm perm, sym dep gthr)", rANS32x64_ymmPerm_16w_decode_avx2_varA_13 }, { "dec avx2 (ymm perm, sym idp gthr)", rANS32x64_ymmPerm_16w_decode_avx2_varB_13 }, { "dec avx512 (xmm shfl, sym dep gthr)", rANS32x64_xmmShfl_16w_decode_avx512_varA_13 }, { "dec avx512 (xmm shfl, sym idp gthr)", rANS32x64_xmmShfl_16w_decode_avx512_varB_13 }, { "dec avx512 (xmm shfl2, sym dep gthr)", rANS32x64_xmmShfl2_16w_decode_avx512_varA_13, true }, { "dec avx512 (xmm shfl2, sym idp gthr)", rANS32x64_xmmShfl2_16w_decode_avx512_varB_13 }, { "dec avx512 (ymm shfl, sym dep gthr)", rANS32x64_ymmShfl_16w_decode_avx512_varA_13 }, { "dec avx512 (ymm shfl, sym idp gthr)", rANS32x64_ymmShfl_16w_decode_avx512_varB_13 }, { "dec avx512 (ymm shfl2, sym dep gthr)", rANS32x64_ymmShfl2_16w_decode_avx512_varA_13, true }, { "dec avx512 (ymm shfl2, sym idp gthr)", rANS32x64_ymmShfl2_16w_decode_avx512_varB_13 }, { "dec avx512 (zmm perm, sym dep gthr)", rANS32x64_zmmPerm_16w_decode_avx512_varA_13 }, { "dec avx512 (zmm perm, sym idp gthr)", rANS32x64_zmmPerm_16w_decode_avx512_varB_13 }, {}}},
  { "rANS32x64 16w (raw)", 12, {{ "enc scalar", rANS32x64_16w_encode_scalar_12, true }, {}}, {{ "dec scalar", rANS32x64_16w_decode_scalar_12 }, { "dec avx2 (xmm shfl, sym dep gthr)", rANS32x64_xmmShfl_16w_decode_avx2_varA_12 }, { "dec avx2 (xmm shfl, sym idp gthr)", rANS32x64_xmmShfl_16w_decode_avx2_varB_12 }, { "dec avx2 (xmm shfl, 1x gthr)", rANS32x64_xmmShfl_16w_decode_avx2_varC_12 }, { "dec avx2 (xmm shfl2, sym dep gthr)", rANS32x64_xmmShfl2_16w_decode_avx2_varA_12 }, { "dec avx2 (xmm shfl2, sym idp gthr)", rANS32x64_xmmShfl2_16w_decode_avx2_varB_12 }, { "dec avx2 (xmm shfl2, 1x gthr)", rANS32x64_xmmShfl2_16w_decode_avx2_varC_12, true }, { "dec avx2 (ymm perm, sym dep gthr)", rANS32x64_ymmPerm_16w_decode_avx2_varA_12 }, { "dec avx2 (ymm perm, sym idp gthr)", rANS32x64_ymmPerm_16w_decode_avx2_varB_12 }, { "dec avx2 (ymm perm, 1x gthr)", rANS32x64_ymmPerm_16w_decode_avx2_varC_12 }, { "dec avx512 (xmm shfl, sym dep gthr)", rANS32x64_xmmShfl_16w_decode_avx512_varA_12 }, { "dec avx512 (xmm shfl, sym idp gthr)", rANS32x64_xmmShfl_16w_decode_avx512_varB_12 }, { "dec avx512 (xmm shfl, 1x gthr)", rANS32x64_xmmShfl_16w_decode_avx512_varC_12 }, { "dec avx512 (xmm shfl2, sym dep gthr)", rANS32x64_xmmShfl2_16w_decode_avx512_varA_12 }, { "dec avx512 (xmm shfl2, sym idp gthr)", rANS32x64_xmmShfl2_16w_decode_avx512_varB_12 }, { "dec avx512 (xmm shfl2, 1x gthr)", rANS32x64_xmmShfl2_16w_decode_avx512_varC_12, true }, { "dec avx512 (ymm shfl, sym dep gthr)", rANS32x64_ymmShfl_16w_decode_avx512_varA_12 }, { "dec avx512 (ymm shfl, sym idp gthr)", rANS32x64_ymmShfl_16w_decode_avx512_varB_12 }, { "dec avx512 (ymm shfl, 1x gthr)", rANS32x64_ymmShfl_16w_decode_avx512_varC_12 }, { "dec avx512 (ymm shfl2, sym dep gthr)", rANS32x64_ymmShfl2_16w_decode_avx512_varA_12 }, { "dec avx512 (ymm shfl2, sym idp gthr)", rANS32x64_ymmShfl2_16w_decode_avx512_varB_12 }, { "dec avx512 (ymm shfl2, 1x gthr)", rANS32x64_ymmShfl2_16w_decode_avx512_varC_12, true }, { "dec avx512 (zmm perm, sym dep gthr)", rANS32x64_zmmPerm_16w_decode_avx512_varA_12 }, { "dec avx512 (zmm perm, sym idp gthr)", rANS32x64_zmmPerm_16w_decode_avx512_varB_12 }, { "dec avx512 (zmm perm, 1x gthr)", rANS32x64_zmmPerm_16w_decode_avx512_varC_12 }, { "dec avx512 (xmm shfl, 1x ymm gthr)", rANS32x64_xmmShfl_16w_decode_avx512_ymmGthr_varC_12 }, { "dec avx512 (xmm shfl2, 1x ymm gthr)", rANS32x64_xmmShfl2_16w_decode_avx512_ymmGthr_varC_12, true }, { "dec avx512 (ymm shfl, 1x ymm gthr)", rANS32x64_ymmShfl_16w_decode_avx512_ymmGthr_varC_12 }, { "dec avx512 (ymm shfl2, 1x ymm gthr)", rANS32x64_ymmShfl2_16w_decode_avx512_ymmGthr_varC_12, true }, {}}},
  { "rANS32x64 16w (raw)", 11, {{ "enc scalar", rANS32x64_16w_encode_scalar_11, true }, {}}, {{ "dec scalar", rANS32x64_16w_decode_scalar_11 }, { "dec avx2 (xmm shfl, sym dep gthr)", rANS32x64_xmmShfl_16w_decode_avx2_varA_11 }, { "dec avx2 (xmm shfl, sym idp gthr)", rANS32x64_xmmShfl_16w_decode_avx2_varB_11 }, { "dec avx2 (xmm shfl, 1x gthr)", rANS32x64_xmmShfl_16w_decode_avx2_varC_11 }, { "dec avx2 (xmm shfl2, sym dep gthr)", rANS32x64_xmmShfl2_16w_decode_avx2_varA_11 }, { "dec avx2 (xmm shfl2, sym idp gthr)", rANS32x64_xmmShfl2_16w_decode_avx2_varB_11 }, { "dec avx2 (xmm shfl2, 1x gthr)", rANS32x64_xmmShfl2_16w_decode_avx2_varC_11, true }, { "dec avx2 (ymm perm, sym dep gthr)", rANS32x64_ymmPerm_16w_decode_avx2_varA_11 }, { "dec avx2 (ymm perm, sym idp gthr)", rANS32x64_ymmPerm_16w_decode_avx2_varB_11 }, { "dec avx2 (ymm perm, 1x gthr)", rANS32x64_ymmPerm_16w_decode_avx2_varC_11 }, { "dec avx512 (xmm shfl, sym dep gthr)", rANS32x64_xmmShfl_16w_decode_avx512_varA_11 }, { "dec avx512 (xmm shfl, sym idp gthr)", rANS32x64_xmmShfl_16w_decode_avx512_varB_11 }, { "dec avx512 (xmm shfl, 1x gthr)", rANS32x64_xmmShfl_16w_decode_avx512_varC_11 }, { "dec avx512 (xmm shfl2, sym dep gthr)", rANS32x64_xmmShfl2_16w_decode_avx512_varA_11 }, { "dec avx512 (xmm shfl2, sym idp gthr)", rANS32x64_xmmShfl2_16w_decode_avx512_varB_11 }, { "dec avx512 (xmm shfl2, 1x gthr)", rANS32x64_xmmShfl2_16w_decode_avx512_varC_11, true }, { "dec avx512 (ymm shfl, sym dep gthr)", rANS32x64_ymmShfl_16w_decode_avx512_varA_11 }, { "dec avx512 (ymm shfl, sym idp gthr)", rANS32x64_ymmShfl_16w_decode_avx512_varB_11 }, { "dec avx512 (ymm shfl, 1x gthr)", rANS32x64_ymmShfl_16w_decode_avx512_varC_11 }, { "dec avx512 (ymm shfl2, sym dep gthr)", rANS32x64_ymmShfl2_16w_decode_avx512_varA_11 }, { "dec avx512 (ymm shfl2, sym idp gthr)", rANS32x64_ymmShfl2_16w_decode_avx512_varB_11 }, { "dec avx512 (ymm shfl2, 1x gthr)", rANS32x64_ymmShfl2_16w_decode_avx512_varC_11, true }, { "dec avx512 (zmm perm, sym dep gthr)", rANS32x64_zmmPerm_16w_decode_avx512_varA_11 }, { "dec avx512 (zmm perm, sym idp gthr)", rANS32x64_zmmPerm_16w_decode_avx512_varB_11 }, { "dec avx512 (zmm perm, 1x gthr)", rANS32x64_zmmPerm_16w_decode_avx512_varC_11 }, { "dec avx512 (xmm shfl, 1x ymm gthr)", rANS32x64_xmmShfl_16w_decode_avx512_ymmGthr_varC_11 }, { "dec avx512 (xmm shfl2, 1x ymm gthr)", rANS32x64_xmmShfl2_16w_decode_avx512_ymmGthr_varC_11, true }, { "dec avx512 (ymm shfl, 1x ymm gthr)", rANS32x64_ymmShfl_16w_decode_avx512_ymmGthr_varC_11 }, { "dec avx512 (ymm shfl2, 1x ymm gthr)", rANS32x64_ymmShfl2_16w_decode_avx512_ymmGthr_varC_11, true }, {}}},
  { "rANS32x64 16w (raw)", 10, {{ "enc scalar", rANS32x64_16w_encode_scalar_10, true }, {}}, {{ "dec scalar", rANS32x64_16w_decode_scalar_10 }, { "dec avx2 (xmm shfl, sym dep gthr)", rANS32x64_xmmShfl_16w_decode_avx2_varA_10 }, { "dec avx2 (xmm shfl, sym idp gthr)", rANS32x64_xmmShfl_16w_decode_avx2_varB_10 }, { "dec avx2 (xmm shfl, 1x gthr)", rANS32x64_xmmShfl_16w_decode_avx2_varC_10 }, { "dec avx2 (xmm shfl2, sym dep gthr)", rANS32x64_xmmShfl2_16w_decode_avx2_varA_10 }, { "dec avx2 (xmm shfl2, sym idp gthr)", rANS32x64_xmmShfl2_16w_decode_avx2_varB_10 }, { "dec avx2 (xmm shfl2, 1x gthr)", rANS32x64_xmmShfl2_16w_decode_avx2_varC_10, true }, { "dec avx2 (ymm perm, sym dep gthr)", rANS32x64_ymmPerm_16w_decode_avx2_varA_10 }, { "dec avx2 (ymm perm, sym idp gthr)", rANS32x64_ymmPerm_16w_decode_avx2_varB_10 }, { "dec avx2 (ymm perm, 1x gthr)", rANS32x64_ymmPerm_16w_decode_avx2_varC_10 }, { "dec avx512 (xmm shfl, sym dep gthr)", rANS32x64_xmmShfl_16w_decode_avx512_varA_10 }, { "dec avx512 (xmm shfl, sym idp gthr)", rANS32x64_xmmShfl_16w_decode_avx512_varB_10 }, { "dec avx512 (xmm shfl, 1x gthr)", rANS32x64_xmmShfl_16w_decode_avx512_varC_10 }, { "dec avx512 (xmm shfl2, sym dep gthr)", rANS32x64_xmmShfl2_16w_decode_avx512_varA_10 }, { "dec avx512 (xmm shfl2, sym idp gthr)", rANS32x64_xmmShfl2_16w_decode_avx512_varB_10 }, { "dec avx512 (xmm shfl2, 1x gthr)", rANS32x64_xmmShfl2_16w_decode_avx512_varC_10, true }, { "dec avx512 (ymm shfl, sym dep gthr)", rANS32x64_ymmShfl_16w_decode_avx512_varA_10 }, { "dec avx512 (ymm shfl, sym idp gthr)", rANS32x64_ymmShfl_16w_decode_avx512_varB_10 }, { "dec avx512 (ymm shfl, 1x gthr)", rANS32x64_ymmShfl_16w_decode_avx512_varC_10 }, { "dec avx512 (ymm shfl2, sym dep gthr)", rANS32x64_ymmShfl2_16w_decode_avx512_varA_10 }, { "dec avx512 (ymm shfl2, sym idp gthr)", rANS32x64_ymmShfl2_16w_decode_avx512_varB_10 }, { "dec avx512 (ymm shfl2, 1x gthr)", rANS32x64_ymmShfl2_16w_decode_avx512_varC_10, true }, { "dec avx512 (zmm perm, sym dep gthr)", rANS32x64_zmmPerm_16w_decode_avx512_varA_10 }, { "dec avx512 (zmm perm, sym idp gthr)", rANS32x64_zmmPerm_16w_decode_avx512_varB_10 }, { "dec avx512 (zmm perm, 1x gthr)", rANS32x64_zmmPerm_16w_decode_avx512_varC_10 }, { "dec avx512 (xmm shfl, 1x ymm gthr)", rANS32x64_xmmShfl_16w_decode_avx512_ymmGthr_varC_10 }, { "dec avx512 (xmm shfl2, 1x ymm gthr)", rANS32x64_xmmShfl2_16w_decode_avx512_ymmGthr_varC_10, true }, { "dec avx512 (ymm shfl, 1x ymm gthr)", rANS32x64_ymmShfl_16w_decode_avx512_ymmGthr_varC_10 }, { "dec avx512 (ymm shfl2, 1x ymm gthr)", rANS32x64_ymmShfl2_16w_decode_avx512_ymmGthr_varC_10, true }, {}}},
  
  { "rANS32x16 16w (raw)", 15, {{ "enc scalar", rANS32x16_16w_encode_scalar_15, true }, {}}, {{ "dec scalar", rANS32x16_16w_decode_scalar_15 }, { "dec avx2 (xmm shfl, sym dep gthr)", rANS32x16_xmmShfl_16w_decode_avx2_varA_15, true }, { "dec avx2 (xmm shfl, sym idp gthr)", rANS32x16_xmmShfl_16w_decode_avx2_varB_15, true }, { "dec avx2 (ymm perm, sym dep gthr)", rANS32x16_ymmPerm_16w_decode_avx2_varA_15 }, { "dec avx2 (ymm perm, sym idp gthr)", rANS32x16_ymmPerm_16w_decode_avx2_varB_15 }, {}}},
  { "rANS32x16 16w (raw)", 14, {{ "enc scalar", rANS32x16_16w_encode_scalar_14, true }, {}}, {{ "dec scalar", rANS32x16_16w_decode_scalar_14 }, { "dec avx2 (xmm shfl, sym dep gthr)", rANS32x16_xmmShfl_16w_decode_avx2_varA_14, true }, { "dec avx2 (xmm shfl, sym idp gthr)", rANS32x16_xmmShfl_16w_decode_avx2_varB_14, true }, { "dec avx2 (ymm perm, sym dep gthr)", rANS32x16_ymmPerm_16w_decode_avx2_varA_14 }, { "dec avx2 (ymm perm, sym idp gthr)", rANS32x16_ymmPerm_16w_decode_avx2_varB_14 }, {}}},
  { "rANS32x16 16w (raw)", 13, {{ "enc scalar", rANS32x16_16w_encode_scalar_13, true }, {}}, {{ "dec scalar", rANS32x16_16w_decode_scalar_13 }, { "dec avx2 (xmm shfl, sym dep gthr)", rANS32x16_xmmShfl_16w_decode_avx2_varA_13, true }, { "dec avx2 (xmm shfl, sym idp gthr)", rANS32x16_xmmShfl_16w_decode_avx2_varB_13, true }, { "dec avx2 (ymm perm, sym dep gthr)", rANS32x16_ymmPerm_16w_decode_avx2_varA_13 }, { "dec avx2 (ymm perm, sym idp gthr)", rANS32x16_ymmPerm_16w_decode_avx2_varB_13 }, {}}},
  { "rANS32x16 16w (raw)", 12, {{ "enc scalar", rANS32x16_16w_encode_scalar_12, true }, {}}, {{ "dec scalar", rANS32x16_16w_decode_scalar_12 }, { "dec avx2 (xmm shfl, sym dep gthr)", rANS32x16_xmmShfl_16w_decode_avx2_varA_12 }, { "dec avx2 (xmm shfl, sym idp gthr)", rANS32x16_xmmShfl_16w_decode_avx2_varB_12 }, { "dec avx2 (xmm shfl, 1x gthr)", rANS32x16_xmmShfl_16w_decode_avx2_varC_12, true }, { "dec avx2 (ymm perm, sym dep gthr)", rANS32x16_ymmPerm_16w_decode_avx2_varA_12 }, { "dec avx2 (ymm perm, sym idp gthr)", rANS32x16_ymmPerm_16w_decode_avx2_varB_12 }, { "dec avx2 (ymm perm, 1x gthr)", rANS32x16_ymmPerm_16w_decode_avx2_varC_12 }, {}}},
  { "rANS32x16 16w (raw)", 11, {{ "enc scalar", rANS32x16_16w_encode_scalar_11, true }, {}}, {{ "dec scalar", rANS32x16_16w_decode_scalar_11 }, { "dec avx2 (xmm shfl, sym dep gthr)", rANS32x16_xmmShfl_16w_decode_avx2_varA_11 }, { "dec avx2 (xmm shfl, sym idp gthr)", rANS32x16_xmmShfl_16w_decode_avx2_varB_11 }, { "dec avx2 (xmm shfl, 1x gthr)", rANS32x16_xmmShfl_16w_decode_avx2_varC_11, true }, { "dec avx2 (ymm perm, sym dep gthr)", rANS32x16_ymmPerm_16w_decode_avx2_varA_11 }, { "dec avx2 (ymm perm, sym idp gthr)", rANS32x16_ymmPerm_16w_decode_avx2_varB_11 }, { "dec avx2 (ymm perm, 1x gthr)", rANS32x16_ymmPerm_16w_decode_avx2_varC_11 }, {}}},
  { "rANS32x16 16w (raw)", 10, {{ "enc scalar", rANS32x16_16w_encode_scalar_10, true }, {}}, {{ "dec scalar", rANS32x16_16w_decode_scalar_10 }, { "dec avx2 (xmm shfl, sym dep gthr)", rANS32x16_xmmShfl_16w_decode_avx2_varA_10 }, { "dec avx2 (xmm shfl, sym idp gthr)", rANS32x16_xmmShfl_16w_decode_avx2_varB_10 }, { "dec avx2 (xmm shfl, 1x gthr)", rANS32x16_xmmShfl_16w_decode_avx2_varC_10, true }, { "dec avx2 (ymm perm, sym dep gthr)", rANS32x16_ymmPerm_16w_decode_avx2_varA_10 }, { "dec avx2 (ymm perm, sym idp gthr)", rANS32x16_ymmPerm_16w_decode_avx2_varB_10 }, { "dec avx2 (ymm perm, 1x gthr)", rANS32x16_ymmPerm_16w_decode_avx2_varC_10 }, {}}},

  { "rANS32x32 32blk 16w (raw)", 15, {{ "enc scalar", rANS32x32_32blk_16w_encode_scalar_15, true }, {}}, {{ "dec scalar", rANS32x32_32blk_16w_decode_scalar_15 }, { "dec avx2 (sym dep gthr)", rANS32x32_32blk_16w_decode_avx2_varA_15 }, { "dec avx2 (sym dep gthr 2x)", rANS32x32_32blk_16w_decode_avx2_varA2_15, true }, { "dec avx2 (sym idp gthr)", rANS32x32_32blk_16w_decode_avx2_varB_15 }, { "dec avx2 (sym idp gthr 2x)", rANS32x32_32blk_16w_decode_avx2_varB2_15, true }, {}}},
  { "rANS32x32 32blk 16w (raw)", 14, {{ "enc scalar", rANS32x32_32blk_16w_encode_scalar_14, true }, {}}, {{ "dec scalar", rANS32x32_32blk_16w_decode_scalar_14 }, { "dec avx2 (sym dep gthr)", rANS32x32_32blk_16w_decode_avx2_varA_14 }, { "dec avx2 (sym dep gthr 2x)", rANS32x32_32blk_16w_decode_avx2_varA2_14, true }, { "dec avx2 (sym idp gthr)", rANS32x32_32blk_16w_decode_avx2_varB_14 }, { "dec avx2 (sym idp gthr 2x)", rANS32x32_32blk_16w_decode_avx2_varB2_14, true }, {}}},
  { "rANS32x32 32blk 16w (raw)", 13, {{ "enc scalar", rANS32x32_32blk_16w_encode_scalar_13, true }, {}}, {{ "dec scalar", rANS32x32_32blk_16w_decode_scalar_13 }, { "dec avx2 (sym dep gthr)", rANS32x32_32blk_16w_decode_avx2_varA_13 }, { "dec avx2 (sym dep gthr 2x)", rANS32x32_32blk_16w_decode_avx2_varA2_13, true }, { "dec avx2 (sym idp gthr)", rANS32x32_32blk_16w_decode_avx2_varB_13 }, { "dec avx2 (sym idp gthr 2x)", rANS32x32_32blk_16w_decode_avx2_varB2_13, true }, {}}},
  { "rANS32x32 32blk 16w (raw)", 12, {{ "enc scalar", rANS32x32_32blk_16w_encode_scalar_12, true }, {}}, {{ "dec scalar", rANS32x32_32blk_16w_decode_scalar_12 }, { "dec avx2 (sym dep gthr)", rANS32x32_32blk_16w_decode_avx2_varA_12 }, { "dec avx2 (sym dep gthr 2x)", rANS32x32_32blk_16w_decode_avx2_varA2_12, }, { "dec avx2 (sym idp gthr)", rANS32x32_32blk_16w_decode_avx2_varB_12 }, { "dec avx2 (sym idp gthr 2x)", rANS32x32_32blk_16w_decode_avx2_varB2_12 }, { "dec avx2 (1x gthr)", rANS32x32_32blk_16w_decode_avx2_varC_12 }, { "dec avx2 (1x gthr 2x)", rANS32x32_32blk_16w_decode_avx2_varC2_12, true }, {}}},
  { "rANS32x32 32blk 16w (raw)", 11, {{ "enc scalar", rANS32x32_32blk_16w_encode_scalar_11, true }, {}}, {{ "dec scalar", rANS32x32_32blk_16w_decode_scalar_11 }, { "dec avx2 (sym dep gthr)", rANS32x32_32blk_16w_decode_avx2_varA_11 }, { "dec avx2 (sym dep gthr 2x)", rANS32x32_32blk_16w_decode_avx2_varA2_11, }, { "dec avx2 (sym idp gthr)", rANS32x32_32blk_16w_decode_avx2_varB_11 }, { "dec avx2 (sym idp gthr 2x)", rANS32x32_32blk_16w_decode_avx2_varB2_11 }, { "dec avx2 (1x gthr)", rANS32x32_32blk_16w_decode_avx2_varC_11 }, { "dec avx2 (1x gthr 2x)", rANS32x32_32blk_16w_decode_avx2_varC2_11, true }, {}}},
  { "rANS32x32 32blk 16w (raw)", 10, {{ "enc scalar", rANS32x32_32blk_16w_encode_scalar_10, true }, {}}, {{ "dec scalar", rANS32x32_32blk_16w_decode_scalar_10 }, { "dec avx2 (sym dep gthr)", rANS32x32_32blk_16w_decode_avx2_varA_10 }, { "dec avx2 (sym dep gthr 2x)", rANS32x32_32blk_16w_decode_avx2_varA2_10, }, { "dec avx2 (sym idp gthr)", rANS32x32_32blk_16w_decode_avx2_varB_10 }, { "dec avx2 (sym idp gthr 2x)", rANS32x32_32blk_16w_decode_avx2_varB2_10 }, { "dec avx2 (1x gthr)", rANS32x32_32blk_16w_decode_avx2_varC_10 }, { "dec avx2 (1x gthr 2x)", rANS32x32_32blk_16w_decode_avx2_varC2_10, true }, {}}},

  { "rANS32x32 32blk 8w (raw)", 15, {{ "enc scalar", rANS32x32_32blk_8w_encode_scalar_15, true }, {}}, {{ "dec scalar", rANS32x32_32blk_8w_decode_scalar_15 }, { "dec avx2 (sym dep gthr)", rANS32x32_32blk_8w_decode_avx2_varA_15 }, { "dec avx2 (sym dep gthr 2x)", rANS32x32_32blk_8w_decode_avx2_varA2_15, true }, { "dec avx2 (sym idp gthr)", rANS32x32_32blk_8w_decode_avx2_varB_15 }, { "dec avx2 (sym idp gthr 2x)", rANS32x32_32blk_8w_decode_avx2_varB2_15, true }, {}}},
  { "rANS32x32 32blk 8w (raw)", 14, {{ "enc scalar", rANS32x32_32blk_8w_encode_scalar_14, true }, {}}, {{ "dec scalar", rANS32x32_32blk_8w_decode_scalar_14 }, { "dec avx2 (sym dep gthr)", rANS32x32_32blk_8w_decode_avx2_varA_14 }, { "dec avx2 (sym dep gthr 2x)", rANS32x32_32blk_8w_decode_avx2_varA2_14, true }, { "dec avx2 (sym idp gthr)", rANS32x32_32blk_8w_decode_avx2_varB_14 }, { "dec avx2 (sym idp gthr 2x)", rANS32x32_32blk_8w_decode_avx2_varB2_14, true }, {}}},
  { "rANS32x32 32blk 8w (raw)", 13, {{ "enc scalar", rANS32x32_32blk_8w_encode_scalar_13, true }, {}}, {{ "dec scalar", rANS32x32_32blk_8w_decode_scalar_13 }, { "dec avx2 (sym dep gthr)", rANS32x32_32blk_8w_decode_avx2_varA_13 }, { "dec avx2 (sym dep gthr 2x)", rANS32x32_32blk_8w_decode_avx2_varA2_13, true }, { "dec avx2 (sym idp gthr)", rANS32x32_32blk_8w_decode_avx2_varB_13 }, { "dec avx2 (sym idp gthr 2x)", rANS32x32_32blk_8w_decode_avx2_varB2_13, true }, {}}},
  { "rANS32x32 32blk 8w (raw)", 12, {{ "enc scalar", rANS32x32_32blk_8w_encode_scalar_12, true }, {}}, {{ "dec scalar", rANS32x32_32blk_8w_decode_scalar_12 }, { "dec avx2 (sym dep gthr)", rANS32x32_32blk_8w_decode_avx2_varA_12 }, { "dec avx2 (sym dep gthr 2x)", rANS32x32_32blk_8w_decode_avx2_varA2_12 }, { "dec avx2 (sym idp gthr)", rANS32x32_32blk_8w_decode_avx2_varB_12 }, { "dec avx2 (sym idp gthr 2x)", rANS32x32_32blk_8w_decode_avx2_varB2_12 }, { "dec avx2 (1x gthr)", rANS32x32_32blk_8w_decode_avx2_varC_12 }, { "dec avx2 (1x gthr 2x)", rANS32x32_32blk_8w_decode_avx2_varC2_12, true }, {}}},
  { "rANS32x32 32blk 8w (raw)", 11, {{ "enc scalar", rANS32x32_32blk_8w_encode_scalar_11, true }, {}}, {{ "dec scalar", rANS32x32_32blk_8w_decode_scalar_11 }, { "dec avx2 (sym dep gthr)", rANS32x32_32blk_8w_decode_avx2_varA_11 }, { "dec avx2 (sym dep gthr 2x)", rANS32x32_32blk_8w_decode_avx2_varA2_11 }, { "dec avx2 (sym idp gthr)", rANS32x32_32blk_8w_decode_avx2_varB_11 }, { "dec avx2 (sym idp gthr 2x)", rANS32x32_32blk_8w_decode_avx2_varB2_11 }, { "dec avx2 (1x gthr)", rANS32x32_32blk_8w_decode_avx2_varC_11 }, { "dec avx2 (1x gthr 2x)", rANS32x32_32blk_8w_decode_avx2_varC2_11, true }, {}}},
  { "rANS32x32 32blk 8w (raw)", 10, {{ "enc scalar", rANS32x32_32blk_8w_encode_scalar_10, true }, {}}, {{ "dec scalar", rANS32x32_32blk_8w_decode_scalar_10 }, { "dec avx2 (sym dep gthr)", rANS32x32_32blk_8w_decode_avx2_varA_10 }, { "dec avx2 (sym dep gthr 2x)", rANS32x32_32blk_8w_decode_avx2_varA2_10 }, { "dec avx2 (sym idp gthr)", rANS32x32_32blk_8w_decode_avx2_varB_10 }, { "dec avx2 (sym idp gthr 2x)", rANS32x32_32blk_8w_decode_avx2_varB2_10 }, { "dec avx2 (1x gthr)", rANS32x32_32blk_8w_decode_avx2_varC_10 }, { "dec avx2 (1x gthr 2x)", rANS32x32_32blk_8w_decode_avx2_varC2_10, true }, {}}},
};

//////////////////////////////////////////////////////////////////////////

const char ArgumentAllVariants[] = "--all";
const char ArgumentHistMin[] = "--hist-min";
const char ArgumentHistMax[] = "--hist-max";
const char ArgumentInclude32Blk[] = "--include-32blk";
const char ArgumentIncludeRaw[] = "--include-raw";
const char ArgumentIncludeMT[] = "--include-mt";
const char ArgumentExcludeBlock[] = "--exclude-block";
const char ArgumentExclude16[] = "--exclude-16";
const char ArgumentExclude32[] = "--exclude-32";
const char ArgumentExclude64[] = "--exclude-64";
const char ArgumentNoSleep[] = "--no-sleep";
const char ArgumentCpuCore[] = "--cpu-core";
const char ArgumentRuns[] = "--runs";
const char ArgumentRunsEncode[] = "--runs-enc";
const char ArgumentRunsDecode[] = "--runs-dec";
const char ArgumentTest[] = "--test";
const char ArgumentMaxSimd[] = "--max-simd";
const char ArgumentMaxSimdAVX512BW[] = "avx512bw";
const char ArgumentMaxSimdAVX512F[] = "avx512f";
const char ArgumentMaxSimdAVX2[] = "avx2";
const char ArgumentMaxSimdAVX[] = "avx";
const char ArgumentMaxSimdSSE42[] = "sse4.2";
const char ArgumentMaxSimdSSE41[] = "sse4.1";
const char ArgumentMaxSimdSSSE3[] = "ssse3";
const char ArgumentMaxSimdSSE3[] = "sse3";
const char ArgumentMaxSimdSSE2[] = "sse2";
const char ArgumentMaxSimdNone[] = "none";

//////////////////////////////////////////////////////////////////////////

int32_t main(const int32_t argc, char **pArgv)
{
  if (argc == 1)
  {
    puts("Invalid Parameter.\n\nUsage: hsrans <filename>");
    printf("\t%s \t\t\tRun all variants of the specified codecs, not just the ones that we'd expect to be fast\n", ArgumentAllVariants);
    printf("\t%s <10-15> \tRestrict codecs to a number of histogram bits\n", ArgumentHistMin);
    printf("\t%s <10-15> \tRestrict codecs to a number of histogram bits\n", ArgumentHistMax);
    printf("\t%s \t\tRun the (single-threaded) benchmark on a specific core\n", ArgumentCpuCore);
    printf("\t%s \t\tInclude multi-threading optimized variants\n", ArgumentIncludeMT);
    printf("\t%s \t\tInclude RAW variants with one only one histogram for the entire file\n", ArgumentIncludeRaw);
    printf("\t%s \tInclude 32 block variants (which are generally quite slow), requires '%s'\n", ArgumentInclude32Blk, ArgumentIncludeRaw);
    printf("\t%s \tExclude the main (variable block size) variants form the benchmark\n", ArgumentExcludeBlock);
    printf("\t%s \t\tExclude 16 state variants from the benchmark (only RAW)\n", ArgumentExclude16);
    printf("\t%s \t\tExclude 32 state variants from the benchmark\n", ArgumentExclude32);
    printf("\t%s \t\tExclude 64 state variants from the benchmark\n", ArgumentExclude64);
    printf("\t%s <uint>\t\tRun the benchmark for a specified amount of times (default: 2 encode, 16 decode; will override '%s'/'%s')\n", ArgumentRuns, ArgumentRunsEncode, ArgumentRunsDecode);
    printf("\t%s <uint>\tWhen Encoding: Run the benchmark for a specified amount of times (default: 2)\n", ArgumentRunsEncode);
    printf("\t%s <uint>\tWhen Decoding: Run the benchmark for a specified amount of times (default: 16)\n", ArgumentRunsDecode);
    printf("\t%s\t\tPrevent sleeping between runs/codecs (may lead to thermal throttling)\n", ArgumentNoSleep);
    printf("\t%s\t\t\tRun as test scenario, fail on error, call codecs\n", ArgumentTest);
    printf("\t%s <%s / %s / %s / %s / %s / %s / %s / %s / %s / %s>\n\t\t\t\tRestrict SIMD functions to specific instruction set\n", ArgumentMaxSimd, ArgumentMaxSimdAVX512BW, ArgumentMaxSimdAVX512F, ArgumentMaxSimdAVX2, ArgumentMaxSimdAVX, ArgumentMaxSimdSSE42, ArgumentMaxSimdSSE41, ArgumentMaxSimdSSSE3, ArgumentMaxSimdSSE3, ArgumentMaxSimdSSE2, ArgumentMaxSimdNone);
    return 1;
  }

  const char *filename = pArgv[1];

  // Parse additional arguments.
  if (argc > 2)
  {
    size_t argIndex = 2;
    size_t argsRemaining = (size_t)argc - 2;

    while (argsRemaining)
    {
      if (argsRemaining >= 1 && strncmp(pArgv[argIndex], ArgumentAllVariants, sizeof(ArgumentAllVariants)) == 0)
      {
        argIndex++;
        argsRemaining--;
        _OnlyRelevantCodecs = false;
      }
      else if (argsRemaining >= 1 && strncmp(pArgv[argIndex], ArgumentIncludeMT, sizeof(ArgumentIncludeMT)) == 0)
      {
        argIndex++;
        argsRemaining--;
        _IncludeMT = true;
      }
      else if (argsRemaining >= 1 && strncmp(pArgv[argIndex], ArgumentIncludeRaw, sizeof(ArgumentIncludeRaw)) == 0)
      {
        argIndex++;
        argsRemaining--;
        _IncludeRaw = true;
      }
      else if (argsRemaining >= 1 && strncmp(pArgv[argIndex], ArgumentInclude32Blk, sizeof(ArgumentInclude32Blk)) == 0)
      {
        argIndex++;
        argsRemaining--;
        _Include32Block = true;
      }
      else if (argsRemaining >= 1 && strncmp(pArgv[argIndex], ArgumentExcludeBlock, sizeof(ArgumentExcludeBlock)) == 0)
      {
        argIndex++;
        argsRemaining--;
        _ExcludeBlock = true;
      }
      else if (argsRemaining >= 1 && strncmp(pArgv[argIndex], ArgumentExclude16, sizeof(ArgumentExclude16)) == 0)
      {
        argIndex++;
        argsRemaining--;
        _Exclude32x16 = true;
      }
      else if (argsRemaining >= 1 && strncmp(pArgv[argIndex], ArgumentExclude32, sizeof(ArgumentExclude32)) == 0)
      {
        argIndex++;
        argsRemaining--;
        _Exclude32x32 = true;
      }
      else if (argsRemaining >= 1 && strncmp(pArgv[argIndex], ArgumentExclude64, sizeof(ArgumentExclude64)) == 0)
      {
        argIndex++;
        argsRemaining--;
        _Exclude32x64 = true;
      }
      else if (argsRemaining >= 1 && strncmp(pArgv[argIndex], ArgumentNoSleep, sizeof(ArgumentNoSleep)) == 0)
      {
        argIndex++;
        argsRemaining--;
        _DisableSleep = true;
      }
      else if (argsRemaining >= 1 && strncmp(pArgv[argIndex], ArgumentTest, sizeof(ArgumentTest)) == 0)
      {
        argIndex++;
        argsRemaining--;
        _IsTest = true;
        _DisableSleep = true;
        _EncodeRunCount = 1;
        _DecodeRunCount = 1;
        _Include32Block = true;
        _IncludeRaw = true;
        _IncludeMT = true;
        _OnlyRelevantCodecs = false;
      }
      else if (argsRemaining >= 2 && strncmp(pArgv[argIndex], ArgumentRuns, sizeof(ArgumentRuns)) == 0)
      {
        _RunCount = strtoull(pArgv[argIndex + 1], nullptr, 10);

        if (_RunCount > MaxRunCount)
        {
          puts("Invalid Parameter.");
          return 1;
        }
        else if (_RunCount == 0)
        {
          _RunCount = 1;
          _DisableSleep = true;
        }

        _EncodeRunCount = _DecodeRunCount = _RunCount;

        argIndex += 2;
        argsRemaining -= 2;
      }
      else if (argsRemaining >= 2 && strncmp(pArgv[argIndex], ArgumentRunsEncode, sizeof(ArgumentRunsEncode)) == 0)
      {
        _EncodeRunCount = strtoull(pArgv[argIndex + 1], nullptr, 10);

        if (_EncodeRunCount > MaxRunCount)
        {
          puts("Invalid Parameter.");
          return 1;
        }
        else if (_EncodeRunCount == 0)
        {
          _EncodeRunCount = 1;
          _DisableSleep = true;
        }

        argIndex += 2;
        argsRemaining -= 2;
      }
      else if (argsRemaining >= 2 && strncmp(pArgv[argIndex], ArgumentRunsDecode, sizeof(ArgumentRunsDecode)) == 0)
      {
        _DecodeRunCount = strtoull(pArgv[argIndex + 1], nullptr, 10);

        if (_DecodeRunCount > MaxRunCount)
        {
          puts("Invalid Parameter.");
          return 1;
        }
        else if (_DecodeRunCount == 0)
        {
          _DecodeRunCount = 1;
          _DisableSleep = true;
        }

        argIndex += 2;
        argsRemaining -= 2;
      }
      else if (argsRemaining >= 2 && strncmp(pArgv[argIndex], ArgumentHistMax, sizeof(ArgumentHistMax)) == 0)
      {
        _HistMax = strtoull(pArgv[argIndex + 1], nullptr, 10);

        argIndex += 2;
        argsRemaining -= 2;
      }
      else if (argsRemaining >= 2 && strncmp(pArgv[argIndex], ArgumentHistMin, sizeof(ArgumentHistMin)) == 0)
      {
        _HistMin = strtoull(pArgv[argIndex + 1], nullptr, 10);

        argIndex += 2;
        argsRemaining -= 2;
      }
      else if (argsRemaining >= 2 && strncmp(pArgv[argIndex], ArgumentCpuCore, sizeof(ArgumentCpuCore)) == 0)
      {
        // For more consistent benchmarking results.
        const size_t cpuCoreIndex = strtoull(pArgv[argIndex + 1], nullptr, 10);

        argIndex += 2;
        argsRemaining -= 2;

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
      else if (argsRemaining >= 2 && strncmp(pArgv[argIndex], ArgumentMaxSimd, sizeof(ArgumentMaxSimd)) == 0)
      {
        _DetectCPUFeatures();

        do
        {
          if (strncmp(pArgv[argIndex + 1], ArgumentMaxSimdAVX512BW, sizeof(ArgumentMaxSimdAVX512BW)) == 0)
          {
            if (!avx512BWSupported)
            {
              puts("AVX512BW is not supported by this platform. Aborting.");
              return 1;
            }

            // In future versions with other simd flavours better than avx512 supported, disable them here.

            break;
          }

          avx512PFSupported = false;
          avx512ERSupported = false;
          avx512CDSupported = false;
          avx512BWSupported = false;
          avx512DQSupported = false;
          avx512VLSupported = false;
          avx512IFMASupported = false;
          avx512VBMISupported = false;
          avx512VNNISupported = false;
          avx512VBMI2Supported = false;
          avx512POPCNTDQSupported = false;
          avx512BITALGSupported = false;
          avx5124VNNIWSupported = false;
          avx5124FMAPSSupported = false;

          if (strncmp(pArgv[argIndex + 1], ArgumentMaxSimdAVX512F, sizeof(ArgumentMaxSimdAVX512F)) == 0)
          {
            if (!avx512FSupported)
            {
              puts("AVX512F is not supported by this platform. Aborting.");
              return 1;
            }

            // In future versions with other simd flavours better than avx512 supported, disable them here.

            break;
          }

          avx512FSupported = false;

          if (strncmp(pArgv[argIndex + 1], ArgumentMaxSimdAVX2, sizeof(ArgumentMaxSimdAVX2)) == 0)
          {
            if (!avx2Supported)
            {
              puts("AVX2 is not supported by this platform. Aborting.");
              return 1;
            }

            break;
          }

          avx2Supported = false;
          fma3Supported = false;

          if (strncmp(pArgv[argIndex + 1], ArgumentMaxSimdAVX, sizeof(ArgumentMaxSimdAVX)) == 0)
          {
            if (!avxSupported)
            {
              puts("AVX is not supported by this platform. Aborting.");
              return 1;
            }

            break;
          }

          avxSupported = false;

          if (strncmp(pArgv[argIndex + 1], ArgumentMaxSimdSSE42, sizeof(ArgumentMaxSimdSSE42)) == 0)
          {
            if (!sse42Supported)
            {
              puts("SSE4.2 is not supported by this platform. Aborting.");
              return 1;
            }

            break;
          }

          sse42Supported = false;

          if (strncmp(pArgv[argIndex + 1], ArgumentMaxSimdSSE41, sizeof(ArgumentMaxSimdSSE41)) == 0)
          {
            if (!sse41Supported)
            {
              puts("SSE4.1 is not supported by this platform. Aborting.");
              return 1;
            }

            break;
          }

          sse41Supported = false;

          if (strncmp(pArgv[argIndex + 1], ArgumentMaxSimdSSSE3, sizeof(ArgumentMaxSimdSSSE3)) == 0)
          {
            if (!ssse3Supported)
            {
              puts("SSSE3 is not supported by this platform. Aborting.");
              return 1;
            }

            break;
          }

          ssse3Supported = false;

          if (strncmp(pArgv[argIndex + 1], ArgumentMaxSimdSSE3, sizeof(ArgumentMaxSimdSSE3)) == 0)
          {
            if (!sse3Supported)
            {
              puts("SSE3 is not supported by this platform. Aborting.");
              return 1;
            }

            break;
          }

          sse3Supported = false;

          if (strncmp(pArgv[argIndex + 1], ArgumentMaxSimdSSE2, sizeof(ArgumentMaxSimdSSE2)) == 0)
          {
            if (!sse2Supported)
            {
              puts("SSE2 is not supported by this platform. Aborting.");
              return 1;
            }

            break;
          }

          sse2Supported = false;

          if (strncmp(pArgv[argIndex + 1], ArgumentMaxSimdNone, sizeof(ArgumentMaxSimdNone)) == 0)
          {
            printf("%s %s is only intended for testing purposes and will only restrict some codecs to no SIMD\n", ArgumentMaxSimd, ArgumentMaxSimdNone);

            break;
          }

          printf("Invalid SIMD Variant '%s' specified.", pArgv[argIndex + 1]);
          return 1;

        } while (false);

        argIndex += 2;
        argsRemaining -= 2;
      }
      else
      {
        printf("Invalid Parameter '%s'. Aborting.", pArgv[argIndex]);
        return 1;
      }
    }
  }

  // Read File.
  {
    FILE *pFile = fopen(filename, "rb");

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

    pUncompressedData = (uint8_t *)ALIGNED_ALLOC(64, fileSize);
    pDecompressedData = (uint8_t *)ALIGNED_ALLOC(64, fileSize);

    compressedDataCapacity = 0;
    compressedDataCapacity = rans_max(compressedDataCapacity, rANS32x64_16w_capacity(fileSize));
    compressedDataCapacity = rans_max(compressedDataCapacity, rANS32x32_16w_capacity(fileSize));
    compressedDataCapacity = rans_max(compressedDataCapacity, rANS32x16_16w_capacity(fileSize));
    compressedDataCapacity = rans_max(compressedDataCapacity, rANS32x32_32blk_16w_capacity(fileSize));
    compressedDataCapacity = rans_max(compressedDataCapacity, rANS32x32_32blk_8w_capacity(fileSize));
    compressedDataCapacity = rans_max(compressedDataCapacity, block_rANS32x32_16w_capacity(fileSize));
    compressedDataCapacity = rans_max(compressedDataCapacity, block_rANS32x64_16w_capacity(fileSize));
    compressedDataCapacity = rans_max(compressedDataCapacity, mt_rANS32x32_16w_capacity(fileSize));
    compressedDataCapacity = rans_max(compressedDataCapacity, mt_rANS32x64_16w_capacity(fileSize));

    pCompressedData = (uint8_t *)ALIGNED_ALLOC(64, compressedDataCapacity);

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

  _DetectCPUFeatures();

  // Print info. 
  {
    printf("File: '%s' (%" PRIu64 " Bytes)\n", filename, fileSize);

    printf("CPU: '%s' [%s] (Family: 0x%" PRIX8 " / Model: 0x%" PRIX8 " (0x%" PRIX8 ") / Stepping: 0x%" PRIX8 ")\nFeatures:", _CpuName, _GetCPUArchitectureName(), _CpuFamily, _CpuModel, _CpuExtModel, _CpuStepping);

    bool anysse = false;
    bool anyavx512 = false;

#define PRINT_WITH_CPUFLAG(flag, name, b, desc) if (flag) { if (!b) { fputs(desc, stdout); b = true; } else { fputs("/", stdout); } fputs(name, stdout); }
#define PRINT_SSE(flag, name) PRINT_WITH_CPUFLAG(flag, name, anysse, " SSE");
#define PRINT_AVX512(flag, name) PRINT_WITH_CPUFLAG(flag, name, anyavx512, " AVX-512");

    PRINT_SSE(sse2Supported, "2");
    PRINT_SSE(sse3Supported, "3");
    PRINT_SSE(sse41Supported, "4.1");
    PRINT_SSE(sse42Supported, "4.2");

    if (ssse3Supported)
      fputs(" SSSE3", stdout);

    if (avx2Supported)
      fputs(" AVX", stdout);

    if (avx2Supported)
      fputs(" AVX2", stdout);

    PRINT_AVX512(avx512FSupported, "F");
    PRINT_AVX512(avx512PFSupported, "PF");
    PRINT_AVX512(avx512ERSupported, "ER");
    PRINT_AVX512(avx512CDSupported, "CD");
    PRINT_AVX512(avx512BWSupported, "BW");
    PRINT_AVX512(avx512DQSupported, "DQ");
    PRINT_AVX512(avx512VLSupported, "VL");
    PRINT_AVX512(avx512IFMASupported, "IFMA");
    PRINT_AVX512(avx512VBMISupported, "VBMI");
    PRINT_AVX512(avx512VNNISupported, "VNNI");
    PRINT_AVX512(avx512VBMI2Supported, "VBMI2");
    PRINT_AVX512(avx512POPCNTDQSupported, "POPCNTDQ");
    PRINT_AVX512(avx512BITALGSupported, "BITALG");
    PRINT_AVX512(avx5124VNNIWSupported, "4VNNIW");
    PRINT_AVX512(avx5124FMAPSSupported, "4FMAPS");

    if (fma3Supported)
      fputs(" FMA3", stdout);

    if (aesNiSupported)
      fputs(" AES-NI", stdout);

#undef PRINT_WITH_CPUFLAG
#undef PRINT_SSE
#undef PRINT_AVX512

    puts("\n");
  }

  puts("Codec Type (Enc/Dec Impl)            Hist  Ratio      Minimum            Average          ( StdDev.         )   Maximum          Average        ( StdDev.           )");
  puts("~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~");

  for (size_t codecId = 0; codecId < sizeof(_Codecs) / sizeof(_Codecs[0]); codecId++)
  {
    static hist_t hist;
    make_hist(&hist, pUncompressedData, fileSize, _Codecs[codecId].totalSymbolCountBits);
    bool skipCodec = false;

    skipCodec |= (!_IncludeMT && strstr(_Codecs[codecId].name, " (independent blocks)") != nullptr);
    skipCodec |= (!_IncludeRaw && strstr(_Codecs[codecId].name, " (raw)") != nullptr);
    skipCodec |= (!_Include32Block && strstr(_Codecs[codecId].name, " 32blk ") != nullptr);
    skipCodec |= (_ExcludeBlock && strstr(_Codecs[codecId].name, " (variable block size)") != nullptr);
    skipCodec |= (_Exclude32x16 && strstr(_Codecs[codecId].name, "32x16") != nullptr);
    skipCodec |= (_Exclude32x32 && strstr(_Codecs[codecId].name, "32x32") != nullptr);
    skipCodec |= (_Exclude32x64 && strstr(_Codecs[codecId].name, "32x64") != nullptr);
    skipCodec |= _Codecs[codecId].totalSymbolCountBits > _HistMax;
    skipCodec |= _Codecs[codecId].totalSymbolCountBits < _HistMin;

    if (skipCodec)
      continue;

    printf("%-37s %2" PRIu32 " | -------- | ---------------- | ------------------------------------ | -------------- | ------------------------------------\n", _Codecs[codecId].name, _Codecs[codecId].totalSymbolCountBits);

    size_t encodedSize = 0;
    _RunCount = _EncodeRunCount;

    for (size_t codecFuncIndex = 0; codecFuncIndex < MaxEncoderCount; codecFuncIndex++)
    {
      if (_Codecs[codecId].encoders[codecFuncIndex].name == nullptr)
        break;

      if (_OnlyRelevantCodecs && !_Codecs[codecId].encoders[codecFuncIndex].candidateForFastest)
          continue;

      if (strstr(_Codecs[codecId].encoders[codecFuncIndex].name, " avx2 ") != nullptr && !avx2Supported)
      {
        printf("  %-38s |          | (Skipped; No AVX2 available)\n", _Codecs[codecId].encoders[codecFuncIndex].name);
        continue;
      }
      else if (strstr(_Codecs[codecId].encoders[codecFuncIndex].name, " avx512 ") != nullptr && (!avx512FSupported || !avx512DQSupported || !avx512BWSupported))
      {
        printf("  %-38s |          | (Skipped, No AVX-512 F/DQ/BW available)\n", _Codecs[codecId].encoders[codecFuncIndex].name);
        continue;
      }

      memset(pCompressedData, 0xCC, compressedDataCapacity);

      if (_RunCount > 1)
      {
        printf("\r  (dry run)");
        encodedSize = _Codecs[codecId].encoders[codecFuncIndex].func(pUncompressedData, fileSize, pCompressedData, compressedDataCapacity, &hist);
      }

      SleepNs(2500ULL * 1000 * 1000);

      for (size_t run = 0; run < _RunCount; run++)
      {
        const uint64_t startTick = GetCurrentTimeTicks();
        const uint64_t startClock = __rdtsc();
        encodedSize = _Codecs[codecId].encoders[codecFuncIndex].func(pUncompressedData, fileSize, pCompressedData, compressedDataCapacity, &hist);
        const uint64_t endClock = __rdtsc();
        const uint64_t endTick = GetCurrentTimeTicks();

        _mm_mfence();

        _NsPerRun[run] = TicksToNs(endTick - startTick);
        _ClocksPerRun[run] = endClock - startClock;

        printf("\r  %-38s | %6.2f %% | compressed to %" PRIu64 " bytes (%6.3f clocks/byte, %5.2f MiB/s)", _Codecs[codecId].encoders[codecFuncIndex].name, encodedSize / (double)fileSize * 100.0, encodedSize, (endClock - startClock) / (double)fileSize, (fileSize / (1024.0 * 1024.0)) / (TicksToNs(endTick - startTick) * 1e-9));

        SleepNs(rans_min(_NsPerRun[run] * 2ULL, 500ULL * 1000 * 1000));
      }

      printf("\r  %-38s | %6.2f %% ", _Codecs[codecId].encoders[codecFuncIndex].name, encodedSize / (double)fileSize * 100.0);
      print_perf_info(fileSize);

      if (_Codecs[codecId].decoders[0].func != nullptr)
      {
        const size_t decodedSize = _Codecs[codecId].decoders[0].func(pCompressedData, encodedSize, pDecompressedData, fileSize);

        if (decodedSize != fileSize || !Validate(pDecompressedData, pUncompressedData, fileSize))
        {
          puts("Failed to validate.");

          if (_IsTest)
            return 1;
        }
      }
      else
      {
        puts("Unable to validate, no decoder available.");

        if (_IsTest)
          return 2;
      }
    }

    size_t decodedSize = 0;
    _RunCount = _DecodeRunCount;

    for (size_t codecFuncIndex = 0; codecFuncIndex < MaxDecoderCount; codecFuncIndex++)
    {
      if (_Codecs[codecId].decoders[codecFuncIndex].name == nullptr)
        break;

      if (_OnlyRelevantCodecs && !_Codecs[codecId].decoders[codecFuncIndex].candidateForFastest)
        continue;

      if (strstr(_Codecs[codecId].decoders[codecFuncIndex].name, " avx2 ") != nullptr && !avx2Supported)
      {
        printf("  %-38s |          | (Skipped; No AVX2 available)\n", _Codecs[codecId].decoders[codecFuncIndex].name);
        continue;
      }
      else if (strstr(_Codecs[codecId].decoders[codecFuncIndex].name, " avx512 ") != nullptr && (!avx512FSupported || !avx512DQSupported || !avx512BWSupported))
      {
        printf("  %-38s |          | (Skipped, No AVX-512 F/DQ/BW available)\n", _Codecs[codecId].decoders[codecFuncIndex].name);
        continue;
      }

      memset(pDecompressedData, 0xCC, fileSize);

      if (_RunCount > 1)
      {
        printf("\r(dry run)");
        decodedSize = _Codecs[codecId].decoders[codecFuncIndex].func(pCompressedData, encodedSize, pDecompressedData, fileSize);
      }

      SleepNs(2500ULL * 1000 * 1000);

      for (size_t run = 0; run < _RunCount; run++)
      {
        const uint64_t startTick = GetCurrentTimeTicks();
        const uint64_t startClock = __rdtsc();
        decodedSize = _Codecs[codecId].decoders[codecFuncIndex].func(pCompressedData, encodedSize, pDecompressedData, fileSize);
        const uint64_t endClock = __rdtsc();
        const uint64_t endTick = GetCurrentTimeTicks();

        _mm_mfence();

        _NsPerRun[run] = TicksToNs(endTick - startTick);
        _ClocksPerRun[run] = endClock - startClock;

        printf("\r  %-38s |          | decompressed to %" PRIu64 " bytes. (%6.3f clocks/byte, %5.2f MiB/s)", _Codecs[codecId].decoders[codecFuncIndex].name, decodedSize, (endClock - startClock) / (double)fileSize, (fileSize / (1024.0 * 1024.0)) / (TicksToNs(endTick - startTick) * 1e-9));

        SleepNs(rans_min(_NsPerRun[run] * 2ULL, 500ULL * 1000 * 1000));
      }

      printf("\r  %-38s |          ", _Codecs[codecId].decoders[codecFuncIndex].name);
      print_perf_info(fileSize);

      if (decodedSize != fileSize || !Validate(pDecompressedData, pUncompressedData, fileSize))
      {
        puts("\nFailed to validate.");

        if (_IsTest)
          return 1;
      }
    }
  }

  ALIGNED_FREE(pUncompressedData);
  ALIGNED_FREE(pCompressedData);
  ALIGNED_FREE(pDecompressedData);

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
  if (!_DisableSleep && _RunCount > 1)
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
