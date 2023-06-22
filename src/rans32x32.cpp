#include "hist.h"
#include "simd_platform.h"

#include <string.h>

constexpr size_t StateCount = 32;

size_t rANS32x32_capacity(const size_t inputSize)
{
  return inputSize + StateCount + sizeof(uint16_t) * 256 + sizeof(uint32_t) * StateCount; // buffer + histogram + state
}

//////////////////////////////////////////////////////////////////////////

inline uint8_t decode_symbol_basic(uint32_t *pState, const hist_dec_t *pHist)
{
  const uint32_t state = *pState;
  const uint32_t slot = state & (TotalSymbolCount - 1);
  const uint8_t symbol = pHist->cumulInv[slot];
  const uint32_t previousState = (state >> TotalSymbolCountBits) * (uint32_t)pHist->symbolCount[symbol] + slot - (uint32_t)pHist->cumul[symbol];

  *pState = previousState;

  return symbol;
}

//////////////////////////////////////////////////////////////////////////

size_t rANS32x32_encode_basic(const uint8_t *pInData, const size_t length, uint8_t *pOutData, const size_t outCapacity, const hist_t *pHist)
{
  if (outCapacity < rANS32x32_capacity(length))
    return 0;

  uint32_t states[StateCount];
  
  for (size_t i = 0; i < StateCount; i++)
    states[i]  = DecodeConsumePoint;

  uint8_t *pWrite = pOutData + outCapacity - 1;

  int64_t i = length - 1;

  for (; i >= (int64_t)(StateCount - 1); i -= StateCount)
  {
    for (size_t j = 0; j < StateCount; j++)
    {
      const uint8_t in = pInData[i - j];
      const uint32_t symbolCount = pHist->symbolCount[in];
      const uint32_t max = EncodeEmitPoint * symbolCount;

      uint32_t state = states[StateCount - 1 - j];

      while (state >= max)
      {
        *pWrite = (uint8_t)(state & 0xFF);
        pWrite--;
        state >>= 8;
      }

      states[StateCount - 1 - j] = ((state / symbolCount) << TotalSymbolCountBits) + (uint32_t)pHist->cumul[in] + (state % symbolCount);
    }
  }

  for (size_t j = 0; i >= 0; j++, i--) // yes, the `i >= 0` is intentional!
  {
    const uint8_t in = pInData[i];
    const uint32_t symbolCount = pHist->symbolCount[in];
    const uint32_t max = EncodeEmitPoint * symbolCount;

    uint32_t state = states[StateCount - 1 - j];

    while (state >= max)
    {
      *pWrite = (uint8_t)(state & 0xFF);
      pWrite--;
      state >>= 8;
    }

    states[StateCount - 1 - j] = ((state / symbolCount) << TotalSymbolCountBits) + (uint32_t)pHist->cumul[in] + (state % symbolCount);
  }

  for (size_t j = 0; j < StateCount; j++)
    reinterpret_cast<uint32_t *>(pOutData)[j] = states[j];

  const size_t writtenSize = (pOutData + outCapacity - 1) - pWrite;
  memmove(pOutData + sizeof(uint32_t) * StateCount, pWrite + 1, writtenSize);

  return writtenSize + sizeof(uint32_t) * StateCount;
}

size_t rANS32x32_decode_basic(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outLength, const hist_dec_t *pHist)
{
  if (inLength == 0)
    return 0;

  uint32_t states[StateCount];
  
  for (size_t i = 0; i < StateCount; i++)
    states[i] = reinterpret_cast<const uint32_t *>(pInData)[i];
  
  const size_t outLengthInStates = outLength - StateCount + 1;
  size_t inIndex = sizeof(uint32_t) * StateCount;
  size_t i = 0;

  for (; i < outLengthInStates; i += StateCount)
  {
    for (size_t j = 0; j < StateCount; j++)
    {
      uint32_t state = states[j];

      pOutData[i + j] = decode_symbol_basic(&state, pHist);

      while (state < DecodeConsumePoint)
        state = state << 8 | pInData[inIndex++];

      states[j] = state;
    }
  }

  for (size_t j = 0; i < outLength; i++, j++) // yes, the `i` comparison is intentional.
  {
    uint32_t state = states[j];

    pOutData[i] = decode_symbol_basic(&state, pHist);

    while (state < DecodeConsumePoint)
      state = state << 8 | pInData[inIndex++];

    states[j] = state;
  }

  return outLength;
}

#ifndef _MSC_VER
__attribute__((target("avx2")))
#endif
size_t rANS32x32_decode_avx2_basic(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outLength, const hist_dec2_t *pHist)
{
  if (inLength == 0)
    return 0;

  uint32_t states[StateCount];

  for (size_t i = 0; i < StateCount; i++)
    states[i] = reinterpret_cast<const uint32_t *>(pInData)[i];

  typedef __m256i simd_t;
  simd_t statesX8[StateCount / (sizeof(simd_t) / sizeof(uint32_t))];

  for (size_t i = 0; i < sizeof(statesX8) / sizeof(simd_t); i++)
    statesX8[i] = _mm256_loadu_si256(reinterpret_cast<const simd_t *>(pInData + i * sizeof(simd_t)));

  const size_t outLengthInStates = outLength - StateCount + 1;
  size_t inIndex = sizeof(uint32_t) * StateCount;
  size_t i = 0;

  const simd_t symCountMask = _mm256_set1_epi32(TotalSymbolCount - 1);
  const simd_t lower16 = _mm256_set1_epi32(0xFFFF);
  const simd_t lower8 = _mm256_set1_epi32(0xFF);

  for (; i < outLengthInStates; i += StateCount)
  {
    // const uint32_t slot = state & (TotalSymbolCount - 1);
    const simd_t slot0 = _mm256_and_si256(statesX8[0], symCountMask);
    const simd_t slot1 = _mm256_and_si256(statesX8[1], symCountMask);
    const simd_t slot2 = _mm256_and_si256(statesX8[2], symCountMask);
    const simd_t slot3 = _mm256_and_si256(statesX8[3], symCountMask);

    // const uint8_t symbol = pHist->cumulInv[slot];
    simd_t symbol0 = _mm256_i32gather_epi32(reinterpret_cast<const int32_t *>(pHist->cumulInv), slot0, sizeof(uint8_t));
    simd_t symbol1 = _mm256_i32gather_epi32(reinterpret_cast<const int32_t *>(pHist->cumulInv), slot1, sizeof(uint8_t));
    simd_t symbol2 = _mm256_i32gather_epi32(reinterpret_cast<const int32_t *>(pHist->cumulInv), slot2, sizeof(uint8_t));
    simd_t symbol3 = _mm256_i32gather_epi32(reinterpret_cast<const int32_t *>(pHist->cumulInv), slot3, sizeof(uint8_t));

    // since they were int32_t turn into uint8_t
    symbol0 = _mm256_and_si256(symbol0, lower8);
    symbol1 = _mm256_and_si256(symbol1, lower8);
    symbol2 = _mm256_and_si256(symbol2, lower8);
    symbol3 = _mm256_and_si256(symbol3, lower8);

    // pack symbols to one si256. (could possibly be `_mm256_cvtepi32_epi8` on avx-512f + avx-512-vl)
    const simd_t symPack01 = _mm256_packus_epi32(symbol0, symbol1);
    const simd_t symPack23 = _mm256_packus_epi32(symbol2, symbol3);

    // we could simply encode in lane-order, but then it might be a bit messy to implement the encoder.
#ifdef VARIANT_128
    const simd_t symPack0123 = _mm256_packus_epi16(symPack01, symPack23);

    const __m256 packAsPs = _mm256_castsi256_ps(symPack0123);
    const simd_t symPack0123fliped = _mm256_castps_si256(_mm256_permute2f128_ps(packAsPs, packAsPs, _MM_SHUFFLE2(0, 1)));

    // This is stupid, but going across 128 bit AVX lanes is always a bit of a mess.
    const __m128i lo = _mm256_castsi256_si128(symPack0123);
    const __m128i hi = _mm256_castsi256_si128(symPack0123fliped);

    const __m128i loPack = _mm_unpacklo_epi32(lo, hi);
    const __m128i hiPack = _mm_unpackhi_epi32(lo, hi);

    _mm_storeu_si128(reinterpret_cast<__m128i *>(pOutData + i), loPack);
    _mm_storeu_si128(reinterpret_cast<__m128i *>(pOutData + i + sizeof(__m128i)), hiPack);
#else
    const simd_t symPack01b = _mm256_permute4x64_epi64(symPack01, 0xD8);
    const simd_t symPack23b = _mm256_permute4x64_epi64(symPack23, 0xD8);

    const simd_t symPackaAbB = _mm256_packus_epi16(symPack01b, symPack23b);
    const simd_t symPack0123 = _mm256_permute4x64_epi64(symPackaAbB, 0xD8);

    _mm256_storeu_si256(reinterpret_cast<__m256i *>(pOutData + i), symPack0123);
#endif

    // retrieve pack.
    const simd_t pack0 = _mm256_i32gather_epi32(reinterpret_cast<const int32_t *>(pHist->symbols), symbol0, sizeof(uint32_t));
    const simd_t pack1 = _mm256_i32gather_epi32(reinterpret_cast<const int32_t *>(pHist->symbols), symbol1, sizeof(uint32_t));
    const simd_t pack2 = _mm256_i32gather_epi32(reinterpret_cast<const int32_t *>(pHist->symbols), symbol2, sizeof(uint32_t));
    const simd_t pack3 = _mm256_i32gather_epi32(reinterpret_cast<const int32_t *>(pHist->symbols), symbol3, sizeof(uint32_t));

    // freq, cumul.
    const simd_t cumul0 = _mm256_srli_epi32(pack0, 16);
    const simd_t freq0 = _mm256_and_si256(pack0, lower16);
    const simd_t cumul1 = _mm256_srli_epi32(pack1, 16);
    const simd_t freq1 = _mm256_and_si256(pack1, lower16);
    const simd_t cumul2 = _mm256_srli_epi32(pack2, 16);
    const simd_t freq2 = _mm256_and_si256(pack2, lower16);
    const simd_t cumul3 = _mm256_srli_epi32(pack3, 16);
    const simd_t freq3 = _mm256_and_si256(pack3, lower16);

    // state = (state >> TotalSymbolCountBits) * freq + slot - cumul;
    const simd_t state0 = _mm256_add_epi32(_mm256_mul_epu32(_mm256_srli_epi32(statesX8[0], TotalSymbolCountBits), freq0), _mm256_sub_epi32(slot0, cumul0));
    const simd_t state1 = _mm256_add_epi32(_mm256_mul_epu32(_mm256_srli_epi32(statesX8[1], TotalSymbolCountBits), freq1), _mm256_sub_epi32(slot1, cumul1));
    const simd_t state2 = _mm256_add_epi32(_mm256_mul_epu32(_mm256_srli_epi32(statesX8[2], TotalSymbolCountBits), freq2), _mm256_sub_epi32(slot2, cumul2));
    const simd_t state3 = _mm256_add_epi32(_mm256_mul_epu32(_mm256_srli_epi32(statesX8[3], TotalSymbolCountBits), freq3), _mm256_sub_epi32(slot3, cumul3));

    // now to the messy part...

    for (size_t j = 0; j < StateCount; j++)
    {
      uint32_t state = states[j];

      const uint32_t slot = state & (TotalSymbolCount - 1);
      const uint8_t symbol = pHist->cumulInv[slot];

      const uint32_t freq = pHist->symbols[symbol].freq;
      const uint32_t cumul = pHist->symbols[symbol].cumul;
      
      state = (state >> TotalSymbolCountBits) * freq + slot - cumul;

      //pOutData[i + j] = symbol;

      while (state < DecodeConsumePoint)
        state = state << 8 | pInData[inIndex++];

      states[j] = state;
    }
  }

  for (size_t j = 0; j < sizeof(statesX8) / sizeof(simd_t); j++)
    _mm256_storeu_si256(reinterpret_cast<simd_t *>(reinterpret_cast<uint8_t *>(states) + j * sizeof(simd_t)), statesX8[j]);

  for (size_t j = 0; i < outLength; i++, j++) // yes, the `i` comparison is intentional.
  {
    uint32_t state = states[j];

    const uint32_t slot = state & (TotalSymbolCount - 1);
    const uint8_t symbol = pHist->cumulInv[slot];

    const uint32_t freq = pHist->symbols[symbol].freq;
    const uint32_t cumul = pHist->symbols[symbol].cumul;

    state = (state >> TotalSymbolCountBits) * freq + slot - cumul;

    pOutData[i + j] = symbol;

    while (state < DecodeConsumePoint)
      state = state << 8 | pInData[inIndex++];

    states[j] = state;
  }

  return outLength;
}
