#include "hist.h"
#include "simd_platform.h"

#include <string.h>

#include <inttypes.h>
#include <stdio.h>

constexpr size_t StateCount = 32; // Needs to be a power of two.

size_t rANS32x32_capacity(const size_t inputSize)
{
  return inputSize + StateCount + sizeof(uint16_t) * 256 + sizeof(uint32_t) * StateCount * 2 + sizeof(uint64_t) * 2; // buffer + histogram + state
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

#define IF_RELEVANT if (false)

size_t rANS32x32_encode_basic(const uint8_t *pInData, const size_t length, uint8_t *pOutData, const size_t outCapacity, const hist_t *pHist)
{
  if (outCapacity < rANS32x32_capacity(length))
    return 0;

  uint32_t states[StateCount];
  uint8_t *pEnd[StateCount];
  uint8_t *pStart[StateCount];

  // Init Pointers & States.
  {
    uint8_t *pWrite = pOutData + outCapacity - 1;
    const size_t blockSize = (length + StateCount) / StateCount;

    for (int64_t i = StateCount - 1; i >= 0; i--)
    {
      states[i] = DecodeConsumePoint;
      pStart[i] = pEnd[i] = pWrite;
      pWrite -= blockSize;
    }
  }

  const uint8_t idx2idx[] = { 0x00, 0x01, 0x02, 0x03, 0x08, 0x09, 0x0A, 0x0B, 0x10, 0x11, 0x12, 0x13, 0x18, 0x19, 0x1A, 0x1B, 0x04, 0x05, 0x06, 0x07, 0x0C, 0x0D, 0x0E, 0x0F, 0x14, 0x15, 0x16, 0x17, 0x1C, 0x1D, 0x1E, 0x1F };
  static_assert(sizeof(idx2idx) == StateCount);

  int64_t i = length - 1;
  i &= ~(size_t)(StateCount - 1);
  i += StateCount;

  for (int64_t j = 0; j < StateCount; j++)
  {
    const uint8_t index = idx2idx[j];

    if (i - (int64_t)StateCount + (int64_t)index < (int64_t)length)
    {
      const uint8_t in = pInData[i - StateCount + index];
      const uint32_t symbolCount = pHist->symbolCount[in];
      const uint32_t max = EncodeEmitPoint * symbolCount;

      const size_t stateIndex = j;

      uint32_t state = states[stateIndex];

      IF_RELEVANT
        printf(">> [%02" PRIX64 "] read %02" PRIX8 " (at %8" PRIX64 ") | state: %8" PRIX32 " =>", j, in, i - StateCount + index, state);

      while (state >= max)
      {
        *pStart[j] = (uint8_t)(state & 0xFF);
        pStart[j]--;
        state >>= 8;

        IF_RELEVANT
          printf(" (wrote %02" PRIX8 ": %8" PRIX32 ")", pStart[j][1], state);
      }

      states[stateIndex] = ((state / symbolCount) << TotalSymbolCountBits) + (uint32_t)pHist->cumul[in] + (state % symbolCount);

      IF_RELEVANT
        printf(" %8" PRIX32 "\n", states[stateIndex]);
    }
  }

  i -= StateCount;

  for (; i >= StateCount; i -= StateCount)
  {
    for (size_t j = 0; j < StateCount; j++)
    {
      const uint8_t index = idx2idx[j];

      const uint8_t in = pInData[i - StateCount + index];
      const uint32_t symbolCount = pHist->symbolCount[in];
      const uint32_t max = EncodeEmitPoint * symbolCount;

      const size_t stateIndex = j;

      uint32_t state = states[stateIndex];

      IF_RELEVANT
        printf(">> [%02" PRIX64 "] read %02" PRIX8 " (at %8" PRIX64 ") | state: %8" PRIX32 " =>", j, in, i - StateCount + index, state);

      while (state >= max)
      {
        *pStart[j] = (uint8_t)(state & 0xFF);
        pStart[j]--;
        state >>= 8;

        IF_RELEVANT
          printf(" (wrote %02" PRIX8 ": %8" PRIX32 ")", pStart[j][1], state);
      }

      states[stateIndex] = ((state / symbolCount) << TotalSymbolCountBits) + (uint32_t)pHist->cumul[in] + (state % symbolCount);

      IF_RELEVANT
        printf(" %8" PRIX32 "\n", states[stateIndex]);
    }
  }

  uint8_t *pWrite = pOutData;
  size_t outIndex = 0;

  *reinterpret_cast<uint64_t *>(pWrite + outIndex) = (uint64_t)length;
  outIndex += sizeof(uint64_t);

  // compressed expected length.
  outIndex += sizeof(uint64_t);

  for (size_t j = 0; j < 256; j++)
  {
    *reinterpret_cast<uint16_t *>(pWrite + outIndex) = pHist->symbolCount[j];
    outIndex += sizeof(uint16_t);
  }

  for (size_t j = 0; j < StateCount; j++)
  {
    *reinterpret_cast<uint32_t *>(pWrite + outIndex) = states[j];
    outIndex += sizeof(uint32_t);
  }

  size_t outIndexBlockSizes = outIndex;
  outIndex += (StateCount - 1) * sizeof(uint32_t);

  for (size_t j = 0; j < StateCount; j++)
  {
    const size_t size = pEnd[j] - pStart[j];

    if (j < StateCount - 1) // we don't need size info for the last one.
    {
      *reinterpret_cast<uint32_t *>(pWrite + outIndexBlockSizes) = (uint32_t)size;
      outIndexBlockSizes += sizeof(uint32_t);
    }

    memmove(pWrite + outIndex, pStart[j] + 1, size);
    outIndex += size;
  }

  *reinterpret_cast<uint64_t *>(pOutData + sizeof(uint64_t)) = outIndex; // write total output length.

  return outIndex;
}

size_t rANS32x32_decode_basic(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity)
{
  if (inLength <= sizeof(uint64_t) * 2 + sizeof(uint32_t) * StateCount * 2 + sizeof(uint16_t) * 256)
    return 0;

  size_t inputIndex = 0;
  const uint64_t expectedOutputLength = *reinterpret_cast<const uint64_t *>(pInData + inputIndex);
  inputIndex += sizeof(uint64_t);

  if (expectedOutputLength > outCapacity)
    return 0;

  const uint64_t expectedInputLength = *reinterpret_cast<const uint64_t *>(pInData + inputIndex);
  inputIndex += sizeof(uint64_t);

  if (inLength < expectedInputLength)
    return 0;

  hist_dec_t hist;

  for (size_t i = 0; i < 256; i++)
  {
    hist.symbolCount[i] = *reinterpret_cast<const uint16_t *>(pInData + inputIndex);
    inputIndex += sizeof(uint16_t);
  }

  if (!inplace_make_hist_dec(&hist))
    return 0;

  uint32_t states[StateCount];
  
  for (size_t i = 0; i < StateCount; i++)
  {
    states[i] = *reinterpret_cast<const uint32_t *>(pInData + inputIndex);
    inputIndex += sizeof(uint32_t);
  }

  const uint8_t *pReadHead[StateCount];
  pReadHead[0] = pInData + inputIndex + sizeof(uint32_t) * (StateCount - 1);

  for (size_t i = 1; i < StateCount; i++)
  {
    const uint32_t blockSize = *reinterpret_cast<const uint32_t *>(pInData + inputIndex);
    inputIndex += sizeof(uint32_t);

    pReadHead[i] = pReadHead[i - 1] + blockSize;
  }

  const uint8_t idx2idx[] = { 0x00, 0x01, 0x02, 0x03, 0x08, 0x09, 0x0A, 0x0B, 0x10, 0x11, 0x12, 0x13, 0x18, 0x19, 0x1A, 0x1B, 0x04, 0x05, 0x06, 0x07, 0x0C, 0x0D, 0x0E, 0x0F, 0x14, 0x15, 0x16, 0x17, 0x1C, 0x1D, 0x1E, 0x1F };
  static_assert(sizeof(idx2idx) == StateCount);

  const size_t outLengthInStates = expectedOutputLength - StateCount + 1;
  size_t i = 0;

  for (; i < outLengthInStates; i += StateCount)
  {
    for (size_t j = 0; j < StateCount; j++)
    {
      const uint8_t index = idx2idx[j];
      uint32_t state = states[j];

      IF_RELEVANT
        printf("<< [%02" PRIX64 "] state: %8" PRIX32 " => ", j, state);

      pOutData[i + index] = decode_symbol_basic(&state, &hist);

      IF_RELEVANT
        printf("%8" PRIX32 " | wrote %02" PRIX8 " (at %8" PRIX64 ")", state, pOutData[i + index], i + index);

      while (state < DecodeConsumePoint)
      {
        state = state << 8 | *pReadHead[j];

        IF_RELEVANT
          printf(" (consumed %02" PRIX8 ": %8" PRIX32 ")", *pReadHead[j], state);

        pReadHead[j]++;
      }

      IF_RELEVANT
        puts("");

      states[j] = state;
    }
  }

  for (size_t j = 0; j < StateCount; j++)
  {
    const uint8_t index = idx2idx[j];

    if (i + index < expectedInputLength)
    {
      uint32_t state = states[j];

      IF_RELEVANT
        printf("<< [%02" PRIX64 "] state: %8" PRIX32 " => ", j, state);

      pOutData[i + index] = decode_symbol_basic(&state, &hist);

      IF_RELEVANT
        printf("%8" PRIX32 " | wrote %02" PRIX8 " (at %8" PRIX64 ")", state, pOutData[i + index], i + index);

      while (state < DecodeConsumePoint)
      {
        state = state << 8 | *pReadHead[j];

        IF_RELEVANT
          printf(" (consumed %02" PRIX8 ": %8" PRIX32 ")", *pReadHead[j], state);

        pReadHead[j]++;
      }

      IF_RELEVANT
        puts("");

      states[j] = state;
    }
  }

  return expectedOutputLength;
}

#ifndef _MSC_VER
__attribute__((target("avx2")))
#endif
size_t rANS32x32_decode_avx2_basic(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity)
{
  if (inLength == 0)
    return 0;

  size_t inputIndex = 0;
  const uint64_t expectedOutputLength = *reinterpret_cast<const uint64_t *>(pInData + inputIndex);
  inputIndex += sizeof(uint64_t);

  if (expectedOutputLength > outCapacity)
    return 0;

  const uint64_t expectedInputLength = *reinterpret_cast<const uint64_t *>(pInData + inputIndex);
  inputIndex += sizeof(uint64_t);

  if (inLength < expectedInputLength)
    return 0;

  hist_dec2_t hist;

  for (size_t i = 0; i < 256; i++)
  {
    hist.symbols[i].freq = *reinterpret_cast<const uint16_t *>(pInData + inputIndex);
    inputIndex += sizeof(uint16_t);
  }

  if (!inplace_make_hist_dec2(&hist))
    return 0;

  uint32_t states[StateCount];

  for (size_t i = 0; i < StateCount; i++)
  {
    states[i] = *reinterpret_cast<const uint32_t *>(pInData + inputIndex);
    inputIndex += sizeof(uint32_t);
  }

  const uint8_t *pReadHead[StateCount];
  pReadHead[0] = pInData + inputIndex + sizeof(uint32_t) * (StateCount - 1);

  for (size_t i = 1; i < StateCount; i++)
  {
    const uint32_t blockSize = *reinterpret_cast<const uint32_t *>(pInData + inputIndex);
    inputIndex += sizeof(uint32_t);

    pReadHead[i] = pReadHead[i - 1] + blockSize;
  }

  typedef __m256i simd_t;
  simd_t statesX8[StateCount / (sizeof(simd_t) / sizeof(uint32_t))];

  for (size_t i = 0; i < sizeof(statesX8) / sizeof(simd_t); i++)
    statesX8[i] = _mm256_loadu_si256(reinterpret_cast<const simd_t *>(pInData + i * sizeof(simd_t)));

  const size_t outLengthInStates = expectedOutputLength - StateCount + 1;
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
    simd_t symbol0 = _mm256_i32gather_epi32(reinterpret_cast<const int32_t *>(&hist.cumulInv), slot0, sizeof(uint8_t));
    simd_t symbol1 = _mm256_i32gather_epi32(reinterpret_cast<const int32_t *>(&hist.cumulInv), slot1, sizeof(uint8_t));
    simd_t symbol2 = _mm256_i32gather_epi32(reinterpret_cast<const int32_t *>(&hist.cumulInv), slot2, sizeof(uint8_t));
    simd_t symbol3 = _mm256_i32gather_epi32(reinterpret_cast<const int32_t *>(&hist.cumulInv), slot3, sizeof(uint8_t));

    // since they were int32_t turn into uint8_t
    symbol0 = _mm256_and_si256(symbol0, lower8);
    symbol1 = _mm256_and_si256(symbol1, lower8);
    symbol2 = _mm256_and_si256(symbol2, lower8);
    symbol3 = _mm256_and_si256(symbol3, lower8);

    // pack symbols to one si256. (could possibly be `_mm256_cvtepi32_epi8` on avx-512f + avx-512-vl)
    const simd_t symPack01 = _mm256_packus_epi32(symbol0, symbol1);
    const simd_t symPack23 = _mm256_packus_epi32(symbol2, symbol3);

#ifndef DECODE_IN_ORDER
    const simd_t symPack0123 = _mm256_packus_epi16(symPack01, symPack23); // 00 01 02 03 08 09 0A 0B 10 11 12 13 18 19 1A 1B 04 05 06 07 0C 0D 0E 0F 14 15 16 17 1C 1D 1E 1F

    _mm256_storeu_si256(reinterpret_cast<__m256i *>(pOutData + i), symPack0123);
#else
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
#endif

    // retrieve pack.
    const simd_t pack0 = _mm256_i32gather_epi32(reinterpret_cast<const int32_t *>(&hist.symbols), symbol0, sizeof(uint32_t));
    const simd_t pack1 = _mm256_i32gather_epi32(reinterpret_cast<const int32_t *>(&hist.symbols), symbol1, sizeof(uint32_t));
    const simd_t pack2 = _mm256_i32gather_epi32(reinterpret_cast<const int32_t *>(&hist.symbols), symbol2, sizeof(uint32_t));
    const simd_t pack3 = _mm256_i32gather_epi32(reinterpret_cast<const int32_t *>(&hist.symbols), symbol3, sizeof(uint32_t));

    // freq, cumul.
    const simd_t cumul0 = _mm256_srli_epi32(pack0, 16);
    const simd_t freq0 = _mm256_and_si256(pack0, lower16);
    const simd_t cumul1 = _mm256_srli_epi32(pack1, 16);
    const simd_t freq1 = _mm256_and_si256(pack1, lower16);
    const simd_t cumul2 = _mm256_srli_epi32(pack2, 16);
    const simd_t freq2 = _mm256_and_si256(pack2, lower16);
    const simd_t cumul3 = _mm256_srli_epi32(pack3, 16);
    const simd_t freq3 = _mm256_and_si256(pack3, lower16);

    // (state >> TotalSymbolCountBits);

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
      const uint8_t symbol = hist.cumulInv[slot];

      const uint32_t freq = hist.symbols[symbol].freq;
      const uint32_t cumul = hist.symbols[symbol].cumul;

      state = (state >> TotalSymbolCountBits) * freq + slot - cumul;

      //pOutData[i + j] = symbol;

      while (state < DecodeConsumePoint)
        state = state << 8 | pInData[inIndex++];

      states[j] = state;
    }
  }

  for (size_t j = 0; j < sizeof(statesX8) / sizeof(simd_t); j++)
    _mm256_storeu_si256(reinterpret_cast<simd_t *>(reinterpret_cast<uint8_t *>(states) + j * sizeof(simd_t)), statesX8[j]);

  for (size_t j = 0; i < expectedOutputLength; i++, j++) // yes, the `i` comparison is intentional.
  {
    uint32_t state = states[j];

    const uint32_t slot = state & (TotalSymbolCount - 1);
    const uint8_t symbol = hist.cumulInv[slot];

    const uint32_t freq = hist.symbols[symbol].freq;
    const uint32_t cumul = hist.symbols[symbol].cumul;

    state = (state >> TotalSymbolCountBits) * freq + slot - cumul;

    pOutData[i + j] = symbol;

    while (state < DecodeConsumePoint)
      state = state << 8 | pInData[inIndex++];

    states[j] = state;
  }

  return expectedOutputLength;
}
