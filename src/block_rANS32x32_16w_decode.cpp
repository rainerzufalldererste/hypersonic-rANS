#include "block_rANS32x32_16w.h"

#include "hist.h"
#include "simd_platform.h"

#include <string.h>
#include <math.h>

constexpr size_t StateCount = 32; // Needs to be a power of two.
constexpr bool DecodeNoBranch = false;

//////////////////////////////////////////////////////////////////////////

static const uint8_t _Rans32x32_idx2idx[] = { 0x00, 0x01, 0x02, 0x03, 0x10, 0x11, 0x12, 0x13, 0x04, 0x05, 0x06, 0x07, 0x14, 0x15, 0x16, 0x17, 0x08, 0x09, 0x0A, 0x0B, 0x18, 0x19, 0x1A, 0x1B, 0x0C, 0x0D, 0x0E, 0x0F, 0x1C, 0x1D, 0x1E, 0x1F };
static_assert(sizeof(_Rans32x32_idx2idx) == StateCount);

//////////////////////////////////////////////////////////////////////////

extern const uint8_t _ShuffleLutShfl32[256 * 8];
extern const uint8_t _ShuffleLutPerm32[256 * 8];
extern const uint8_t _DoubleShuffleLutShfl32[256 * 8 * 2];

//////////////////////////////////////////////////////////////////////////

template <typename hist_type>
struct _rans_decode_state_t
{
  uint32_t states[StateCount];
  hist_type hist;
  const uint16_t *pReadHead;
};

enum rans32x32_decoder_type_t
{
  r32x32_dt_scalar,
  r32x32_dt_avx2_large_cache_15_to_13,
  r32x32_dt_avx2_small_cache_15_to_13,
  r32x32_dt_avx2_large_cache_12_to_10,
  r32x32_dt_avx2_small_cache_12_to_10,
};

template <rans32x32_decoder_type_t type, uint32_t TotalSymbolCountBits, typename hist_type>
struct rans32x32_16w_decoder
{
  static size_t decode_section(_rans_decode_state_t<hist_type> *pState, uint8_t *pOutData, const size_t startIndex, const size_t endIndex);
};

template <uint32_t TotalSymbolCountBits>
struct rans32x32_16w_decoder<r32x32_dt_scalar, TotalSymbolCountBits, hist_dec_t<TotalSymbolCountBits>>
{
  static size_t decode_section(_rans_decode_state_t<hist_dec_t<TotalSymbolCountBits>> *pState, uint8_t *pOutData, const size_t startIndex, const size_t endIndex)
  {
    constexpr uint32_t TotalSymbolCount = ((uint32_t)1 << TotalSymbolCountBits);

    size_t i = startIndex;

    for (; i < endIndex; i += StateCount)
    {
      for (size_t j = 0; j < StateCount; j++)
      {
        const uint8_t index = _Rans32x32_idx2idx[j];
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
#ifndef _MSC_VER
__attribute__((target("avx2")))
#endif
static size_t _block_rans32x32_decode_section_avx2_varA(_rans_decode_state_t<hist_dec2_t<TotalSymbolCountBits>> *pState, uint8_t *pOutData, const size_t startIndex, const size_t endIndex)
{
  if constexpr (!WriteAligned32)
    if ((reinterpret_cast<size_t>(pOutData) & (StateCount - 1)) == 0)
      return _block_rans32x32_decode_section_avx2_varA<TotalSymbolCountBits, ShuffleMask16, true>(pState, pOutData, startIndex, endIndex);

  constexpr uint32_t TotalSymbolCount = ((uint32_t)1 << TotalSymbolCountBits);

  typedef __m256i simd_t;
  simd_t statesX8[StateCount / (sizeof(simd_t) / sizeof(uint32_t))];

  for (size_t i = 0; i < sizeof(statesX8) / sizeof(simd_t); i++)
    statesX8[i] = _mm256_loadu_si256(reinterpret_cast<const simd_t *>(reinterpret_cast<const uint8_t *>(pState->states) + i * sizeof(simd_t)));

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

    // const uint8_t symbol = pHist->cumulInv[slot];
    simd_t symbol0 = _mm256_i32gather_epi32(reinterpret_cast<const int32_t *>(&pState->hist.cumulInv), slot0, sizeof(uint8_t));
    simd_t symbol1 = _mm256_i32gather_epi32(reinterpret_cast<const int32_t *>(&pState->hist.cumulInv), slot1, sizeof(uint8_t));
    simd_t symbol2 = _mm256_i32gather_epi32(reinterpret_cast<const int32_t *>(&pState->hist.cumulInv), slot2, sizeof(uint8_t));
    simd_t symbol3 = _mm256_i32gather_epi32(reinterpret_cast<const int32_t *>(&pState->hist.cumulInv), slot3, sizeof(uint8_t));

    // since they were int32_t turn into uint8_t
    symbol0 = _mm256_and_si256(symbol0, lower8);
    symbol1 = _mm256_and_si256(symbol1, lower8);
    symbol2 = _mm256_and_si256(symbol2, lower8);
    symbol3 = _mm256_and_si256(symbol3, lower8);

    // pack symbols to one si256. (could possibly be `_mm256_cvtepi32_epi8` on avx-512f + avx-512-vl) (`_mm256_slli_epi32` + `_mm256_or_si256` packing is slower)
    const simd_t symPack01 = _mm256_packus_epi32(symbol0, symbol1);
    const simd_t symPack23 = _mm256_packus_epi32(symbol2, symbol3);
    const simd_t symPack0123 = _mm256_packus_epi16(symPack01, symPack23); // `00 01 02 03 08 09 0A 0B 10 11 12 13 18 19 1A 1B 04 05 06 07 0C 0D 0E 0F 14 15 16 17 1C 1D 1E 1F`

    // We intentionally encoded in a way to not have to do horrible things here.
    if constexpr (WriteAligned32)
      _mm256_stream_si256(reinterpret_cast<__m256i *>(pOutData + i), symPack0123);
    else
      _mm256_storeu_si256(reinterpret_cast<__m256i *>(pOutData + i), symPack0123);

    // retrieve pack.
    const simd_t pack0 = _mm256_i32gather_epi32(reinterpret_cast<const int32_t *>(&pState->hist.symbols), symbol0, sizeof(uint32_t));
    const simd_t pack1 = _mm256_i32gather_epi32(reinterpret_cast<const int32_t *>(&pState->hist.symbols), symbol1, sizeof(uint32_t));
    const simd_t pack2 = _mm256_i32gather_epi32(reinterpret_cast<const int32_t *>(&pState->hist.symbols), symbol2, sizeof(uint32_t));
    const simd_t pack3 = _mm256_i32gather_epi32(reinterpret_cast<const int32_t *>(&pState->hist.symbols), symbol3, sizeof(uint32_t));

    // freq, cumul.
    const simd_t cumul0 = _mm256_srli_epi32(pack0, 16);
    const simd_t freq0 = _mm256_and_si256(pack0, lower16);
    const simd_t cumul1 = _mm256_srli_epi32(pack1, 16);
    const simd_t freq1 = _mm256_and_si256(pack1, lower16);
    const simd_t cumul2 = _mm256_srli_epi32(pack2, 16);
    const simd_t freq2 = _mm256_and_si256(pack2, lower16);
    const simd_t cumul3 = _mm256_srli_epi32(pack3, 16);
    const simd_t freq3 = _mm256_and_si256(pack3, lower16);

    // const uint32_t shiftedState = (state >> TotalSymbolCountBits);
    const simd_t shiftedState0 = _mm256_srli_epi32(statesX8[0], TotalSymbolCountBits);
    const simd_t shiftedState1 = _mm256_srli_epi32(statesX8[1], TotalSymbolCountBits);
    const simd_t shiftedState2 = _mm256_srli_epi32(statesX8[2], TotalSymbolCountBits);
    const simd_t shiftedState3 = _mm256_srli_epi32(statesX8[3], TotalSymbolCountBits);

    // const uint32_t freqScaled = shiftedState * freq;
    const __m256i freqScaled0 = _mm256_mullo_epi32(shiftedState0, freq0);
    const __m256i freqScaled1 = _mm256_mullo_epi32(shiftedState1, freq1);
    const __m256i freqScaled2 = _mm256_mullo_epi32(shiftedState2, freq2);
    const __m256i freqScaled3 = _mm256_mullo_epi32(shiftedState3, freq3);

    // state = freqScaled + slot - cumul;
    const simd_t state0 = _mm256_add_epi32(freqScaled0, _mm256_sub_epi32(slot0, cumul0));
    const simd_t state1 = _mm256_add_epi32(freqScaled1, _mm256_sub_epi32(slot1, cumul1));
    const simd_t state2 = _mm256_add_epi32(freqScaled2, _mm256_sub_epi32(slot2, cumul2));
    const simd_t state3 = _mm256_add_epi32(freqScaled3, _mm256_sub_epi32(slot3, cumul3));

    // now to the messy part...
    {
      // read input for blocks 0, 1.
      const __m128i newWords0 = _mm_loadu_si128(reinterpret_cast<const __m128i *>(pState->pReadHead));

      // (state < DecodeConsumePoint16) ? -1 : 0 | well, actually (DecodeConsumePoint16 > state) ? -1 : 0
      const simd_t cmp0 = _mm256_cmpgt_epi32(decodeConsumePoint, state0);
      const simd_t cmp1 = _mm256_cmpgt_epi32(decodeConsumePoint, state1);
      const simd_t cmp2 = _mm256_cmpgt_epi32(decodeConsumePoint, state2);
      const simd_t cmp3 = _mm256_cmpgt_epi32(decodeConsumePoint, state3);

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

        // advance read head & read input for blocks 1, 2, 3.
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

        // matching: state << 16
        const simd_t matchShiftedState0 = _mm256_sllv_epi32(state0, _mm256_and_si256(cmp0, _16));
        const simd_t matchShiftedState1 = _mm256_sllv_epi32(state1, _mm256_and_si256(cmp1, _16));
        const simd_t matchShiftedState2 = _mm256_sllv_epi32(state2, _mm256_and_si256(cmp2, _16));
        const simd_t matchShiftedState3 = _mm256_sllv_epi32(state3, _mm256_and_si256(cmp3, _16));

        // shuffle new words in place.
        const __m128i newWordXmm0 = _mm_shuffle_epi8(newWords0, lut0);
        const __m128i newWordXmm1 = _mm_shuffle_epi8(newWords1, lut1);
        const __m128i newWordXmm2 = _mm_shuffle_epi8(newWords2, lut2);
        const __m128i newWordXmm3 = _mm_shuffle_epi8(newWords3, lut3);

        // expand new word.
        const __m256i newWord0 = _mm256_cvtepu16_epi32(newWordXmm0);
        const __m256i newWord1 = _mm256_cvtepu16_epi32(newWordXmm1);
        const __m256i newWord2 = _mm256_cvtepu16_epi32(newWordXmm2);
        const __m256i newWord3 = _mm256_cvtepu16_epi32(newWordXmm3);

        // state = state << 16 | newWord;
        statesX8[0] = _mm256_or_si256(matchShiftedState0, newWord0);
        statesX8[1] = _mm256_or_si256(matchShiftedState1, newWord1);
        statesX8[2] = _mm256_or_si256(matchShiftedState2, newWord2);
        statesX8[3] = _mm256_or_si256(matchShiftedState3, newWord3);
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

        // advance read head & read input for blocks 1, 2, 3.
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

        // finalize lookups.
        lut0 = _mm_or_si128(_mm_shuffle_epi8(lut0, shuffleDoubleMask), shuffleUpper16Bit);
        lut1 = _mm_or_si128(_mm_shuffle_epi8(lut1, shuffleDoubleMask), shuffleUpper16Bit);
        lut2 = _mm_or_si128(_mm_shuffle_epi8(lut2, shuffleDoubleMask), shuffleUpper16Bit);
        lut3 = _mm_or_si128(_mm_shuffle_epi8(lut3, shuffleDoubleMask), shuffleUpper16Bit);

        // matching: state << 16
        const simd_t matchShiftedState0 = _mm256_sllv_epi32(state0, _mm256_and_si256(cmp0, _16));
        const simd_t matchShiftedState1 = _mm256_sllv_epi32(state1, _mm256_and_si256(cmp1, _16));
        const simd_t matchShiftedState2 = _mm256_sllv_epi32(state2, _mm256_and_si256(cmp2, _16));
        const simd_t matchShiftedState3 = _mm256_sllv_epi32(state3, _mm256_and_si256(cmp3, _16));

        // shuffle new words in place.
        const __m128i newWordXmm0 = _mm_shuffle_epi8(newWords0, lut0);
        const __m128i newWordXmm1 = _mm_shuffle_epi8(newWords1, lut1);
        const __m128i newWordXmm2 = _mm_shuffle_epi8(newWords2, lut2);
        const __m128i newWordXmm3 = _mm_shuffle_epi8(newWords3, lut3);

        // expand new word.
        const __m256i newWord0 = _mm256_cvtepu16_epi32(newWordXmm0);
        const __m256i newWord1 = _mm256_cvtepu16_epi32(newWordXmm1);
        const __m256i newWord2 = _mm256_cvtepu16_epi32(newWordXmm2);
        const __m256i newWord3 = _mm256_cvtepu16_epi32(newWordXmm3);

        // state = state << 16 | newWord;
        statesX8[0] = _mm256_or_si256(matchShiftedState0, newWord0);
        statesX8[1] = _mm256_or_si256(matchShiftedState1, newWord1);
        statesX8[2] = _mm256_or_si256(matchShiftedState2, newWord2);
        statesX8[3] = _mm256_or_si256(matchShiftedState3, newWord3);
      }
    }
  }

  for (size_t j = 0; j < sizeof(statesX8) / sizeof(simd_t); j++)
    _mm256_storeu_si256(reinterpret_cast<simd_t *>(reinterpret_cast<uint8_t *>(pState->states) + j * sizeof(simd_t)), statesX8[j]);

  return i;
}

template <uint32_t TotalSymbolCountBits>
struct rans32x32_16w_decoder<r32x32_dt_avx2_large_cache_15_to_13, TotalSymbolCountBits, hist_dec2_t<TotalSymbolCountBits>>
{
  template <bool WriteAligned = false>
  static size_t decode_section(_rans_decode_state_t<hist_dec2_t<TotalSymbolCountBits>> *pState, uint8_t *pOutData, const size_t startIndex, const size_t endIndex)
  {
    return _block_rans32x32_decode_section_avx2_varA<TotalSymbolCountBits, true>(pState, pOutData, startIndex, endIndex);
  }
};

template <uint32_t TotalSymbolCountBits>
struct rans32x32_16w_decoder<r32x32_dt_avx2_small_cache_15_to_13, TotalSymbolCountBits, hist_dec2_t<TotalSymbolCountBits>>
{
  template <bool WriteAligned = false>
  static size_t decode_section(_rans_decode_state_t<hist_dec2_t<TotalSymbolCountBits>> *pState, uint8_t *pOutData, const size_t startIndex, const size_t endIndex)
  {
    return _block_rans32x32_decode_section_avx2_varA<TotalSymbolCountBits, false>(pState, pOutData, startIndex, endIndex);
  }
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

//////////////////////////////////////////////////////////////////////////

template <uint32_t TotalSymbolCountBits, rans32x32_decoder_type_t Impl, typename hist_type>
size_t block_rANS32x32_16w_decode(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity)
{
  if (inLength < sizeof(uint64_t) * 2 + sizeof(uint32_t) * StateCount + sizeof(uint16_t) * 256)
    return 0;

  static_assert(TotalSymbolCountBits < 16);
  constexpr uint32_t TotalSymbolCount = ((uint32_t)1 << TotalSymbolCountBits);

  size_t inputIndex = 0;
  const uint64_t expectedOutputLength = *reinterpret_cast<const uint64_t *>(pInData + inputIndex);
  inputIndex += sizeof(uint64_t);

  if (expectedOutputLength > outCapacity)
    return 0;

  const uint64_t expectedInputLength = *reinterpret_cast<const uint64_t *>(pInData + inputIndex);
  inputIndex += sizeof(uint64_t);

  if (inLength < expectedInputLength)
    return 0;

  _rans_decode_state_t<hist_type> decodeState;

  for (size_t i = 0; i < StateCount; i++)
  {
    decodeState.states[i] = *reinterpret_cast<const uint32_t *>(pInData + inputIndex);
    inputIndex += sizeof(uint32_t);
  }

  decodeState.pReadHead = reinterpret_cast<const uint16_t *>(pInData + inputIndex);
  const size_t outLengthInStates = expectedOutputLength - StateCount + 1;
  size_t i = 0;
  hist_t hist;

  do
  {
    const uint64_t blockSize = *reinterpret_cast<const uint64_t *>(decodeState.pReadHead);
    decodeState.pReadHead += sizeof(uint64_t) / sizeof(uint16_t);

    for (size_t j = 0; j < 256; j++)
    {
      hist.symbolCount[j] = *decodeState.pReadHead;
      decodeState.pReadHead++;
    }

    if (!_init_from_hist(&decodeState.hist, &hist, TotalSymbolCountBits))
      return 0;

    uint64_t blockEndInStates = (i + blockSize);

    if (blockEndInStates > outLengthInStates)
      blockEndInStates = outLengthInStates;
    else if ((blockEndInStates & (StateCount - 1)) != 0)
      return 0;

    i = rans32x32_16w_decoder<Impl, TotalSymbolCountBits, hist_type>::decode_section(&decodeState, pOutData, i, blockEndInStates);

    if (i > outLengthInStates)
    {
      if (i >= expectedOutputLength)
        return expectedOutputLength;
      else
        break;
    }

  } while (i < outLengthInStates);

  if (i < expectedOutputLength)
  {
    hist_dec_t<TotalSymbolCountBits> histDec;
    memcpy(&histDec.symbolCount, &hist.symbolCount, sizeof(hist.symbolCount));

    if (!inplace_make_hist_dec<TotalSymbolCountBits>(&histDec))
      return 0;

    for (size_t j = 0; j < StateCount; j++)
    {
      const uint8_t index = _Rans32x32_idx2idx[j];

      if (i + index < expectedOutputLength)
      {
        uint32_t state = decodeState.states[j];

        const uint32_t slot = state & (TotalSymbolCount - 1);
        const uint8_t symbol = histDec.cumulInv[slot];
        pOutData[i + index] = symbol;

        state = (state >> TotalSymbolCountBits) * (uint32_t)histDec.symbolCount[symbol] + slot - (uint32_t)histDec.cumul[symbol];

        if constexpr (DecodeNoBranch)
        {
          const bool read = state < DecodeConsumePoint16;
          const uint32_t newState = state << 16 | *decodeState.pReadHead;
          state = read ? newState : state;
          decodeState.pReadHead += (size_t)read;
        }
        else
        {
          if (state < DecodeConsumePoint16)
          {
            state = state << 16 | *decodeState.pReadHead;
            decodeState.pReadHead++;
          }
        }

        decodeState.states[j] = state;
      }
    }
  }

  return expectedOutputLength;
}

//////////////////////////////////////////////////////////////////////////

template <uint32_t TotalSymbolCountBits>
static size_t block_rANS32x32_decode_wrapper(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity)
{
  _DetectCPUFeatures();

  if (avx2Supported)
  {
    if constexpr (TotalSymbolCountBits >= 13)
      return block_rANS32x32_16w_decode<TotalSymbolCountBits, r32x32_dt_avx2_large_cache_15_to_13, hist_dec2_t<TotalSymbolCountBits>>(pInData, inLength, pOutData, outCapacity);
  }

  // Fallback.
  return block_rANS32x32_16w_decode<TotalSymbolCountBits, r32x32_dt_scalar, hist_dec_t<TotalSymbolCountBits>>(pInData, inLength, pOutData, outCapacity);
}

//////////////////////////////////////////////////////////////////////////

size_t block_rANS32x32_16w_decode_15(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity)
{
  return block_rANS32x32_decode_wrapper<15>(pInData, inLength, pOutData, outCapacity);
}

size_t block_rANS32x32_16w_decode_14(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity)
{
  return block_rANS32x32_decode_wrapper<14>(pInData, inLength, pOutData, outCapacity);
}

size_t block_rANS32x32_16w_decode_13(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity)
{
  return block_rANS32x32_decode_wrapper<13>(pInData, inLength, pOutData, outCapacity);
}

size_t block_rANS32x32_16w_decode_12(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity)
{
  return block_rANS32x32_decode_wrapper<12>(pInData, inLength, pOutData, outCapacity);
}

size_t block_rANS32x32_16w_decode_11(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity)
{
  return block_rANS32x32_decode_wrapper<11>(pInData, inLength, pOutData, outCapacity);
}

size_t block_rANS32x32_16w_decode_10(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity)
{
  return block_rANS32x32_decode_wrapper<10>(pInData, inLength, pOutData, outCapacity);
}
