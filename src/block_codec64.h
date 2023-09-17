#ifndef block_codec64_h__
#define block_codec64_h__

#include "hist.h"
#include "simd_platform.h"

#include <string.h>

constexpr size_t StateCount = 64; // Needs to be a power of two.

//////////////////////////////////////////////////////////////////////////

extern const uint8_t _ShuffleLutShfl32[256 * 8];
extern const uint8_t _ShuffleLutPerm32[256 * 8];
extern const uint8_t _DoubleShuffleLutShfl32[256 * 8 * 2];

constexpr bool EncodeNoBranch = false;
constexpr bool DecodeNoBranch = false;

//////////////////////////////////////////////////////////////////////////

static const uint8_t _Rans32x64_idx2idx[] =
{
  0x00, 0x01, 0x02, 0x03, 0x10, 0x11, 0x12, 0x13, 0x04, 0x05, 0x06, 0x07, 0x14, 0x15, 0x16, 0x17,
  0x08, 0x09, 0x0A, 0x0B, 0x18, 0x19, 0x1A, 0x1B, 0x0C, 0x0D, 0x0E, 0x0F, 0x1C, 0x1D, 0x1E, 0x1F,
  0x20, 0x21, 0x22, 0x23, 0x30, 0x31, 0x32, 0x33, 0x24, 0x25, 0x26, 0x27, 0x34, 0x35, 0x36, 0x37,
  0x28, 0x29, 0x2A, 0x2B, 0x38, 0x39, 0x3A, 0x3B, 0x2C, 0x2D, 0x2E, 0x2F, 0x3C, 0x3D, 0x3E, 0x3F,
};

static_assert(sizeof(_Rans32x64_idx2idx) == StateCount);

struct _rans_encode_state64_t
{
  uint32_t states[StateCount];
  hist_t hist;
  uint16_t *pEnd, *pStart; // both compressed.
};

//////////////////////////////////////////////////////////////////////////

enum rans32x64_encoder_type_t
{
  r32x64_et_scalar,
};

template <rans32x64_encoder_type_t type>
struct rans32x64_16w_encoder
{
  template <uint32_t TotalSymbolCountBits>
  static void encode_section(_rans_encode_state64_t *pState, const uint8_t *pInData, const size_t startIndex, const size_t targetIndex);
};

//////////////////////////////////////////////////////////////////////////

template <>
struct rans32x64_16w_encoder<r32x64_et_scalar>
{
  template <uint32_t TotalSymbolCountBits>
  static void encode_section(_rans_encode_state64_t *pState, const uint8_t *pInData, const size_t startIndex, const size_t targetIndex)
  {
    int64_t targetCmp = targetIndex + StateCount;

    constexpr size_t EncodeEmitPoint = ((DecodeConsumePoint16 >> TotalSymbolCountBits) << 16);

    for (int64_t i = startIndex; i >= (int64_t)targetCmp; i -= StateCount)
    {
      for (int64_t j = StateCount - 1; j >= 0; j--)
      {
        const uint8_t index = _Rans32x64_idx2idx[j];

        const uint8_t in = pInData[i - StateCount + index];
        const uint32_t symbolCount = pState->hist.symbolCount[in];
        const uint32_t max = EncodeEmitPoint * symbolCount;

        const size_t stateIndex = j;

        uint32_t state = pState->states[stateIndex];

        if constexpr (EncodeNoBranch)
        {
          const bool write = state >= max;
          *pState->pStart = (uint16_t)(state & 0xFFFF);
          *pState->pStart -= (size_t)write;
          state = write ? state >> 16 : state;
        }
        else
        {
          if (state >= max)
          {
            *pState->pStart = (uint16_t)(state & 0xFFFF);
            pState->pStart--;
            state >>= 16;
          }
        }

        pState->states[stateIndex] = ((state / symbolCount) << TotalSymbolCountBits) + (uint32_t)pState->hist.cumul[in] + (state % symbolCount);
      }
    }
  }
};

//////////////////////////////////////////////////////////////////////////

template <typename hist_type>
struct _rans_decode_state64_t
{
#ifdef _MSC_VER
  __declspec(align(64))
#else
  __attribute__((aligned(64)))
#endif
    uint32_t states[StateCount];

  hist_type hist;
  const uint16_t *pReadHead;
};

enum rans32x64_decoder_type_t
{
  r32x64_dt_scalar,
  r32x64_dt_avx2_large_cache_15_to_13,
  r32x64_dt_avx2_small_cache_15_to_13,
  r32x64_dt_avx2_large_cache_12_to_10,
  r32x64_dt_avx2_small_cache_12_to_10,
  r32x64_dt_avx512_large_cache_15_to_13,
  r32x64_dt_avx512_small_cache_15_to_13,
  r32x64_dt_avx512_large_cache_12_to_10,
  r32x64_dt_avx512_small_cache_12_to_10,
  r32x64_dt_zen4_12_to_10,
};

template <rans32x64_decoder_type_t type, uint32_t TotalSymbolCountBits, typename hist_type>
struct rans32x64_16w_decoder
{
  static size_t decode_section(_rans_decode_state64_t<hist_type> *pState, uint8_t *pOutData, const size_t startIndex, const size_t endIndex);
};

//////////////////////////////////////////////////////////////////////////

template <uint32_t TotalSymbolCountBits>
static bool _init_from_hist(hist_dec_t<TotalSymbolCountBits> *pDecHist, hist_t *pIncompleteHist, const uint32_t totalSymbolCountBits)
{
  (void)totalSymbolCountBits;

  memcpy(&pDecHist->symbolCount, &pIncompleteHist->symbolCount, sizeof(pIncompleteHist->symbolCount));

  return inplace_make_hist_dec(pDecHist);
}

template <uint32_t TotalSymbolCountBits>
static bool _init_from_hist(hist_dec2_t<TotalSymbolCountBits> *pDecHist, hist_t *pIncompleteHist, const uint32_t totalSymbolCountBits)
{
  if (!inplace_complete_hist(pIncompleteHist, totalSymbolCountBits))
    return false;

  make_dec2_hist(pDecHist, pIncompleteHist);

  return true;
}

template <uint32_t TotalSymbolCountBits>
static bool _init_from_hist(hist_dec_pack_t<TotalSymbolCountBits> *pDecHist, hist_t *pIncompleteHist, const uint32_t totalSymbolCountBits)
{
  if (!inplace_complete_hist(pIncompleteHist, totalSymbolCountBits))
    return false;

  make_dec_pack_hist(pDecHist, pIncompleteHist);

  return true;
}

//////////////////////////////////////////////////////////////////////////

template <uint32_t TotalSymbolCountBits>
struct rans32x64_16w_decoder<r32x64_dt_scalar, TotalSymbolCountBits, hist_dec_t<TotalSymbolCountBits>>
{
  static size_t decode_section(_rans_decode_state64_t<hist_dec_t<TotalSymbolCountBits>> *pState, uint8_t *pOutData, const size_t startIndex, const size_t endIndex)
  {
    constexpr uint32_t TotalSymbolCount = ((uint32_t)1 << TotalSymbolCountBits);

    size_t i = startIndex;

    for (; i < endIndex; i += StateCount)
    {
      for (size_t j = 0; j < StateCount; j++)
      {
        const uint8_t index = _Rans32x64_idx2idx[j];
        uint32_t state = pState->states[j];

        const uint32_t slot = state & (TotalSymbolCount - 1);
        const uint8_t symbol = pState->hist.cumulInv[slot];
        pOutData[i + index] = symbol;

        state = (state >> TotalSymbolCountBits) * (uint32_t)pState->hist.symbolCount[symbol] + slot - (uint32_t)pState->hist.cumul[symbol];

        if constexpr (DecodeNoBranch)
        {
          const bool read = state < DecodeConsumePoint16;
          const uint32_t newState = state << 16 | *pState->pReadHead;
          state = read ? newState : state;
          pState->pReadHead += (size_t)read;
        }
        else
        {
          if (state < DecodeConsumePoint16)
          {
            state = state << 16 | *pState->pReadHead;
            pState->pReadHead++;
          }
        }

        pState->states[j] = state;
      }
    }

    return i;
  }
};

template <uint32_t TotalSymbolCountBits, bool ShuffleMask16, bool WriteAligned32 = false>
#if !defined(_MSC_VER) || defined(__llvm__)
__attribute__((target("avx2")))
#endif
static size_t _block_rans32x64_decode_section_avx2_varA(_rans_decode_state64_t<hist_dec2_t<TotalSymbolCountBits>> *pState, uint8_t *pOutData, const size_t startIndex, const size_t endIndex)
{
  if constexpr (!WriteAligned32)
    if ((reinterpret_cast<size_t>(pOutData) & (32 - 1)) == 0)
      return _block_rans32x64_decode_section_avx2_varA<TotalSymbolCountBits, ShuffleMask16, true>(pState, pOutData, startIndex, endIndex);

  constexpr uint32_t TotalSymbolCount = ((uint32_t)1 << TotalSymbolCountBits);
  static_assert(TotalSymbolCountBits < 16);

  typedef __m256i simd_t;
  simd_t statesX8[StateCount / (sizeof(simd_t) / sizeof(uint32_t))];

  for (size_t i = 0; i < sizeof(statesX8) / sizeof(simd_t); i++)
    statesX8[i] = _mm256_load_si256(reinterpret_cast<const simd_t *>(reinterpret_cast<const uint8_t *>(pState->states) + i * sizeof(simd_t)));

  size_t i = startIndex;

  const simd_t symCountMask = _mm256_set1_epi32(TotalSymbolCount - 1);
  const simd_t lower16 = _mm256_set1_epi32(0xFFFF);
  const simd_t lower8 = _mm256_set1_epi32(0xFF);
  const simd_t decodeConsumePoint = _mm256_set1_epi32(DecodeConsumePoint16);
  const simd_t _16 = _mm256_set1_epi32(16);
  const __m128i shuffleDoubleMask = _mm_set_epi8(7, 7, 6, 6, 5, 5, 4, 4, 3, 3, 2, 2, 1, 1, 0, 0);
  const __m128i shuffleUpper16Bit = _mm_set1_epi16(0x0100);

  for (; i < endIndex; i += StateCount)
  {
    // const uint32_t slot = state & (TotalSymbolCount - 1);
    const simd_t slot0 = _mm256_and_si256(statesX8[0], symCountMask);
    const simd_t slot1 = _mm256_and_si256(statesX8[1], symCountMask);
    const simd_t slot2 = _mm256_and_si256(statesX8[2], symCountMask);
    const simd_t slot3 = _mm256_and_si256(statesX8[3], symCountMask);
    const simd_t slot4 = _mm256_and_si256(statesX8[4], symCountMask);
    const simd_t slot5 = _mm256_and_si256(statesX8[5], symCountMask);
    const simd_t slot6 = _mm256_and_si256(statesX8[6], symCountMask);
    const simd_t slot7 = _mm256_and_si256(statesX8[7], symCountMask);

    // const uint8_t symbol = pHist->cumulInv[slot];
    simd_t symbol0 = _mm256_i32gather_epi32(reinterpret_cast<const int32_t *>(&pState->hist.cumulInv), slot0, sizeof(uint8_t));
    simd_t symbol1 = _mm256_i32gather_epi32(reinterpret_cast<const int32_t *>(&pState->hist.cumulInv), slot1, sizeof(uint8_t));
    simd_t symbol2 = _mm256_i32gather_epi32(reinterpret_cast<const int32_t *>(&pState->hist.cumulInv), slot2, sizeof(uint8_t));
    simd_t symbol3 = _mm256_i32gather_epi32(reinterpret_cast<const int32_t *>(&pState->hist.cumulInv), slot3, sizeof(uint8_t));
    simd_t symbol4 = _mm256_i32gather_epi32(reinterpret_cast<const int32_t *>(&pState->hist.cumulInv), slot4, sizeof(uint8_t));
    simd_t symbol5 = _mm256_i32gather_epi32(reinterpret_cast<const int32_t *>(&pState->hist.cumulInv), slot5, sizeof(uint8_t));
    simd_t symbol6 = _mm256_i32gather_epi32(reinterpret_cast<const int32_t *>(&pState->hist.cumulInv), slot6, sizeof(uint8_t));
    simd_t symbol7 = _mm256_i32gather_epi32(reinterpret_cast<const int32_t *>(&pState->hist.cumulInv), slot7, sizeof(uint8_t));

    // since they were int32_t turn into uint8_t
    symbol0 = _mm256_and_si256(symbol0, lower8);
    symbol1 = _mm256_and_si256(symbol1, lower8);
    symbol2 = _mm256_and_si256(symbol2, lower8);
    symbol3 = _mm256_and_si256(symbol3, lower8);
    symbol4 = _mm256_and_si256(symbol4, lower8);
    symbol5 = _mm256_and_si256(symbol5, lower8);
    symbol6 = _mm256_and_si256(symbol6, lower8);
    symbol7 = _mm256_and_si256(symbol7, lower8);

    // pack symbols to one si256. (could possibly be `_mm256_cvtepi32_epi8` on avx-512f + avx-512-vl) (`_mm256_slli_epi32` + `_mm256_or_si256` packing is slower)
    const simd_t symPack01 = _mm256_packus_epi32(symbol0, symbol1);
    const simd_t symPack23 = _mm256_packus_epi32(symbol2, symbol3);
    const simd_t symPack0123 = _mm256_packus_epi16(symPack01, symPack23); // `00 01 02 03 08 09 0A 0B 10 11 12 13 18 19 1A 1B 04 05 06 07 0C 0D 0E 0F 14 15 16 17 1C 1D 1E 1F`

    // We intentionally encoded in a way to not have to do horrible things here.
    if constexpr (WriteAligned32)
      _mm256_stream_si256(reinterpret_cast<__m256i *>(pOutData + i), symPack0123);
    else
      _mm256_storeu_si256(reinterpret_cast<__m256i *>(pOutData + i), symPack0123);

    const simd_t symPack45 = _mm256_packus_epi32(symbol4, symbol5);
    const simd_t symPack67 = _mm256_packus_epi32(symbol6, symbol7);
    const simd_t symPack4567 = _mm256_packus_epi16(symPack45, symPack67); // same weird order.

    if constexpr (WriteAligned32)
      _mm256_stream_si256(reinterpret_cast<__m256i *>(pOutData + i + StateCount / 2), symPack4567);
    else
      _mm256_storeu_si256(reinterpret_cast<__m256i *>(pOutData + i + StateCount / 2), symPack4567);

    // retrieve pack.
    const simd_t pack0 = _mm256_i32gather_epi32(reinterpret_cast<const int32_t *>(&pState->hist.symbols), symbol0, sizeof(uint32_t));
    const simd_t pack1 = _mm256_i32gather_epi32(reinterpret_cast<const int32_t *>(&pState->hist.symbols), symbol1, sizeof(uint32_t));
    const simd_t pack2 = _mm256_i32gather_epi32(reinterpret_cast<const int32_t *>(&pState->hist.symbols), symbol2, sizeof(uint32_t));
    const simd_t pack3 = _mm256_i32gather_epi32(reinterpret_cast<const int32_t *>(&pState->hist.symbols), symbol3, sizeof(uint32_t));
    const simd_t pack4 = _mm256_i32gather_epi32(reinterpret_cast<const int32_t *>(&pState->hist.symbols), symbol4, sizeof(uint32_t));
    const simd_t pack5 = _mm256_i32gather_epi32(reinterpret_cast<const int32_t *>(&pState->hist.symbols), symbol5, sizeof(uint32_t));
    const simd_t pack6 = _mm256_i32gather_epi32(reinterpret_cast<const int32_t *>(&pState->hist.symbols), symbol6, sizeof(uint32_t));
    const simd_t pack7 = _mm256_i32gather_epi32(reinterpret_cast<const int32_t *>(&pState->hist.symbols), symbol7, sizeof(uint32_t));

    // freq, cumul.
    const simd_t cumul0 = _mm256_srli_epi32(pack0, 16);
    const simd_t freq0 = _mm256_and_si256(pack0, lower16);
    const simd_t cumul1 = _mm256_srli_epi32(pack1, 16);
    const simd_t freq1 = _mm256_and_si256(pack1, lower16);
    const simd_t cumul2 = _mm256_srli_epi32(pack2, 16);
    const simd_t freq2 = _mm256_and_si256(pack2, lower16);
    const simd_t cumul3 = _mm256_srli_epi32(pack3, 16);
    const simd_t freq3 = _mm256_and_si256(pack3, lower16);
    const simd_t cumul4 = _mm256_srli_epi32(pack4, 16);
    const simd_t freq4 = _mm256_and_si256(pack4, lower16);
    const simd_t cumul5 = _mm256_srli_epi32(pack5, 16);
    const simd_t freq5 = _mm256_and_si256(pack5, lower16);
    const simd_t cumul6 = _mm256_srli_epi32(pack6, 16);
    const simd_t freq6 = _mm256_and_si256(pack6, lower16);
    const simd_t cumul7 = _mm256_srli_epi32(pack7, 16);
    const simd_t freq7 = _mm256_and_si256(pack7, lower16);

    // const uint32_t shiftedState = (state >> TotalSymbolCountBits);
    const simd_t shiftedState0 = _mm256_srli_epi32(statesX8[0], TotalSymbolCountBits);
    const simd_t shiftedState1 = _mm256_srli_epi32(statesX8[1], TotalSymbolCountBits);
    const simd_t shiftedState2 = _mm256_srli_epi32(statesX8[2], TotalSymbolCountBits);
    const simd_t shiftedState3 = _mm256_srli_epi32(statesX8[3], TotalSymbolCountBits);
    const simd_t shiftedState4 = _mm256_srli_epi32(statesX8[4], TotalSymbolCountBits);
    const simd_t shiftedState5 = _mm256_srli_epi32(statesX8[5], TotalSymbolCountBits);
    const simd_t shiftedState6 = _mm256_srli_epi32(statesX8[6], TotalSymbolCountBits);
    const simd_t shiftedState7 = _mm256_srli_epi32(statesX8[7], TotalSymbolCountBits);

    // const uint32_t freqScaled = shiftedState * freq;
    const __m256i freqScaled0 = _mm256_mullo_epi32(shiftedState0, freq0);
    const __m256i freqScaled1 = _mm256_mullo_epi32(shiftedState1, freq1);
    const __m256i freqScaled2 = _mm256_mullo_epi32(shiftedState2, freq2);
    const __m256i freqScaled3 = _mm256_mullo_epi32(shiftedState3, freq3);
    const __m256i freqScaled4 = _mm256_mullo_epi32(shiftedState4, freq4);
    const __m256i freqScaled5 = _mm256_mullo_epi32(shiftedState5, freq5);
    const __m256i freqScaled6 = _mm256_mullo_epi32(shiftedState6, freq6);
    const __m256i freqScaled7 = _mm256_mullo_epi32(shiftedState7, freq7);

    // state = freqScaled + slot - cumul;
    const simd_t state0 = _mm256_add_epi32(freqScaled0, _mm256_sub_epi32(slot0, cumul0));
    const simd_t state1 = _mm256_add_epi32(freqScaled1, _mm256_sub_epi32(slot1, cumul1));
    const simd_t state2 = _mm256_add_epi32(freqScaled2, _mm256_sub_epi32(slot2, cumul2));
    const simd_t state3 = _mm256_add_epi32(freqScaled3, _mm256_sub_epi32(slot3, cumul3));
    const simd_t state4 = _mm256_add_epi32(freqScaled4, _mm256_sub_epi32(slot4, cumul4));
    const simd_t state5 = _mm256_add_epi32(freqScaled5, _mm256_sub_epi32(slot5, cumul5));
    const simd_t state6 = _mm256_add_epi32(freqScaled6, _mm256_sub_epi32(slot6, cumul6));
    const simd_t state7 = _mm256_add_epi32(freqScaled7, _mm256_sub_epi32(slot7, cumul7));

    // now to the messy part...
    {
      // read input for blocks 0.
      const __m128i newWords0 = _mm_loadu_si128(reinterpret_cast<const __m128i *>(pState->pReadHead));

      // (state < DecodeConsumePoint16) ? -1 : 0 | well, actually (DecodeConsumePoint16 > state) ? -1 : 0
      const simd_t cmp0 = _mm256_cmpgt_epi32(decodeConsumePoint, state0);
      const simd_t cmp1 = _mm256_cmpgt_epi32(decodeConsumePoint, state1);
      const simd_t cmp2 = _mm256_cmpgt_epi32(decodeConsumePoint, state2);
      const simd_t cmp3 = _mm256_cmpgt_epi32(decodeConsumePoint, state3);
      const simd_t cmp4 = _mm256_cmpgt_epi32(decodeConsumePoint, state4);
      const simd_t cmp5 = _mm256_cmpgt_epi32(decodeConsumePoint, state5);
      const simd_t cmp6 = _mm256_cmpgt_epi32(decodeConsumePoint, state6);
      const simd_t cmp7 = _mm256_cmpgt_epi32(decodeConsumePoint, state7);

      if constexpr (ShuffleMask16)
      {
        // get masks of those compares & start loading shuffle masks.
        const uint32_t cmpMask0 = (uint32_t)_mm256_movemask_ps(_mm256_castsi256_ps(cmp0));
        __m128i lut0 = _mm_load_si128(reinterpret_cast<const __m128i *>(&_DoubleShuffleLutShfl32[cmpMask0 << 4])); // `* 16`.

        const uint32_t cmpMask1 = (uint32_t)_mm256_movemask_ps(_mm256_castsi256_ps(cmp1));
        __m128i lut1 = _mm_load_si128(reinterpret_cast<const __m128i *>(&_DoubleShuffleLutShfl32[cmpMask1 << 4])); // `* 16`.

        const uint32_t cmpMask2 = (uint32_t)_mm256_movemask_ps(_mm256_castsi256_ps(cmp2));
        __m128i lut2 = _mm_load_si128(reinterpret_cast<const __m128i *>(&_DoubleShuffleLutShfl32[cmpMask2 << 4])); // `* 16`.

        const uint32_t cmpMask3 = (uint32_t)_mm256_movemask_ps(_mm256_castsi256_ps(cmp3));
        __m128i lut3 = _mm_load_si128(reinterpret_cast<const __m128i *>(&_DoubleShuffleLutShfl32[cmpMask3 << 4])); // `* 16`.

        const uint32_t cmpMask4 = (uint32_t)_mm256_movemask_ps(_mm256_castsi256_ps(cmp4));
        __m128i lut4 = _mm_load_si128(reinterpret_cast<const __m128i *>(&_DoubleShuffleLutShfl32[cmpMask4 << 4])); // `* 16`.

        const uint32_t cmpMask5 = (uint32_t)_mm256_movemask_ps(_mm256_castsi256_ps(cmp5));
        __m128i lut5 = _mm_load_si128(reinterpret_cast<const __m128i *>(&_DoubleShuffleLutShfl32[cmpMask5 << 4])); // `* 16`.

        const uint32_t cmpMask6 = (uint32_t)_mm256_movemask_ps(_mm256_castsi256_ps(cmp6));
        __m128i lut6 = _mm_load_si128(reinterpret_cast<const __m128i *>(&_DoubleShuffleLutShfl32[cmpMask6 << 4])); // `* 16`.

        const uint32_t cmpMask7 = (uint32_t)_mm256_movemask_ps(_mm256_castsi256_ps(cmp7));
        __m128i lut7 = _mm_load_si128(reinterpret_cast<const __m128i *>(&_DoubleShuffleLutShfl32[cmpMask7 << 4])); // `* 16`.

        // advance read head & read input for blocks 1, 2, 3, 4, 5, 6, 7.
        const uint32_t maskPop0 = (uint32_t)__builtin_popcount(cmpMask0);
        pState->pReadHead += maskPop0;

        const __m128i newWords1 = _mm_loadu_si128(reinterpret_cast<const __m128i *>(pState->pReadHead));

        const uint32_t maskPop1 = (uint32_t)__builtin_popcount(cmpMask1);
        pState->pReadHead += maskPop1;

        const __m128i newWords2 = _mm_loadu_si128(reinterpret_cast<const __m128i *>(pState->pReadHead));

        const uint32_t maskPop2 = (uint32_t)__builtin_popcount(cmpMask2);
        pState->pReadHead += maskPop2;

        const __m128i newWords3 = _mm_loadu_si128(reinterpret_cast<const __m128i *>(pState->pReadHead));

        const uint32_t maskPop3 = (uint32_t)__builtin_popcount(cmpMask3);
        pState->pReadHead += maskPop3;

        const __m128i newWords4 = _mm_loadu_si128(reinterpret_cast<const __m128i *>(pState->pReadHead));

        const uint32_t maskPop4 = (uint32_t)__builtin_popcount(cmpMask4);
        pState->pReadHead += maskPop4;

        const __m128i newWords5 = _mm_loadu_si128(reinterpret_cast<const __m128i *>(pState->pReadHead));

        const uint32_t maskPop5 = (uint32_t)__builtin_popcount(cmpMask5);
        pState->pReadHead += maskPop5;

        const __m128i newWords6 = _mm_loadu_si128(reinterpret_cast<const __m128i *>(pState->pReadHead));

        const uint32_t maskPop6 = (uint32_t)__builtin_popcount(cmpMask6);
        pState->pReadHead += maskPop6;

        const __m128i newWords7 = _mm_loadu_si128(reinterpret_cast<const __m128i *>(pState->pReadHead));

        const uint32_t maskPop7 = (uint32_t)__builtin_popcount(cmpMask7);
        pState->pReadHead += maskPop7;

        // matching: state << 16
        const simd_t matchShiftedState0 = _mm256_sllv_epi32(state0, _mm256_and_si256(cmp0, _16));
        const simd_t matchShiftedState1 = _mm256_sllv_epi32(state1, _mm256_and_si256(cmp1, _16));
        const simd_t matchShiftedState2 = _mm256_sllv_epi32(state2, _mm256_and_si256(cmp2, _16));
        const simd_t matchShiftedState3 = _mm256_sllv_epi32(state3, _mm256_and_si256(cmp3, _16));
        const simd_t matchShiftedState4 = _mm256_sllv_epi32(state4, _mm256_and_si256(cmp4, _16));
        const simd_t matchShiftedState5 = _mm256_sllv_epi32(state5, _mm256_and_si256(cmp5, _16));
        const simd_t matchShiftedState6 = _mm256_sllv_epi32(state6, _mm256_and_si256(cmp6, _16));
        const simd_t matchShiftedState7 = _mm256_sllv_epi32(state7, _mm256_and_si256(cmp7, _16));

        // shuffle new words in place.
        const __m128i newWordXmm0 = _mm_shuffle_epi8(newWords0, lut0);
        const __m128i newWordXmm1 = _mm_shuffle_epi8(newWords1, lut1);
        const __m128i newWordXmm2 = _mm_shuffle_epi8(newWords2, lut2);
        const __m128i newWordXmm3 = _mm_shuffle_epi8(newWords3, lut3);
        const __m128i newWordXmm4 = _mm_shuffle_epi8(newWords4, lut4);
        const __m128i newWordXmm5 = _mm_shuffle_epi8(newWords5, lut5);
        const __m128i newWordXmm6 = _mm_shuffle_epi8(newWords6, lut6);
        const __m128i newWordXmm7 = _mm_shuffle_epi8(newWords7, lut7);

        // expand new word.
        const __m256i newWord0 = _mm256_cvtepu16_epi32(newWordXmm0);
        const __m256i newWord1 = _mm256_cvtepu16_epi32(newWordXmm1);
        const __m256i newWord2 = _mm256_cvtepu16_epi32(newWordXmm2);
        const __m256i newWord3 = _mm256_cvtepu16_epi32(newWordXmm3);
        const __m256i newWord4 = _mm256_cvtepu16_epi32(newWordXmm4);
        const __m256i newWord5 = _mm256_cvtepu16_epi32(newWordXmm5);
        const __m256i newWord6 = _mm256_cvtepu16_epi32(newWordXmm6);
        const __m256i newWord7 = _mm256_cvtepu16_epi32(newWordXmm7);

        // state = state << 16 | newWord;
        statesX8[0] = _mm256_or_si256(matchShiftedState0, newWord0);
        statesX8[1] = _mm256_or_si256(matchShiftedState1, newWord1);
        statesX8[2] = _mm256_or_si256(matchShiftedState2, newWord2);
        statesX8[3] = _mm256_or_si256(matchShiftedState3, newWord3);
        statesX8[4] = _mm256_or_si256(matchShiftedState4, newWord4);
        statesX8[5] = _mm256_or_si256(matchShiftedState5, newWord5);
        statesX8[6] = _mm256_or_si256(matchShiftedState6, newWord6);
        statesX8[7] = _mm256_or_si256(matchShiftedState7, newWord7);
      }
      else
      {
        // get masks of those compares & start loading shuffle masks.
        const uint32_t cmpMask0 = (uint32_t)_mm256_movemask_ps(_mm256_castsi256_ps(cmp0));
        __m128i lut0 = _mm_lddqu_si128(reinterpret_cast<const __m128i *>(&_ShuffleLutShfl32[cmpMask0 << 3])); // `* 8`.

        const uint32_t cmpMask1 = (uint32_t)_mm256_movemask_ps(_mm256_castsi256_ps(cmp1));
        __m128i lut1 = _mm_lddqu_si128(reinterpret_cast<const __m128i *>(&_ShuffleLutShfl32[cmpMask1 << 3])); // `* 8`.

        const uint32_t cmpMask2 = (uint32_t)_mm256_movemask_ps(_mm256_castsi256_ps(cmp2));
        __m128i lut2 = _mm_lddqu_si128(reinterpret_cast<const __m128i *>(&_ShuffleLutShfl32[cmpMask2 << 3])); // `* 8`.

        const uint32_t cmpMask3 = (uint32_t)_mm256_movemask_ps(_mm256_castsi256_ps(cmp3));
        __m128i lut3 = _mm_lddqu_si128(reinterpret_cast<const __m128i *>(&_ShuffleLutShfl32[cmpMask3 << 3])); // `* 8`.

        const uint32_t cmpMask4 = (uint32_t)_mm256_movemask_ps(_mm256_castsi256_ps(cmp4));
        __m128i lut4 = _mm_lddqu_si128(reinterpret_cast<const __m128i *>(&_ShuffleLutShfl32[cmpMask4 << 3])); // `* 8`.

        const uint32_t cmpMask5 = (uint32_t)_mm256_movemask_ps(_mm256_castsi256_ps(cmp5));
        __m128i lut5 = _mm_lddqu_si128(reinterpret_cast<const __m128i *>(&_ShuffleLutShfl32[cmpMask5 << 3])); // `* 8`.

        const uint32_t cmpMask6 = (uint32_t)_mm256_movemask_ps(_mm256_castsi256_ps(cmp6));
        __m128i lut6 = _mm_lddqu_si128(reinterpret_cast<const __m128i *>(&_ShuffleLutShfl32[cmpMask6 << 3])); // `* 8`.

        const uint32_t cmpMask7 = (uint32_t)_mm256_movemask_ps(_mm256_castsi256_ps(cmp7));
        __m128i lut7 = _mm_lddqu_si128(reinterpret_cast<const __m128i *>(&_ShuffleLutShfl32[cmpMask7 << 3])); // `* 8`.

        // advance read head & read input for blocks 1, 2, 3, 4, 5, 6, 7.
        const uint32_t maskPop0 = (uint32_t)__builtin_popcount(cmpMask0);
        pState->pReadHead += maskPop0;

        const __m128i newWords1 = _mm_loadu_si128(reinterpret_cast<const __m128i *>(pState->pReadHead));

        const uint32_t maskPop1 = (uint32_t)__builtin_popcount(cmpMask1);
        pState->pReadHead += maskPop1;

        const __m128i newWords2 = _mm_loadu_si128(reinterpret_cast<const __m128i *>(pState->pReadHead));

        const uint32_t maskPop2 = (uint32_t)__builtin_popcount(cmpMask2);
        pState->pReadHead += maskPop2;

        const __m128i newWords3 = _mm_loadu_si128(reinterpret_cast<const __m128i *>(pState->pReadHead));

        const uint32_t maskPop3 = (uint32_t)__builtin_popcount(cmpMask3);
        pState->pReadHead += maskPop3;

        const __m128i newWords4 = _mm_loadu_si128(reinterpret_cast<const __m128i *>(pState->pReadHead));

        const uint32_t maskPop4 = (uint32_t)__builtin_popcount(cmpMask4);
        pState->pReadHead += maskPop4;

        const __m128i newWords5 = _mm_loadu_si128(reinterpret_cast<const __m128i *>(pState->pReadHead));

        const uint32_t maskPop5 = (uint32_t)__builtin_popcount(cmpMask5);
        pState->pReadHead += maskPop5;

        const __m128i newWords6 = _mm_loadu_si128(reinterpret_cast<const __m128i *>(pState->pReadHead));

        const uint32_t maskPop6 = (uint32_t)__builtin_popcount(cmpMask6);
        pState->pReadHead += maskPop6;

        const __m128i newWords7 = _mm_loadu_si128(reinterpret_cast<const __m128i *>(pState->pReadHead));

        const uint32_t maskPop7 = (uint32_t)__builtin_popcount(cmpMask7);
        pState->pReadHead += maskPop7;

        // finalize lookups.
        lut0 = _mm_or_si128(_mm_shuffle_epi8(lut0, shuffleDoubleMask), shuffleUpper16Bit);
        lut1 = _mm_or_si128(_mm_shuffle_epi8(lut1, shuffleDoubleMask), shuffleUpper16Bit);
        lut2 = _mm_or_si128(_mm_shuffle_epi8(lut2, shuffleDoubleMask), shuffleUpper16Bit);
        lut3 = _mm_or_si128(_mm_shuffle_epi8(lut3, shuffleDoubleMask), shuffleUpper16Bit);
        lut4 = _mm_or_si128(_mm_shuffle_epi8(lut4, shuffleDoubleMask), shuffleUpper16Bit);
        lut5 = _mm_or_si128(_mm_shuffle_epi8(lut5, shuffleDoubleMask), shuffleUpper16Bit);
        lut6 = _mm_or_si128(_mm_shuffle_epi8(lut6, shuffleDoubleMask), shuffleUpper16Bit);
        lut7 = _mm_or_si128(_mm_shuffle_epi8(lut7, shuffleDoubleMask), shuffleUpper16Bit);

        // matching: state << 16
        const simd_t matchShiftedState0 = _mm256_sllv_epi32(state0, _mm256_and_si256(cmp0, _16));
        const simd_t matchShiftedState1 = _mm256_sllv_epi32(state1, _mm256_and_si256(cmp1, _16));
        const simd_t matchShiftedState2 = _mm256_sllv_epi32(state2, _mm256_and_si256(cmp2, _16));
        const simd_t matchShiftedState3 = _mm256_sllv_epi32(state3, _mm256_and_si256(cmp3, _16));
        const simd_t matchShiftedState4 = _mm256_sllv_epi32(state4, _mm256_and_si256(cmp4, _16));
        const simd_t matchShiftedState5 = _mm256_sllv_epi32(state5, _mm256_and_si256(cmp5, _16));
        const simd_t matchShiftedState6 = _mm256_sllv_epi32(state6, _mm256_and_si256(cmp6, _16));
        const simd_t matchShiftedState7 = _mm256_sllv_epi32(state7, _mm256_and_si256(cmp7, _16));

        // shuffle new words in place.
        const __m128i newWordXmm0 = _mm_shuffle_epi8(newWords0, lut0);
        const __m128i newWordXmm1 = _mm_shuffle_epi8(newWords1, lut1);
        const __m128i newWordXmm2 = _mm_shuffle_epi8(newWords2, lut2);
        const __m128i newWordXmm3 = _mm_shuffle_epi8(newWords3, lut3);
        const __m128i newWordXmm4 = _mm_shuffle_epi8(newWords4, lut4);
        const __m128i newWordXmm5 = _mm_shuffle_epi8(newWords5, lut5);
        const __m128i newWordXmm6 = _mm_shuffle_epi8(newWords6, lut6);
        const __m128i newWordXmm7 = _mm_shuffle_epi8(newWords7, lut7);

        // expand new word.
        const __m256i newWord0 = _mm256_cvtepu16_epi32(newWordXmm0);
        const __m256i newWord1 = _mm256_cvtepu16_epi32(newWordXmm1);
        const __m256i newWord2 = _mm256_cvtepu16_epi32(newWordXmm2);
        const __m256i newWord3 = _mm256_cvtepu16_epi32(newWordXmm3);
        const __m256i newWord4 = _mm256_cvtepu16_epi32(newWordXmm4);
        const __m256i newWord5 = _mm256_cvtepu16_epi32(newWordXmm5);
        const __m256i newWord6 = _mm256_cvtepu16_epi32(newWordXmm6);
        const __m256i newWord7 = _mm256_cvtepu16_epi32(newWordXmm7);

        // state = state << 16 | newWord;
        statesX8[0] = _mm256_or_si256(matchShiftedState0, newWord0);
        statesX8[1] = _mm256_or_si256(matchShiftedState1, newWord1);
        statesX8[2] = _mm256_or_si256(matchShiftedState2, newWord2);
        statesX8[3] = _mm256_or_si256(matchShiftedState3, newWord3);
        statesX8[4] = _mm256_or_si256(matchShiftedState4, newWord4);
        statesX8[5] = _mm256_or_si256(matchShiftedState5, newWord5);
        statesX8[6] = _mm256_or_si256(matchShiftedState6, newWord6);
        statesX8[7] = _mm256_or_si256(matchShiftedState7, newWord7);
      }
    }
  }

  for (size_t j = 0; j < sizeof(statesX8) / sizeof(simd_t); j++)
    _mm256_store_si256(reinterpret_cast<simd_t *>(reinterpret_cast<uint8_t *>(pState->states) + j * sizeof(simd_t)), statesX8[j]);

  return i;
}

template <uint32_t TotalSymbolCountBits, bool ShuffleMask16, bool WriteAligned32 = false>
#if !defined(_MSC_VER) || defined(__llvm__)
__attribute__((target("avx2")))
#endif
static size_t _block_rans32x64_decode_section_avx2_varC(_rans_decode_state64_t<hist_dec_pack_t<TotalSymbolCountBits>> *pState, uint8_t *pOutData, const size_t startIndex, const size_t endIndex)
{
  if constexpr (!WriteAligned32)
    if ((reinterpret_cast<size_t>(pOutData) & (32 - 1)) == 0)
      return _block_rans32x64_decode_section_avx2_varC<TotalSymbolCountBits, ShuffleMask16, true>(pState, pOutData, startIndex, endIndex);

  constexpr uint32_t TotalSymbolCount = ((uint32_t)1 << TotalSymbolCountBits);
  static_assert(TotalSymbolCountBits < 16);

  typedef __m256i simd_t;
  simd_t statesX8[StateCount / (sizeof(simd_t) / sizeof(uint32_t))];

  for (size_t i = 0; i < sizeof(statesX8) / sizeof(simd_t); i++)
    statesX8[i] = _mm256_load_si256(reinterpret_cast<const simd_t *>(reinterpret_cast<const uint8_t *>(pState->states) + i * sizeof(simd_t)));

  size_t i = startIndex;

  const simd_t symCountMask = _mm256_set1_epi32(TotalSymbolCount - 1);
  const simd_t lower12 = _mm256_set1_epi32((1 << 12) - 1);
  const simd_t lower8 = _mm256_set1_epi32(0xFF);
  const simd_t decodeConsumePoint = _mm256_set1_epi32(DecodeConsumePoint16);
  const simd_t _16 = _mm256_set1_epi32(16);
  const __m128i shuffleDoubleMask = _mm_set_epi8(7, 7, 6, 6, 5, 5, 4, 4, 3, 3, 2, 2, 1, 1, 0, 0);
  const __m128i shuffleUpper16Bit = _mm_set1_epi16(0x0100);

  for (; i < endIndex; i += StateCount)
  {
    // const uint32_t slot = state & (TotalSymbolCount - 1);
    const simd_t slot0 = _mm256_and_si256(statesX8[0], symCountMask);
    const simd_t slot1 = _mm256_and_si256(statesX8[1], symCountMask);
    const simd_t slot2 = _mm256_and_si256(statesX8[2], symCountMask);
    const simd_t slot3 = _mm256_and_si256(statesX8[3], symCountMask);
    const simd_t slot4 = _mm256_and_si256(statesX8[4], symCountMask);
    const simd_t slot5 = _mm256_and_si256(statesX8[5], symCountMask);
    const simd_t slot6 = _mm256_and_si256(statesX8[6], symCountMask);
    const simd_t slot7 = _mm256_and_si256(statesX8[7], symCountMask);

    // retrieve pack.
    const simd_t pack0 = _mm256_i32gather_epi32(reinterpret_cast<const int32_t *>(pState->hist.symbol), slot0, sizeof(uint32_t));
    const simd_t pack1 = _mm256_i32gather_epi32(reinterpret_cast<const int32_t *>(pState->hist.symbol), slot1, sizeof(uint32_t));
    const simd_t pack2 = _mm256_i32gather_epi32(reinterpret_cast<const int32_t *>(pState->hist.symbol), slot2, sizeof(uint32_t));
    const simd_t pack3 = _mm256_i32gather_epi32(reinterpret_cast<const int32_t *>(pState->hist.symbol), slot3, sizeof(uint32_t));
    const simd_t pack4 = _mm256_i32gather_epi32(reinterpret_cast<const int32_t *>(pState->hist.symbol), slot4, sizeof(uint32_t));
    const simd_t pack5 = _mm256_i32gather_epi32(reinterpret_cast<const int32_t *>(pState->hist.symbol), slot5, sizeof(uint32_t));
    const simd_t pack6 = _mm256_i32gather_epi32(reinterpret_cast<const int32_t *>(pState->hist.symbol), slot6, sizeof(uint32_t));
    const simd_t pack7 = _mm256_i32gather_epi32(reinterpret_cast<const int32_t *>(pState->hist.symbol), slot7, sizeof(uint32_t));

    // const uint32_t shiftedState = (state >> TotalSymbolCountBits);
    const simd_t shiftedState0 = _mm256_srli_epi32(statesX8[0], TotalSymbolCountBits);
    const simd_t shiftedState1 = _mm256_srli_epi32(statesX8[1], TotalSymbolCountBits);
    const simd_t shiftedState2 = _mm256_srli_epi32(statesX8[2], TotalSymbolCountBits);
    const simd_t shiftedState3 = _mm256_srli_epi32(statesX8[3], TotalSymbolCountBits);
    const simd_t shiftedState4 = _mm256_srli_epi32(statesX8[4], TotalSymbolCountBits);
    const simd_t shiftedState5 = _mm256_srli_epi32(statesX8[5], TotalSymbolCountBits);
    const simd_t shiftedState6 = _mm256_srli_epi32(statesX8[6], TotalSymbolCountBits);
    const simd_t shiftedState7 = _mm256_srli_epi32(statesX8[7], TotalSymbolCountBits);

    // unpack symbol.
    const simd_t symbol0 = _mm256_and_si256(pack0, lower8);
    const simd_t symbol1 = _mm256_and_si256(pack1, lower8);
    const simd_t symbol2 = _mm256_and_si256(pack2, lower8);
    const simd_t symbol3 = _mm256_and_si256(pack3, lower8);
    const simd_t symbol4 = _mm256_and_si256(pack4, lower8);
    const simd_t symbol5 = _mm256_and_si256(pack5, lower8);
    const simd_t symbol6 = _mm256_and_si256(pack6, lower8);
    const simd_t symbol7 = _mm256_and_si256(pack7, lower8);

    // pack symbols to one si256. (could possibly be `_mm256_cvtepi32_epi8` on avx-512f + avx-512-vl) (`_mm256_slli_epi32` + `_mm256_or_si256` packing is slower)
    const simd_t symPack01 = _mm256_packus_epi32(symbol0, symbol1);
    const simd_t symPack23 = _mm256_packus_epi32(symbol2, symbol3);
    const simd_t symPack0123 = _mm256_packus_epi16(symPack01, symPack23); // `00 01 02 03 08 09 0A 0B 10 11 12 13 18 19 1A 1B 04 05 06 07 0C 0D 0E 0F 14 15 16 17 1C 1D 1E 1F`

    // We intentionally encoded in a way to not have to do horrible things here.
    if constexpr (WriteAligned32)
      _mm256_stream_si256(reinterpret_cast<__m256i *>(pOutData + i), symPack0123);
    else
      _mm256_storeu_si256(reinterpret_cast<__m256i *>(pOutData + i), symPack0123);

    const simd_t symPack45 = _mm256_packus_epi32(symbol4, symbol5);
    const simd_t symPack67 = _mm256_packus_epi32(symbol6, symbol7);
    const simd_t symPack4567 = _mm256_packus_epi16(symPack45, symPack67); // same weird order.

    if constexpr (WriteAligned32)
      _mm256_stream_si256(reinterpret_cast<__m256i *>(pOutData + i + StateCount / 2), symPack4567);
    else
      _mm256_storeu_si256(reinterpret_cast<__m256i *>(pOutData + i + StateCount / 2), symPack4567);

    // unpack freq, cumul.
    const simd_t cumul0 = _mm256_and_si256(_mm256_srli_epi32(pack0, 8), lower12);
    const simd_t freq0 = _mm256_srli_epi32(pack0, 20);
    const simd_t cumul1 = _mm256_and_si256(_mm256_srli_epi32(pack1, 8), lower12);
    const simd_t freq1 = _mm256_srli_epi32(pack1, 20);
    const simd_t cumul2 = _mm256_and_si256(_mm256_srli_epi32(pack2, 8), lower12);
    const simd_t freq2 = _mm256_srli_epi32(pack2, 20);
    const simd_t cumul3 = _mm256_and_si256(_mm256_srli_epi32(pack3, 8), lower12);
    const simd_t freq3 = _mm256_srli_epi32(pack3, 20);
    const simd_t cumul4 = _mm256_and_si256(_mm256_srli_epi32(pack4, 8), lower12);
    const simd_t freq4 = _mm256_srli_epi32(pack4, 20);
    const simd_t cumul5 = _mm256_and_si256(_mm256_srli_epi32(pack5, 8), lower12);
    const simd_t freq5 = _mm256_srli_epi32(pack5, 20);
    const simd_t cumul6 = _mm256_and_si256(_mm256_srli_epi32(pack6, 8), lower12);
    const simd_t freq6 = _mm256_srli_epi32(pack6, 20);
    const simd_t cumul7 = _mm256_and_si256(_mm256_srli_epi32(pack7, 8), lower12);
    const simd_t freq7 = _mm256_srli_epi32(pack7, 20);

    // const uint32_t freqScaled = shiftedState * freq;
    const __m256i freqScaled0 = _mm256_mullo_epi32(shiftedState0, freq0);
    const __m256i freqScaled1 = _mm256_mullo_epi32(shiftedState1, freq1);
    const __m256i freqScaled2 = _mm256_mullo_epi32(shiftedState2, freq2);
    const __m256i freqScaled3 = _mm256_mullo_epi32(shiftedState3, freq3);
    const __m256i freqScaled4 = _mm256_mullo_epi32(shiftedState4, freq4);
    const __m256i freqScaled5 = _mm256_mullo_epi32(shiftedState5, freq5);
    const __m256i freqScaled6 = _mm256_mullo_epi32(shiftedState6, freq6);
    const __m256i freqScaled7 = _mm256_mullo_epi32(shiftedState7, freq7);

    // state = freqScaled + slot - cumul;
    const simd_t state0 = _mm256_add_epi32(freqScaled0, _mm256_sub_epi32(slot0, cumul0));
    const simd_t state1 = _mm256_add_epi32(freqScaled1, _mm256_sub_epi32(slot1, cumul1));
    const simd_t state2 = _mm256_add_epi32(freqScaled2, _mm256_sub_epi32(slot2, cumul2));
    const simd_t state3 = _mm256_add_epi32(freqScaled3, _mm256_sub_epi32(slot3, cumul3));
    const simd_t state4 = _mm256_add_epi32(freqScaled4, _mm256_sub_epi32(slot4, cumul4));
    const simd_t state5 = _mm256_add_epi32(freqScaled5, _mm256_sub_epi32(slot5, cumul5));
    const simd_t state6 = _mm256_add_epi32(freqScaled6, _mm256_sub_epi32(slot6, cumul6));
    const simd_t state7 = _mm256_add_epi32(freqScaled7, _mm256_sub_epi32(slot7, cumul7));

    // now to the messy part...
    {
      // read input for blocks 0.
      const __m128i newWords0 = _mm_loadu_si128(reinterpret_cast<const __m128i *>(pState->pReadHead));

      // (state < DecodeConsumePoint16) ? -1 : 0 | well, actually (DecodeConsumePoint16 > state) ? -1 : 0
      const simd_t cmp0 = _mm256_cmpgt_epi32(decodeConsumePoint, state0);
      const simd_t cmp1 = _mm256_cmpgt_epi32(decodeConsumePoint, state1);
      const simd_t cmp2 = _mm256_cmpgt_epi32(decodeConsumePoint, state2);
      const simd_t cmp3 = _mm256_cmpgt_epi32(decodeConsumePoint, state3);
      const simd_t cmp4 = _mm256_cmpgt_epi32(decodeConsumePoint, state4);
      const simd_t cmp5 = _mm256_cmpgt_epi32(decodeConsumePoint, state5);
      const simd_t cmp6 = _mm256_cmpgt_epi32(decodeConsumePoint, state6);
      const simd_t cmp7 = _mm256_cmpgt_epi32(decodeConsumePoint, state7);

      if constexpr (ShuffleMask16)
      {
        // get masks of those compares & start loading shuffle masks.
        const uint32_t cmpMask0 = (uint32_t)_mm256_movemask_ps(_mm256_castsi256_ps(cmp0));
        __m128i lut0 = _mm_load_si128(reinterpret_cast<const __m128i *>(&_DoubleShuffleLutShfl32[cmpMask0 << 4])); // `* 16`.

        const uint32_t cmpMask1 = (uint32_t)_mm256_movemask_ps(_mm256_castsi256_ps(cmp1));
        __m128i lut1 = _mm_load_si128(reinterpret_cast<const __m128i *>(&_DoubleShuffleLutShfl32[cmpMask1 << 4])); // `* 16`.

        const uint32_t cmpMask2 = (uint32_t)_mm256_movemask_ps(_mm256_castsi256_ps(cmp2));
        __m128i lut2 = _mm_load_si128(reinterpret_cast<const __m128i *>(&_DoubleShuffleLutShfl32[cmpMask2 << 4])); // `* 16`.

        const uint32_t cmpMask3 = (uint32_t)_mm256_movemask_ps(_mm256_castsi256_ps(cmp3));
        __m128i lut3 = _mm_load_si128(reinterpret_cast<const __m128i *>(&_DoubleShuffleLutShfl32[cmpMask3 << 4])); // `* 16`.

        const uint32_t cmpMask4 = (uint32_t)_mm256_movemask_ps(_mm256_castsi256_ps(cmp4));
        __m128i lut4 = _mm_load_si128(reinterpret_cast<const __m128i *>(&_DoubleShuffleLutShfl32[cmpMask4 << 4])); // `* 16`.

        const uint32_t cmpMask5 = (uint32_t)_mm256_movemask_ps(_mm256_castsi256_ps(cmp5));
        __m128i lut5 = _mm_load_si128(reinterpret_cast<const __m128i *>(&_DoubleShuffleLutShfl32[cmpMask5 << 4])); // `* 16`.

        const uint32_t cmpMask6 = (uint32_t)_mm256_movemask_ps(_mm256_castsi256_ps(cmp6));
        __m128i lut6 = _mm_load_si128(reinterpret_cast<const __m128i *>(&_DoubleShuffleLutShfl32[cmpMask6 << 4])); // `* 16`.

        const uint32_t cmpMask7 = (uint32_t)_mm256_movemask_ps(_mm256_castsi256_ps(cmp7));
        __m128i lut7 = _mm_load_si128(reinterpret_cast<const __m128i *>(&_DoubleShuffleLutShfl32[cmpMask7 << 4])); // `* 16`.

        // advance read head & read input for blocks 1, 2, 3, 4, 5, 6, 7.
        const uint32_t maskPop0 = (uint32_t)__builtin_popcount(cmpMask0);
        pState->pReadHead += maskPop0;

        const __m128i newWords1 = _mm_loadu_si128(reinterpret_cast<const __m128i *>(pState->pReadHead));

        const uint32_t maskPop1 = (uint32_t)__builtin_popcount(cmpMask1);
        pState->pReadHead += maskPop1;

        const __m128i newWords2 = _mm_loadu_si128(reinterpret_cast<const __m128i *>(pState->pReadHead));

        const uint32_t maskPop2 = (uint32_t)__builtin_popcount(cmpMask2);
        pState->pReadHead += maskPop2;

        const __m128i newWords3 = _mm_loadu_si128(reinterpret_cast<const __m128i *>(pState->pReadHead));

        const uint32_t maskPop3 = (uint32_t)__builtin_popcount(cmpMask3);
        pState->pReadHead += maskPop3;

        const __m128i newWords4 = _mm_loadu_si128(reinterpret_cast<const __m128i *>(pState->pReadHead));

        const uint32_t maskPop4 = (uint32_t)__builtin_popcount(cmpMask4);
        pState->pReadHead += maskPop4;

        const __m128i newWords5 = _mm_loadu_si128(reinterpret_cast<const __m128i *>(pState->pReadHead));

        const uint32_t maskPop5 = (uint32_t)__builtin_popcount(cmpMask5);
        pState->pReadHead += maskPop5;

        const __m128i newWords6 = _mm_loadu_si128(reinterpret_cast<const __m128i *>(pState->pReadHead));

        const uint32_t maskPop6 = (uint32_t)__builtin_popcount(cmpMask6);
        pState->pReadHead += maskPop6;

        const __m128i newWords7 = _mm_loadu_si128(reinterpret_cast<const __m128i *>(pState->pReadHead));

        const uint32_t maskPop7 = (uint32_t)__builtin_popcount(cmpMask7);
        pState->pReadHead += maskPop7;

        // matching: state << 16
        const simd_t matchShiftedState0 = _mm256_sllv_epi32(state0, _mm256_and_si256(cmp0, _16));
        const simd_t matchShiftedState1 = _mm256_sllv_epi32(state1, _mm256_and_si256(cmp1, _16));
        const simd_t matchShiftedState2 = _mm256_sllv_epi32(state2, _mm256_and_si256(cmp2, _16));
        const simd_t matchShiftedState3 = _mm256_sllv_epi32(state3, _mm256_and_si256(cmp3, _16));
        const simd_t matchShiftedState4 = _mm256_sllv_epi32(state4, _mm256_and_si256(cmp4, _16));
        const simd_t matchShiftedState5 = _mm256_sllv_epi32(state5, _mm256_and_si256(cmp5, _16));
        const simd_t matchShiftedState6 = _mm256_sllv_epi32(state6, _mm256_and_si256(cmp6, _16));
        const simd_t matchShiftedState7 = _mm256_sllv_epi32(state7, _mm256_and_si256(cmp7, _16));

        // shuffle new words in place.
        const __m128i newWordXmm0 = _mm_shuffle_epi8(newWords0, lut0);
        const __m128i newWordXmm1 = _mm_shuffle_epi8(newWords1, lut1);
        const __m128i newWordXmm2 = _mm_shuffle_epi8(newWords2, lut2);
        const __m128i newWordXmm3 = _mm_shuffle_epi8(newWords3, lut3);
        const __m128i newWordXmm4 = _mm_shuffle_epi8(newWords4, lut4);
        const __m128i newWordXmm5 = _mm_shuffle_epi8(newWords5, lut5);
        const __m128i newWordXmm6 = _mm_shuffle_epi8(newWords6, lut6);
        const __m128i newWordXmm7 = _mm_shuffle_epi8(newWords7, lut7);

        // expand new word.
        const __m256i newWord0 = _mm256_cvtepu16_epi32(newWordXmm0);
        const __m256i newWord1 = _mm256_cvtepu16_epi32(newWordXmm1);
        const __m256i newWord2 = _mm256_cvtepu16_epi32(newWordXmm2);
        const __m256i newWord3 = _mm256_cvtepu16_epi32(newWordXmm3);
        const __m256i newWord4 = _mm256_cvtepu16_epi32(newWordXmm4);
        const __m256i newWord5 = _mm256_cvtepu16_epi32(newWordXmm5);
        const __m256i newWord6 = _mm256_cvtepu16_epi32(newWordXmm6);
        const __m256i newWord7 = _mm256_cvtepu16_epi32(newWordXmm7);

        // state = state << 16 | newWord;
        statesX8[0] = _mm256_or_si256(matchShiftedState0, newWord0);
        statesX8[1] = _mm256_or_si256(matchShiftedState1, newWord1);
        statesX8[2] = _mm256_or_si256(matchShiftedState2, newWord2);
        statesX8[3] = _mm256_or_si256(matchShiftedState3, newWord3);
        statesX8[4] = _mm256_or_si256(matchShiftedState4, newWord4);
        statesX8[5] = _mm256_or_si256(matchShiftedState5, newWord5);
        statesX8[6] = _mm256_or_si256(matchShiftedState6, newWord6);
        statesX8[7] = _mm256_or_si256(matchShiftedState7, newWord7);
      }
      else
      {
        // get masks of those compares & start loading shuffle masks.
        const uint32_t cmpMask0 = (uint32_t)_mm256_movemask_ps(_mm256_castsi256_ps(cmp0));
        __m128i lut0 = _mm_lddqu_si128(reinterpret_cast<const __m128i *>(&_ShuffleLutShfl32[cmpMask0 << 3])); // `* 8`.

        const uint32_t cmpMask1 = (uint32_t)_mm256_movemask_ps(_mm256_castsi256_ps(cmp1));
        __m128i lut1 = _mm_lddqu_si128(reinterpret_cast<const __m128i *>(&_ShuffleLutShfl32[cmpMask1 << 3])); // `* 8`.

        const uint32_t cmpMask2 = (uint32_t)_mm256_movemask_ps(_mm256_castsi256_ps(cmp2));
        __m128i lut2 = _mm_lddqu_si128(reinterpret_cast<const __m128i *>(&_ShuffleLutShfl32[cmpMask2 << 3])); // `* 8`.

        const uint32_t cmpMask3 = (uint32_t)_mm256_movemask_ps(_mm256_castsi256_ps(cmp3));
        __m128i lut3 = _mm_lddqu_si128(reinterpret_cast<const __m128i *>(&_ShuffleLutShfl32[cmpMask3 << 3])); // `* 8`.

        const uint32_t cmpMask4 = (uint32_t)_mm256_movemask_ps(_mm256_castsi256_ps(cmp4));
        __m128i lut4 = _mm_lddqu_si128(reinterpret_cast<const __m128i *>(&_ShuffleLutShfl32[cmpMask4 << 3])); // `* 8`.

        const uint32_t cmpMask5 = (uint32_t)_mm256_movemask_ps(_mm256_castsi256_ps(cmp5));
        __m128i lut5 = _mm_lddqu_si128(reinterpret_cast<const __m128i *>(&_ShuffleLutShfl32[cmpMask5 << 3])); // `* 8`.

        const uint32_t cmpMask6 = (uint32_t)_mm256_movemask_ps(_mm256_castsi256_ps(cmp6));
        __m128i lut6 = _mm_lddqu_si128(reinterpret_cast<const __m128i *>(&_ShuffleLutShfl32[cmpMask6 << 3])); // `* 8`.

        const uint32_t cmpMask7 = (uint32_t)_mm256_movemask_ps(_mm256_castsi256_ps(cmp7));
        __m128i lut7 = _mm_lddqu_si128(reinterpret_cast<const __m128i *>(&_ShuffleLutShfl32[cmpMask7 << 3])); // `* 8`.

        // advance read head & read input for blocks 1, 2, 3, 4, 5, 6, 7.
        const uint32_t maskPop0 = (uint32_t)__builtin_popcount(cmpMask0);
        pState->pReadHead += maskPop0;

        const __m128i newWords1 = _mm_loadu_si128(reinterpret_cast<const __m128i *>(pState->pReadHead));

        const uint32_t maskPop1 = (uint32_t)__builtin_popcount(cmpMask1);
        pState->pReadHead += maskPop1;

        const __m128i newWords2 = _mm_loadu_si128(reinterpret_cast<const __m128i *>(pState->pReadHead));

        const uint32_t maskPop2 = (uint32_t)__builtin_popcount(cmpMask2);
        pState->pReadHead += maskPop2;

        const __m128i newWords3 = _mm_loadu_si128(reinterpret_cast<const __m128i *>(pState->pReadHead));

        const uint32_t maskPop3 = (uint32_t)__builtin_popcount(cmpMask3);
        pState->pReadHead += maskPop3;

        const __m128i newWords4 = _mm_loadu_si128(reinterpret_cast<const __m128i *>(pState->pReadHead));

        const uint32_t maskPop4 = (uint32_t)__builtin_popcount(cmpMask4);
        pState->pReadHead += maskPop4;

        const __m128i newWords5 = _mm_loadu_si128(reinterpret_cast<const __m128i *>(pState->pReadHead));

        const uint32_t maskPop5 = (uint32_t)__builtin_popcount(cmpMask5);
        pState->pReadHead += maskPop5;

        const __m128i newWords6 = _mm_loadu_si128(reinterpret_cast<const __m128i *>(pState->pReadHead));

        const uint32_t maskPop6 = (uint32_t)__builtin_popcount(cmpMask6);
        pState->pReadHead += maskPop6;

        const __m128i newWords7 = _mm_loadu_si128(reinterpret_cast<const __m128i *>(pState->pReadHead));

        const uint32_t maskPop7 = (uint32_t)__builtin_popcount(cmpMask7);
        pState->pReadHead += maskPop7;

        // finalize lookups.
        lut0 = _mm_or_si128(_mm_shuffle_epi8(lut0, shuffleDoubleMask), shuffleUpper16Bit);
        lut1 = _mm_or_si128(_mm_shuffle_epi8(lut1, shuffleDoubleMask), shuffleUpper16Bit);
        lut2 = _mm_or_si128(_mm_shuffle_epi8(lut2, shuffleDoubleMask), shuffleUpper16Bit);
        lut3 = _mm_or_si128(_mm_shuffle_epi8(lut3, shuffleDoubleMask), shuffleUpper16Bit);
        lut4 = _mm_or_si128(_mm_shuffle_epi8(lut4, shuffleDoubleMask), shuffleUpper16Bit);
        lut5 = _mm_or_si128(_mm_shuffle_epi8(lut5, shuffleDoubleMask), shuffleUpper16Bit);
        lut6 = _mm_or_si128(_mm_shuffle_epi8(lut6, shuffleDoubleMask), shuffleUpper16Bit);
        lut7 = _mm_or_si128(_mm_shuffle_epi8(lut7, shuffleDoubleMask), shuffleUpper16Bit);

        // matching: state << 16
        const simd_t matchShiftedState0 = _mm256_sllv_epi32(state0, _mm256_and_si256(cmp0, _16));
        const simd_t matchShiftedState1 = _mm256_sllv_epi32(state1, _mm256_and_si256(cmp1, _16));
        const simd_t matchShiftedState2 = _mm256_sllv_epi32(state2, _mm256_and_si256(cmp2, _16));
        const simd_t matchShiftedState3 = _mm256_sllv_epi32(state3, _mm256_and_si256(cmp3, _16));
        const simd_t matchShiftedState4 = _mm256_sllv_epi32(state4, _mm256_and_si256(cmp4, _16));
        const simd_t matchShiftedState5 = _mm256_sllv_epi32(state5, _mm256_and_si256(cmp5, _16));
        const simd_t matchShiftedState6 = _mm256_sllv_epi32(state6, _mm256_and_si256(cmp6, _16));
        const simd_t matchShiftedState7 = _mm256_sllv_epi32(state7, _mm256_and_si256(cmp7, _16));

        // shuffle new words in place.
        const __m128i newWordXmm0 = _mm_shuffle_epi8(newWords0, lut0);
        const __m128i newWordXmm1 = _mm_shuffle_epi8(newWords1, lut1);
        const __m128i newWordXmm2 = _mm_shuffle_epi8(newWords2, lut2);
        const __m128i newWordXmm3 = _mm_shuffle_epi8(newWords3, lut3);
        const __m128i newWordXmm4 = _mm_shuffle_epi8(newWords4, lut4);
        const __m128i newWordXmm5 = _mm_shuffle_epi8(newWords5, lut5);
        const __m128i newWordXmm6 = _mm_shuffle_epi8(newWords6, lut6);
        const __m128i newWordXmm7 = _mm_shuffle_epi8(newWords7, lut7);

        // expand new word.
        const __m256i newWord0 = _mm256_cvtepu16_epi32(newWordXmm0);
        const __m256i newWord1 = _mm256_cvtepu16_epi32(newWordXmm1);
        const __m256i newWord2 = _mm256_cvtepu16_epi32(newWordXmm2);
        const __m256i newWord3 = _mm256_cvtepu16_epi32(newWordXmm3);
        const __m256i newWord4 = _mm256_cvtepu16_epi32(newWordXmm4);
        const __m256i newWord5 = _mm256_cvtepu16_epi32(newWordXmm5);
        const __m256i newWord6 = _mm256_cvtepu16_epi32(newWordXmm6);
        const __m256i newWord7 = _mm256_cvtepu16_epi32(newWordXmm7);

        // state = state << 16 | newWord;
        statesX8[0] = _mm256_or_si256(matchShiftedState0, newWord0);
        statesX8[1] = _mm256_or_si256(matchShiftedState1, newWord1);
        statesX8[2] = _mm256_or_si256(matchShiftedState2, newWord2);
        statesX8[3] = _mm256_or_si256(matchShiftedState3, newWord3);
        statesX8[4] = _mm256_or_si256(matchShiftedState4, newWord4);
        statesX8[5] = _mm256_or_si256(matchShiftedState5, newWord5);
        statesX8[6] = _mm256_or_si256(matchShiftedState6, newWord6);
        statesX8[7] = _mm256_or_si256(matchShiftedState7, newWord7);
      }
    }
  }

  for (size_t j = 0; j < sizeof(statesX8) / sizeof(simd_t); j++)
    _mm256_store_si256(reinterpret_cast<simd_t *>(reinterpret_cast<uint8_t *>(pState->states) + j * sizeof(simd_t)), statesX8[j]);

  return i;
}

template <uint32_t TotalSymbolCountBits, bool WriteAligned32 = false>
#if !defined(_MSC_VER) || defined(__llvm__)
#ifdef __llvm__
#if __clang_major__ >= 16
__attribute__((target("arch=znver4")))
#else
__attribute__((target("avx512vl")))
#endif
#else
__attribute__((target("avx512f", "avx512bw", "avx512vl")))
#endif
#endif
static size_t _block_rans32x64_decode_section_znver4_varC(_rans_decode_state64_t<hist_dec_pack_t<TotalSymbolCountBits>> *pState, uint8_t *pOutData, const size_t startIndex, const size_t endIndex)
{
  if constexpr (!WriteAligned32)
    if ((reinterpret_cast<size_t>(pOutData) & (32 - 1)) == 0)
      return _block_rans32x64_decode_section_znver4_varC<TotalSymbolCountBits, true>(pState, pOutData, startIndex, endIndex);

  constexpr uint32_t TotalSymbolCount = ((uint32_t)1 << TotalSymbolCountBits);
  static_assert(TotalSymbolCountBits < 16);

  typedef __m256i simd_t;
  simd_t statesX8[StateCount / (sizeof(simd_t) / sizeof(uint32_t))];

  for (size_t i = 0; i < sizeof(statesX8) / sizeof(simd_t); i++)
    statesX8[i] = _mm256_load_si256(reinterpret_cast<const simd_t *>(reinterpret_cast<const uint8_t *>(pState->states) + i * sizeof(simd_t)));

  size_t i = startIndex;

  const simd_t symCountMask = _mm256_set1_epi32(TotalSymbolCount - 1);
  
  simd_t lower12;
  
  if constexpr (TotalSymbolCountBits == 12)
    lower12 = symCountMask;
  else
    lower12 = _mm256_set1_epi32((1 << 12) - 1);

  const simd_t lower8 = _mm256_set1_epi32(0xFF);
  const simd_t decodeConsumePoint = _mm256_set1_epi32(DecodeConsumePoint16);

  // Trying to get other compilers as close as possible to clang codegen.

  for (; i < endIndex; i += StateCount)
  {
    // const uint32_t slot = state & (TotalSymbolCount - 1);
    const simd_t slot0 = _mm256_and_si256(statesX8[0], symCountMask);
    const simd_t slot1 = _mm256_and_si256(statesX8[1], symCountMask);
    const simd_t slot2 = _mm256_and_si256(statesX8[2], symCountMask);
    const simd_t slot3 = _mm256_and_si256(statesX8[3], symCountMask);
    const simd_t slot4 = _mm256_and_si256(statesX8[4], symCountMask);
    const simd_t slot5 = _mm256_and_si256(statesX8[5], symCountMask);

    // const uint32_t shiftedState = (state >> TotalSymbolCountBits);
    const simd_t shiftedState4 = _mm256_srli_epi32(statesX8[4], TotalSymbolCountBits);
    const simd_t shiftedState5 = _mm256_srli_epi32(statesX8[5], TotalSymbolCountBits);

    // retrieve pack.
    const simd_t pack0 = _mm256_i32gather_epi32(reinterpret_cast<const int32_t *>(pState->hist.symbol), slot0, sizeof(uint32_t));
    const simd_t pack1 = _mm256_i32gather_epi32(reinterpret_cast<const int32_t *>(pState->hist.symbol), slot1, sizeof(uint32_t));
    const simd_t pack2 = _mm256_i32gather_epi32(reinterpret_cast<const int32_t *>(pState->hist.symbol), slot2, sizeof(uint32_t));

    const simd_t slot6 = _mm256_and_si256(statesX8[6], symCountMask);
    const simd_t shiftedState6 = _mm256_srli_epi32(statesX8[6], TotalSymbolCountBits);

    const simd_t pack3 = _mm256_i32gather_epi32(reinterpret_cast<const int32_t *>(pState->hist.symbol), slot3, sizeof(uint32_t));

    const simd_t slot7 = _mm256_and_si256(statesX8[7], symCountMask);
    const simd_t shiftedState7 = _mm256_srli_epi32(statesX8[7], TotalSymbolCountBits);

    // start unpacking cumul.
    // unpack symbol.
    // unpack freq, cumul.
    const simd_t halfUnpackedCumul0 = _mm256_srli_epi32(pack0, 8);
    const simd_t symbol1 = _mm256_and_si256(pack1, lower8);
    const simd_t halfUnpackedCumul1 = _mm256_srli_epi32(pack1, 8);
    const simd_t freq1 = _mm256_srli_epi32(pack1, 20);

    const simd_t pack4 = _mm256_i32gather_epi32(reinterpret_cast<const int32_t *>(pState->hist.symbol), slot4, sizeof(uint32_t));
    const simd_t pack5 = _mm256_i32gather_epi32(reinterpret_cast<const int32_t *>(pState->hist.symbol), slot5, sizeof(uint32_t));

    const simd_t cumul0 = _mm256_and_si256(halfUnpackedCumul0, lower12);
    const simd_t cumul1 = _mm256_and_si256(halfUnpackedCumul1, lower12);

    // smc = slot - cumul;
    const simd_t smc0 = _mm256_sub_epi32(slot0, cumul0);
    const simd_t smc1 = _mm256_sub_epi32(slot1, cumul1);

    const simd_t symbol2 = _mm256_and_si256(pack2, lower8);

    const simd_t halfUnpackedCumul5 = _mm256_srli_epi32(pack5, 8);
    const simd_t symbol4 = _mm256_and_si256(pack4, lower8);
    const simd_t halfUnpackedCumul4 = _mm256_srli_epi32(pack4, 8);
    const simd_t freq4 = _mm256_srli_epi32(pack4, 20);

    const simd_t pack6 = _mm256_i32gather_epi32(reinterpret_cast<const int32_t *>(pState->hist.symbol), slot6, sizeof(uint32_t));

    const simd_t cumul5 = _mm256_and_si256(halfUnpackedCumul5, lower12);
    const simd_t cumul4 = _mm256_and_si256(halfUnpackedCumul4, lower12);

    const simd_t smc5 = _mm256_sub_epi32(slot5, cumul5);
    const simd_t smc4 = _mm256_sub_epi32(slot4, cumul4);

    const simd_t symbol5 = _mm256_and_si256(pack5, lower8);
    const simd_t freq5 = _mm256_srli_epi32(pack5, 20);

    // const uint32_t freqScaled = shiftedState * freq;
    const __m256i freqScaled5 = _mm256_mullo_epi32(shiftedState5, freq5);

    const simd_t halfUnpackedCumul6 = _mm256_srli_epi32(pack6, 8);
    const simd_t symbol6 = _mm256_and_si256(pack6, lower8);
    const simd_t freq6 = _mm256_srli_epi32(pack6, 20);

    const simd_t pack7 = _mm256_i32gather_epi32(reinterpret_cast<const int32_t *>(pState->hist.symbol), slot7, sizeof(uint32_t));

    const simd_t cumul6 = _mm256_and_si256(halfUnpackedCumul6, lower12);
    const simd_t symbol0 = _mm256_and_si256(pack0, lower8);
    const simd_t freq0 = _mm256_srli_epi32(pack0, 20);

    const simd_t smc6 = _mm256_sub_epi32(slot6, cumul6);
    const simd_t halfUnpackedCumul2 = _mm256_srli_epi32(pack2, 8);

    // pack symbols to one si256.
    const simd_t symPack45 = _mm256_packus_epi32(symbol4, symbol5);
    
    const simd_t shiftedState2 = _mm256_srli_epi32(statesX8[2], TotalSymbolCountBits); // not sure if this one. (-64).
    const simd_t freq2 = _mm256_srli_epi32(pack2, 20);
    const simd_t cumul2 = _mm256_and_si256(halfUnpackedCumul2, lower12);
    const simd_t halfUnpackedCumul3 = _mm256_srli_epi32(pack3, 8);

    const __m256i freqScaled2 = _mm256_mullo_epi32(shiftedState2, freq2);

    const simd_t cumul3 = _mm256_and_si256(halfUnpackedCumul3, lower12);
    const simd_t smc2 = _mm256_sub_epi32(slot2, cumul2);
    const simd_t symbol3 = _mm256_and_si256(pack3, lower8);
    const simd_t freq3 = _mm256_srli_epi32(pack3, 20);
    const simd_t smc3 = _mm256_sub_epi32(slot3, cumul3);
    const simd_t halfUnpackedCumul7 = _mm256_srli_epi32(pack7, 8);

    // state = freqScaled + smc;
    const simd_t state2 = _mm256_add_epi32(freqScaled2, smc2);

    const simd_t cumul7 = _mm256_and_si256(halfUnpackedCumul7, lower12);

    // (state < DecodeConsumePoint16) ? -1 : 0 | well, actually (DecodeConsumePoint16 > state) ? -1 : 0
    const __mmask8 cmp2 = _mm256_cmpgt_epi32_mask(decodeConsumePoint, state2);

    const simd_t smc7 = _mm256_sub_epi32(slot7, cumul7);

    const simd_t symPack23 = _mm256_packus_epi32(symbol2, symbol3); // this is not quite right but implies that the gathers are not in order...

    const simd_t symbol7 = _mm256_and_si256(pack7, lower8);
    const simd_t shiftedState1 = _mm256_srli_epi32(statesX8[1], TotalSymbolCountBits); // not sure if this one. (-96).
    const simd_t freq7 = _mm256_srli_epi32(pack7, 20);

    // matching: state << 16
    const simd_t matchShiftedState2 = _mm256_mask_slli_epi32(state2, cmp2, state2, 16);

    const simd_t symPack01 = _mm256_packus_epi32(symbol0, symbol1);
    const simd_t symPack0123 = _mm256_packus_epi16(symPack01, symPack23); // `00 01 02 03 08 09 0A 0B 10 11 12 13 18 19 1A 1B 04 05 06 07 0C 0D 0E 0F 14 15 16 17 1C 1D 1E 1F`

    const simd_t shiftedState3 = _mm256_srli_epi32(statesX8[3], TotalSymbolCountBits); // not sure if this one. (-32).

    const __m256i freqScaled1 = _mm256_mullo_epi32(shiftedState1, freq1);

    // We intentionally encoded in a way to not have to do horrible things here.
    if constexpr (WriteAligned32)
      _mm256_stream_si256(reinterpret_cast<__m256i *>(pOutData + i), symPack0123);
    else
      _mm256_storeu_si256(reinterpret_cast<__m256i *>(pOutData + i), symPack0123);

    const simd_t symPack67 = _mm256_packus_epi32(symbol6, symbol7);
    const simd_t shiftedState0 = _mm256_srli_epi32(statesX8[0], TotalSymbolCountBits); // not sure if this one. (-128).

    const simd_t symPack4567 = _mm256_packus_epi16(symPack45, symPack67); // `00 01 02 03 08 09 0A 0B 10 11 12 13 18 19 1A 1B 04 05 06 07 0C 0D 0E 0F 14 15 16 17 1C 1D 1E 1F`

    if constexpr (WriteAligned32)
      _mm256_stream_si256(reinterpret_cast<__m256i *>(pOutData + i + StateCount / 2), symPack4567);
    else
      _mm256_storeu_si256(reinterpret_cast<__m256i *>(pOutData + i + StateCount / 2), symPack4567);

    const __m256i freqScaled0 = _mm256_mullo_epi32(shiftedState0, freq0);
    const __m256i freqScaled3 = _mm256_mullo_epi32(shiftedState3, freq3);

    const simd_t state1 = _mm256_add_epi32(freqScaled1, smc1);
    const __mmask8 cmp1 = _mm256_cmpgt_epi32_mask(decodeConsumePoint, state1);

    const __m256i freqScaled4 = _mm256_mullo_epi32(shiftedState4, freq4);
    const __m256i freqScaled6 = _mm256_mullo_epi32(shiftedState6, freq6);

    const simd_t state0 = _mm256_add_epi32(freqScaled0, smc0);
    const simd_t state3 = _mm256_add_epi32(freqScaled3, smc3);

    const __mmask8 cmp0 = _mm256_cmpgt_epi32_mask(decodeConsumePoint, state0);

    const __m256i freqScaled7 = _mm256_mullo_epi32(shiftedState7, freq7);

    const __mmask8 cmp3 = _mm256_cmpgt_epi32_mask(decodeConsumePoint, state3);

    const simd_t state4 = _mm256_add_epi32(freqScaled4, smc4);
    const simd_t state5 = _mm256_add_epi32(freqScaled5, smc5);
    const simd_t state6 = _mm256_add_epi32(freqScaled6, smc6);

    const __mmask8 cmp4 = _mm256_cmpgt_epi32_mask(decodeConsumePoint, state4);

    // read input for blocks 0.
    const __m128i newWords0 = _mm_loadu_si128(reinterpret_cast<const __m128i *>(pState->pReadHead));

    const __mmask8 cmp5 = _mm256_cmpgt_epi32_mask(decodeConsumePoint, state5);

    const simd_t state7 = _mm256_add_epi32(freqScaled7, smc7);

    const __mmask8 cmp6 = _mm256_cmpgt_epi32_mask(decodeConsumePoint, state6);

    // advance read head & read input for blocks 1, 2, 3, 4, 5, 6, 7.
    const uint32_t maskPop0 = (uint32_t)__builtin_popcount(cmp0);

    const __mmask8 cmp7 = _mm256_cmpgt_epi32_mask(decodeConsumePoint, state7);

    // get index from mask.
    const uint64_t cmpMaskIdx0 = cmp0 << 4; // `* 16`.

    const simd_t matchShiftedState0 = _mm256_mask_slli_epi32(state0, cmp0, state0, 16);
    const simd_t matchShiftedState1 = _mm256_mask_slli_epi32(state1, cmp1, state1, 16);
    const simd_t matchShiftedState3 = _mm256_mask_slli_epi32(state3, cmp3, state3, 16);

    pState->pReadHead += maskPop0;
    const __m128i newWords1 = _mm_loadu_si128(reinterpret_cast<const __m128i *>(pState->pReadHead));

    const uint32_t maskPop1 = (uint32_t)__builtin_popcount(cmp1);
    const uint64_t cmpMaskIdx1 = cmp1 << 4; // `* 16`.

    const simd_t matchShiftedState4 = _mm256_mask_slli_epi32(state4, cmp4, state4, 16);
    const simd_t matchShiftedState5 = _mm256_mask_slli_epi32(state5, cmp5, state5, 16);
    const simd_t matchShiftedState6 = _mm256_mask_slli_epi32(state6, cmp6, state6, 16);

    const uint64_t cmpMaskIdx2 = cmp2 << 4; // `* 16`.

    const simd_t matchShiftedState7 = _mm256_mask_slli_epi32(state7, cmp7, state7, 16);

    pState->pReadHead += maskPop1;
    const __m128i newWords2 = _mm_loadu_si128(reinterpret_cast<const __m128i *>(pState->pReadHead));

    const uint32_t maskPop2 = (uint32_t)__builtin_popcount(cmp2);

    pState->pReadHead += maskPop2;
    const __m128i newWords3 = _mm_loadu_si128(reinterpret_cast<const __m128i *>(pState->pReadHead));

    const uint32_t maskPop3 = (uint32_t)__builtin_popcount(cmp3);
    const uint64_t cmpMaskIdx3 = cmp3 << 4; // `* 16`.

    pState->pReadHead += maskPop3;
    const __m128i newWords4 = _mm_loadu_si128(reinterpret_cast<const __m128i *>(pState->pReadHead));

    const uint32_t maskPop4 = (uint32_t)__builtin_popcount(cmp4);
    const uint64_t cmpMaskIdx4 = cmp4 << 4; // `* 16`.

    pState->pReadHead += maskPop4;
    const __m128i newWords5 = _mm_loadu_si128(reinterpret_cast<const __m128i *>(pState->pReadHead));

    const uint32_t maskPop5 = (uint32_t)__builtin_popcount(cmp5);
    const uint64_t cmpMaskIdx5 = cmp5 << 4; // `* 16`.

    pState->pReadHead += maskPop5;
    const __m128i newWords6 = _mm_loadu_si128(reinterpret_cast<const __m128i *>(pState->pReadHead));

    const uint32_t maskPop6 = (uint32_t)__builtin_popcount(cmp6);
    const uint64_t cmpMaskIdx6 = cmp6 << 4; // `* 16`.

    pState->pReadHead += maskPop6;
    const __m128i newWords7 = _mm_loadu_si128(reinterpret_cast<const __m128i *>(pState->pReadHead));

    const uint32_t maskPop7 = (uint32_t)__builtin_popcount(cmp7);
    const uint64_t cmpMaskIdx7 = cmp7 << 4; // `* 16`.

    pState->pReadHead += maskPop7;

    // get masks of those compares & start loading shuffle masks.
    // shuffle new words in place.
    const __m128i lut0 = _mm_load_si128(reinterpret_cast<const __m128i *>(&_DoubleShuffleLutShfl32[cmpMaskIdx0]));
    const __m128i newWordXmm0 = _mm_shuffle_epi8(newWords0, lut0);

    const __m128i lut1 = _mm_load_si128(reinterpret_cast<const __m128i *>(&_DoubleShuffleLutShfl32[cmpMaskIdx1]));
    const __m128i newWordXmm1 = _mm_shuffle_epi8(newWords1, lut1);

    const __m128i lut2 = _mm_load_si128(reinterpret_cast<const __m128i *>(&_DoubleShuffleLutShfl32[cmpMaskIdx2]));
    const __m128i newWordXmm2 = _mm_shuffle_epi8(newWords2, lut2);

    const __m128i lut3 = _mm_load_si128(reinterpret_cast<const __m128i *>(&_DoubleShuffleLutShfl32[cmpMaskIdx3]));
    const __m128i newWordXmm3 = _mm_shuffle_epi8(newWords3, lut3);

    const __m128i lut4 = _mm_load_si128(reinterpret_cast<const __m128i *>(&_DoubleShuffleLutShfl32[cmpMaskIdx4]));
    const __m128i newWordXmm4 = _mm_shuffle_epi8(newWords4, lut4);

    const __m128i lut5 = _mm_load_si128(reinterpret_cast<const __m128i *>(&_DoubleShuffleLutShfl32[cmpMaskIdx5]));
    const __m128i newWordXmm5 = _mm_shuffle_epi8(newWords5, lut5);

    const __m128i lut6 = _mm_load_si128(reinterpret_cast<const __m128i *>(&_DoubleShuffleLutShfl32[cmpMaskIdx6]));
    const __m128i newWordXmm6 = _mm_shuffle_epi8(newWords6, lut6);

    const __m128i lut7 = _mm_load_si128(reinterpret_cast<const __m128i *>(&_DoubleShuffleLutShfl32[cmpMaskIdx7]));
    const __m128i newWordXmm7 = _mm_shuffle_epi8(newWords7, lut7);

    // expand new word.
    const __m256i newWord0 = _mm256_cvtepu16_epi32(newWordXmm0);
    const __m256i newWord1 = _mm256_cvtepu16_epi32(newWordXmm1);
    const __m256i newWord2 = _mm256_cvtepu16_epi32(newWordXmm2);
    const __m256i newWord3 = _mm256_cvtepu16_epi32(newWordXmm3);

    // state = state << 16 | newWord;
    statesX8[0] = _mm256_or_si256(matchShiftedState0, newWord0);
    statesX8[1] = _mm256_or_si256(matchShiftedState1, newWord1);

    const __m256i newWord4 = _mm256_cvtepu16_epi32(newWordXmm4);
    const __m256i newWord5 = _mm256_cvtepu16_epi32(newWordXmm5);

    statesX8[2] = _mm256_or_si256(matchShiftedState2, newWord2);
    statesX8[3] = _mm256_or_si256(matchShiftedState3, newWord3);
    statesX8[4] = _mm256_or_si256(matchShiftedState4, newWord4);
    statesX8[5] = _mm256_or_si256(matchShiftedState5, newWord5);

    const __m256i newWord6 = _mm256_cvtepu16_epi32(newWordXmm6);
    const __m256i newWord7 = _mm256_cvtepu16_epi32(newWordXmm7);

    statesX8[6] = _mm256_or_si256(matchShiftedState6, newWord6);
    statesX8[7] = _mm256_or_si256(matchShiftedState7, newWord7);
  }

  for (size_t j = 0; j < sizeof(statesX8) / sizeof(simd_t); j++)
    _mm256_store_si256(reinterpret_cast<simd_t *>(reinterpret_cast<uint8_t *>(pState->states) + j * sizeof(simd_t)), statesX8[j]);

  return i;
}

template <uint32_t TotalSymbolCountBits, bool ShuffleMask16, bool YmmShuffle, bool WriteAligned64 = false>
#if !defined(_MSC_VER) || defined(__llvm__)
#ifdef __llvm__
__attribute__((target("avx512bw")))
#else
__attribute__((target("avx512f", "avx512bw")))
#endif
#endif
static size_t _block_rans32x64_decode_section_avx512_varA(_rans_decode_state64_t<hist_dec2_t<TotalSymbolCountBits>> *pState, uint8_t *pOutData, const size_t startIndex, const size_t endIndex)
{
  if constexpr (!WriteAligned64)
    if ((reinterpret_cast<size_t>(pOutData) & (64 - 1)) == 0)
      return _block_rans32x64_decode_section_avx512_varA<TotalSymbolCountBits, ShuffleMask16, YmmShuffle, true>(pState, pOutData, startIndex, endIndex);

  constexpr uint32_t TotalSymbolCount = ((uint32_t)1 << TotalSymbolCountBits);
  static_assert(TotalSymbolCountBits < 16);

  typedef __m512i simd_t;
  simd_t statesX8[StateCount / (sizeof(simd_t) / sizeof(uint32_t))];

  for (size_t i = 0; i < sizeof(statesX8) / sizeof(simd_t); i++)
    statesX8[i] = _mm512_loadu_si512(reinterpret_cast<const simd_t *>(reinterpret_cast<const uint8_t *>(pState->states) + i * sizeof(simd_t)));

  size_t i = startIndex;

  const simd_t symCountMask = _mm512_set1_epi32(TotalSymbolCount - 1);
  const simd_t lower16 = _mm512_set1_epi32(0xFFFF);
  const simd_t lower8 = _mm512_set1_epi32(0xFF);
  const simd_t decodeConsumePoint = _mm512_set1_epi32(DecodeConsumePoint16);
  const simd_t symbolPermuteMask = _mm512_set_epi32(15, 7, 14, 6, 11, 3, 10, 2, 13, 5, 12, 4, 9, 1, 8, 0);
  const __m128i shuffleDoubleMask = _mm_set_epi8(7, 7, 6, 6, 5, 5, 4, 4, 3, 3, 2, 2, 1, 1, 0, 0);
  const __m128i shuffleUpper16Bit = _mm_set1_epi16(0x0100);

  for (; i < endIndex; i += StateCount)
  {
    // const uint32_t slot = state & (TotalSymbolCount - 1);
    const simd_t slot0 = _mm512_and_si512(statesX8[0], symCountMask);
    const simd_t slot1 = _mm512_and_si512(statesX8[1], symCountMask);
    const simd_t slot2 = _mm512_and_si512(statesX8[2], symCountMask);
    const simd_t slot3 = _mm512_and_si512(statesX8[3], symCountMask);

    // retrieve pack.
    simd_t symbol0 = _mm512_i32gather_epi32(slot0, reinterpret_cast<const int32_t *>(pState->hist.cumulInv), sizeof(uint8_t));
    simd_t symbol1 = _mm512_i32gather_epi32(slot1, reinterpret_cast<const int32_t *>(pState->hist.cumulInv), sizeof(uint8_t));
    simd_t symbol2 = _mm512_i32gather_epi32(slot2, reinterpret_cast<const int32_t *>(pState->hist.cumulInv), sizeof(uint8_t));
    simd_t symbol3 = _mm512_i32gather_epi32(slot3, reinterpret_cast<const int32_t *>(pState->hist.cumulInv), sizeof(uint8_t));

    // const uint32_t shiftedState = (state >> TotalSymbolCountBits);
    const simd_t shiftedState0 = _mm512_srli_epi32(statesX8[0], TotalSymbolCountBits);
    const simd_t shiftedState1 = _mm512_srli_epi32(statesX8[1], TotalSymbolCountBits);
    const simd_t shiftedState2 = _mm512_srli_epi32(statesX8[2], TotalSymbolCountBits);
    const simd_t shiftedState3 = _mm512_srli_epi32(statesX8[3], TotalSymbolCountBits);

    symbol0 = _mm512_and_si512(symbol0, lower8);
    symbol1 = _mm512_and_si512(symbol1, lower8);
    symbol2 = _mm512_and_si512(symbol2, lower8);
    symbol3 = _mm512_and_si512(symbol3, lower8);

    // retrieve pack.
    const simd_t pack0 = _mm512_i32gather_epi32(symbol0, reinterpret_cast<const int32_t *>(pState->hist.symbols), sizeof(uint32_t));
    const simd_t pack1 = _mm512_i32gather_epi32(symbol1, reinterpret_cast<const int32_t *>(pState->hist.symbols), sizeof(uint32_t));
    const simd_t pack2 = _mm512_i32gather_epi32(symbol2, reinterpret_cast<const int32_t *>(pState->hist.symbols), sizeof(uint32_t));
    const simd_t pack3 = _mm512_i32gather_epi32(symbol3, reinterpret_cast<const int32_t *>(pState->hist.symbols), sizeof(uint32_t));

    // pack symbols to one si512.
    const simd_t symPack01 = _mm512_packus_epi32(symbol0, symbol1);
    const simd_t symPack23 = _mm512_packus_epi32(symbol2, symbol3);
    const simd_t symPack0123 = _mm512_packus_epi16(symPack01, symPack23); // only god knows how this is packed now.
    const simd_t symPackCompat = _mm512_permutexvar_epi32(symbolPermuteMask, symPack0123); // we could get rid of this if we'd chose to reorder everything fittingly.

    // freq, cumul.
    const simd_t cumul0 = _mm512_srli_epi32(pack0, 16);
    const simd_t freq0 = _mm512_and_si512(pack0, lower16);
    const simd_t cumul1 = _mm512_srli_epi32(pack1, 16);
    const simd_t freq1 = _mm512_and_si512(pack1, lower16);
    const simd_t cumul2 = _mm512_srli_epi32(pack2, 16);
    const simd_t freq2 = _mm512_and_si512(pack2, lower16);
    const simd_t cumul3 = _mm512_srli_epi32(pack3, 16);
    const simd_t freq3 = _mm512_and_si512(pack3, lower16);

    // We intentionally encoded in a way to not have to do horrible things here.
    if constexpr (WriteAligned64)
      _mm512_stream_si512(reinterpret_cast<simd_t *>(pOutData + i), symPackCompat);
    else
      _mm512_storeu_si512(reinterpret_cast<simd_t *>(pOutData + i), symPackCompat);

    // const uint32_t freqScaled = shiftedState * freq;
    const simd_t freqScaled0 = _mm512_mullo_epi32(shiftedState0, freq0);
    const simd_t freqScaled1 = _mm512_mullo_epi32(shiftedState1, freq1);
    const simd_t freqScaled2 = _mm512_mullo_epi32(shiftedState2, freq2);
    const simd_t freqScaled3 = _mm512_mullo_epi32(shiftedState3, freq3);

    // state = freqScaled + slot - cumul;
    const simd_t state0 = _mm512_add_epi32(freqScaled0, _mm512_sub_epi32(slot0, cumul0));
    const simd_t state1 = _mm512_add_epi32(freqScaled1, _mm512_sub_epi32(slot1, cumul1));
    const simd_t state2 = _mm512_add_epi32(freqScaled2, _mm512_sub_epi32(slot2, cumul2));
    const simd_t state3 = _mm512_add_epi32(freqScaled3, _mm512_sub_epi32(slot3, cumul3));

    // now to the messy part...
    {
      // read input for blocks 0.
      const __m128i newWords0a = _mm_loadu_si128(reinterpret_cast<const __m128i *>(pState->pReadHead));

      // (state < DecodeConsumePoint16) ? -1 : 0 | well, actually (DecodeConsumePoint16 > state) ? -1 : 0
      const __mmask16 cmpMask0 = _mm512_cmpgt_epi32_mask(decodeConsumePoint, state0);
      const __mmask16 cmpMask1 = _mm512_cmpgt_epi32_mask(decodeConsumePoint, state1);
      const __mmask16 cmpMask2 = _mm512_cmpgt_epi32_mask(decodeConsumePoint, state2);
      const __mmask16 cmpMask3 = _mm512_cmpgt_epi32_mask(decodeConsumePoint, state3);

      if constexpr (ShuffleMask16)
      {
        // get masks of those compares & start loading shuffle masks.
        const uint32_t cmpMask0a = cmpMask0 & 0xFF;
        const uint32_t cmpMask0b = cmpMask0 >> 8;
        __m128i lut0a = _mm_load_si128(reinterpret_cast<const __m128i *>(&_DoubleShuffleLutShfl32[cmpMask0a << 4])); // `* 16`.
        __m128i lut0b = _mm_load_si128(reinterpret_cast<const __m128i *>(&_DoubleShuffleLutShfl32[cmpMask0b << 4])); // `* 16`.

        const uint32_t cmpMask1a = cmpMask1 & 0xFF;
        const uint32_t cmpMask1b = cmpMask1 >> 8;
        __m128i lut1a = _mm_load_si128(reinterpret_cast<const __m128i *>(&_DoubleShuffleLutShfl32[cmpMask1a << 4])); // `* 16`.
        __m128i lut1b = _mm_load_si128(reinterpret_cast<const __m128i *>(&_DoubleShuffleLutShfl32[cmpMask1b << 4])); // `* 16`.

        const uint32_t cmpMask2a = cmpMask2 & 0xFF;
        const uint32_t cmpMask2b = cmpMask2 >> 8;
        __m128i lut2a = _mm_load_si128(reinterpret_cast<const __m128i *>(&_DoubleShuffleLutShfl32[cmpMask2a << 4])); // `* 16`.
        __m128i lut2b = _mm_load_si128(reinterpret_cast<const __m128i *>(&_DoubleShuffleLutShfl32[cmpMask2b << 4])); // `* 16`.

        const uint32_t cmpMask3a = cmpMask3 & 0xFF;
        const uint32_t cmpMask3b = cmpMask3 >> 8;
        __m128i lut3a = _mm_load_si128(reinterpret_cast<const __m128i *>(&_DoubleShuffleLutShfl32[cmpMask3a << 4])); // `* 16`.
        __m128i lut3b = _mm_load_si128(reinterpret_cast<const __m128i *>(&_DoubleShuffleLutShfl32[cmpMask3b << 4])); // `* 16`.

        // advance read head & read input for blocks 1, 2, 3, 4, 5, 6, 7.
        const uint32_t maskPop0a = (uint32_t)__builtin_popcount(cmpMask0a);
        pState->pReadHead += maskPop0a;

        const __m128i newWords0b = _mm_loadu_si128(reinterpret_cast<const __m128i *>(pState->pReadHead));

        const uint32_t maskPop0b = (uint32_t)__builtin_popcount(cmpMask0b);
        pState->pReadHead += maskPop0b;

        const __m128i newWords1a = _mm_loadu_si128(reinterpret_cast<const __m128i *>(pState->pReadHead));

        const uint32_t maskPop1a = (uint32_t)__builtin_popcount(cmpMask1a);
        pState->pReadHead += maskPop1a;

        const __m128i newWords1b = _mm_loadu_si128(reinterpret_cast<const __m128i *>(pState->pReadHead));

        const uint32_t maskPop1b = (uint32_t)__builtin_popcount(cmpMask1b);
        pState->pReadHead += maskPop1b;

        const __m128i newWords2a = _mm_loadu_si128(reinterpret_cast<const __m128i *>(pState->pReadHead));

        const uint32_t maskPop2a = (uint32_t)__builtin_popcount(cmpMask2a);
        pState->pReadHead += maskPop2a;

        const __m128i newWords2b = _mm_loadu_si128(reinterpret_cast<const __m128i *>(pState->pReadHead));

        const uint32_t maskPop2b = (uint32_t)__builtin_popcount(cmpMask2b);
        pState->pReadHead += maskPop2b;

        const __m128i newWords3a = _mm_loadu_si128(reinterpret_cast<const __m128i *>(pState->pReadHead));

        const uint32_t maskPop3a = (uint32_t)__builtin_popcount(cmpMask3a);
        pState->pReadHead += maskPop3a;

        const __m128i newWords3b = _mm_loadu_si128(reinterpret_cast<const __m128i *>(pState->pReadHead));

        const uint32_t maskPop3b = (uint32_t)__builtin_popcount(cmpMask3b);
        pState->pReadHead += maskPop3b;

        // matching: state << 16
        const simd_t matchShiftedState0 = _mm512_mask_slli_epi32(state0, cmpMask0, state0, 16);
        const simd_t matchShiftedState1 = _mm512_mask_slli_epi32(state1, cmpMask1, state1, 16);
        const simd_t matchShiftedState2 = _mm512_mask_slli_epi32(state2, cmpMask2, state2, 16);
        const simd_t matchShiftedState3 = _mm512_mask_slli_epi32(state3, cmpMask3, state3, 16);

        if constexpr (YmmShuffle)
        {
          // shuffle new words in place.
          const __m256i newWordXmm0 = _mm256_shuffle_epi8(_mm256_set_m128i(newWords0b, newWords0a), _mm256_set_m128i(lut0b, lut0a));
          const __m256i newWordXmm1 = _mm256_shuffle_epi8(_mm256_set_m128i(newWords1b, newWords1a), _mm256_set_m128i(lut1b, lut1a));
          const __m256i newWordXmm2 = _mm256_shuffle_epi8(_mm256_set_m128i(newWords2b, newWords2a), _mm256_set_m128i(lut2b, lut2a));
          const __m256i newWordXmm3 = _mm256_shuffle_epi8(_mm256_set_m128i(newWords3b, newWords3a), _mm256_set_m128i(lut3b, lut3a));

          // expand new word.
          const simd_t newWord0 = _mm512_cvtepu16_epi32(newWordXmm0);
          const simd_t newWord1 = _mm512_cvtepu16_epi32(newWordXmm1);
          const simd_t newWord2 = _mm512_cvtepu16_epi32(newWordXmm2);
          const simd_t newWord3 = _mm512_cvtepu16_epi32(newWordXmm3);

          // state = state << 16 | newWord;
          statesX8[0] = _mm512_or_si512(matchShiftedState0, newWord0);
          statesX8[1] = _mm512_or_si512(matchShiftedState1, newWord1);
          statesX8[2] = _mm512_or_si512(matchShiftedState2, newWord2);
          statesX8[3] = _mm512_or_si512(matchShiftedState3, newWord3);
        }
        else
        {
          // shuffle new words in place.
          const __m128i newWordXmm0a = _mm_shuffle_epi8(newWords0a, lut0a);
          const __m128i newWordXmm0b = _mm_shuffle_epi8(newWords0b, lut0b);
          const __m128i newWordXmm1a = _mm_shuffle_epi8(newWords1a, lut1a);
          const __m128i newWordXmm1b = _mm_shuffle_epi8(newWords1b, lut1b);
          const __m128i newWordXmm2a = _mm_shuffle_epi8(newWords2a, lut2a);
          const __m128i newWordXmm2b = _mm_shuffle_epi8(newWords2b, lut2b);
          const __m128i newWordXmm3a = _mm_shuffle_epi8(newWords3a, lut3a);
          const __m128i newWordXmm3b = _mm_shuffle_epi8(newWords3b, lut3b);

          // expand new word.
          const simd_t newWord0 = _mm512_cvtepu16_epi32(_mm256_set_m128i(newWordXmm0b, newWordXmm0a));
          const simd_t newWord1 = _mm512_cvtepu16_epi32(_mm256_set_m128i(newWordXmm1b, newWordXmm1a));
          const simd_t newWord2 = _mm512_cvtepu16_epi32(_mm256_set_m128i(newWordXmm2b, newWordXmm2a));
          const simd_t newWord3 = _mm512_cvtepu16_epi32(_mm256_set_m128i(newWordXmm3b, newWordXmm3a));

          // state = state << 16 | newWord;
          statesX8[0] = _mm512_or_si512(matchShiftedState0, newWord0);
          statesX8[1] = _mm512_or_si512(matchShiftedState1, newWord1);
          statesX8[2] = _mm512_or_si512(matchShiftedState2, newWord2);
          statesX8[3] = _mm512_or_si512(matchShiftedState3, newWord3);
        }
      }
      else
      {
        // get masks of those compares & start loading shuffle masks.
        const uint32_t cmpMask0a = cmpMask0 & 0xFF;
        const uint32_t cmpMask0b = cmpMask0 >> 8;
        __m128i lut0a = _mm_lddqu_si128(reinterpret_cast<const __m128i *>(&_ShuffleLutShfl32[cmpMask0a << 3])); // `* 8`.
        __m128i lut0b = _mm_lddqu_si128(reinterpret_cast<const __m128i *>(&_ShuffleLutShfl32[cmpMask0b << 3])); // `* 8`.

        const uint32_t cmpMask1a = cmpMask1 & 0xFF;
        const uint32_t cmpMask1b = cmpMask1 >> 8;
        __m128i lut1a = _mm_lddqu_si128(reinterpret_cast<const __m128i *>(&_ShuffleLutShfl32[cmpMask1a << 3])); // `* 8`.
        __m128i lut1b = _mm_lddqu_si128(reinterpret_cast<const __m128i *>(&_ShuffleLutShfl32[cmpMask1b << 3])); // `* 8`.

        const uint32_t cmpMask2a = cmpMask2 & 0xFF;
        const uint32_t cmpMask2b = cmpMask2 >> 8;
        __m128i lut2a = _mm_lddqu_si128(reinterpret_cast<const __m128i *>(&_ShuffleLutShfl32[cmpMask2a << 3])); // `* 8`.
        __m128i lut2b = _mm_lddqu_si128(reinterpret_cast<const __m128i *>(&_ShuffleLutShfl32[cmpMask2b << 3])); // `* 8`.

        const uint32_t cmpMask3a = cmpMask3 & 0xFF;
        const uint32_t cmpMask3b = cmpMask3 >> 8;
        __m128i lut3a = _mm_lddqu_si128(reinterpret_cast<const __m128i *>(&_ShuffleLutShfl32[cmpMask3a << 3])); // `* 8`.
        __m128i lut3b = _mm_lddqu_si128(reinterpret_cast<const __m128i *>(&_ShuffleLutShfl32[cmpMask3b << 3])); // `* 8`.

        // advance read head & read input for blocks 1, 2, 3, 4, 5, 6, 7.
        const uint32_t maskPop0a = (uint32_t)__builtin_popcount(cmpMask0a);
        pState->pReadHead += maskPop0a;

        const __m128i newWords0b = _mm_loadu_si128(reinterpret_cast<const __m128i *>(pState->pReadHead));

        const uint32_t maskPop0b = (uint32_t)__builtin_popcount(cmpMask0b);
        pState->pReadHead += maskPop0b;

        const __m128i newWords1a = _mm_loadu_si128(reinterpret_cast<const __m128i *>(pState->pReadHead));

        const uint32_t maskPop1a = (uint32_t)__builtin_popcount(cmpMask1a);
        pState->pReadHead += maskPop1a;

        const __m128i newWords1b = _mm_loadu_si128(reinterpret_cast<const __m128i *>(pState->pReadHead));

        const uint32_t maskPop1b = (uint32_t)__builtin_popcount(cmpMask1b);
        pState->pReadHead += maskPop1b;

        const __m128i newWords2a = _mm_loadu_si128(reinterpret_cast<const __m128i *>(pState->pReadHead));

        const uint32_t maskPop2a = (uint32_t)__builtin_popcount(cmpMask2a);
        pState->pReadHead += maskPop2a;

        const __m128i newWords2b = _mm_loadu_si128(reinterpret_cast<const __m128i *>(pState->pReadHead));

        const uint32_t maskPop2b = (uint32_t)__builtin_popcount(cmpMask2b);
        pState->pReadHead += maskPop2b;

        const __m128i newWords3a = _mm_loadu_si128(reinterpret_cast<const __m128i *>(pState->pReadHead));

        const uint32_t maskPop3a = (uint32_t)__builtin_popcount(cmpMask3a);
        pState->pReadHead += maskPop3a;

        const __m128i newWords3b = _mm_loadu_si128(reinterpret_cast<const __m128i *>(pState->pReadHead));

        const uint32_t maskPop3b = (uint32_t)__builtin_popcount(cmpMask3b);
        pState->pReadHead += maskPop3b;

        // finalize lookups.
        lut0a = _mm_or_si128(_mm_shuffle_epi8(lut0a, shuffleDoubleMask), shuffleUpper16Bit);
        lut0b = _mm_or_si128(_mm_shuffle_epi8(lut0b, shuffleDoubleMask), shuffleUpper16Bit);
        lut1a = _mm_or_si128(_mm_shuffle_epi8(lut1a, shuffleDoubleMask), shuffleUpper16Bit);
        lut1b = _mm_or_si128(_mm_shuffle_epi8(lut1b, shuffleDoubleMask), shuffleUpper16Bit);
        lut2a = _mm_or_si128(_mm_shuffle_epi8(lut2a, shuffleDoubleMask), shuffleUpper16Bit);
        lut2b = _mm_or_si128(_mm_shuffle_epi8(lut2b, shuffleDoubleMask), shuffleUpper16Bit);
        lut3a = _mm_or_si128(_mm_shuffle_epi8(lut3a, shuffleDoubleMask), shuffleUpper16Bit);
        lut3b = _mm_or_si128(_mm_shuffle_epi8(lut3b, shuffleDoubleMask), shuffleUpper16Bit);

        // matching: state << 16
        const simd_t matchShiftedState0 = _mm512_mask_slli_epi32(state0, cmpMask0, state0, 16);
        const simd_t matchShiftedState1 = _mm512_mask_slli_epi32(state1, cmpMask1, state1, 16);
        const simd_t matchShiftedState2 = _mm512_mask_slli_epi32(state2, cmpMask2, state2, 16);
        const simd_t matchShiftedState3 = _mm512_mask_slli_epi32(state3, cmpMask3, state3, 16);

        if constexpr (YmmShuffle)
        {
          // shuffle new words in place.
          const __m256i newWordXmm0 = _mm256_shuffle_epi8(_mm256_set_m128i(newWords0b, newWords0a), _mm256_set_m128i(lut0b, lut0a));
          const __m256i newWordXmm1 = _mm256_shuffle_epi8(_mm256_set_m128i(newWords1b, newWords1a), _mm256_set_m128i(lut1b, lut1a));
          const __m256i newWordXmm2 = _mm256_shuffle_epi8(_mm256_set_m128i(newWords2b, newWords2a), _mm256_set_m128i(lut2b, lut2a));
          const __m256i newWordXmm3 = _mm256_shuffle_epi8(_mm256_set_m128i(newWords3b, newWords3a), _mm256_set_m128i(lut3b, lut3a));

          // expand new word.
          const simd_t newWord0 = _mm512_cvtepu16_epi32(newWordXmm0);
          const simd_t newWord1 = _mm512_cvtepu16_epi32(newWordXmm1);
          const simd_t newWord2 = _mm512_cvtepu16_epi32(newWordXmm2);
          const simd_t newWord3 = _mm512_cvtepu16_epi32(newWordXmm3);

          // state = state << 16 | newWord;
          statesX8[0] = _mm512_or_si512(matchShiftedState0, newWord0);
          statesX8[1] = _mm512_or_si512(matchShiftedState1, newWord1);
          statesX8[2] = _mm512_or_si512(matchShiftedState2, newWord2);
          statesX8[3] = _mm512_or_si512(matchShiftedState3, newWord3);
        }
        else
        {
          // shuffle new words in place.
          const __m128i newWordXmm0a = _mm_shuffle_epi8(newWords0a, lut0a);
          const __m128i newWordXmm0b = _mm_shuffle_epi8(newWords0b, lut0b);
          const __m128i newWordXmm1a = _mm_shuffle_epi8(newWords1a, lut1a);
          const __m128i newWordXmm1b = _mm_shuffle_epi8(newWords1b, lut1b);
          const __m128i newWordXmm2a = _mm_shuffle_epi8(newWords2a, lut2a);
          const __m128i newWordXmm2b = _mm_shuffle_epi8(newWords2b, lut2b);
          const __m128i newWordXmm3a = _mm_shuffle_epi8(newWords3a, lut3a);
          const __m128i newWordXmm3b = _mm_shuffle_epi8(newWords3b, lut3b);

          // expand new word.
          const simd_t newWord0 = _mm512_cvtepu16_epi32(_mm256_set_m128i(newWordXmm0b, newWordXmm0a));
          const simd_t newWord1 = _mm512_cvtepu16_epi32(_mm256_set_m128i(newWordXmm1b, newWordXmm1a));
          const simd_t newWord2 = _mm512_cvtepu16_epi32(_mm256_set_m128i(newWordXmm2b, newWordXmm2a));
          const simd_t newWord3 = _mm512_cvtepu16_epi32(_mm256_set_m128i(newWordXmm3b, newWordXmm3a));

          // state = state << 16 | newWord;
          statesX8[0] = _mm512_or_si512(matchShiftedState0, newWord0);
          statesX8[1] = _mm512_or_si512(matchShiftedState1, newWord1);
          statesX8[2] = _mm512_or_si512(matchShiftedState2, newWord2);
          statesX8[3] = _mm512_or_si512(matchShiftedState3, newWord3);
        }
      }
    }
  }

  for (size_t j = 0; j < sizeof(statesX8) / sizeof(simd_t); j++)
    _mm512_storeu_si512(reinterpret_cast<simd_t *>(reinterpret_cast<uint8_t *>(pState->states) + j * sizeof(simd_t)), statesX8[j]);

  return i;
}

template <uint32_t TotalSymbolCountBits, bool ShuffleMask16, bool YmmShuffle, bool WriteAligned64 = false>
#if !defined(_MSC_VER) || defined(__llvm__)
#ifdef __llvm__
__attribute__((target("avx512bw")))
#else
__attribute__((target("avx512f", "avx512bw")))
#endif
#endif
static size_t _block_rans32x64_decode_section_avx512_varC(_rans_decode_state64_t<hist_dec_pack_t<TotalSymbolCountBits>> *pState, uint8_t *pOutData, const size_t startIndex, const size_t endIndex)
{
  if constexpr (!WriteAligned64)
    if ((reinterpret_cast<size_t>(pOutData) & (63 - 1)) == 0)
      return _block_rans32x64_decode_section_avx512_varC<TotalSymbolCountBits, ShuffleMask16, YmmShuffle, true>(pState, pOutData, startIndex, endIndex);

  constexpr uint32_t TotalSymbolCount = ((uint32_t)1 << TotalSymbolCountBits);
  static_assert(TotalSymbolCountBits < 16);

  typedef __m512i simd_t;
  simd_t statesX8[StateCount / (sizeof(simd_t) / sizeof(uint32_t))];

  for (size_t i = 0; i < sizeof(statesX8) / sizeof(simd_t); i++)
    statesX8[i] = _mm512_loadu_si512(reinterpret_cast<const simd_t *>(reinterpret_cast<const uint8_t *>(pState->states) + i * sizeof(simd_t)));

  size_t i = startIndex;

  const simd_t symCountMask = _mm512_set1_epi32(TotalSymbolCount - 1);
  const simd_t lower12 = _mm512_set1_epi32((1 << 12) - 1);
  const simd_t lower8 = _mm512_set1_epi32(0xFF);
  const simd_t decodeConsumePoint = _mm512_set1_epi32(DecodeConsumePoint16);
  const simd_t symbolPermuteMask = _mm512_set_epi32(15, 7, 14, 6, 11, 3, 10, 2, 13, 5, 12, 4, 9, 1, 8, 0);
  const __m128i shuffleDoubleMask = _mm_set_epi8(7, 7, 6, 6, 5, 5, 4, 4, 3, 3, 2, 2, 1, 1, 0, 0);
  const __m128i shuffleUpper16Bit = _mm_set1_epi16(0x0100);

  for (; i < endIndex; i += StateCount)
  {
    // const uint32_t slot = state & (TotalSymbolCount - 1);
    const simd_t slot0 = _mm512_and_si512(statesX8[0], symCountMask);
    const simd_t slot1 = _mm512_and_si512(statesX8[1], symCountMask);
    const simd_t slot2 = _mm512_and_si512(statesX8[2], symCountMask);
    const simd_t slot3 = _mm512_and_si512(statesX8[3], symCountMask);

    // retrieve pack.
    const simd_t pack0 = _mm512_i32gather_epi32(slot0, reinterpret_cast<const int32_t *>(pState->hist.symbol), sizeof(uint32_t));
    const simd_t pack1 = _mm512_i32gather_epi32(slot1, reinterpret_cast<const int32_t *>(pState->hist.symbol), sizeof(uint32_t));
    const simd_t pack2 = _mm512_i32gather_epi32(slot2, reinterpret_cast<const int32_t *>(pState->hist.symbol), sizeof(uint32_t));
    const simd_t pack3 = _mm512_i32gather_epi32(slot3, reinterpret_cast<const int32_t *>(pState->hist.symbol), sizeof(uint32_t));

    // const uint32_t shiftedState = (state >> TotalSymbolCountBits);
    const simd_t shiftedState0 = _mm512_srli_epi32(statesX8[0], TotalSymbolCountBits);
    const simd_t shiftedState1 = _mm512_srli_epi32(statesX8[1], TotalSymbolCountBits);
    const simd_t shiftedState2 = _mm512_srli_epi32(statesX8[2], TotalSymbolCountBits);
    const simd_t shiftedState3 = _mm512_srli_epi32(statesX8[3], TotalSymbolCountBits);

    // unpack symbol.
    const simd_t symbol0 = _mm512_and_si512(pack0, lower8);
    const simd_t symbol1 = _mm512_and_si512(pack1, lower8);
    const simd_t symbol2 = _mm512_and_si512(pack2, lower8);
    const simd_t symbol3 = _mm512_and_si512(pack3, lower8);

    // pack symbols to one si512.
    const simd_t symPack01 = _mm512_packus_epi32(symbol0, symbol1);
    const simd_t symPack23 = _mm512_packus_epi32(symbol2, symbol3);
    const simd_t symPack0123 = _mm512_packus_epi16(symPack01, symPack23); // only god knows how this is packed now.
    const simd_t symPackCompat = _mm512_permutexvar_epi32(symbolPermuteMask, symPack0123); // we could get rid of this if we'd chose to reorder everything fittingly.

    // unpack freq, cumul.
    const simd_t cumul0 = _mm512_and_si512(_mm512_srli_epi32(pack0, 8), lower12);
    const simd_t freq0 = _mm512_srli_epi32(pack0, 20);
    const simd_t cumul1 = _mm512_and_si512(_mm512_srli_epi32(pack1, 8), lower12);
    const simd_t freq1 = _mm512_srli_epi32(pack1, 20);
    const simd_t cumul2 = _mm512_and_si512(_mm512_srli_epi32(pack2, 8), lower12);
    const simd_t freq2 = _mm512_srli_epi32(pack2, 20);
    const simd_t cumul3 = _mm512_and_si512(_mm512_srli_epi32(pack3, 8), lower12);
    const simd_t freq3 = _mm512_srli_epi32(pack3, 20);

    // We intentionally encoded in a way to not have to do horrible things here.
    if constexpr (WriteAligned64)
      _mm512_stream_si512(reinterpret_cast<simd_t *>(pOutData + i), symPackCompat);
    else
      _mm512_storeu_si512(reinterpret_cast<simd_t *>(pOutData + i), symPackCompat);

    // const uint32_t freqScaled = shiftedState * freq;
    const simd_t freqScaled0 = _mm512_mullo_epi32(shiftedState0, freq0);
    const simd_t freqScaled1 = _mm512_mullo_epi32(shiftedState1, freq1);
    const simd_t freqScaled2 = _mm512_mullo_epi32(shiftedState2, freq2);
    const simd_t freqScaled3 = _mm512_mullo_epi32(shiftedState3, freq3);

    // state = freqScaled + slot - cumul;
    const simd_t state0 = _mm512_add_epi32(freqScaled0, _mm512_sub_epi32(slot0, cumul0));
    const simd_t state1 = _mm512_add_epi32(freqScaled1, _mm512_sub_epi32(slot1, cumul1));
    const simd_t state2 = _mm512_add_epi32(freqScaled2, _mm512_sub_epi32(slot2, cumul2));
    const simd_t state3 = _mm512_add_epi32(freqScaled3, _mm512_sub_epi32(slot3, cumul3));

    // now to the messy part...
    {
      // read input for blocks 0.
      const __m128i newWords0a = _mm_loadu_si128(reinterpret_cast<const __m128i *>(pState->pReadHead));

      // (state < DecodeConsumePoint16) ? -1 : 0 | well, actually (DecodeConsumePoint16 > state) ? -1 : 0
      const __mmask16 cmpMask0 = _mm512_cmpgt_epi32_mask(decodeConsumePoint, state0);
      const __mmask16 cmpMask1 = _mm512_cmpgt_epi32_mask(decodeConsumePoint, state1);
      const __mmask16 cmpMask2 = _mm512_cmpgt_epi32_mask(decodeConsumePoint, state2);
      const __mmask16 cmpMask3 = _mm512_cmpgt_epi32_mask(decodeConsumePoint, state3);

      if constexpr (ShuffleMask16)
      {
        // get masks of those compares & start loading shuffle masks.
        const uint32_t cmpMask0a = cmpMask0 & 0xFF;
        const uint32_t cmpMask0b = cmpMask0 >> 8;
        __m128i lut0a = _mm_load_si128(reinterpret_cast<const __m128i *>(&_DoubleShuffleLutShfl32[cmpMask0a << 4])); // `* 16`.
        __m128i lut0b = _mm_load_si128(reinterpret_cast<const __m128i *>(&_DoubleShuffleLutShfl32[cmpMask0b << 4])); // `* 16`.

        const uint32_t cmpMask1a = cmpMask1 & 0xFF;
        const uint32_t cmpMask1b = cmpMask1 >> 8;
        __m128i lut1a = _mm_load_si128(reinterpret_cast<const __m128i *>(&_DoubleShuffleLutShfl32[cmpMask1a << 4])); // `* 16`.
        __m128i lut1b = _mm_load_si128(reinterpret_cast<const __m128i *>(&_DoubleShuffleLutShfl32[cmpMask1b << 4])); // `* 16`.

        const uint32_t cmpMask2a = cmpMask2 & 0xFF;
        const uint32_t cmpMask2b = cmpMask2 >> 8;
        __m128i lut2a = _mm_load_si128(reinterpret_cast<const __m128i *>(&_DoubleShuffleLutShfl32[cmpMask2a << 4])); // `* 16`.
        __m128i lut2b = _mm_load_si128(reinterpret_cast<const __m128i *>(&_DoubleShuffleLutShfl32[cmpMask2b << 4])); // `* 16`.

        const uint32_t cmpMask3a = cmpMask3 & 0xFF;
        const uint32_t cmpMask3b = cmpMask3 >> 8;
        __m128i lut3a = _mm_load_si128(reinterpret_cast<const __m128i *>(&_DoubleShuffleLutShfl32[cmpMask3a << 4])); // `* 16`.
        __m128i lut3b = _mm_load_si128(reinterpret_cast<const __m128i *>(&_DoubleShuffleLutShfl32[cmpMask3b << 4])); // `* 16`.

        // advance read head & read input for blocks 1, 2, 3, 4, 5, 6, 7.
        const uint32_t maskPop0a = (uint32_t)__builtin_popcount(cmpMask0a);
        pState->pReadHead += maskPop0a;

        const __m128i newWords0b = _mm_loadu_si128(reinterpret_cast<const __m128i *>(pState->pReadHead));

        const uint32_t maskPop0b = (uint32_t)__builtin_popcount(cmpMask0b);
        pState->pReadHead += maskPop0b;

        const __m128i newWords1a = _mm_loadu_si128(reinterpret_cast<const __m128i *>(pState->pReadHead));

        const uint32_t maskPop1a = (uint32_t)__builtin_popcount(cmpMask1a);
        pState->pReadHead += maskPop1a;

        const __m128i newWords1b = _mm_loadu_si128(reinterpret_cast<const __m128i *>(pState->pReadHead));

        const uint32_t maskPop1b = (uint32_t)__builtin_popcount(cmpMask1b);
        pState->pReadHead += maskPop1b;

        const __m128i newWords2a = _mm_loadu_si128(reinterpret_cast<const __m128i *>(pState->pReadHead));

        const uint32_t maskPop2a = (uint32_t)__builtin_popcount(cmpMask2a);
        pState->pReadHead += maskPop2a;

        const __m128i newWords2b = _mm_loadu_si128(reinterpret_cast<const __m128i *>(pState->pReadHead));

        const uint32_t maskPop2b = (uint32_t)__builtin_popcount(cmpMask2b);
        pState->pReadHead += maskPop2b;

        const __m128i newWords3a = _mm_loadu_si128(reinterpret_cast<const __m128i *>(pState->pReadHead));

        const uint32_t maskPop3a = (uint32_t)__builtin_popcount(cmpMask3a);
        pState->pReadHead += maskPop3a;

        const __m128i newWords3b = _mm_loadu_si128(reinterpret_cast<const __m128i *>(pState->pReadHead));

        const uint32_t maskPop3b = (uint32_t)__builtin_popcount(cmpMask3b);
        pState->pReadHead += maskPop3b;

        // matching: state << 16
        const simd_t matchShiftedState0 = _mm512_mask_slli_epi32(state0, cmpMask0, state0, 16);
        const simd_t matchShiftedState1 = _mm512_mask_slli_epi32(state1, cmpMask1, state1, 16);
        const simd_t matchShiftedState2 = _mm512_mask_slli_epi32(state2, cmpMask2, state2, 16);
        const simd_t matchShiftedState3 = _mm512_mask_slli_epi32(state3, cmpMask3, state3, 16);

        if constexpr (YmmShuffle)
        {
          // shuffle new words in place.
          const __m256i newWordXmm0 = _mm256_shuffle_epi8(_mm256_set_m128i(newWords0b, newWords0a), _mm256_set_m128i(lut0b, lut0a));
          const __m256i newWordXmm1 = _mm256_shuffle_epi8(_mm256_set_m128i(newWords1b, newWords1a), _mm256_set_m128i(lut1b, lut1a));
          const __m256i newWordXmm2 = _mm256_shuffle_epi8(_mm256_set_m128i(newWords2b, newWords2a), _mm256_set_m128i(lut2b, lut2a));
          const __m256i newWordXmm3 = _mm256_shuffle_epi8(_mm256_set_m128i(newWords3b, newWords3a), _mm256_set_m128i(lut3b, lut3a));

          // expand new word.
          const simd_t newWord0 = _mm512_cvtepu16_epi32(newWordXmm0);
          const simd_t newWord1 = _mm512_cvtepu16_epi32(newWordXmm1);
          const simd_t newWord2 = _mm512_cvtepu16_epi32(newWordXmm2);
          const simd_t newWord3 = _mm512_cvtepu16_epi32(newWordXmm3);

          // state = state << 16 | newWord;
          statesX8[0] = _mm512_or_si512(matchShiftedState0, newWord0);
          statesX8[1] = _mm512_or_si512(matchShiftedState1, newWord1);
          statesX8[2] = _mm512_or_si512(matchShiftedState2, newWord2);
          statesX8[3] = _mm512_or_si512(matchShiftedState3, newWord3);
        }
        else
        {
          // shuffle new words in place.
          const __m128i newWordXmm0a = _mm_shuffle_epi8(newWords0a, lut0a);
          const __m128i newWordXmm0b = _mm_shuffle_epi8(newWords0b, lut0b);
          const __m128i newWordXmm1a = _mm_shuffle_epi8(newWords1a, lut1a);
          const __m128i newWordXmm1b = _mm_shuffle_epi8(newWords1b, lut1b);
          const __m128i newWordXmm2a = _mm_shuffle_epi8(newWords2a, lut2a);
          const __m128i newWordXmm2b = _mm_shuffle_epi8(newWords2b, lut2b);
          const __m128i newWordXmm3a = _mm_shuffle_epi8(newWords3a, lut3a);
          const __m128i newWordXmm3b = _mm_shuffle_epi8(newWords3b, lut3b);

          // expand new word.
          const simd_t newWord0 = _mm512_cvtepu16_epi32(_mm256_set_m128i(newWordXmm0b, newWordXmm0a));
          const simd_t newWord1 = _mm512_cvtepu16_epi32(_mm256_set_m128i(newWordXmm1b, newWordXmm1a));
          const simd_t newWord2 = _mm512_cvtepu16_epi32(_mm256_set_m128i(newWordXmm2b, newWordXmm2a));
          const simd_t newWord3 = _mm512_cvtepu16_epi32(_mm256_set_m128i(newWordXmm3b, newWordXmm3a));

          // state = state << 16 | newWord;
          statesX8[0] = _mm512_or_si512(matchShiftedState0, newWord0);
          statesX8[1] = _mm512_or_si512(matchShiftedState1, newWord1);
          statesX8[2] = _mm512_or_si512(matchShiftedState2, newWord2);
          statesX8[3] = _mm512_or_si512(matchShiftedState3, newWord3);
        }
      }
      else
      {
        // get masks of those compares & start loading shuffle masks.
        const uint32_t cmpMask0a = cmpMask0 & 0xFF;
        const uint32_t cmpMask0b = cmpMask0 >> 8;
        __m128i lut0a = _mm_lddqu_si128(reinterpret_cast<const __m128i *>(&_ShuffleLutShfl32[cmpMask0a << 3])); // `* 8`.
        __m128i lut0b = _mm_lddqu_si128(reinterpret_cast<const __m128i *>(&_ShuffleLutShfl32[cmpMask0b << 3])); // `* 8`.

        const uint32_t cmpMask1a = cmpMask1 & 0xFF;
        const uint32_t cmpMask1b = cmpMask1 >> 8;
        __m128i lut1a = _mm_lddqu_si128(reinterpret_cast<const __m128i *>(&_ShuffleLutShfl32[cmpMask1a << 3])); // `* 8`.
        __m128i lut1b = _mm_lddqu_si128(reinterpret_cast<const __m128i *>(&_ShuffleLutShfl32[cmpMask1b << 3])); // `* 8`.

        const uint32_t cmpMask2a = cmpMask2 & 0xFF;
        const uint32_t cmpMask2b = cmpMask2 >> 8;
        __m128i lut2a = _mm_lddqu_si128(reinterpret_cast<const __m128i *>(&_ShuffleLutShfl32[cmpMask2a << 3])); // `* 8`.
        __m128i lut2b = _mm_lddqu_si128(reinterpret_cast<const __m128i *>(&_ShuffleLutShfl32[cmpMask2b << 3])); // `* 8`.

        const uint32_t cmpMask3a = cmpMask3 & 0xFF;
        const uint32_t cmpMask3b = cmpMask3 >> 8;
        __m128i lut3a = _mm_lddqu_si128(reinterpret_cast<const __m128i *>(&_ShuffleLutShfl32[cmpMask3a << 3])); // `* 8`.
        __m128i lut3b = _mm_lddqu_si128(reinterpret_cast<const __m128i *>(&_ShuffleLutShfl32[cmpMask3b << 3])); // `* 8`.

        // advance read head & read input for blocks 1, 2, 3, 4, 5, 6, 7.
        const uint32_t maskPop0a = (uint32_t)__builtin_popcount(cmpMask0a);
        pState->pReadHead += maskPop0a;

        const __m128i newWords0b = _mm_loadu_si128(reinterpret_cast<const __m128i *>(pState->pReadHead));

        const uint32_t maskPop0b = (uint32_t)__builtin_popcount(cmpMask0b);
        pState->pReadHead += maskPop0b;

        const __m128i newWords1a = _mm_loadu_si128(reinterpret_cast<const __m128i *>(pState->pReadHead));

        const uint32_t maskPop1a = (uint32_t)__builtin_popcount(cmpMask1a);
        pState->pReadHead += maskPop1a;

        const __m128i newWords1b = _mm_loadu_si128(reinterpret_cast<const __m128i *>(pState->pReadHead));

        const uint32_t maskPop1b = (uint32_t)__builtin_popcount(cmpMask1b);
        pState->pReadHead += maskPop1b;

        const __m128i newWords2a = _mm_loadu_si128(reinterpret_cast<const __m128i *>(pState->pReadHead));

        const uint32_t maskPop2a = (uint32_t)__builtin_popcount(cmpMask2a);
        pState->pReadHead += maskPop2a;

        const __m128i newWords2b = _mm_loadu_si128(reinterpret_cast<const __m128i *>(pState->pReadHead));

        const uint32_t maskPop2b = (uint32_t)__builtin_popcount(cmpMask2b);
        pState->pReadHead += maskPop2b;

        const __m128i newWords3a = _mm_loadu_si128(reinterpret_cast<const __m128i *>(pState->pReadHead));

        const uint32_t maskPop3a = (uint32_t)__builtin_popcount(cmpMask3a);
        pState->pReadHead += maskPop3a;

        const __m128i newWords3b = _mm_loadu_si128(reinterpret_cast<const __m128i *>(pState->pReadHead));

        const uint32_t maskPop3b = (uint32_t)__builtin_popcount(cmpMask3b);
        pState->pReadHead += maskPop3b;

        // finalize lookups.
        lut0a = _mm_or_si128(_mm_shuffle_epi8(lut0a, shuffleDoubleMask), shuffleUpper16Bit);
        lut0b = _mm_or_si128(_mm_shuffle_epi8(lut0b, shuffleDoubleMask), shuffleUpper16Bit);
        lut1a = _mm_or_si128(_mm_shuffle_epi8(lut1a, shuffleDoubleMask), shuffleUpper16Bit);
        lut1b = _mm_or_si128(_mm_shuffle_epi8(lut1b, shuffleDoubleMask), shuffleUpper16Bit);
        lut2a = _mm_or_si128(_mm_shuffle_epi8(lut2a, shuffleDoubleMask), shuffleUpper16Bit);
        lut2b = _mm_or_si128(_mm_shuffle_epi8(lut2b, shuffleDoubleMask), shuffleUpper16Bit);
        lut3a = _mm_or_si128(_mm_shuffle_epi8(lut3a, shuffleDoubleMask), shuffleUpper16Bit);
        lut3b = _mm_or_si128(_mm_shuffle_epi8(lut3b, shuffleDoubleMask), shuffleUpper16Bit);

        // matching: state << 16
        const simd_t matchShiftedState0 = _mm512_mask_slli_epi32(state0, cmpMask0, state0, 16);
        const simd_t matchShiftedState1 = _mm512_mask_slli_epi32(state1, cmpMask1, state1, 16);
        const simd_t matchShiftedState2 = _mm512_mask_slli_epi32(state2, cmpMask2, state2, 16);
        const simd_t matchShiftedState3 = _mm512_mask_slli_epi32(state3, cmpMask3, state3, 16);

        if constexpr (YmmShuffle)
        {
          // shuffle new words in place.
          const __m256i newWordXmm0 = _mm256_shuffle_epi8(_mm256_set_m128i(newWords0b, newWords0a), _mm256_set_m128i(lut0b, lut0a));
          const __m256i newWordXmm1 = _mm256_shuffle_epi8(_mm256_set_m128i(newWords1b, newWords1a), _mm256_set_m128i(lut1b, lut1a));
          const __m256i newWordXmm2 = _mm256_shuffle_epi8(_mm256_set_m128i(newWords2b, newWords2a), _mm256_set_m128i(lut2b, lut2a));
          const __m256i newWordXmm3 = _mm256_shuffle_epi8(_mm256_set_m128i(newWords3b, newWords3a), _mm256_set_m128i(lut3b, lut3a));

          // expand new word.
          const simd_t newWord0 = _mm512_cvtepu16_epi32(newWordXmm0);
          const simd_t newWord1 = _mm512_cvtepu16_epi32(newWordXmm1);
          const simd_t newWord2 = _mm512_cvtepu16_epi32(newWordXmm2);
          const simd_t newWord3 = _mm512_cvtepu16_epi32(newWordXmm3);

          // state = state << 16 | newWord;
          statesX8[0] = _mm512_or_si512(matchShiftedState0, newWord0);
          statesX8[1] = _mm512_or_si512(matchShiftedState1, newWord1);
          statesX8[2] = _mm512_or_si512(matchShiftedState2, newWord2);
          statesX8[3] = _mm512_or_si512(matchShiftedState3, newWord3);
        }
        else
        {
          // shuffle new words in place.
          const __m128i newWordXmm0a = _mm_shuffle_epi8(newWords0a, lut0a);
          const __m128i newWordXmm0b = _mm_shuffle_epi8(newWords0b, lut0b);
          const __m128i newWordXmm1a = _mm_shuffle_epi8(newWords1a, lut1a);
          const __m128i newWordXmm1b = _mm_shuffle_epi8(newWords1b, lut1b);
          const __m128i newWordXmm2a = _mm_shuffle_epi8(newWords2a, lut2a);
          const __m128i newWordXmm2b = _mm_shuffle_epi8(newWords2b, lut2b);
          const __m128i newWordXmm3a = _mm_shuffle_epi8(newWords3a, lut3a);
          const __m128i newWordXmm3b = _mm_shuffle_epi8(newWords3b, lut3b);

          // expand new word.
          const simd_t newWord0 = _mm512_cvtepu16_epi32(_mm256_set_m128i(newWordXmm0b, newWordXmm0a));
          const simd_t newWord1 = _mm512_cvtepu16_epi32(_mm256_set_m128i(newWordXmm1b, newWordXmm1a));
          const simd_t newWord2 = _mm512_cvtepu16_epi32(_mm256_set_m128i(newWordXmm2b, newWordXmm2a));
          const simd_t newWord3 = _mm512_cvtepu16_epi32(_mm256_set_m128i(newWordXmm3b, newWordXmm3a));

          // state = state << 16 | newWord;
          statesX8[0] = _mm512_or_si512(matchShiftedState0, newWord0);
          statesX8[1] = _mm512_or_si512(matchShiftedState1, newWord1);
          statesX8[2] = _mm512_or_si512(matchShiftedState2, newWord2);
          statesX8[3] = _mm512_or_si512(matchShiftedState3, newWord3);
        }
      }
    }
  }

  for (size_t j = 0; j < sizeof(statesX8) / sizeof(simd_t); j++)
    _mm512_storeu_si512(reinterpret_cast<simd_t *>(reinterpret_cast<uint8_t *>(pState->states) + j * sizeof(simd_t)), statesX8[j]);

  return i;
}

template <uint32_t TotalSymbolCountBits>
struct rans32x64_16w_decoder<r32x64_dt_avx2_large_cache_15_to_13, TotalSymbolCountBits, hist_dec2_t<TotalSymbolCountBits>>
{
  template <bool WriteAligned = false>
  static size_t decode_section(_rans_decode_state64_t<hist_dec2_t<TotalSymbolCountBits>> *pState, uint8_t *pOutData, const size_t startIndex, const size_t endIndex)
  {
    return _block_rans32x64_decode_section_avx2_varA<TotalSymbolCountBits, true>(pState, pOutData, startIndex, endIndex);
  }
};

template <uint32_t TotalSymbolCountBits>
struct rans32x64_16w_decoder<r32x64_dt_avx2_small_cache_15_to_13, TotalSymbolCountBits, hist_dec2_t<TotalSymbolCountBits>>
{
  template <bool WriteAligned = false>
  static size_t decode_section(_rans_decode_state64_t<hist_dec2_t<TotalSymbolCountBits>> *pState, uint8_t *pOutData, const size_t startIndex, const size_t endIndex)
  {
    return _block_rans32x64_decode_section_avx2_varA<TotalSymbolCountBits, false>(pState, pOutData, startIndex, endIndex);
  }
};

template <uint32_t TotalSymbolCountBits>
struct rans32x64_16w_decoder<r32x64_dt_avx2_large_cache_12_to_10, TotalSymbolCountBits, hist_dec_pack_t<TotalSymbolCountBits>>
{
  template <bool WriteAligned = false>
  static size_t decode_section(_rans_decode_state64_t<hist_dec_pack_t<TotalSymbolCountBits>> *pState, uint8_t *pOutData, const size_t startIndex, const size_t endIndex)
  {
    return _block_rans32x64_decode_section_avx2_varC<TotalSymbolCountBits, true>(pState, pOutData, startIndex, endIndex);
  }
};

template <uint32_t TotalSymbolCountBits>
struct rans32x64_16w_decoder<r32x64_dt_zen4_12_to_10, TotalSymbolCountBits, hist_dec_pack_t<TotalSymbolCountBits>>
{
  template <bool WriteAligned = false>
  static size_t decode_section(_rans_decode_state64_t<hist_dec_pack_t<TotalSymbolCountBits>> *pState, uint8_t *pOutData, const size_t startIndex, const size_t endIndex)
  {
    return _block_rans32x64_decode_section_znver4_varC<TotalSymbolCountBits>(pState, pOutData, startIndex, endIndex);
  }
};

template <uint32_t TotalSymbolCountBits>
struct rans32x64_16w_decoder<r32x64_dt_avx2_small_cache_12_to_10, TotalSymbolCountBits, hist_dec_pack_t<TotalSymbolCountBits>>
{
  template <bool WriteAligned = false>
  static size_t decode_section(_rans_decode_state64_t<hist_dec_pack_t<TotalSymbolCountBits>> *pState, uint8_t *pOutData, const size_t startIndex, const size_t endIndex)
  {
    return _block_rans32x64_decode_section_avx2_varC<TotalSymbolCountBits, false>(pState, pOutData, startIndex, endIndex);
  }
};

template <uint32_t TotalSymbolCountBits>
struct rans32x64_16w_decoder<r32x64_dt_avx512_large_cache_15_to_13, TotalSymbolCountBits, hist_dec2_t<TotalSymbolCountBits>>
{
  template <bool WriteAligned = false>
  static size_t decode_section(_rans_decode_state64_t<hist_dec2_t<TotalSymbolCountBits>> *pState, uint8_t *pOutData, const size_t startIndex, const size_t endIndex)
  {
    return _block_rans32x64_decode_section_avx512_varA<TotalSymbolCountBits, true, true>(pState, pOutData, startIndex, endIndex);
  }
};

template <uint32_t TotalSymbolCountBits>
struct rans32x64_16w_decoder<r32x64_dt_avx512_small_cache_15_to_13, TotalSymbolCountBits, hist_dec2_t<TotalSymbolCountBits>>
{
  template <bool WriteAligned = false>
  static size_t decode_section(_rans_decode_state64_t<hist_dec2_t<TotalSymbolCountBits>> *pState, uint8_t *pOutData, const size_t startIndex, const size_t endIndex)
  {
    return _block_rans32x64_decode_section_avx512_varA<TotalSymbolCountBits, false, true>(pState, pOutData, startIndex, endIndex);
  }
};

template <uint32_t TotalSymbolCountBits>
struct rans32x64_16w_decoder<r32x64_dt_avx512_large_cache_12_to_10, TotalSymbolCountBits, hist_dec_pack_t<TotalSymbolCountBits>>
{
  template <bool WriteAligned = false>
  static size_t decode_section(_rans_decode_state64_t<hist_dec_pack_t<TotalSymbolCountBits>> *pState, uint8_t *pOutData, const size_t startIndex, const size_t endIndex)
  {
    return _block_rans32x64_decode_section_avx512_varC<TotalSymbolCountBits, true, true>(pState, pOutData, startIndex, endIndex);
  }
};

template <uint32_t TotalSymbolCountBits>
struct rans32x64_16w_decoder<r32x64_dt_avx512_small_cache_12_to_10, TotalSymbolCountBits, hist_dec_pack_t<TotalSymbolCountBits>>
{
  template <bool WriteAligned = false>
  static size_t decode_section(_rans_decode_state64_t<hist_dec_pack_t<TotalSymbolCountBits>> *pState, uint8_t *pOutData, const size_t startIndex, const size_t endIndex)
  {
    return _block_rans32x64_decode_section_avx512_varC<TotalSymbolCountBits, false, true>(pState, pOutData, startIndex, endIndex);
  }
};

#endif // block_codec64_h__
