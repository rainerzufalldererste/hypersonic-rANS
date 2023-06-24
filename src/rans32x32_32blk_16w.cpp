#include "hist.h"
#include "simd_platform.h"

#include <string.h>
#include <inttypes.h>
#include <stdio.h>

constexpr size_t StateCount = 32; // Needs to be a power of two.
constexpr bool DecodeNoBranch = false;
constexpr bool EncodeNoBranch = false;

size_t rANS32x32_32blk_16w_capacity(const size_t inputSize)
{
  return inputSize + StateCount + sizeof(uint16_t) * 256 + sizeof(uint32_t) * StateCount * 2 + sizeof(uint64_t) * 2; // buffer + histogram + state
}

#define IF_RELEVANT if (false)

#ifndef _MSC_VER
#define __debugbreak __builtin_trap
#endif

//////////////////////////////////////////////////////////////////////////

template <uint32_t TotalSymbolCountBits>
inline uint8_t decode_symbol_scalar1(uint32_t *pState, const hist_dec_t<TotalSymbolCountBits> *pHist)
{
  constexpr uint32_t TotalSymbolCount = ((uint32_t)1 << TotalSymbolCountBits);

  const uint32_t state = *pState;
  const uint32_t slot = state & (TotalSymbolCount - 1);
  const uint8_t symbol = pHist->cumulInv[slot];
  const uint32_t previousState = (state >> TotalSymbolCountBits) * (uint32_t)pHist->symbolCount[symbol] + slot - (uint32_t)pHist->cumul[symbol];
  
  IF_RELEVANT printf("%8" PRIX32 " (slot: %4" PRIX32 ", freq: %4" PRIX16 ", cumul %4" PRIX16 ")", previousState, slot, pHist->symbolCount[symbol], pHist->cumul[symbol]);

  *pState = previousState;

  return symbol;
}

//////////////////////////////////////////////////////////////////////////

template <uint32_t TotalSymbolCountBits>
size_t rANS32x32_32blk_16w_encode_scalar(const uint8_t *pInData, const size_t length, uint8_t *pOutData, const size_t outCapacity, const hist_t *pHist)
{
  if (outCapacity < rANS32x32_32blk_16w_capacity(length))
    return 0;

  static_assert(TotalSymbolCountBits < 16);
  constexpr size_t EncodeEmitPoint = ((DecodeConsumePoint16 >> TotalSymbolCountBits) << 16);

  uint32_t states[StateCount];
  uint8_t *pEnd[StateCount];
  uint8_t *pStart[StateCount];

  // Init Pointers & States.
  {
    uint8_t *pWrite = pOutData + outCapacity - 1;
    const size_t blockSize = (length + StateCount) / StateCount;

    for (int64_t i = StateCount - 1; i >= 0; i--)
    {
      states[i] = DecodeConsumePoint16;
      pStart[i] = pEnd[i] = pWrite;
      pWrite -= blockSize;
    }
  }

  const uint8_t idx2idx[] = { 0x00, 0x01, 0x02, 0x03, 0x10, 0x11, 0x12, 0x13, 0x04, 0x05, 0x06, 0x07, 0x14, 0x15, 0x16, 0x17, 0x08, 0x09, 0x0A, 0x0B , 0x18, 0x19, 0x1A, 0x1B, 0x0C, 0x0D, 0x0E, 0x0F, 0x1C, 0x1D, 0x1E, 0x1F };
  static_assert(sizeof(idx2idx) == StateCount);

  int64_t i = length - 1;
  i &= ~(size_t)(StateCount - 1);
  i += StateCount;

  for (size_t j = 0; j < StateCount; j++)
  {
    const uint8_t index = idx2idx[j];

    if (i - (int64_t)StateCount + (int64_t)index < (int64_t)length)
    {
      const uint8_t in = pInData[i - StateCount + index];
      const uint32_t symbolCount = pHist->symbolCount[in];
      const uint32_t max = EncodeEmitPoint * symbolCount;

      const size_t stateIndex = j;

      uint32_t state = states[stateIndex];

      if constexpr (EncodeNoBranch)
      {
        const bool write = state > max;
        *reinterpret_cast<uint16_t *>(pStart[j]) = (uint16_t)(state & 0xFFFF);
        pStart[j] -= sizeof(uint16_t) * (size_t)write;
        state = write ? state >> 16 : state;
      }
      else
      {
        if (state > max)
        {
          *reinterpret_cast<uint16_t *>(pStart[j]) = (uint16_t)(state & 0xFFFF);
          pStart[j] -= sizeof(uint16_t);
          state >>= 16;
        }
      }

      states[stateIndex] = ((state / symbolCount) << TotalSymbolCountBits) + (uint32_t)pHist->cumul[in] + (state % symbolCount);
    }
  }

  i -= StateCount;

  for (; i >= (int64_t)StateCount; i -= StateCount)
  {
    for (size_t j = 0; j < StateCount; j++)
    {
      const uint8_t index = idx2idx[j];

      const uint8_t in = pInData[i - StateCount + index];
      const uint32_t symbolCount = pHist->symbolCount[in];
      const uint32_t max = EncodeEmitPoint * symbolCount;

      const size_t stateIndex = j;

      uint32_t state = states[stateIndex];

      if constexpr (EncodeNoBranch)
      {
        const bool write = state > max;
        *reinterpret_cast<uint16_t *>(pStart[j]) = (uint16_t)(state & 0xFFFF);
        pStart[j] -= sizeof(uint16_t) * (size_t)write;
        state = write ? state >> 16 : state;
      }
      else
      {
        if (state > max)
        {
          *reinterpret_cast<uint16_t *>(pStart[j]) = (uint16_t)(state & 0xFFFF);
          pStart[j] -= sizeof(uint16_t);
          state >>= 16;
        }
      }

      states[stateIndex] = ((state / symbolCount) << TotalSymbolCountBits) + (uint32_t)pHist->cumul[in] + (state % symbolCount);
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

    memmove(pWrite + outIndex, pStart[j] + sizeof(uint16_t), size);
    outIndex += size;

  }

  *reinterpret_cast<uint64_t *>(pOutData + sizeof(uint64_t)) = outIndex; // write total output length.

  return outIndex;
}

template <uint32_t TotalSymbolCountBits>
size_t rANS32x32_32blk_16w_decode_scalar(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity)
{
  if (inLength < sizeof(uint64_t) * 2 + sizeof(uint32_t) * (StateCount * 2 - 1) + sizeof(uint16_t) * 256)
    return 0;

  static_assert(TotalSymbolCountBits < 16);

  size_t inputIndex = 0;
  const uint64_t expectedOutputLength = *reinterpret_cast<const uint64_t *>(pInData + inputIndex);
  inputIndex += sizeof(uint64_t);

  if (expectedOutputLength > outCapacity)
    return 0;

  const uint64_t expectedInputLength = *reinterpret_cast<const uint64_t *>(pInData + inputIndex);
  inputIndex += sizeof(uint64_t);

  if (inLength < expectedInputLength)
    return 0;

  hist_dec_t<TotalSymbolCountBits> hist;

  for (size_t i = 0; i < 256; i++)
  {
    hist.symbolCount[i] = *reinterpret_cast<const uint16_t *>(pInData + inputIndex);
    inputIndex += sizeof(uint16_t);
  }

  if (!inplace_make_hist_dec<TotalSymbolCountBits>(&hist))
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

  const uint8_t idx2idx[] = { 0x00, 0x01, 0x02, 0x03, 0x10, 0x11, 0x12, 0x13, 0x04, 0x05, 0x06, 0x07, 0x14, 0x15, 0x16, 0x17, 0x08, 0x09, 0x0A, 0x0B , 0x18, 0x19, 0x1A, 0x1B, 0x0C, 0x0D, 0x0E, 0x0F, 0x1C, 0x1D, 0x1E, 0x1F };
  static_assert(sizeof(idx2idx) == StateCount);

  const size_t outLengthInStates = expectedOutputLength - StateCount + 1;
  size_t i = 0;

  for (; i < outLengthInStates; i += StateCount)
  {
    for (size_t j = 0; j < StateCount; j++)
    {
      const uint8_t index = idx2idx[j];
      uint32_t state = states[j];

      IF_RELEVANT printf("<< [%02" PRIX64 "] state: %8" PRIX32 " => ", j, state);

      pOutData[i + index] = decode_symbol_scalar1<TotalSymbolCountBits>(&state, &hist);

      IF_RELEVANT printf(" | wrote %02" PRIX8 " (at %8" PRIX64 ")", pOutData[i + index], i + index);

      if constexpr (DecodeNoBranch)
      {
        const bool read = state < DecodeConsumePoint16;
        const uint32_t newState = state << 16 | *reinterpret_cast<const uint16_t *>(pReadHead[j]);
        state = read ? newState : state;
        pReadHead[j] += sizeof(uint16_t) * (size_t)read;
      }
      else
      {
        if (state < DecodeConsumePoint16)
        {
          state = state << 16 | *reinterpret_cast<const uint16_t *>(pReadHead[j]);
          IF_RELEVANT printf(" (consumed %04" PRIX16 ": %8" PRIX32 ")", *reinterpret_cast<const uint16_t *>(pReadHead[j]), state);
          pReadHead[j] += sizeof(uint16_t);
        }
      }
      IF_RELEVANT puts("");
      states[j] = state;
    }
    IF_RELEVANT
    {
    puts("\nWrote:");

    for (size_t j = 0; j < StateCount; j++)
      printf("%02" PRIX8 " ", pOutData[i + j]);

    puts("");

    for (size_t j = 0; j < StateCount; j++)
      printf("%c  ", (char)pOutData[i + j]);

    puts("\n");
    }
  }

  for (size_t j = 0; j < StateCount; j++)
  {
    const uint8_t index = idx2idx[j];

    if (i + index < expectedOutputLength)
    {
      uint32_t state = states[j];

      pOutData[i + index] = decode_symbol_scalar1<TotalSymbolCountBits>(&state, &hist);

      if constexpr (DecodeNoBranch)
      {
        const bool read = state < DecodeConsumePoint16;
        const uint32_t newState = state << 16 | *reinterpret_cast<const uint16_t *>(pReadHead[j]);
        state = read ? newState : state;
        pReadHead[j] += sizeof(uint16_t) * (size_t)read;
      }
      else
      {
        if (state < DecodeConsumePoint16)
        {
          state = state << 16 | *reinterpret_cast<const uint16_t *>(pReadHead[j]);
          pReadHead[j] += sizeof(uint16_t);
        }
      }

      states[j] = state;
    }
  }

  return expectedOutputLength;
}

template <uint32_t TotalSymbolCountBits>
#ifndef _MSC_VER
__attribute__((target("avx2")))
#endif
size_t rANS32x32_32blk_16w_decode_avx2_varA(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity)
{
  if (inLength <= sizeof(uint64_t) * 2)
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

  hist_dec2_t<TotalSymbolCountBits> hist;

  for (size_t i = 0; i < 256; i++)
  {
    hist.symbols[i].freq = *reinterpret_cast<const uint16_t *>(pInData + inputIndex);
    inputIndex += sizeof(uint16_t);
  }

  if (!inplace_make_hist_dec2<TotalSymbolCountBits>(&hist))
    return 0;

  uint32_t states[StateCount];

  for (size_t i = 0; i < StateCount; i++)
  {
    states[i] = *reinterpret_cast<const uint32_t *>(pInData + inputIndex);
    inputIndex += sizeof(uint32_t);
  }

  const uint8_t *pReadHead[StateCount];
  pReadHead[0] = pInData + inputIndex + sizeof(uint32_t) * (StateCount - 1);

  int32_t readHeadOffsets[StateCount];
  readHeadOffsets[0] = 0;

  const int32_t *pReadHeadSIMD = reinterpret_cast<const int32_t *>(pReadHead[0]);

  for (size_t i = 1; i < StateCount; i++)
  {
    const uint32_t blockSize = *reinterpret_cast<const uint32_t *>(pInData + inputIndex);
    inputIndex += sizeof(uint32_t);

    pReadHead[i] = pReadHead[i - 1] + blockSize;
    readHeadOffsets[i] = (uint32_t)(pReadHead[i] - reinterpret_cast<const uint8_t *>(pReadHeadSIMD)) / sizeof(uint16_t);
  }

  typedef __m256i simd_t;
  simd_t statesX8[StateCount / (sizeof(simd_t) / sizeof(uint32_t))];
  simd_t readHeadOffsetsX8[StateCount / (sizeof(simd_t) / sizeof(uint32_t))];

  for (size_t i = 0; i < sizeof(statesX8) / sizeof(simd_t); i++)
  {
    statesX8[i] = _mm256_loadu_si256(reinterpret_cast<const simd_t *>(reinterpret_cast<const uint8_t *>(states) + i * sizeof(simd_t)));
    readHeadOffsetsX8[i] = _mm256_loadu_si256(reinterpret_cast<const simd_t *>(reinterpret_cast<const uint8_t *>(readHeadOffsets) + i * sizeof(simd_t)));
  }

  const size_t outLengthInStates = expectedOutputLength - StateCount + 1;
  size_t i = 0;

  const simd_t symCountMask = _mm256_set1_epi32(TotalSymbolCount - 1);
  const simd_t lower16 = _mm256_set1_epi32(0xFFFF);
  const simd_t lower8 = _mm256_set1_epi32(0xFF);
  const simd_t decodeConsumePoint = _mm256_set1_epi32(DecodeConsumePoint16);

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
    const simd_t symPack0123 = _mm256_packus_epi16(symPack01, symPack23); // `00 01 02 03 08 09 0A 0B 10 11 12 13 18 19 1A 1B 04 05 06 07 0C 0D 0E 0F 14 15 16 17 1C 1D 1E 1F`

    // We intentionally encoded in a way to not have to do horrible things here.
    _mm256_storeu_si256(reinterpret_cast<__m256i *>(pOutData + i), symPack0123);

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
    
    // request next uint16_t.
    const simd_t newWord0 = _mm256_i32gather_epi32(pReadHeadSIMD, readHeadOffsetsX8[0], sizeof(uint16_t));
    const simd_t newWord1 = _mm256_i32gather_epi32(pReadHeadSIMD, readHeadOffsetsX8[1], sizeof(uint16_t));
    const simd_t newWord2 = _mm256_i32gather_epi32(pReadHeadSIMD, readHeadOffsetsX8[2], sizeof(uint16_t));
    const simd_t newWord3 = _mm256_i32gather_epi32(pReadHeadSIMD, readHeadOffsetsX8[3], sizeof(uint16_t));

    // (state < DecodeConsumePoint16) ? -1 : 0 | well, actually (DecodeConsumePoint16 - 1 > state) ? 1 : 0
    const simd_t cmp0 = _mm256_cmpgt_epi32(decodeConsumePoint, state0);
    const simd_t cmp1 = _mm256_cmpgt_epi32(decodeConsumePoint, state1);
    const simd_t cmp2 = _mm256_cmpgt_epi32(decodeConsumePoint, state2);
    const simd_t cmp3 = _mm256_cmpgt_epi32(decodeConsumePoint, state3);

    // matching: state << 16
    const simd_t extractMatchShifted0 = _mm256_slli_epi32(_mm256_and_si256(state0, cmp0), 16);
    const simd_t extractMatchShifted1 = _mm256_slli_epi32(_mm256_and_si256(state1, cmp1), 16);
    const simd_t extractMatchShifted2 = _mm256_slli_epi32(_mm256_and_si256(state2, cmp2), 16);
    const simd_t extractMatchShifted3 = _mm256_slli_epi32(_mm256_and_si256(state3, cmp3), 16);

    // Increment matching read head indexes | well, actually subtract -1.
    readHeadOffsetsX8[0] = _mm256_sub_epi32(readHeadOffsetsX8[0], cmp0);
    readHeadOffsetsX8[1] = _mm256_sub_epi32(readHeadOffsetsX8[1], cmp1);
    readHeadOffsetsX8[2] = _mm256_sub_epi32(readHeadOffsetsX8[2], cmp2);
    readHeadOffsetsX8[3] = _mm256_sub_epi32(readHeadOffsetsX8[3], cmp3);

    // grab current uint16_t from the read heads.
    const simd_t extractMatchNextByte0 = _mm256_and_si256(_mm256_and_si256(cmp0, lower16), newWord0);
    const simd_t extractMatchNextByte1 = _mm256_and_si256(_mm256_and_si256(cmp1, lower16), newWord1);
    const simd_t extractMatchNextByte2 = _mm256_and_si256(_mm256_and_si256(cmp2, lower16), newWord2);
    const simd_t extractMatchNextByte3 = _mm256_and_si256(_mm256_and_si256(cmp3, lower16), newWord3);

    // matching: (state << 16) | newWord
    const simd_t matchingStates0 = _mm256_or_si256(extractMatchShifted0, extractMatchNextByte0);
    const simd_t matchingStates1 = _mm256_or_si256(extractMatchShifted1, extractMatchNextByte1);
    const simd_t matchingStates2 = _mm256_or_si256(extractMatchShifted2, extractMatchNextByte2);
    const simd_t matchingStates3 = _mm256_or_si256(extractMatchShifted3, extractMatchNextByte3);

    // select non matching.
    const simd_t nonMatchingStates0 = _mm256_andnot_si256(cmp0, state0);
    const simd_t nonMatchingStates1 = _mm256_andnot_si256(cmp1, state1);
    const simd_t nonMatchingStates2 = _mm256_andnot_si256(cmp2, state2);
    const simd_t nonMatchingStates3 = _mm256_andnot_si256(cmp3, state3);

    // combine.
    statesX8[0] = _mm256_or_si256(matchingStates0, nonMatchingStates0);
    statesX8[1] = _mm256_or_si256(matchingStates1, nonMatchingStates1);
    statesX8[2] = _mm256_or_si256(matchingStates2, nonMatchingStates2);
    statesX8[3] = _mm256_or_si256(matchingStates3, nonMatchingStates3);
  }

  for (size_t j = 0; j < sizeof(statesX8) / sizeof(simd_t); j++)
  {
    _mm256_storeu_si256(reinterpret_cast<simd_t *>(reinterpret_cast<uint8_t *>(states) + j * sizeof(simd_t)), statesX8[j]);
    _mm256_storeu_si256(reinterpret_cast<simd_t *>(reinterpret_cast<uint8_t *>(readHeadOffsets) + j * sizeof(simd_t)), readHeadOffsetsX8[j]);
  }

  for (size_t j = 0; j < StateCount; j++)
    pReadHead[j] = reinterpret_cast<const uint8_t *>(reinterpret_cast<const uint16_t *>(pReadHeadSIMD) + readHeadOffsets[j]);

  const uint8_t idx2idx[] = { 0x00, 0x01, 0x02, 0x03, 0x10, 0x11, 0x12, 0x13, 0x04, 0x05, 0x06, 0x07, 0x14, 0x15, 0x16, 0x17, 0x08, 0x09, 0x0A, 0x0B , 0x18, 0x19, 0x1A, 0x1B, 0x0C, 0x0D, 0x0E, 0x0F, 0x1C, 0x1D, 0x1E, 0x1F };
  static_assert(sizeof(idx2idx) == StateCount);
  
  for (size_t j = 0; j < StateCount; j++)
  {
    const uint8_t index = idx2idx[j];

    if (i + index < expectedOutputLength)
    {
      uint32_t state = states[j];

      const uint32_t slot = state & (TotalSymbolCount - 1);
      const uint8_t symbol = hist.cumulInv[slot];
      state = (state >> TotalSymbolCountBits) * (uint32_t)hist.symbols[symbol].freq + slot - (uint32_t)hist.symbols[symbol].cumul;

      pOutData[i + index] = symbol;

      if constexpr (DecodeNoBranch)
      {
        const bool read = state < DecodeConsumePoint16;
        const uint32_t newState = state << 16 | *reinterpret_cast<const uint16_t *>(pReadHead[j]);
        state = read ? newState : state;
        pReadHead[j] += sizeof(uint16_t) * (size_t)read;
      }
      else
      {
        if (state < DecodeConsumePoint16)
        {
          state = state << 16 | *reinterpret_cast<const uint16_t *>(pReadHead[j]);
          pReadHead[j] += sizeof(uint16_t);
        }
      }

      states[j] = state;
    }
  }

  return expectedOutputLength;
}

template <uint32_t TotalSymbolCountBits>
#ifndef _MSC_VER
__attribute__((target("avx2")))
#endif
size_t rANS32x32_32blk_16w_decode_avx2_varA2(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity)
{
  if (inLength <= sizeof(uint64_t) * 2)
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

  hist_dec2_t<TotalSymbolCountBits> hist;

  for (size_t i = 0; i < 256; i++)
  {
    hist.symbols[i].freq = *reinterpret_cast<const uint16_t *>(pInData + inputIndex);
    inputIndex += sizeof(uint16_t);
  }

  if (!inplace_make_hist_dec2<TotalSymbolCountBits>(&hist))
    return 0;

  uint32_t states[StateCount];

  for (size_t i = 0; i < StateCount; i++)
  {
    states[i] = *reinterpret_cast<const uint32_t *>(pInData + inputIndex);
    inputIndex += sizeof(uint32_t);
  }

  const uint8_t *pReadHead[StateCount];
  pReadHead[0] = pInData + inputIndex + sizeof(uint32_t) * (StateCount - 1);

  int32_t readHeadOffsets[StateCount];
  readHeadOffsets[0] = 0;

  const int32_t *pReadHeadSIMD = reinterpret_cast<const int32_t *>(pReadHead[0]);

  for (size_t i = 1; i < StateCount; i++)
  {
    const uint32_t blockSize = *reinterpret_cast<const uint32_t *>(pInData + inputIndex);
    inputIndex += sizeof(uint32_t);

    pReadHead[i] = pReadHead[i - 1] + blockSize;
    readHeadOffsets[i] = (uint32_t)(pReadHead[i] - reinterpret_cast<const uint8_t *>(pReadHeadSIMD)) / sizeof(uint16_t);
  }

  typedef __m256i simd_t;
  simd_t statesX8[StateCount / (sizeof(simd_t) / sizeof(uint32_t))];
  simd_t readHeadOffsetsX8[StateCount / (sizeof(simd_t) / sizeof(uint32_t))];

  for (size_t i = 0; i < sizeof(statesX8) / sizeof(simd_t); i++)
  {
    statesX8[i] = _mm256_loadu_si256(reinterpret_cast<const simd_t *>(reinterpret_cast<const uint8_t *>(states) + i * sizeof(simd_t)));
    readHeadOffsetsX8[i] = _mm256_loadu_si256(reinterpret_cast<const simd_t *>(reinterpret_cast<const uint8_t *>(readHeadOffsets) + i * sizeof(simd_t)));
  }

  const size_t outLengthInStates = expectedOutputLength - StateCount + 1;
  const size_t outLengthInDoubleStates = expectedOutputLength - StateCount * 2 + 1;
  size_t i = 0;

  const simd_t symCountMask = _mm256_set1_epi32(TotalSymbolCount - 1);
  const simd_t lower16 = _mm256_set1_epi32(0xFFFF);
  const simd_t lower8 = _mm256_set1_epi32(0xFF);
  const simd_t decodeConsumePoint = _mm256_set1_epi32(DecodeConsumePoint16);

  for (; i < outLengthInDoubleStates; i += StateCount * 2)
  {
    // request next uint16_t.
    simd_t newWord0 = _mm256_i32gather_epi32(pReadHeadSIMD, readHeadOffsetsX8[0], sizeof(uint16_t));
    simd_t newWord1 = _mm256_i32gather_epi32(pReadHeadSIMD, readHeadOffsetsX8[1], sizeof(uint16_t));
    simd_t newWord2 = _mm256_i32gather_epi32(pReadHeadSIMD, readHeadOffsetsX8[2], sizeof(uint16_t));
    simd_t newWord3 = _mm256_i32gather_epi32(pReadHeadSIMD, readHeadOffsetsX8[3], sizeof(uint16_t));

    //////////////////////////////////////////////////////////////////////////
    // Block 1:
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
      const simd_t symPack0123 = _mm256_packus_epi16(symPack01, symPack23); // `00 01 02 03 08 09 0A 0B 10 11 12 13 18 19 1A 1B 04 05 06 07 0C 0D 0E 0F 14 15 16 17 1C 1D 1E 1F`

      // We intentionally encoded in a way to not have to do horrible things here.
      _mm256_storeu_si256(reinterpret_cast<__m256i *>(pOutData + i), symPack0123);

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
      
      // (state < DecodeConsumePoint16) ? -1 : 0 | well, actually (DecodeConsumePoint16 - 1 > state) ? 1 : 0
      const simd_t cmp0 = _mm256_cmpgt_epi32(decodeConsumePoint, state0);
      const simd_t cmp1 = _mm256_cmpgt_epi32(decodeConsumePoint, state1);
      const simd_t cmp2 = _mm256_cmpgt_epi32(decodeConsumePoint, state2);
      const simd_t cmp3 = _mm256_cmpgt_epi32(decodeConsumePoint, state3);

      // matching: state << 16
      const simd_t extractMatchShifted0 = _mm256_slli_epi32(_mm256_and_si256(state0, cmp0), 16);
      const simd_t extractMatchShifted1 = _mm256_slli_epi32(_mm256_and_si256(state1, cmp1), 16);
      const simd_t extractMatchShifted2 = _mm256_slli_epi32(_mm256_and_si256(state2, cmp2), 16);
      const simd_t extractMatchShifted3 = _mm256_slli_epi32(_mm256_and_si256(state3, cmp3), 16);

      // Increment matching read head indexes | well, actually subtract -1.
      readHeadOffsetsX8[0] = _mm256_sub_epi32(readHeadOffsetsX8[0], cmp0);
      readHeadOffsetsX8[1] = _mm256_sub_epi32(readHeadOffsetsX8[1], cmp1);
      readHeadOffsetsX8[2] = _mm256_sub_epi32(readHeadOffsetsX8[2], cmp2);
      readHeadOffsetsX8[3] = _mm256_sub_epi32(readHeadOffsetsX8[3], cmp3);

      // grab current uint16_t from the read heads.
      const simd_t extractMatchNextByte0 = _mm256_and_si256(_mm256_and_si256(cmp0, lower16), newWord0);
      const simd_t extractMatchNextByte1 = _mm256_and_si256(_mm256_and_si256(cmp1, lower16), newWord1);
      const simd_t extractMatchNextByte2 = _mm256_and_si256(_mm256_and_si256(cmp2, lower16), newWord2);
      const simd_t extractMatchNextByte3 = _mm256_and_si256(_mm256_and_si256(cmp3, lower16), newWord3);

      // matching: (state << 16) | newWord
      const simd_t matchingStates0 = _mm256_or_si256(extractMatchShifted0, extractMatchNextByte0);
      const simd_t matchingStates1 = _mm256_or_si256(extractMatchShifted1, extractMatchNextByte1);
      const simd_t matchingStates2 = _mm256_or_si256(extractMatchShifted2, extractMatchNextByte2);
      const simd_t matchingStates3 = _mm256_or_si256(extractMatchShifted3, extractMatchNextByte3);

      // select non matching.
      const simd_t nonMatchingStates0 = _mm256_andnot_si256(cmp0, state0);
      const simd_t nonMatchingStates1 = _mm256_andnot_si256(cmp1, state1);
      const simd_t nonMatchingStates2 = _mm256_andnot_si256(cmp2, state2);
      const simd_t nonMatchingStates3 = _mm256_andnot_si256(cmp3, state3);

      // combine.
      statesX8[0] = _mm256_or_si256(matchingStates0, nonMatchingStates0);
      statesX8[1] = _mm256_or_si256(matchingStates1, nonMatchingStates1);
      statesX8[2] = _mm256_or_si256(matchingStates2, nonMatchingStates2);
      statesX8[3] = _mm256_or_si256(matchingStates3, nonMatchingStates3);

      // derive next byte.
      newWord0 = _mm256_or_si256(_mm256_andnot_si256(cmp0, newWord0), _mm256_and_si256(cmp0, _mm256_srli_epi32(newWord0, 16)));
      newWord1 = _mm256_or_si256(_mm256_andnot_si256(cmp1, newWord1), _mm256_and_si256(cmp1, _mm256_srli_epi32(newWord1, 16)));
      newWord2 = _mm256_or_si256(_mm256_andnot_si256(cmp2, newWord2), _mm256_and_si256(cmp2, _mm256_srli_epi32(newWord2, 16)));
      newWord3 = _mm256_or_si256(_mm256_andnot_si256(cmp3, newWord3), _mm256_and_si256(cmp3, _mm256_srli_epi32(newWord3, 16)));
    }

    //////////////////////////////////////////////////////////////////////////
    // Block 2:
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
      const simd_t symPack0123 = _mm256_packus_epi16(symPack01, symPack23); // `00 01 02 03 08 09 0A 0B 10 11 12 13 18 19 1A 1B 04 05 06 07 0C 0D 0E 0F 14 15 16 17 1C 1D 1E 1F`

      // We intentionally encoded in a way to not have to do horrible things here.
      _mm256_storeu_si256(reinterpret_cast<__m256i *>(pOutData + i + StateCount), symPack0123);

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
      
      // (state < DecodeConsumePoint16) ? -1 : 0 | well, actually (DecodeConsumePoint16 - 1 > state) ? 1 : 0
      const simd_t cmp0 = _mm256_cmpgt_epi32(decodeConsumePoint, state0);
      const simd_t cmp1 = _mm256_cmpgt_epi32(decodeConsumePoint, state1);
      const simd_t cmp2 = _mm256_cmpgt_epi32(decodeConsumePoint, state2);
      const simd_t cmp3 = _mm256_cmpgt_epi32(decodeConsumePoint, state3);

      // matching: state << 16
      const simd_t extractMatchShifted0 = _mm256_slli_epi32(_mm256_and_si256(state0, cmp0), 16);
      const simd_t extractMatchShifted1 = _mm256_slli_epi32(_mm256_and_si256(state1, cmp1), 16);
      const simd_t extractMatchShifted2 = _mm256_slli_epi32(_mm256_and_si256(state2, cmp2), 16);
      const simd_t extractMatchShifted3 = _mm256_slli_epi32(_mm256_and_si256(state3, cmp3), 16);

      // Increment matching read head indexes | well, actually subtract -1.
      readHeadOffsetsX8[0] = _mm256_sub_epi32(readHeadOffsetsX8[0], cmp0);
      readHeadOffsetsX8[1] = _mm256_sub_epi32(readHeadOffsetsX8[1], cmp1);
      readHeadOffsetsX8[2] = _mm256_sub_epi32(readHeadOffsetsX8[2], cmp2);
      readHeadOffsetsX8[3] = _mm256_sub_epi32(readHeadOffsetsX8[3], cmp3);

      // grab current uint16_t from the read heads.
      const simd_t extractMatchNextByte0 = _mm256_and_si256(_mm256_and_si256(cmp0, lower16), newWord0);
      const simd_t extractMatchNextByte1 = _mm256_and_si256(_mm256_and_si256(cmp1, lower16), newWord1);
      const simd_t extractMatchNextByte2 = _mm256_and_si256(_mm256_and_si256(cmp2, lower16), newWord2);
      const simd_t extractMatchNextByte3 = _mm256_and_si256(_mm256_and_si256(cmp3, lower16), newWord3);

      // matching: (state << 16) | newWord
      const simd_t matchingStates0 = _mm256_or_si256(extractMatchShifted0, extractMatchNextByte0);
      const simd_t matchingStates1 = _mm256_or_si256(extractMatchShifted1, extractMatchNextByte1);
      const simd_t matchingStates2 = _mm256_or_si256(extractMatchShifted2, extractMatchNextByte2);
      const simd_t matchingStates3 = _mm256_or_si256(extractMatchShifted3, extractMatchNextByte3);

      // select non matching.
      const simd_t nonMatchingStates0 = _mm256_andnot_si256(cmp0, state0);
      const simd_t nonMatchingStates1 = _mm256_andnot_si256(cmp1, state1);
      const simd_t nonMatchingStates2 = _mm256_andnot_si256(cmp2, state2);
      const simd_t nonMatchingStates3 = _mm256_andnot_si256(cmp3, state3);

      // combine.
      statesX8[0] = _mm256_or_si256(matchingStates0, nonMatchingStates0);
      statesX8[1] = _mm256_or_si256(matchingStates1, nonMatchingStates1);
      statesX8[2] = _mm256_or_si256(matchingStates2, nonMatchingStates2);
      statesX8[3] = _mm256_or_si256(matchingStates3, nonMatchingStates3);

      // no need to derive next byte, we'll gather anyways.
    }
  }

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
    const simd_t symPack0123 = _mm256_packus_epi16(symPack01, symPack23); // `00 01 02 03 08 09 0A 0B 10 11 12 13 18 19 1A 1B 04 05 06 07 0C 0D 0E 0F 14 15 16 17 1C 1D 1E 1F`

    // We intentionally encoded in a way to not have to do horrible things here.
    _mm256_storeu_si256(reinterpret_cast<__m256i *>(pOutData + i), symPack0123);

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
    
    // request next uint16_t.
    const simd_t newWord0 = _mm256_i32gather_epi32(pReadHeadSIMD, readHeadOffsetsX8[0], sizeof(uint16_t));
    const simd_t newWord1 = _mm256_i32gather_epi32(pReadHeadSIMD, readHeadOffsetsX8[1], sizeof(uint16_t));
    const simd_t newWord2 = _mm256_i32gather_epi32(pReadHeadSIMD, readHeadOffsetsX8[2], sizeof(uint16_t));
    const simd_t newWord3 = _mm256_i32gather_epi32(pReadHeadSIMD, readHeadOffsetsX8[3], sizeof(uint16_t));

    // (state < DecodeConsumePoint16) ? -1 : 0 | well, actually (DecodeConsumePoint16 - 1 > state) ? 1 : 0
    const simd_t cmp0 = _mm256_cmpgt_epi32(decodeConsumePoint, state0);
    const simd_t cmp1 = _mm256_cmpgt_epi32(decodeConsumePoint, state1);
    const simd_t cmp2 = _mm256_cmpgt_epi32(decodeConsumePoint, state2);
    const simd_t cmp3 = _mm256_cmpgt_epi32(decodeConsumePoint, state3);

    // matching: state << 16
    const simd_t extractMatchShifted0 = _mm256_slli_epi32(_mm256_and_si256(state0, cmp0), 16);
    const simd_t extractMatchShifted1 = _mm256_slli_epi32(_mm256_and_si256(state1, cmp1), 16);
    const simd_t extractMatchShifted2 = _mm256_slli_epi32(_mm256_and_si256(state2, cmp2), 16);
    const simd_t extractMatchShifted3 = _mm256_slli_epi32(_mm256_and_si256(state3, cmp3), 16);

    // Increment matching read head indexes | well, actually subtract -1.
    readHeadOffsetsX8[0] = _mm256_sub_epi32(readHeadOffsetsX8[0], cmp0);
    readHeadOffsetsX8[1] = _mm256_sub_epi32(readHeadOffsetsX8[1], cmp1);
    readHeadOffsetsX8[2] = _mm256_sub_epi32(readHeadOffsetsX8[2], cmp2);
    readHeadOffsetsX8[3] = _mm256_sub_epi32(readHeadOffsetsX8[3], cmp3);

    // grab current uint16_t from the read heads.
    const simd_t extractMatchNextByte0 = _mm256_and_si256(_mm256_and_si256(cmp0, lower16), newWord0);
    const simd_t extractMatchNextByte1 = _mm256_and_si256(_mm256_and_si256(cmp1, lower16), newWord1);
    const simd_t extractMatchNextByte2 = _mm256_and_si256(_mm256_and_si256(cmp2, lower16), newWord2);
    const simd_t extractMatchNextByte3 = _mm256_and_si256(_mm256_and_si256(cmp3, lower16), newWord3);

    // matching: (state << 16) | newWord
    const simd_t matchingStates0 = _mm256_or_si256(extractMatchShifted0, extractMatchNextByte0);
    const simd_t matchingStates1 = _mm256_or_si256(extractMatchShifted1, extractMatchNextByte1);
    const simd_t matchingStates2 = _mm256_or_si256(extractMatchShifted2, extractMatchNextByte2);
    const simd_t matchingStates3 = _mm256_or_si256(extractMatchShifted3, extractMatchNextByte3);

    // select non matching.
    const simd_t nonMatchingStates0 = _mm256_andnot_si256(cmp0, state0);
    const simd_t nonMatchingStates1 = _mm256_andnot_si256(cmp1, state1);
    const simd_t nonMatchingStates2 = _mm256_andnot_si256(cmp2, state2);
    const simd_t nonMatchingStates3 = _mm256_andnot_si256(cmp3, state3);

    // combine.
    statesX8[0] = _mm256_or_si256(matchingStates0, nonMatchingStates0);
    statesX8[1] = _mm256_or_si256(matchingStates1, nonMatchingStates1);
    statesX8[2] = _mm256_or_si256(matchingStates2, nonMatchingStates2);
    statesX8[3] = _mm256_or_si256(matchingStates3, nonMatchingStates3);
  }

  for (size_t j = 0; j < sizeof(statesX8) / sizeof(simd_t); j++)
  {
    _mm256_storeu_si256(reinterpret_cast<simd_t *>(reinterpret_cast<uint8_t *>(states) + j * sizeof(simd_t)), statesX8[j]);
    _mm256_storeu_si256(reinterpret_cast<simd_t *>(reinterpret_cast<uint8_t *>(readHeadOffsets) + j * sizeof(simd_t)), readHeadOffsetsX8[j]);
  }

  for (size_t j = 0; j < StateCount; j++)
    pReadHead[j] = reinterpret_cast<const uint8_t *>(reinterpret_cast<const uint16_t *>(pReadHeadSIMD) + readHeadOffsets[j]);

  const uint8_t idx2idx[] = { 0x00, 0x01, 0x02, 0x03, 0x10, 0x11, 0x12, 0x13, 0x04, 0x05, 0x06, 0x07, 0x14, 0x15, 0x16, 0x17, 0x08, 0x09, 0x0A, 0x0B , 0x18, 0x19, 0x1A, 0x1B, 0x0C, 0x0D, 0x0E, 0x0F, 0x1C, 0x1D, 0x1E, 0x1F };
  static_assert(sizeof(idx2idx) == StateCount);

  for (size_t j = 0; j < StateCount; j++)
  {
    const uint8_t index = idx2idx[j];

    if (i + index < expectedOutputLength)
    {
      uint32_t state = states[j];

      const uint32_t slot = state & (TotalSymbolCount - 1);
      const uint8_t symbol = hist.cumulInv[slot];
      state = (state >> TotalSymbolCountBits) * (uint32_t)hist.symbols[symbol].freq + slot - (uint32_t)hist.symbols[symbol].cumul;

      pOutData[i + index] = symbol;

      if constexpr (DecodeNoBranch)
      {
        const bool read = state < DecodeConsumePoint16;
        const uint32_t newState = state << 16 | *reinterpret_cast<const uint16_t *>(pReadHead[j]);
        state = read ? newState : state;
        pReadHead[j] += sizeof(uint16_t) * (size_t)read;
      }
      else
      {
        if (state < DecodeConsumePoint16)
        {
          state = state << 16 | *reinterpret_cast<const uint16_t *>(pReadHead[j]);
          pReadHead[j] += sizeof(uint16_t);
        }
      }

      states[j] = state;
    }
  }

  return expectedOutputLength;
}

template <uint32_t TotalSymbolCountBits>
#ifndef _MSC_VER
__attribute__((target("avx2")))
#endif
size_t rANS32x32_32blk_16w_decode_avx2_varB(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity)
{
  if (inLength <= sizeof(uint64_t) * 2)
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

  hist_t hist;

  for (size_t i = 0; i < 256; i++)
  {
    hist.symbolCount[i] = *reinterpret_cast<const uint16_t *>(pInData + inputIndex);
    inputIndex += sizeof(uint16_t);
  }

  if (!inplace_complete_hist(&hist, TotalSymbolCountBits))
    return 0;

  hist_dec3_t<TotalSymbolCountBits> histDec;
  make_dec3_hist<TotalSymbolCountBits>(&histDec, &hist);

  uint32_t states[StateCount];

  for (size_t i = 0; i < StateCount; i++)
  {
    states[i] = *reinterpret_cast<const uint32_t *>(pInData + inputIndex);
    inputIndex += sizeof(uint32_t);
  }

  const uint8_t *pReadHead[StateCount];
  pReadHead[0] = pInData + inputIndex + sizeof(uint32_t) * (StateCount - 1);

  int32_t readHeadOffsets[StateCount];
  readHeadOffsets[0] = 0;

  const int32_t *pReadHeadSIMD = reinterpret_cast<const int32_t *>(pReadHead[0]);

  for (size_t i = 1; i < StateCount; i++)
  {
    const uint32_t blockSize = *reinterpret_cast<const uint32_t *>(pInData + inputIndex);
    inputIndex += sizeof(uint32_t);

    pReadHead[i] = pReadHead[i - 1] + blockSize;
    readHeadOffsets[i] = (uint32_t)(pReadHead[i] - reinterpret_cast<const uint8_t *>(pReadHeadSIMD)) / sizeof(uint16_t);
  }

  typedef __m256i simd_t;
  simd_t statesX8[StateCount / (sizeof(simd_t) / sizeof(uint32_t))];
  simd_t readHeadOffsetsX8[StateCount / (sizeof(simd_t) / sizeof(uint32_t))];

  for (size_t i = 0; i < sizeof(statesX8) / sizeof(simd_t); i++)
  {
    statesX8[i] = _mm256_loadu_si256(reinterpret_cast<const simd_t *>(reinterpret_cast<const uint8_t *>(states) + i * sizeof(simd_t)));
    readHeadOffsetsX8[i] = _mm256_loadu_si256(reinterpret_cast<const simd_t *>(reinterpret_cast<const uint8_t *>(readHeadOffsets) + i * sizeof(simd_t)));
  }

  const size_t outLengthInStates = expectedOutputLength - StateCount + 1;
  size_t i = 0;

  const simd_t symCountMask = _mm256_set1_epi32(TotalSymbolCount - 1);
  const simd_t lower16 = _mm256_set1_epi32(0xFFFF);
  const simd_t lower8 = _mm256_set1_epi32(0xFF);
  const simd_t decodeConsumePoint = _mm256_set1_epi32(DecodeConsumePoint16);

  for (; i < outLengthInStates; i += StateCount)
  {
    // const uint32_t slot = state & (TotalSymbolCount - 1);
    const simd_t slot0 = _mm256_and_si256(statesX8[0], symCountMask);
    const simd_t slot1 = _mm256_and_si256(statesX8[1], symCountMask);
    const simd_t slot2 = _mm256_and_si256(statesX8[2], symCountMask);
    const simd_t slot3 = _mm256_and_si256(statesX8[3], symCountMask);

    // const uint8_t symbol = pHist->cumulInv[slot];
    simd_t symbol0 = _mm256_i32gather_epi32(reinterpret_cast<const int32_t *>(&histDec.cumulInv), slot0, sizeof(uint8_t));
    simd_t symbol1 = _mm256_i32gather_epi32(reinterpret_cast<const int32_t *>(&histDec.cumulInv), slot1, sizeof(uint8_t));
    simd_t symbol2 = _mm256_i32gather_epi32(reinterpret_cast<const int32_t *>(&histDec.cumulInv), slot2, sizeof(uint8_t));
    simd_t symbol3 = _mm256_i32gather_epi32(reinterpret_cast<const int32_t *>(&histDec.cumulInv), slot3, sizeof(uint8_t));

    // retrieve pack.
    const simd_t pack0 = _mm256_i32gather_epi32(reinterpret_cast<const int32_t *>(&histDec.symbolFomCumul), slot0, sizeof(uint32_t));
    const simd_t pack1 = _mm256_i32gather_epi32(reinterpret_cast<const int32_t *>(&histDec.symbolFomCumul), slot1, sizeof(uint32_t));
    const simd_t pack2 = _mm256_i32gather_epi32(reinterpret_cast<const int32_t *>(&histDec.symbolFomCumul), slot2, sizeof(uint32_t));
    const simd_t pack3 = _mm256_i32gather_epi32(reinterpret_cast<const int32_t *>(&histDec.symbolFomCumul), slot3, sizeof(uint32_t));

    // const uint32_t shiftedState = (state >> TotalSymbolCountBits);
    const simd_t shiftedState0 = _mm256_srli_epi32(statesX8[0], TotalSymbolCountBits);
    const simd_t shiftedState1 = _mm256_srli_epi32(statesX8[1], TotalSymbolCountBits);
    const simd_t shiftedState2 = _mm256_srli_epi32(statesX8[2], TotalSymbolCountBits);
    const simd_t shiftedState3 = _mm256_srli_epi32(statesX8[3], TotalSymbolCountBits);

    // since they were int32_t turn into uint8_t
    symbol0 = _mm256_and_si256(symbol0, lower8);
    symbol1 = _mm256_and_si256(symbol1, lower8);
    symbol2 = _mm256_and_si256(symbol2, lower8);
    symbol3 = _mm256_and_si256(symbol3, lower8);

    // pack symbols to one si256. (could possibly be `_mm256_cvtepi32_epi8` on avx-512f + avx-512-vl)
    const simd_t symPack01 = _mm256_packus_epi32(symbol0, symbol1);
    const simd_t symPack23 = _mm256_packus_epi32(symbol2, symbol3);
    const simd_t symPack0123 = _mm256_packus_epi16(symPack01, symPack23); // `00 01 02 03 08 09 0A 0B 10 11 12 13 18 19 1A 1B 04 05 06 07 0C 0D 0E 0F 14 15 16 17 1C 1D 1E 1F`

    // We intentionally encoded in a way to not have to do horrible things here.
    _mm256_storeu_si256(reinterpret_cast<__m256i *>(pOutData + i), symPack0123);

    // freq, cumul.
    const simd_t cumul0 = _mm256_srli_epi32(pack0, 16);
    const simd_t freq0 = _mm256_and_si256(pack0, lower16);
    const simd_t cumul1 = _mm256_srli_epi32(pack1, 16);
    const simd_t freq1 = _mm256_and_si256(pack1, lower16);
    const simd_t cumul2 = _mm256_srli_epi32(pack2, 16);
    const simd_t freq2 = _mm256_and_si256(pack2, lower16);
    const simd_t cumul3 = _mm256_srli_epi32(pack3, 16);
    const simd_t freq3 = _mm256_and_si256(pack3, lower16);

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
    
    // request next uint16_t.
    const simd_t newWord0 = _mm256_i32gather_epi32(pReadHeadSIMD, readHeadOffsetsX8[0], sizeof(uint16_t));
    const simd_t newWord1 = _mm256_i32gather_epi32(pReadHeadSIMD, readHeadOffsetsX8[1], sizeof(uint16_t));
    const simd_t newWord2 = _mm256_i32gather_epi32(pReadHeadSIMD, readHeadOffsetsX8[2], sizeof(uint16_t));
    const simd_t newWord3 = _mm256_i32gather_epi32(pReadHeadSIMD, readHeadOffsetsX8[3], sizeof(uint16_t));

    // (state < DecodeConsumePoint16) ? -1 : 0 | well, actually (DecodeConsumePoint16 - 1 > state) ? 1 : 0
    const simd_t cmp0 = _mm256_cmpgt_epi32(decodeConsumePoint, state0);
    const simd_t cmp1 = _mm256_cmpgt_epi32(decodeConsumePoint, state1);
    const simd_t cmp2 = _mm256_cmpgt_epi32(decodeConsumePoint, state2);
    const simd_t cmp3 = _mm256_cmpgt_epi32(decodeConsumePoint, state3);

    // matching: state << 16
    const simd_t extractMatchShifted0 = _mm256_slli_epi32(_mm256_and_si256(state0, cmp0), 16);
    const simd_t extractMatchShifted1 = _mm256_slli_epi32(_mm256_and_si256(state1, cmp1), 16);
    const simd_t extractMatchShifted2 = _mm256_slli_epi32(_mm256_and_si256(state2, cmp2), 16);
    const simd_t extractMatchShifted3 = _mm256_slli_epi32(_mm256_and_si256(state3, cmp3), 16);

    // Increment matching read head indexes | well, actually subtract -1.
    readHeadOffsetsX8[0] = _mm256_sub_epi32(readHeadOffsetsX8[0], cmp0);
    readHeadOffsetsX8[1] = _mm256_sub_epi32(readHeadOffsetsX8[1], cmp1);
    readHeadOffsetsX8[2] = _mm256_sub_epi32(readHeadOffsetsX8[2], cmp2);
    readHeadOffsetsX8[3] = _mm256_sub_epi32(readHeadOffsetsX8[3], cmp3);

    // grab current uint16_t from the read heads.
    const simd_t extractMatchNextByte0 = _mm256_and_si256(_mm256_and_si256(cmp0, lower16), newWord0);
    const simd_t extractMatchNextByte1 = _mm256_and_si256(_mm256_and_si256(cmp1, lower16), newWord1);
    const simd_t extractMatchNextByte2 = _mm256_and_si256(_mm256_and_si256(cmp2, lower16), newWord2);
    const simd_t extractMatchNextByte3 = _mm256_and_si256(_mm256_and_si256(cmp3, lower16), newWord3);

    // matching: (state << 16) | newWord
    const simd_t matchingStates0 = _mm256_or_si256(extractMatchShifted0, extractMatchNextByte0);
    const simd_t matchingStates1 = _mm256_or_si256(extractMatchShifted1, extractMatchNextByte1);
    const simd_t matchingStates2 = _mm256_or_si256(extractMatchShifted2, extractMatchNextByte2);
    const simd_t matchingStates3 = _mm256_or_si256(extractMatchShifted3, extractMatchNextByte3);

    // select non matching.
    const simd_t nonMatchingStates0 = _mm256_andnot_si256(cmp0, state0);
    const simd_t nonMatchingStates1 = _mm256_andnot_si256(cmp1, state1);
    const simd_t nonMatchingStates2 = _mm256_andnot_si256(cmp2, state2);
    const simd_t nonMatchingStates3 = _mm256_andnot_si256(cmp3, state3);

    // combine.
    statesX8[0] = _mm256_or_si256(matchingStates0, nonMatchingStates0);
    statesX8[1] = _mm256_or_si256(matchingStates1, nonMatchingStates1);
    statesX8[2] = _mm256_or_si256(matchingStates2, nonMatchingStates2);
    statesX8[3] = _mm256_or_si256(matchingStates3, nonMatchingStates3);
  }

  for (size_t j = 0; j < sizeof(statesX8) / sizeof(simd_t); j++)
  {
    _mm256_storeu_si256(reinterpret_cast<simd_t *>(reinterpret_cast<uint8_t *>(states) + j * sizeof(simd_t)), statesX8[j]);
    _mm256_storeu_si256(reinterpret_cast<simd_t *>(reinterpret_cast<uint8_t *>(readHeadOffsets) + j * sizeof(simd_t)), readHeadOffsetsX8[j]);
  }

  for (size_t j = 0; j < StateCount; j++)
    pReadHead[j] = reinterpret_cast<const uint8_t *>(reinterpret_cast<const uint16_t *>(pReadHeadSIMD) + readHeadOffsets[j]);

  const uint8_t idx2idx[] = { 0x00, 0x01, 0x02, 0x03, 0x10, 0x11, 0x12, 0x13, 0x04, 0x05, 0x06, 0x07, 0x14, 0x15, 0x16, 0x17, 0x08, 0x09, 0x0A, 0x0B , 0x18, 0x19, 0x1A, 0x1B, 0x0C, 0x0D, 0x0E, 0x0F, 0x1C, 0x1D, 0x1E, 0x1F };
  static_assert(sizeof(idx2idx) == StateCount);

  for (size_t j = 0; j < StateCount; j++)
  {
    const uint8_t index = idx2idx[j];

    if (i + index < expectedOutputLength)
    {
      uint32_t state = states[j];

      const uint32_t slot = state & (TotalSymbolCount - 1);
      const uint8_t symbol = histDec.cumulInv[slot];
      state = (state >> TotalSymbolCountBits) * (uint32_t)histDec.symbolFomCumul[slot].freq + slot - (uint32_t)histDec.symbolFomCumul[slot].cumul;

      pOutData[i + index] = symbol;

      if constexpr (DecodeNoBranch)
      {
        const bool read = state < DecodeConsumePoint16;
        const uint32_t newState = state << 16 | *reinterpret_cast<const uint16_t *>(pReadHead[j]);
        state = read ? newState : state;
        pReadHead[j] += sizeof(uint16_t) * (size_t)read;
      }
      else
      {
        if (state < DecodeConsumePoint16)
        {
          state = state << 16 | *reinterpret_cast<const uint16_t *>(pReadHead[j]);
          pReadHead[j] += sizeof(uint16_t);
        }
      }

      states[j] = state;
    }
  }

  return expectedOutputLength;
}

template <uint32_t TotalSymbolCountBits>
#ifndef _MSC_VER
__attribute__((target("avx2")))
#endif
size_t rANS32x32_32blk_16w_decode_avx2_varB2(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity)
{
  if (inLength <= sizeof(uint64_t) * 2)
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

  hist_t hist;

  for (size_t i = 0; i < 256; i++)
  {
    hist.symbolCount[i] = *reinterpret_cast<const uint16_t *>(pInData + inputIndex);
    inputIndex += sizeof(uint16_t);
  }

  if (!inplace_complete_hist(&hist, TotalSymbolCountBits))
    return 0;

  hist_dec3_t<TotalSymbolCountBits> histDec;
  make_dec3_hist<TotalSymbolCountBits>(&histDec, &hist);

  uint32_t states[StateCount];

  for (size_t i = 0; i < StateCount; i++)
  {
    states[i] = *reinterpret_cast<const uint32_t *>(pInData + inputIndex);
    inputIndex += sizeof(uint32_t);
  }

  const uint8_t *pReadHead[StateCount];
  pReadHead[0] = pInData + inputIndex + sizeof(uint32_t) * (StateCount - 1);

  int32_t readHeadOffsets[StateCount];
  readHeadOffsets[0] = 0;

  const int32_t *pReadHeadSIMD = reinterpret_cast<const int32_t *>(pReadHead[0]);

  for (size_t i = 1; i < StateCount; i++)
  {
    const uint32_t blockSize = *reinterpret_cast<const uint32_t *>(pInData + inputIndex);
    inputIndex += sizeof(uint32_t);

    pReadHead[i] = pReadHead[i - 1] + blockSize;
    readHeadOffsets[i] = (uint32_t)(pReadHead[i] - reinterpret_cast<const uint8_t *>(pReadHeadSIMD)) / sizeof(uint16_t);
  }

  typedef __m256i simd_t;
  simd_t statesX8[StateCount / (sizeof(simd_t) / sizeof(uint32_t))];
  simd_t readHeadOffsetsX8[StateCount / (sizeof(simd_t) / sizeof(uint32_t))];

  for (size_t i = 0; i < sizeof(statesX8) / sizeof(simd_t); i++)
  {
    statesX8[i] = _mm256_loadu_si256(reinterpret_cast<const simd_t *>(reinterpret_cast<const uint8_t *>(states) + i * sizeof(simd_t)));
    readHeadOffsetsX8[i] = _mm256_loadu_si256(reinterpret_cast<const simd_t *>(reinterpret_cast<const uint8_t *>(readHeadOffsets) + i * sizeof(simd_t)));
  }

  const size_t outLengthInStates = expectedOutputLength - StateCount + 1;
  const size_t outLengthInDoubleStates = expectedOutputLength - StateCount * 2 + 1;
  size_t i = 0;

  const simd_t symCountMask = _mm256_set1_epi32(TotalSymbolCount - 1);
  const simd_t lower16 = _mm256_set1_epi32(0xFFFF);
  const simd_t lower8 = _mm256_set1_epi32(0xFF);
  const simd_t decodeConsumePoint = _mm256_set1_epi32(DecodeConsumePoint16);

  for (; i < outLengthInDoubleStates; i += StateCount * 2)
  {
    // request next uint16_t.
    simd_t newWord0 = _mm256_i32gather_epi32(pReadHeadSIMD, readHeadOffsetsX8[0], sizeof(uint16_t));
    simd_t newWord1 = _mm256_i32gather_epi32(pReadHeadSIMD, readHeadOffsetsX8[1], sizeof(uint16_t));
    simd_t newWord2 = _mm256_i32gather_epi32(pReadHeadSIMD, readHeadOffsetsX8[2], sizeof(uint16_t));
    simd_t newWord3 = _mm256_i32gather_epi32(pReadHeadSIMD, readHeadOffsetsX8[3], sizeof(uint16_t));

    //////////////////////////////////////////////////////////////////////////
    // Block 1:
    {
      // const uint32_t slot = state & (TotalSymbolCount - 1);
      const simd_t slot0 = _mm256_and_si256(statesX8[0], symCountMask);
      const simd_t slot1 = _mm256_and_si256(statesX8[1], symCountMask);
      const simd_t slot2 = _mm256_and_si256(statesX8[2], symCountMask);
      const simd_t slot3 = _mm256_and_si256(statesX8[3], symCountMask);

      // const uint8_t symbol = pHist->cumulInv[slot];
      simd_t symbol0 = _mm256_i32gather_epi32(reinterpret_cast<const int32_t *>(&histDec.cumulInv), slot0, sizeof(uint8_t));
      simd_t symbol1 = _mm256_i32gather_epi32(reinterpret_cast<const int32_t *>(&histDec.cumulInv), slot1, sizeof(uint8_t));
      simd_t symbol2 = _mm256_i32gather_epi32(reinterpret_cast<const int32_t *>(&histDec.cumulInv), slot2, sizeof(uint8_t));
      simd_t symbol3 = _mm256_i32gather_epi32(reinterpret_cast<const int32_t *>(&histDec.cumulInv), slot3, sizeof(uint8_t));

      // retrieve pack.
      const simd_t pack0 = _mm256_i32gather_epi32(reinterpret_cast<const int32_t *>(&histDec.symbolFomCumul), slot0, sizeof(uint32_t));
      const simd_t pack1 = _mm256_i32gather_epi32(reinterpret_cast<const int32_t *>(&histDec.symbolFomCumul), slot1, sizeof(uint32_t));
      const simd_t pack2 = _mm256_i32gather_epi32(reinterpret_cast<const int32_t *>(&histDec.symbolFomCumul), slot2, sizeof(uint32_t));
      const simd_t pack3 = _mm256_i32gather_epi32(reinterpret_cast<const int32_t *>(&histDec.symbolFomCumul), slot3, sizeof(uint32_t));

      // const uint32_t shiftedState = (state >> TotalSymbolCountBits);
      const simd_t shiftedState0 = _mm256_srli_epi32(statesX8[0], TotalSymbolCountBits);
      const simd_t shiftedState1 = _mm256_srli_epi32(statesX8[1], TotalSymbolCountBits);
      const simd_t shiftedState2 = _mm256_srli_epi32(statesX8[2], TotalSymbolCountBits);
      const simd_t shiftedState3 = _mm256_srli_epi32(statesX8[3], TotalSymbolCountBits);

      // since they were int32_t turn into uint8_t
      symbol0 = _mm256_and_si256(symbol0, lower8);
      symbol1 = _mm256_and_si256(symbol1, lower8);
      symbol2 = _mm256_and_si256(symbol2, lower8);
      symbol3 = _mm256_and_si256(symbol3, lower8);

      // pack symbols to one si256. (could possibly be `_mm256_cvtepi32_epi8` on avx-512f + avx-512-vl)
      const simd_t symPack01 = _mm256_packus_epi32(symbol0, symbol1);
      const simd_t symPack23 = _mm256_packus_epi32(symbol2, symbol3);
      const simd_t symPack0123 = _mm256_packus_epi16(symPack01, symPack23); // `00 01 02 03 08 09 0A 0B 10 11 12 13 18 19 1A 1B 04 05 06 07 0C 0D 0E 0F 14 15 16 17 1C 1D 1E 1F`

      // We intentionally encoded in a way to not have to do horrible things here.
      _mm256_storeu_si256(reinterpret_cast<__m256i *>(pOutData + i), symPack0123);

      // freq, cumul.
      const simd_t cumul0 = _mm256_srli_epi32(pack0, 16);
      const simd_t freq0 = _mm256_and_si256(pack0, lower16);
      const simd_t cumul1 = _mm256_srli_epi32(pack1, 16);
      const simd_t freq1 = _mm256_and_si256(pack1, lower16);
      const simd_t cumul2 = _mm256_srli_epi32(pack2, 16);
      const simd_t freq2 = _mm256_and_si256(pack2, lower16);
      const simd_t cumul3 = _mm256_srli_epi32(pack3, 16);
      const simd_t freq3 = _mm256_and_si256(pack3, lower16);

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
      
      // (state < DecodeConsumePoint16) ? -1 : 0 | well, actually (DecodeConsumePoint16 - 1 > state) ? 1 : 0
      const simd_t cmp0 = _mm256_cmpgt_epi32(decodeConsumePoint, state0);
      const simd_t cmp1 = _mm256_cmpgt_epi32(decodeConsumePoint, state1);
      const simd_t cmp2 = _mm256_cmpgt_epi32(decodeConsumePoint, state2);
      const simd_t cmp3 = _mm256_cmpgt_epi32(decodeConsumePoint, state3);

      // matching: state << 16
      const simd_t extractMatchShifted0 = _mm256_slli_epi32(_mm256_and_si256(state0, cmp0), 16);
      const simd_t extractMatchShifted1 = _mm256_slli_epi32(_mm256_and_si256(state1, cmp1), 16);
      const simd_t extractMatchShifted2 = _mm256_slli_epi32(_mm256_and_si256(state2, cmp2), 16);
      const simd_t extractMatchShifted3 = _mm256_slli_epi32(_mm256_and_si256(state3, cmp3), 16);

      // Increment matching read head indexes | well, actually subtract -1.
      readHeadOffsetsX8[0] = _mm256_sub_epi32(readHeadOffsetsX8[0], cmp0);
      readHeadOffsetsX8[1] = _mm256_sub_epi32(readHeadOffsetsX8[1], cmp1);
      readHeadOffsetsX8[2] = _mm256_sub_epi32(readHeadOffsetsX8[2], cmp2);
      readHeadOffsetsX8[3] = _mm256_sub_epi32(readHeadOffsetsX8[3], cmp3);

      // grab current byte from the read heads.
      const simd_t extractMatchNextByte0 = _mm256_and_si256(_mm256_and_si256(cmp0, lower16), newWord0);
      const simd_t extractMatchNextByte1 = _mm256_and_si256(_mm256_and_si256(cmp1, lower16), newWord1);
      const simd_t extractMatchNextByte2 = _mm256_and_si256(_mm256_and_si256(cmp2, lower16), newWord2);
      const simd_t extractMatchNextByte3 = _mm256_and_si256(_mm256_and_si256(cmp3, lower16), newWord3);

      // matching: (state << 16) | newWord
      const simd_t matchingStates0 = _mm256_or_si256(extractMatchShifted0, extractMatchNextByte0);
      const simd_t matchingStates1 = _mm256_or_si256(extractMatchShifted1, extractMatchNextByte1);
      const simd_t matchingStates2 = _mm256_or_si256(extractMatchShifted2, extractMatchNextByte2);
      const simd_t matchingStates3 = _mm256_or_si256(extractMatchShifted3, extractMatchNextByte3);

      // select non matching.
      const simd_t nonMatchingStates0 = _mm256_andnot_si256(cmp0, state0);
      const simd_t nonMatchingStates1 = _mm256_andnot_si256(cmp1, state1);
      const simd_t nonMatchingStates2 = _mm256_andnot_si256(cmp2, state2);
      const simd_t nonMatchingStates3 = _mm256_andnot_si256(cmp3, state3);

      // combine.
      statesX8[0] = _mm256_or_si256(matchingStates0, nonMatchingStates0);
      statesX8[1] = _mm256_or_si256(matchingStates1, nonMatchingStates1);
      statesX8[2] = _mm256_or_si256(matchingStates2, nonMatchingStates2);
      statesX8[3] = _mm256_or_si256(matchingStates3, nonMatchingStates3);

      // derive next byte.
      newWord0 = _mm256_or_si256(_mm256_andnot_si256(cmp0, newWord0), _mm256_and_si256(cmp0, _mm256_srli_epi32(newWord0, 16)));
      newWord1 = _mm256_or_si256(_mm256_andnot_si256(cmp1, newWord1), _mm256_and_si256(cmp1, _mm256_srli_epi32(newWord1, 16)));
      newWord2 = _mm256_or_si256(_mm256_andnot_si256(cmp2, newWord2), _mm256_and_si256(cmp2, _mm256_srli_epi32(newWord2, 16)));
      newWord3 = _mm256_or_si256(_mm256_andnot_si256(cmp3, newWord3), _mm256_and_si256(cmp3, _mm256_srli_epi32(newWord3, 16)));
    }

    //////////////////////////////////////////////////////////////////////////
    // Block 2:
    {
      // const uint32_t slot = state & (TotalSymbolCount - 1);
      const simd_t slot0 = _mm256_and_si256(statesX8[0], symCountMask);
      const simd_t slot1 = _mm256_and_si256(statesX8[1], symCountMask);
      const simd_t slot2 = _mm256_and_si256(statesX8[2], symCountMask);
      const simd_t slot3 = _mm256_and_si256(statesX8[3], symCountMask);

      // const uint8_t symbol = pHist->cumulInv[slot];
      simd_t symbol0 = _mm256_i32gather_epi32(reinterpret_cast<const int32_t *>(&histDec.cumulInv), slot0, sizeof(uint8_t));
      simd_t symbol1 = _mm256_i32gather_epi32(reinterpret_cast<const int32_t *>(&histDec.cumulInv), slot1, sizeof(uint8_t));
      simd_t symbol2 = _mm256_i32gather_epi32(reinterpret_cast<const int32_t *>(&histDec.cumulInv), slot2, sizeof(uint8_t));
      simd_t symbol3 = _mm256_i32gather_epi32(reinterpret_cast<const int32_t *>(&histDec.cumulInv), slot3, sizeof(uint8_t));

      // retrieve pack.
      const simd_t pack0 = _mm256_i32gather_epi32(reinterpret_cast<const int32_t *>(&histDec.symbolFomCumul), slot0, sizeof(uint32_t));
      const simd_t pack1 = _mm256_i32gather_epi32(reinterpret_cast<const int32_t *>(&histDec.symbolFomCumul), slot1, sizeof(uint32_t));
      const simd_t pack2 = _mm256_i32gather_epi32(reinterpret_cast<const int32_t *>(&histDec.symbolFomCumul), slot2, sizeof(uint32_t));
      const simd_t pack3 = _mm256_i32gather_epi32(reinterpret_cast<const int32_t *>(&histDec.symbolFomCumul), slot3, sizeof(uint32_t));

      // const uint32_t shiftedState = (state >> TotalSymbolCountBits);
      const simd_t shiftedState0 = _mm256_srli_epi32(statesX8[0], TotalSymbolCountBits);
      const simd_t shiftedState1 = _mm256_srli_epi32(statesX8[1], TotalSymbolCountBits);
      const simd_t shiftedState2 = _mm256_srli_epi32(statesX8[2], TotalSymbolCountBits);
      const simd_t shiftedState3 = _mm256_srli_epi32(statesX8[3], TotalSymbolCountBits);

      // since they were int32_t turn into uint8_t
      symbol0 = _mm256_and_si256(symbol0, lower8);
      symbol1 = _mm256_and_si256(symbol1, lower8);
      symbol2 = _mm256_and_si256(symbol2, lower8);
      symbol3 = _mm256_and_si256(symbol3, lower8);

      // pack symbols to one si256. (could possibly be `_mm256_cvtepi32_epi8` on avx-512f + avx-512-vl)
      const simd_t symPack01 = _mm256_packus_epi32(symbol0, symbol1);
      const simd_t symPack23 = _mm256_packus_epi32(symbol2, symbol3);
      const simd_t symPack0123 = _mm256_packus_epi16(symPack01, symPack23); // `00 01 02 03 08 09 0A 0B 10 11 12 13 18 19 1A 1B 04 05 06 07 0C 0D 0E 0F 14 15 16 17 1C 1D 1E 1F`

      // We intentionally encoded in a way to not have to do horrible things here.
      _mm256_storeu_si256(reinterpret_cast<__m256i *>(pOutData + i + StateCount), symPack0123);

      // freq, cumul.
      const simd_t cumul0 = _mm256_srli_epi32(pack0, 16);
      const simd_t freq0 = _mm256_and_si256(pack0, lower16);
      const simd_t cumul1 = _mm256_srli_epi32(pack1, 16);
      const simd_t freq1 = _mm256_and_si256(pack1, lower16);
      const simd_t cumul2 = _mm256_srli_epi32(pack2, 16);
      const simd_t freq2 = _mm256_and_si256(pack2, lower16);
      const simd_t cumul3 = _mm256_srli_epi32(pack3, 16);
      const simd_t freq3 = _mm256_and_si256(pack3, lower16);

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
      
      // (state < DecodeConsumePoint16) ? -1 : 0 | well, actually (DecodeConsumePoint16 - 1 > state) ? 1 : 0
      const simd_t cmp0 = _mm256_cmpgt_epi32(decodeConsumePoint, state0);
      const simd_t cmp1 = _mm256_cmpgt_epi32(decodeConsumePoint, state1);
      const simd_t cmp2 = _mm256_cmpgt_epi32(decodeConsumePoint, state2);
      const simd_t cmp3 = _mm256_cmpgt_epi32(decodeConsumePoint, state3);

      // matching: state << 16
      const simd_t extractMatchShifted0 = _mm256_slli_epi32(_mm256_and_si256(state0, cmp0), 16);
      const simd_t extractMatchShifted1 = _mm256_slli_epi32(_mm256_and_si256(state1, cmp1), 16);
      const simd_t extractMatchShifted2 = _mm256_slli_epi32(_mm256_and_si256(state2, cmp2), 16);
      const simd_t extractMatchShifted3 = _mm256_slli_epi32(_mm256_and_si256(state3, cmp3), 16);

      // Increment matching read head indexes | well, actually subtract -1.
      readHeadOffsetsX8[0] = _mm256_sub_epi32(readHeadOffsetsX8[0], cmp0);
      readHeadOffsetsX8[1] = _mm256_sub_epi32(readHeadOffsetsX8[1], cmp1);
      readHeadOffsetsX8[2] = _mm256_sub_epi32(readHeadOffsetsX8[2], cmp2);
      readHeadOffsetsX8[3] = _mm256_sub_epi32(readHeadOffsetsX8[3], cmp3);

      // grab current byte from the read heads.
      const simd_t extractMatchNextByte0 = _mm256_and_si256(_mm256_and_si256(cmp0, lower16), newWord0);
      const simd_t extractMatchNextByte1 = _mm256_and_si256(_mm256_and_si256(cmp1, lower16), newWord1);
      const simd_t extractMatchNextByte2 = _mm256_and_si256(_mm256_and_si256(cmp2, lower16), newWord2);
      const simd_t extractMatchNextByte3 = _mm256_and_si256(_mm256_and_si256(cmp3, lower16), newWord3);

      // matching: (state << 16) | newWord
      const simd_t matchingStates0 = _mm256_or_si256(extractMatchShifted0, extractMatchNextByte0);
      const simd_t matchingStates1 = _mm256_or_si256(extractMatchShifted1, extractMatchNextByte1);
      const simd_t matchingStates2 = _mm256_or_si256(extractMatchShifted2, extractMatchNextByte2);
      const simd_t matchingStates3 = _mm256_or_si256(extractMatchShifted3, extractMatchNextByte3);

      // select non matching.
      const simd_t nonMatchingStates0 = _mm256_andnot_si256(cmp0, state0);
      const simd_t nonMatchingStates1 = _mm256_andnot_si256(cmp1, state1);
      const simd_t nonMatchingStates2 = _mm256_andnot_si256(cmp2, state2);
      const simd_t nonMatchingStates3 = _mm256_andnot_si256(cmp3, state3);

      // combine.
      statesX8[0] = _mm256_or_si256(matchingStates0, nonMatchingStates0);
      statesX8[1] = _mm256_or_si256(matchingStates1, nonMatchingStates1);
      statesX8[2] = _mm256_or_si256(matchingStates2, nonMatchingStates2);
      statesX8[3] = _mm256_or_si256(matchingStates3, nonMatchingStates3);

      // no need to derive next byte, we'll gather anyways.
    }
  }

  for (; i < outLengthInStates; i += StateCount)
  {
    // const uint32_t slot = state & (TotalSymbolCount - 1);
    const simd_t slot0 = _mm256_and_si256(statesX8[0], symCountMask);
    const simd_t slot1 = _mm256_and_si256(statesX8[1], symCountMask);
    const simd_t slot2 = _mm256_and_si256(statesX8[2], symCountMask);
    const simd_t slot3 = _mm256_and_si256(statesX8[3], symCountMask);

    // const uint8_t symbol = pHist->cumulInv[slot];
    simd_t symbol0 = _mm256_i32gather_epi32(reinterpret_cast<const int32_t *>(&histDec.cumulInv), slot0, sizeof(uint8_t));
    simd_t symbol1 = _mm256_i32gather_epi32(reinterpret_cast<const int32_t *>(&histDec.cumulInv), slot1, sizeof(uint8_t));
    simd_t symbol2 = _mm256_i32gather_epi32(reinterpret_cast<const int32_t *>(&histDec.cumulInv), slot2, sizeof(uint8_t));
    simd_t symbol3 = _mm256_i32gather_epi32(reinterpret_cast<const int32_t *>(&histDec.cumulInv), slot3, sizeof(uint8_t));

    // retrieve pack.
    const simd_t pack0 = _mm256_i32gather_epi32(reinterpret_cast<const int32_t *>(&histDec.symbolFomCumul), slot0, sizeof(uint32_t));
    const simd_t pack1 = _mm256_i32gather_epi32(reinterpret_cast<const int32_t *>(&histDec.symbolFomCumul), slot1, sizeof(uint32_t));
    const simd_t pack2 = _mm256_i32gather_epi32(reinterpret_cast<const int32_t *>(&histDec.symbolFomCumul), slot2, sizeof(uint32_t));
    const simd_t pack3 = _mm256_i32gather_epi32(reinterpret_cast<const int32_t *>(&histDec.symbolFomCumul), slot3, sizeof(uint32_t));

    // const uint32_t shiftedState = (state >> TotalSymbolCountBits);
    const simd_t shiftedState0 = _mm256_srli_epi32(statesX8[0], TotalSymbolCountBits);
    const simd_t shiftedState1 = _mm256_srli_epi32(statesX8[1], TotalSymbolCountBits);
    const simd_t shiftedState2 = _mm256_srli_epi32(statesX8[2], TotalSymbolCountBits);
    const simd_t shiftedState3 = _mm256_srli_epi32(statesX8[3], TotalSymbolCountBits);

    // since they were int32_t turn into uint8_t
    symbol0 = _mm256_and_si256(symbol0, lower8);
    symbol1 = _mm256_and_si256(symbol1, lower8);
    symbol2 = _mm256_and_si256(symbol2, lower8);
    symbol3 = _mm256_and_si256(symbol3, lower8);

    // pack symbols to one si256. (could possibly be `_mm256_cvtepi32_epi8` on avx-512f + avx-512-vl)
    const simd_t symPack01 = _mm256_packus_epi32(symbol0, symbol1);
    const simd_t symPack23 = _mm256_packus_epi32(symbol2, symbol3);
    const simd_t symPack0123 = _mm256_packus_epi16(symPack01, symPack23); // `00 01 02 03 08 09 0A 0B 10 11 12 13 18 19 1A 1B 04 05 06 07 0C 0D 0E 0F 14 15 16 17 1C 1D 1E 1F`

    // We intentionally encoded in a way to not have to do horrible things here.
    _mm256_storeu_si256(reinterpret_cast<__m256i *>(pOutData + i), symPack0123);

    // freq, cumul.
    const simd_t cumul0 = _mm256_srli_epi32(pack0, 16);
    const simd_t freq0 = _mm256_and_si256(pack0, lower16);
    const simd_t cumul1 = _mm256_srli_epi32(pack1, 16);
    const simd_t freq1 = _mm256_and_si256(pack1, lower16);
    const simd_t cumul2 = _mm256_srli_epi32(pack2, 16);
    const simd_t freq2 = _mm256_and_si256(pack2, lower16);
    const simd_t cumul3 = _mm256_srli_epi32(pack3, 16);
    const simd_t freq3 = _mm256_and_si256(pack3, lower16);

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
    
    // request next uint16_t.
    const simd_t newWord0 = _mm256_i32gather_epi32(pReadHeadSIMD, readHeadOffsetsX8[0], sizeof(uint16_t));
    const simd_t newWord1 = _mm256_i32gather_epi32(pReadHeadSIMD, readHeadOffsetsX8[1], sizeof(uint16_t));
    const simd_t newWord2 = _mm256_i32gather_epi32(pReadHeadSIMD, readHeadOffsetsX8[2], sizeof(uint16_t));
    const simd_t newWord3 = _mm256_i32gather_epi32(pReadHeadSIMD, readHeadOffsetsX8[3], sizeof(uint16_t));

    // (state < DecodeConsumePoint16) ? -1 : 0 | well, actually (DecodeConsumePoint16 - 1 > state) ? 1 : 0
    const simd_t cmp0 = _mm256_cmpgt_epi32(decodeConsumePoint, state0);
    const simd_t cmp1 = _mm256_cmpgt_epi32(decodeConsumePoint, state1);
    const simd_t cmp2 = _mm256_cmpgt_epi32(decodeConsumePoint, state2);
    const simd_t cmp3 = _mm256_cmpgt_epi32(decodeConsumePoint, state3);

    // matching: state << 16
    const simd_t extractMatchShifted0 = _mm256_slli_epi32(_mm256_and_si256(state0, cmp0), 16);
    const simd_t extractMatchShifted1 = _mm256_slli_epi32(_mm256_and_si256(state1, cmp1), 16);
    const simd_t extractMatchShifted2 = _mm256_slli_epi32(_mm256_and_si256(state2, cmp2), 16);
    const simd_t extractMatchShifted3 = _mm256_slli_epi32(_mm256_and_si256(state3, cmp3), 16);

    // Increment matching read head indexes | well, actually subtract -1.
    readHeadOffsetsX8[0] = _mm256_sub_epi32(readHeadOffsetsX8[0], cmp0);
    readHeadOffsetsX8[1] = _mm256_sub_epi32(readHeadOffsetsX8[1], cmp1);
    readHeadOffsetsX8[2] = _mm256_sub_epi32(readHeadOffsetsX8[2], cmp2);
    readHeadOffsetsX8[3] = _mm256_sub_epi32(readHeadOffsetsX8[3], cmp3);

    // grab current uint16_t from the read heads.
    const simd_t extractMatchNextByte0 = _mm256_and_si256(_mm256_and_si256(cmp0, lower16), newWord0);
    const simd_t extractMatchNextByte1 = _mm256_and_si256(_mm256_and_si256(cmp1, lower16), newWord1);
    const simd_t extractMatchNextByte2 = _mm256_and_si256(_mm256_and_si256(cmp2, lower16), newWord2);
    const simd_t extractMatchNextByte3 = _mm256_and_si256(_mm256_and_si256(cmp3, lower16), newWord3);

    // matching: (state << 16) | newWord
    const simd_t matchingStates0 = _mm256_or_si256(extractMatchShifted0, extractMatchNextByte0);
    const simd_t matchingStates1 = _mm256_or_si256(extractMatchShifted1, extractMatchNextByte1);
    const simd_t matchingStates2 = _mm256_or_si256(extractMatchShifted2, extractMatchNextByte2);
    const simd_t matchingStates3 = _mm256_or_si256(extractMatchShifted3, extractMatchNextByte3);

    // select non matching.
    const simd_t nonMatchingStates0 = _mm256_andnot_si256(cmp0, state0);
    const simd_t nonMatchingStates1 = _mm256_andnot_si256(cmp1, state1);
    const simd_t nonMatchingStates2 = _mm256_andnot_si256(cmp2, state2);
    const simd_t nonMatchingStates3 = _mm256_andnot_si256(cmp3, state3);

    // combine.
    statesX8[0] = _mm256_or_si256(matchingStates0, nonMatchingStates0);
    statesX8[1] = _mm256_or_si256(matchingStates1, nonMatchingStates1);
    statesX8[2] = _mm256_or_si256(matchingStates2, nonMatchingStates2);
    statesX8[3] = _mm256_or_si256(matchingStates3, nonMatchingStates3);
  }

  for (size_t j = 0; j < sizeof(statesX8) / sizeof(simd_t); j++)
  {
    _mm256_storeu_si256(reinterpret_cast<simd_t *>(reinterpret_cast<uint8_t *>(states) + j * sizeof(simd_t)), statesX8[j]);
    _mm256_storeu_si256(reinterpret_cast<simd_t *>(reinterpret_cast<uint8_t *>(readHeadOffsets) + j * sizeof(simd_t)), readHeadOffsetsX8[j]);
  }

  for (size_t j = 0; j < StateCount; j++)
    pReadHead[j] = reinterpret_cast<const uint8_t *>(reinterpret_cast<const uint16_t *>(pReadHeadSIMD) + readHeadOffsets[j]);

  const uint8_t idx2idx[] = { 0x00, 0x01, 0x02, 0x03, 0x10, 0x11, 0x12, 0x13, 0x04, 0x05, 0x06, 0x07, 0x14, 0x15, 0x16, 0x17, 0x08, 0x09, 0x0A, 0x0B , 0x18, 0x19, 0x1A, 0x1B, 0x0C, 0x0D, 0x0E, 0x0F, 0x1C, 0x1D, 0x1E, 0x1F };
  static_assert(sizeof(idx2idx) == StateCount);

  for (size_t j = 0; j < StateCount; j++)
  {
    const uint8_t index = idx2idx[j];

    if (i + index < expectedOutputLength)
    {
      uint32_t state = states[j];

      const uint32_t slot = state & (TotalSymbolCount - 1);
      const uint8_t symbol = histDec.cumulInv[slot];
      state = (state >> TotalSymbolCountBits) * (uint32_t)histDec.symbolFomCumul[slot].freq + slot - (uint32_t)histDec.symbolFomCumul[slot].cumul;

      pOutData[i + index] = symbol;

      if constexpr (DecodeNoBranch)
      {
        const bool read = state < DecodeConsumePoint16;
        const uint32_t newState = state << 16 | *reinterpret_cast<const uint16_t *>(pReadHead[j]);
        state = read ? newState : state;
        pReadHead[j] += sizeof(uint16_t) * (size_t)read;
      }
      else
      {
        if (state < DecodeConsumePoint16)
        {
          state = state << 16 | *reinterpret_cast<const uint16_t *>(pReadHead[j]);
          pReadHead[j] += sizeof(uint16_t);
        }
      }

      states[j] = state;
    }
  }

  return expectedOutputLength;
}

//////////////////////////////////////////////////////////////////////////

template <uint32_t TotalSymbolCountBits>
#ifndef _MSC_VER
__attribute__((target("avx2")))
#endif
size_t rANS32x32_32blk_16w_decode_avx2_varC(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity)
{
  if (inLength <= sizeof(uint64_t) * 2)
    return 0;

  static_assert(TotalSymbolCountBits <= 12);
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

  hist_t hist;

  for (size_t i = 0; i < 256; i++)
  {
    hist.symbolCount[i] = *reinterpret_cast<const uint16_t *>(pInData + inputIndex);
    inputIndex += sizeof(uint16_t);
  }

  if (!inplace_complete_hist(&hist, TotalSymbolCountBits))
    return 0;

  hist_dec_pack_t<TotalSymbolCountBits> histDec;
  make_dec_pack_hist<TotalSymbolCountBits>(&histDec, &hist);

  uint32_t states[StateCount];

  for (size_t i = 0; i < StateCount; i++)
  {
    states[i] = *reinterpret_cast<const uint32_t *>(pInData + inputIndex);
    inputIndex += sizeof(uint32_t);
  }

  const uint8_t *pReadHead[StateCount];
  pReadHead[0] = pInData + inputIndex + sizeof(uint32_t) * (StateCount - 1);

  int32_t readHeadOffsets[StateCount];
  readHeadOffsets[0] = 0;

  const int32_t *pReadHeadSIMD = reinterpret_cast<const int32_t *>(pReadHead[0]);

  for (size_t i = 1; i < StateCount; i++)
  {
    const uint32_t blockSize = *reinterpret_cast<const uint32_t *>(pInData + inputIndex);
    inputIndex += sizeof(uint32_t);

    pReadHead[i] = pReadHead[i - 1] + blockSize;
    readHeadOffsets[i] = (uint32_t)(pReadHead[i] - reinterpret_cast<const uint8_t *>(pReadHeadSIMD)) / sizeof(uint16_t);
  }

  typedef __m256i simd_t;
  simd_t statesX8[StateCount / (sizeof(simd_t) / sizeof(uint32_t))];
  simd_t readHeadOffsetsX8[StateCount / (sizeof(simd_t) / sizeof(uint32_t))];

  for (size_t i = 0; i < sizeof(statesX8) / sizeof(simd_t); i++)
  {
    statesX8[i] = _mm256_loadu_si256(reinterpret_cast<const simd_t *>(reinterpret_cast<const uint8_t *>(states) + i * sizeof(simd_t)));
    readHeadOffsetsX8[i] = _mm256_loadu_si256(reinterpret_cast<const simd_t *>(reinterpret_cast<const uint8_t *>(readHeadOffsets) + i * sizeof(simd_t)));
  }

  const size_t outLengthInStates = expectedOutputLength - StateCount + 1;
  size_t i = 0;

  const simd_t symCountMask = _mm256_set1_epi32(TotalSymbolCount - 1);
  const simd_t lower16 = _mm256_set1_epi32(0xFFFF);
  const simd_t lower12 = _mm256_set1_epi32((1 << 12) - 1);
  const simd_t lower8 = _mm256_set1_epi32(0xFF);
  const simd_t decodeConsumePoint = _mm256_set1_epi32(DecodeConsumePoint16);

  for (; i < outLengthInStates; i += StateCount)
  {
    // const uint32_t slot = state & (TotalSymbolCount - 1);
    const simd_t slot0 = _mm256_and_si256(statesX8[0], symCountMask);
    const simd_t slot1 = _mm256_and_si256(statesX8[1], symCountMask);
    const simd_t slot2 = _mm256_and_si256(statesX8[2], symCountMask);
    const simd_t slot3 = _mm256_and_si256(statesX8[3], symCountMask);

    // retrieve pack.
    const simd_t pack0 = _mm256_i32gather_epi32(reinterpret_cast<const int32_t *>(&histDec.symbol), slot0, sizeof(uint32_t));
    const simd_t pack1 = _mm256_i32gather_epi32(reinterpret_cast<const int32_t *>(&histDec.symbol), slot1, sizeof(uint32_t));
    const simd_t pack2 = _mm256_i32gather_epi32(reinterpret_cast<const int32_t *>(&histDec.symbol), slot2, sizeof(uint32_t));
    const simd_t pack3 = _mm256_i32gather_epi32(reinterpret_cast<const int32_t *>(&histDec.symbol), slot3, sizeof(uint32_t));

    // const uint32_t shiftedState = (state >> TotalSymbolCountBits);
    const simd_t shiftedState0 = _mm256_srli_epi32(statesX8[0], TotalSymbolCountBits);
    const simd_t shiftedState1 = _mm256_srli_epi32(statesX8[1], TotalSymbolCountBits);
    const simd_t shiftedState2 = _mm256_srli_epi32(statesX8[2], TotalSymbolCountBits);
    const simd_t shiftedState3 = _mm256_srli_epi32(statesX8[3], TotalSymbolCountBits);

    // unpack symbol.
    const simd_t symbol0 = _mm256_and_si256(pack0, lower8);
    const simd_t symbol1 = _mm256_and_si256(pack1, lower8);
    const simd_t symbol2 = _mm256_and_si256(pack2, lower8);
    const simd_t symbol3 = _mm256_and_si256(pack3, lower8);

    // pack symbols to one si256. (could possibly be `_mm256_cvtepi32_epi8` on avx-512f + avx-512-vl)
    const simd_t symPack01 = _mm256_packus_epi32(symbol0, symbol1);
    const simd_t symPack23 = _mm256_packus_epi32(symbol2, symbol3);
    const simd_t symPack0123 = _mm256_packus_epi16(symPack01, symPack23); // `00 01 02 03 08 09 0A 0B 10 11 12 13 18 19 1A 1B 04 05 06 07 0C 0D 0E 0F 14 15 16 17 1C 1D 1E 1F`

    // We intentionally encoded in a way to not have to do horrible things here.
    _mm256_storeu_si256(reinterpret_cast<__m256i *>(pOutData + i), symPack0123);

    // unpack freq, cumul.
    const simd_t cumul0 = _mm256_and_si256(_mm256_srli_epi32(pack0, 8), lower12);
    const simd_t freq0 = _mm256_srli_epi32(pack0, 20);
    const simd_t cumul1 = _mm256_and_si256(_mm256_srli_epi32(pack1, 8), lower12);
    const simd_t freq1 = _mm256_srli_epi32(pack1, 20);
    const simd_t cumul2 = _mm256_and_si256(_mm256_srli_epi32(pack2, 8), lower12);
    const simd_t freq2 = _mm256_srli_epi32(pack2, 20);
    const simd_t cumul3 = _mm256_and_si256(_mm256_srli_epi32(pack3, 8), lower12);
    const simd_t freq3 = _mm256_srli_epi32(pack3, 20);

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
    
    // request next uint16_t.
    const simd_t newWord0 = _mm256_i32gather_epi32(pReadHeadSIMD, readHeadOffsetsX8[0], sizeof(uint16_t));
    const simd_t newWord1 = _mm256_i32gather_epi32(pReadHeadSIMD, readHeadOffsetsX8[1], sizeof(uint16_t));
    const simd_t newWord2 = _mm256_i32gather_epi32(pReadHeadSIMD, readHeadOffsetsX8[2], sizeof(uint16_t));
    const simd_t newWord3 = _mm256_i32gather_epi32(pReadHeadSIMD, readHeadOffsetsX8[3], sizeof(uint16_t));

    // (state < DecodeConsumePoint16) ? -1 : 0 | well, actually (DecodeConsumePoint16 - 1 > state) ? 1 : 0
    const simd_t cmp0 = _mm256_cmpgt_epi32(decodeConsumePoint, state0);
    const simd_t cmp1 = _mm256_cmpgt_epi32(decodeConsumePoint, state1);
    const simd_t cmp2 = _mm256_cmpgt_epi32(decodeConsumePoint, state2);
    const simd_t cmp3 = _mm256_cmpgt_epi32(decodeConsumePoint, state3);

    // matching: state << 16
    const simd_t extractMatchShifted0 = _mm256_slli_epi32(_mm256_and_si256(state0, cmp0), 16);
    const simd_t extractMatchShifted1 = _mm256_slli_epi32(_mm256_and_si256(state1, cmp1), 16);
    const simd_t extractMatchShifted2 = _mm256_slli_epi32(_mm256_and_si256(state2, cmp2), 16);
    const simd_t extractMatchShifted3 = _mm256_slli_epi32(_mm256_and_si256(state3, cmp3), 16);

    // Increment matching read head indexes | well, actually subtract -1.
    readHeadOffsetsX8[0] = _mm256_sub_epi32(readHeadOffsetsX8[0], cmp0);
    readHeadOffsetsX8[1] = _mm256_sub_epi32(readHeadOffsetsX8[1], cmp1);
    readHeadOffsetsX8[2] = _mm256_sub_epi32(readHeadOffsetsX8[2], cmp2);
    readHeadOffsetsX8[3] = _mm256_sub_epi32(readHeadOffsetsX8[3], cmp3);

    // grab current byte from the read heads.
    const simd_t extractMatchNextByte0 = _mm256_and_si256(_mm256_and_si256(cmp0, lower16), newWord0);
    const simd_t extractMatchNextByte1 = _mm256_and_si256(_mm256_and_si256(cmp1, lower16), newWord1);
    const simd_t extractMatchNextByte2 = _mm256_and_si256(_mm256_and_si256(cmp2, lower16), newWord2);
    const simd_t extractMatchNextByte3 = _mm256_and_si256(_mm256_and_si256(cmp3, lower16), newWord3);

    // matching: (state << 16) | newWord
    const simd_t matchingStates0 = _mm256_or_si256(extractMatchShifted0, extractMatchNextByte0);
    const simd_t matchingStates1 = _mm256_or_si256(extractMatchShifted1, extractMatchNextByte1);
    const simd_t matchingStates2 = _mm256_or_si256(extractMatchShifted2, extractMatchNextByte2);
    const simd_t matchingStates3 = _mm256_or_si256(extractMatchShifted3, extractMatchNextByte3);

    // select non matching.
    const simd_t nonMatchingStates0 = _mm256_andnot_si256(cmp0, state0);
    const simd_t nonMatchingStates1 = _mm256_andnot_si256(cmp1, state1);
    const simd_t nonMatchingStates2 = _mm256_andnot_si256(cmp2, state2);
    const simd_t nonMatchingStates3 = _mm256_andnot_si256(cmp3, state3);

    // combine.
    statesX8[0] = _mm256_or_si256(matchingStates0, nonMatchingStates0);
    statesX8[1] = _mm256_or_si256(matchingStates1, nonMatchingStates1);
    statesX8[2] = _mm256_or_si256(matchingStates2, nonMatchingStates2);
    statesX8[3] = _mm256_or_si256(matchingStates3, nonMatchingStates3);
  }

  for (size_t j = 0; j < sizeof(statesX8) / sizeof(simd_t); j++)
  {
    _mm256_storeu_si256(reinterpret_cast<simd_t *>(reinterpret_cast<uint8_t *>(states) + j * sizeof(simd_t)), statesX8[j]);
    _mm256_storeu_si256(reinterpret_cast<simd_t *>(reinterpret_cast<uint8_t *>(readHeadOffsets) + j * sizeof(simd_t)), readHeadOffsetsX8[j]);
  }

  for (size_t j = 0; j < StateCount; j++)
    pReadHead[j] = reinterpret_cast<const uint8_t *>(reinterpret_cast<const uint16_t *>(pReadHeadSIMD) + readHeadOffsets[j]);

  const uint8_t idx2idx[] = { 0x00, 0x01, 0x02, 0x03, 0x10, 0x11, 0x12, 0x13, 0x04, 0x05, 0x06, 0x07, 0x14, 0x15, 0x16, 0x17, 0x08, 0x09, 0x0A, 0x0B , 0x18, 0x19, 0x1A, 0x1B, 0x0C, 0x0D, 0x0E, 0x0F, 0x1C, 0x1D, 0x1E, 0x1F };
  static_assert(sizeof(idx2idx) == StateCount);

  for (size_t j = 0; j < StateCount; j++)
  {
    const uint8_t index = idx2idx[j];

    if (i + index < expectedOutputLength)
    {
      uint32_t state = states[j];

      const uint32_t slot = state & (TotalSymbolCount - 1);
      const uint8_t symbol = histDec.symbol[slot] & 0xFF;
      state = (state >> TotalSymbolCountBits) * (uint32_t)(histDec.symbol[slot] >> 20) + slot - (uint32_t)((histDec.symbol[slot] >> 8) & 0b1111111111);

      pOutData[i + index] = symbol;

      if constexpr (DecodeNoBranch)
      {
        const bool read = state < DecodeConsumePoint16;
        const uint32_t newState = state << 16 | *reinterpret_cast<const uint16_t *>(pReadHead[j]);
        state = read ? newState : state;
        pReadHead[j] += sizeof(uint16_t) * (size_t)read;
      }
      else
      {
        if (state < DecodeConsumePoint16)
        {
          state = state << 16 | *reinterpret_cast<const uint16_t *>(pReadHead[j]);
          pReadHead[j] += sizeof(uint16_t);
        }
      }

      states[j] = state;
    }
  }

  return expectedOutputLength;
}

template <uint32_t TotalSymbolCountBits>
#ifndef _MSC_VER
__attribute__((target("avx2")))
#endif
size_t rANS32x32_32blk_16w_decode_avx2_varC2(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity)
{
  if (inLength <= sizeof(uint64_t) * 2)
    return 0;

  static_assert(TotalSymbolCountBits <= 12);
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

  hist_t hist;

  for (size_t i = 0; i < 256; i++)
  {
    hist.symbolCount[i] = *reinterpret_cast<const uint16_t *>(pInData + inputIndex);
    inputIndex += sizeof(uint16_t);
  }

  if (!inplace_complete_hist(&hist, TotalSymbolCountBits))
    return 0;

  hist_dec_pack_t<TotalSymbolCountBits> histDec;
  make_dec_pack_hist<TotalSymbolCountBits>(&histDec, &hist);

  uint32_t states[StateCount];

  for (size_t i = 0; i < StateCount; i++)
  {
    states[i] = *reinterpret_cast<const uint32_t *>(pInData + inputIndex);
    inputIndex += sizeof(uint32_t);
  }

  const uint8_t *pReadHead[StateCount];
  pReadHead[0] = pInData + inputIndex + sizeof(uint32_t) * (StateCount - 1);

  int32_t readHeadOffsets[StateCount];
  readHeadOffsets[0] = 0;

  const int32_t *pReadHeadSIMD = reinterpret_cast<const int32_t *>(pReadHead[0]);

  for (size_t i = 1; i < StateCount; i++)
  {
    const uint32_t blockSize = *reinterpret_cast<const uint32_t *>(pInData + inputIndex);
    inputIndex += sizeof(uint32_t);

    pReadHead[i] = pReadHead[i - 1] + blockSize;
    readHeadOffsets[i] = (uint32_t)(pReadHead[i] - reinterpret_cast<const uint8_t *>(pReadHeadSIMD)) / sizeof(uint16_t);
  }

  typedef __m256i simd_t;
  simd_t statesX8[StateCount / (sizeof(simd_t) / sizeof(uint32_t))];
  simd_t readHeadOffsetsX8[StateCount / (sizeof(simd_t) / sizeof(uint32_t))];

  for (size_t i = 0; i < sizeof(statesX8) / sizeof(simd_t); i++)
  {
    statesX8[i] = _mm256_loadu_si256(reinterpret_cast<const simd_t *>(reinterpret_cast<const uint8_t *>(states) + i * sizeof(simd_t)));
    readHeadOffsetsX8[i] = _mm256_loadu_si256(reinterpret_cast<const simd_t *>(reinterpret_cast<const uint8_t *>(readHeadOffsets) + i * sizeof(simd_t)));
  }

  const size_t outLengthInStates = expectedOutputLength - StateCount + 1;
  const size_t outLengthInDoubleStates = expectedOutputLength - StateCount * 2 + 1;
  size_t i = 0;

  const simd_t symCountMask = _mm256_set1_epi32(TotalSymbolCount - 1);
  const simd_t lower16 = _mm256_set1_epi32(0xFFFF);
  const simd_t lower12 = _mm256_set1_epi32((1 << 12) - 1);
  const simd_t lower8 = _mm256_set1_epi32(0xFF);
  const simd_t decodeConsumePoint = _mm256_set1_epi32(DecodeConsumePoint16);

  for (; i < outLengthInDoubleStates; i += StateCount * 2)
  {
    // request next uint16_t.
    simd_t newWord0 = _mm256_i32gather_epi32(pReadHeadSIMD, readHeadOffsetsX8[0], sizeof(uint16_t));
    simd_t newWord1 = _mm256_i32gather_epi32(pReadHeadSIMD, readHeadOffsetsX8[1], sizeof(uint16_t));
    simd_t newWord2 = _mm256_i32gather_epi32(pReadHeadSIMD, readHeadOffsetsX8[2], sizeof(uint16_t));
    simd_t newWord3 = _mm256_i32gather_epi32(pReadHeadSIMD, readHeadOffsetsX8[3], sizeof(uint16_t));

    //////////////////////////////////////////////////////////////////////////
    // Block 1:
    {
      // const uint32_t slot = state & (TotalSymbolCount - 1);
      const simd_t slot0 = _mm256_and_si256(statesX8[0], symCountMask);
      const simd_t slot1 = _mm256_and_si256(statesX8[1], symCountMask);
      const simd_t slot2 = _mm256_and_si256(statesX8[2], symCountMask);
      const simd_t slot3 = _mm256_and_si256(statesX8[3], symCountMask);

      // retrieve pack.
      const simd_t pack0 = _mm256_i32gather_epi32(reinterpret_cast<const int32_t *>(&histDec.symbol), slot0, sizeof(uint32_t));
      const simd_t pack1 = _mm256_i32gather_epi32(reinterpret_cast<const int32_t *>(&histDec.symbol), slot1, sizeof(uint32_t));
      const simd_t pack2 = _mm256_i32gather_epi32(reinterpret_cast<const int32_t *>(&histDec.symbol), slot2, sizeof(uint32_t));
      const simd_t pack3 = _mm256_i32gather_epi32(reinterpret_cast<const int32_t *>(&histDec.symbol), slot3, sizeof(uint32_t));

      // const uint32_t shiftedState = (state >> TotalSymbolCountBits);
      const simd_t shiftedState0 = _mm256_srli_epi32(statesX8[0], TotalSymbolCountBits);
      const simd_t shiftedState1 = _mm256_srli_epi32(statesX8[1], TotalSymbolCountBits);
      const simd_t shiftedState2 = _mm256_srli_epi32(statesX8[2], TotalSymbolCountBits);
      const simd_t shiftedState3 = _mm256_srli_epi32(statesX8[3], TotalSymbolCountBits);

      // unpack symbol.
      const simd_t symbol0 = _mm256_and_si256(pack0, lower8);
      const simd_t symbol1 = _mm256_and_si256(pack1, lower8);
      const simd_t symbol2 = _mm256_and_si256(pack2, lower8);
      const simd_t symbol3 = _mm256_and_si256(pack3, lower8);

      // pack symbols to one si256. (could possibly be `_mm256_cvtepi32_epi8` on avx-512f + avx-512-vl)
      const simd_t symPack01 = _mm256_packus_epi32(symbol0, symbol1);
      const simd_t symPack23 = _mm256_packus_epi32(symbol2, symbol3);
      const simd_t symPack0123 = _mm256_packus_epi16(symPack01, symPack23); // `00 01 02 03 08 09 0A 0B 10 11 12 13 18 19 1A 1B 04 05 06 07 0C 0D 0E 0F 14 15 16 17 1C 1D 1E 1F`

      // We intentionally encoded in a way to not have to do horrible things here.
      _mm256_storeu_si256(reinterpret_cast<__m256i *>(pOutData + i), symPack0123);

      // unpack freq, cumul.
      const simd_t cumul0 = _mm256_and_si256(_mm256_srli_epi32(pack0, 8), lower12);
      const simd_t freq0 = _mm256_srli_epi32(pack0, 20);
      const simd_t cumul1 = _mm256_and_si256(_mm256_srli_epi32(pack1, 8), lower12);
      const simd_t freq1 = _mm256_srli_epi32(pack1, 20);
      const simd_t cumul2 = _mm256_and_si256(_mm256_srli_epi32(pack2, 8), lower12);
      const simd_t freq2 = _mm256_srli_epi32(pack2, 20);
      const simd_t cumul3 = _mm256_and_si256(_mm256_srli_epi32(pack3, 8), lower12);
      const simd_t freq3 = _mm256_srli_epi32(pack3, 20);

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
      
      // (state < DecodeConsumePoint16) ? -1 : 0 | well, actually (DecodeConsumePoint16 - 1 > state) ? 1 : 0
      const simd_t cmp0 = _mm256_cmpgt_epi32(decodeConsumePoint, state0);
      const simd_t cmp1 = _mm256_cmpgt_epi32(decodeConsumePoint, state1);
      const simd_t cmp2 = _mm256_cmpgt_epi32(decodeConsumePoint, state2);
      const simd_t cmp3 = _mm256_cmpgt_epi32(decodeConsumePoint, state3);

      // matching: state << 16
      const simd_t extractMatchShifted0 = _mm256_slli_epi32(_mm256_and_si256(state0, cmp0), 16);
      const simd_t extractMatchShifted1 = _mm256_slli_epi32(_mm256_and_si256(state1, cmp1), 16);
      const simd_t extractMatchShifted2 = _mm256_slli_epi32(_mm256_and_si256(state2, cmp2), 16);
      const simd_t extractMatchShifted3 = _mm256_slli_epi32(_mm256_and_si256(state3, cmp3), 16);

      // Increment matching read head indexes | well, actually subtract -1.
      readHeadOffsetsX8[0] = _mm256_sub_epi32(readHeadOffsetsX8[0], cmp0);
      readHeadOffsetsX8[1] = _mm256_sub_epi32(readHeadOffsetsX8[1], cmp1);
      readHeadOffsetsX8[2] = _mm256_sub_epi32(readHeadOffsetsX8[2], cmp2);
      readHeadOffsetsX8[3] = _mm256_sub_epi32(readHeadOffsetsX8[3], cmp3);

      // grab current byte from the read heads.
      const simd_t extractMatchNextByte0 = _mm256_and_si256(_mm256_and_si256(cmp0, lower16), newWord0);
      const simd_t extractMatchNextByte1 = _mm256_and_si256(_mm256_and_si256(cmp1, lower16), newWord1);
      const simd_t extractMatchNextByte2 = _mm256_and_si256(_mm256_and_si256(cmp2, lower16), newWord2);
      const simd_t extractMatchNextByte3 = _mm256_and_si256(_mm256_and_si256(cmp3, lower16), newWord3);

      // matching: (state << 16) | newWord
      const simd_t matchingStates0 = _mm256_or_si256(extractMatchShifted0, extractMatchNextByte0);
      const simd_t matchingStates1 = _mm256_or_si256(extractMatchShifted1, extractMatchNextByte1);
      const simd_t matchingStates2 = _mm256_or_si256(extractMatchShifted2, extractMatchNextByte2);
      const simd_t matchingStates3 = _mm256_or_si256(extractMatchShifted3, extractMatchNextByte3);

      // select non matching.
      const simd_t nonMatchingStates0 = _mm256_andnot_si256(cmp0, state0);
      const simd_t nonMatchingStates1 = _mm256_andnot_si256(cmp1, state1);
      const simd_t nonMatchingStates2 = _mm256_andnot_si256(cmp2, state2);
      const simd_t nonMatchingStates3 = _mm256_andnot_si256(cmp3, state3);

      // combine.
      statesX8[0] = _mm256_or_si256(matchingStates0, nonMatchingStates0);
      statesX8[1] = _mm256_or_si256(matchingStates1, nonMatchingStates1);
      statesX8[2] = _mm256_or_si256(matchingStates2, nonMatchingStates2);
      statesX8[3] = _mm256_or_si256(matchingStates3, nonMatchingStates3);

      // derive next byte.
      newWord0 = _mm256_or_si256(_mm256_andnot_si256(cmp0, newWord0), _mm256_and_si256(cmp0, _mm256_srli_epi32(newWord0, 16)));
      newWord1 = _mm256_or_si256(_mm256_andnot_si256(cmp1, newWord1), _mm256_and_si256(cmp1, _mm256_srli_epi32(newWord1, 16)));
      newWord2 = _mm256_or_si256(_mm256_andnot_si256(cmp2, newWord2), _mm256_and_si256(cmp2, _mm256_srli_epi32(newWord2, 16)));
      newWord3 = _mm256_or_si256(_mm256_andnot_si256(cmp3, newWord3), _mm256_and_si256(cmp3, _mm256_srli_epi32(newWord3, 16)));
    }

    //////////////////////////////////////////////////////////////////////////
    // Block 2:
    {
      // const uint32_t slot = state & (TotalSymbolCount - 1);
      const simd_t slot0 = _mm256_and_si256(statesX8[0], symCountMask);
      const simd_t slot1 = _mm256_and_si256(statesX8[1], symCountMask);
      const simd_t slot2 = _mm256_and_si256(statesX8[2], symCountMask);
      const simd_t slot3 = _mm256_and_si256(statesX8[3], symCountMask);

      // retrieve pack.
      const simd_t pack0 = _mm256_i32gather_epi32(reinterpret_cast<const int32_t *>(&histDec.symbol), slot0, sizeof(uint32_t));
      const simd_t pack1 = _mm256_i32gather_epi32(reinterpret_cast<const int32_t *>(&histDec.symbol), slot1, sizeof(uint32_t));
      const simd_t pack2 = _mm256_i32gather_epi32(reinterpret_cast<const int32_t *>(&histDec.symbol), slot2, sizeof(uint32_t));
      const simd_t pack3 = _mm256_i32gather_epi32(reinterpret_cast<const int32_t *>(&histDec.symbol), slot3, sizeof(uint32_t));

      // const uint32_t shiftedState = (state >> TotalSymbolCountBits);
      const simd_t shiftedState0 = _mm256_srli_epi32(statesX8[0], TotalSymbolCountBits);
      const simd_t shiftedState1 = _mm256_srli_epi32(statesX8[1], TotalSymbolCountBits);
      const simd_t shiftedState2 = _mm256_srli_epi32(statesX8[2], TotalSymbolCountBits);
      const simd_t shiftedState3 = _mm256_srli_epi32(statesX8[3], TotalSymbolCountBits);

      // unpack symbol.
      const simd_t symbol0 = _mm256_and_si256(pack0, lower8);
      const simd_t symbol1 = _mm256_and_si256(pack1, lower8);
      const simd_t symbol2 = _mm256_and_si256(pack2, lower8);
      const simd_t symbol3 = _mm256_and_si256(pack3, lower8);

      // pack symbols to one si256. (could possibly be `_mm256_cvtepi32_epi8` on avx-512f + avx-512-vl)
      const simd_t symPack01 = _mm256_packus_epi32(symbol0, symbol1);
      const simd_t symPack23 = _mm256_packus_epi32(symbol2, symbol3);
      const simd_t symPack0123 = _mm256_packus_epi16(symPack01, symPack23); // `00 01 02 03 08 09 0A 0B 10 11 12 13 18 19 1A 1B 04 05 06 07 0C 0D 0E 0F 14 15 16 17 1C 1D 1E 1F`

      // We intentionally encoded in a way to not have to do horrible things here.
      _mm256_storeu_si256(reinterpret_cast<__m256i *>(pOutData + i + StateCount), symPack0123);

      // unpack freq, cumul.
      const simd_t cumul0 = _mm256_and_si256(_mm256_srli_epi32(pack0, 8), lower12);
      const simd_t freq0 = _mm256_srli_epi32(pack0, 20);
      const simd_t cumul1 = _mm256_and_si256(_mm256_srli_epi32(pack1, 8), lower12);
      const simd_t freq1 = _mm256_srli_epi32(pack1, 20);
      const simd_t cumul2 = _mm256_and_si256(_mm256_srli_epi32(pack2, 8), lower12);
      const simd_t freq2 = _mm256_srli_epi32(pack2, 20);
      const simd_t cumul3 = _mm256_and_si256(_mm256_srli_epi32(pack3, 8), lower12);
      const simd_t freq3 = _mm256_srli_epi32(pack3, 20);

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
      
      // (state < DecodeConsumePoint16) ? -1 : 0 | well, actually (DecodeConsumePoint16 - 1 > state) ? 1 : 0
      const simd_t cmp0 = _mm256_cmpgt_epi32(decodeConsumePoint, state0);
      const simd_t cmp1 = _mm256_cmpgt_epi32(decodeConsumePoint, state1);
      const simd_t cmp2 = _mm256_cmpgt_epi32(decodeConsumePoint, state2);
      const simd_t cmp3 = _mm256_cmpgt_epi32(decodeConsumePoint, state3);

      // matching: state << 16
      const simd_t extractMatchShifted0 = _mm256_slli_epi32(_mm256_and_si256(state0, cmp0), 16);
      const simd_t extractMatchShifted1 = _mm256_slli_epi32(_mm256_and_si256(state1, cmp1), 16);
      const simd_t extractMatchShifted2 = _mm256_slli_epi32(_mm256_and_si256(state2, cmp2), 16);
      const simd_t extractMatchShifted3 = _mm256_slli_epi32(_mm256_and_si256(state3, cmp3), 16);

      // Increment matching read head indexes | well, actually subtract -1.
      readHeadOffsetsX8[0] = _mm256_sub_epi32(readHeadOffsetsX8[0], cmp0);
      readHeadOffsetsX8[1] = _mm256_sub_epi32(readHeadOffsetsX8[1], cmp1);
      readHeadOffsetsX8[2] = _mm256_sub_epi32(readHeadOffsetsX8[2], cmp2);
      readHeadOffsetsX8[3] = _mm256_sub_epi32(readHeadOffsetsX8[3], cmp3);

      // grab current byte from the read heads.
      const simd_t extractMatchNextByte0 = _mm256_and_si256(_mm256_and_si256(cmp0, lower16), newWord0);
      const simd_t extractMatchNextByte1 = _mm256_and_si256(_mm256_and_si256(cmp1, lower16), newWord1);
      const simd_t extractMatchNextByte2 = _mm256_and_si256(_mm256_and_si256(cmp2, lower16), newWord2);
      const simd_t extractMatchNextByte3 = _mm256_and_si256(_mm256_and_si256(cmp3, lower16), newWord3);

      // matching: (state << 16) | newWord
      const simd_t matchingStates0 = _mm256_or_si256(extractMatchShifted0, extractMatchNextByte0);
      const simd_t matchingStates1 = _mm256_or_si256(extractMatchShifted1, extractMatchNextByte1);
      const simd_t matchingStates2 = _mm256_or_si256(extractMatchShifted2, extractMatchNextByte2);
      const simd_t matchingStates3 = _mm256_or_si256(extractMatchShifted3, extractMatchNextByte3);

      // select non matching.
      const simd_t nonMatchingStates0 = _mm256_andnot_si256(cmp0, state0);
      const simd_t nonMatchingStates1 = _mm256_andnot_si256(cmp1, state1);
      const simd_t nonMatchingStates2 = _mm256_andnot_si256(cmp2, state2);
      const simd_t nonMatchingStates3 = _mm256_andnot_si256(cmp3, state3);

      // combine.
      statesX8[0] = _mm256_or_si256(matchingStates0, nonMatchingStates0);
      statesX8[1] = _mm256_or_si256(matchingStates1, nonMatchingStates1);
      statesX8[2] = _mm256_or_si256(matchingStates2, nonMatchingStates2);
      statesX8[3] = _mm256_or_si256(matchingStates3, nonMatchingStates3);

      // no need to derive next byte, we'll gather anyways.
    }
  }

  for (; i < outLengthInStates; i += StateCount)
  {
    // const uint32_t slot = state & (TotalSymbolCount - 1);
    const simd_t slot0 = _mm256_and_si256(statesX8[0], symCountMask);
    const simd_t slot1 = _mm256_and_si256(statesX8[1], symCountMask);
    const simd_t slot2 = _mm256_and_si256(statesX8[2], symCountMask);
    const simd_t slot3 = _mm256_and_si256(statesX8[3], symCountMask);

    // retrieve pack.
    const simd_t pack0 = _mm256_i32gather_epi32(reinterpret_cast<const int32_t *>(&histDec.symbol), slot0, sizeof(uint32_t));
    const simd_t pack1 = _mm256_i32gather_epi32(reinterpret_cast<const int32_t *>(&histDec.symbol), slot1, sizeof(uint32_t));
    const simd_t pack2 = _mm256_i32gather_epi32(reinterpret_cast<const int32_t *>(&histDec.symbol), slot2, sizeof(uint32_t));
    const simd_t pack3 = _mm256_i32gather_epi32(reinterpret_cast<const int32_t *>(&histDec.symbol), slot3, sizeof(uint32_t));

    // const uint32_t shiftedState = (state >> TotalSymbolCountBits);
    const simd_t shiftedState0 = _mm256_srli_epi32(statesX8[0], TotalSymbolCountBits);
    const simd_t shiftedState1 = _mm256_srli_epi32(statesX8[1], TotalSymbolCountBits);
    const simd_t shiftedState2 = _mm256_srli_epi32(statesX8[2], TotalSymbolCountBits);
    const simd_t shiftedState3 = _mm256_srli_epi32(statesX8[3], TotalSymbolCountBits);

    // unpack symbol.
    const simd_t symbol0 = _mm256_and_si256(pack0, lower8);
    const simd_t symbol1 = _mm256_and_si256(pack1, lower8);
    const simd_t symbol2 = _mm256_and_si256(pack2, lower8);
    const simd_t symbol3 = _mm256_and_si256(pack3, lower8);

    // pack symbols to one si256. (could possibly be `_mm256_cvtepi32_epi8` on avx-512f + avx-512-vl)
    const simd_t symPack01 = _mm256_packus_epi32(symbol0, symbol1);
    const simd_t symPack23 = _mm256_packus_epi32(symbol2, symbol3);
    const simd_t symPack0123 = _mm256_packus_epi16(symPack01, symPack23); // `00 01 02 03 08 09 0A 0B 10 11 12 13 18 19 1A 1B 04 05 06 07 0C 0D 0E 0F 14 15 16 17 1C 1D 1E 1F`

    // We intentionally encoded in a way to not have to do horrible things here.
    _mm256_storeu_si256(reinterpret_cast<__m256i *>(pOutData + i), symPack0123);

    // unpack freq, cumul.
    const simd_t cumul0 = _mm256_and_si256(_mm256_srli_epi32(pack0, 8), lower12);
    const simd_t freq0 = _mm256_srli_epi32(pack0, 20);
    const simd_t cumul1 = _mm256_and_si256(_mm256_srli_epi32(pack1, 8), lower12);
    const simd_t freq1 = _mm256_srli_epi32(pack1, 20);
    const simd_t cumul2 = _mm256_and_si256(_mm256_srli_epi32(pack2, 8), lower12);
    const simd_t freq2 = _mm256_srli_epi32(pack2, 20);
    const simd_t cumul3 = _mm256_and_si256(_mm256_srli_epi32(pack3, 8), lower12);
    const simd_t freq3 = _mm256_srli_epi32(pack3, 20);

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
    
    // request next uint16_t.
    const simd_t newWord0 = _mm256_i32gather_epi32(pReadHeadSIMD, readHeadOffsetsX8[0], sizeof(uint16_t));
    const simd_t newWord1 = _mm256_i32gather_epi32(pReadHeadSIMD, readHeadOffsetsX8[1], sizeof(uint16_t));
    const simd_t newWord2 = _mm256_i32gather_epi32(pReadHeadSIMD, readHeadOffsetsX8[2], sizeof(uint16_t));
    const simd_t newWord3 = _mm256_i32gather_epi32(pReadHeadSIMD, readHeadOffsetsX8[3], sizeof(uint16_t));

    // (state < DecodeConsumePoint16) ? -1 : 0 | well, actually (DecodeConsumePoint16 - 1 > state) ? 1 : 0
    const simd_t cmp0 = _mm256_cmpgt_epi32(decodeConsumePoint, state0);
    const simd_t cmp1 = _mm256_cmpgt_epi32(decodeConsumePoint, state1);
    const simd_t cmp2 = _mm256_cmpgt_epi32(decodeConsumePoint, state2);
    const simd_t cmp3 = _mm256_cmpgt_epi32(decodeConsumePoint, state3);

    // matching: state << 16
    const simd_t extractMatchShifted0 = _mm256_slli_epi32(_mm256_and_si256(state0, cmp0), 16);
    const simd_t extractMatchShifted1 = _mm256_slli_epi32(_mm256_and_si256(state1, cmp1), 16);
    const simd_t extractMatchShifted2 = _mm256_slli_epi32(_mm256_and_si256(state2, cmp2), 16);
    const simd_t extractMatchShifted3 = _mm256_slli_epi32(_mm256_and_si256(state3, cmp3), 16);

    // Increment matching read head indexes | well, actually subtract -1.
    readHeadOffsetsX8[0] = _mm256_sub_epi32(readHeadOffsetsX8[0], cmp0);
    readHeadOffsetsX8[1] = _mm256_sub_epi32(readHeadOffsetsX8[1], cmp1);
    readHeadOffsetsX8[2] = _mm256_sub_epi32(readHeadOffsetsX8[2], cmp2);
    readHeadOffsetsX8[3] = _mm256_sub_epi32(readHeadOffsetsX8[3], cmp3);

    // grab current byte from the read heads.
    const simd_t extractMatchNextByte0 = _mm256_and_si256(_mm256_and_si256(cmp0, lower16), newWord0);
    const simd_t extractMatchNextByte1 = _mm256_and_si256(_mm256_and_si256(cmp1, lower16), newWord1);
    const simd_t extractMatchNextByte2 = _mm256_and_si256(_mm256_and_si256(cmp2, lower16), newWord2);
    const simd_t extractMatchNextByte3 = _mm256_and_si256(_mm256_and_si256(cmp3, lower16), newWord3);

    // matching: (state << 16) | newWord
    const simd_t matchingStates0 = _mm256_or_si256(extractMatchShifted0, extractMatchNextByte0);
    const simd_t matchingStates1 = _mm256_or_si256(extractMatchShifted1, extractMatchNextByte1);
    const simd_t matchingStates2 = _mm256_or_si256(extractMatchShifted2, extractMatchNextByte2);
    const simd_t matchingStates3 = _mm256_or_si256(extractMatchShifted3, extractMatchNextByte3);

    // select non matching.
    const simd_t nonMatchingStates0 = _mm256_andnot_si256(cmp0, state0);
    const simd_t nonMatchingStates1 = _mm256_andnot_si256(cmp1, state1);
    const simd_t nonMatchingStates2 = _mm256_andnot_si256(cmp2, state2);
    const simd_t nonMatchingStates3 = _mm256_andnot_si256(cmp3, state3);

    // combine.
    statesX8[0] = _mm256_or_si256(matchingStates0, nonMatchingStates0);
    statesX8[1] = _mm256_or_si256(matchingStates1, nonMatchingStates1);
    statesX8[2] = _mm256_or_si256(matchingStates2, nonMatchingStates2);
    statesX8[3] = _mm256_or_si256(matchingStates3, nonMatchingStates3);
  }

  for (size_t j = 0; j < sizeof(statesX8) / sizeof(simd_t); j++)
  {
    _mm256_storeu_si256(reinterpret_cast<simd_t *>(reinterpret_cast<uint8_t *>(states) + j * sizeof(simd_t)), statesX8[j]);
    _mm256_storeu_si256(reinterpret_cast<simd_t *>(reinterpret_cast<uint8_t *>(readHeadOffsets) + j * sizeof(simd_t)), readHeadOffsetsX8[j]);
  }

  for (size_t j = 0; j < StateCount; j++)
    pReadHead[j] = reinterpret_cast<const uint8_t *>(reinterpret_cast<const uint16_t *>(pReadHeadSIMD) + readHeadOffsets[j]);

  const uint8_t idx2idx[] = { 0x00, 0x01, 0x02, 0x03, 0x10, 0x11, 0x12, 0x13, 0x04, 0x05, 0x06, 0x07, 0x14, 0x15, 0x16, 0x17, 0x08, 0x09, 0x0A, 0x0B , 0x18, 0x19, 0x1A, 0x1B, 0x0C, 0x0D, 0x0E, 0x0F, 0x1C, 0x1D, 0x1E, 0x1F };
  static_assert(sizeof(idx2idx) == StateCount);

  for (size_t j = 0; j < StateCount; j++)
  {
    const uint8_t index = idx2idx[j];

    if (i + index < expectedOutputLength)
    {
      uint32_t state = states[j];

      const uint32_t slot = state & (TotalSymbolCount - 1);
      const uint8_t symbol = histDec.symbol[slot] & 0xFF;
      state = (state >> TotalSymbolCountBits) * (uint32_t)(histDec.symbol[slot] >> 20) + slot - (uint32_t)((histDec.symbol[slot] >> 8) & 0b1111111111);

      pOutData[i + index] = symbol;

      if constexpr (DecodeNoBranch)
      {
        const bool read = state < DecodeConsumePoint16;
        const uint32_t newState = state << 16 | *reinterpret_cast<const uint16_t *>(pReadHead[j]);
        state = read ? newState : state;
        pReadHead[j] += sizeof(uint16_t) * (size_t)read;
      }
      else
      {
        if (state < DecodeConsumePoint16)
        {
          state = state << 16 | *reinterpret_cast<const uint16_t *>(pReadHead[j]);
          pReadHead[j] += sizeof(uint16_t);
        }
      }

      states[j] = state;
    }
  }

  return expectedOutputLength;
}

//////////////////////////////////////////////////////////////////////////

size_t rANS32x32_32blk_16w_encode_scalar_15(const uint8_t *pInData, const size_t length, uint8_t *pOutData, const size_t outCapacity, const hist_t *pHist) { return rANS32x32_32blk_16w_encode_scalar<15>(pInData, length, pOutData, outCapacity, pHist); }
size_t rANS32x32_32blk_16w_decode_scalar_15(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity) { return rANS32x32_32blk_16w_decode_scalar<15>(pInData, inLength, pOutData, outCapacity); }
size_t rANS32x32_32blk_16w_decode_avx2_varA_15(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity) { return rANS32x32_32blk_16w_decode_avx2_varA<15>(pInData, inLength, pOutData, outCapacity); }
size_t rANS32x32_32blk_16w_decode_avx2_varA2_15(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity) { return rANS32x32_32blk_16w_decode_avx2_varA2<15>(pInData, inLength, pOutData, outCapacity); }
size_t rANS32x32_32blk_16w_decode_avx2_varB_15(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity) { return rANS32x32_32blk_16w_decode_avx2_varB<15>(pInData, inLength, pOutData, outCapacity); }
size_t rANS32x32_32blk_16w_decode_avx2_varB2_15(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity) { return rANS32x32_32blk_16w_decode_avx2_varB2<15>(pInData, inLength, pOutData, outCapacity); }

size_t rANS32x32_32blk_16w_encode_scalar_14(const uint8_t *pInData, const size_t length, uint8_t *pOutData, const size_t outCapacity, const hist_t *pHist) { return rANS32x32_32blk_16w_encode_scalar<14>(pInData, length, pOutData, outCapacity, pHist); }
size_t rANS32x32_32blk_16w_decode_scalar_14(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity) { return rANS32x32_32blk_16w_decode_scalar<14>(pInData, inLength, pOutData, outCapacity); }
size_t rANS32x32_32blk_16w_decode_avx2_varA_14(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity) { return rANS32x32_32blk_16w_decode_avx2_varA<14>(pInData, inLength, pOutData, outCapacity); }
size_t rANS32x32_32blk_16w_decode_avx2_varA2_14(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity) { return rANS32x32_32blk_16w_decode_avx2_varA2<14>(pInData, inLength, pOutData, outCapacity); }
size_t rANS32x32_32blk_16w_decode_avx2_varB_14(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity) { return rANS32x32_32blk_16w_decode_avx2_varB<14>(pInData, inLength, pOutData, outCapacity); }
size_t rANS32x32_32blk_16w_decode_avx2_varB2_14(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity) { return rANS32x32_32blk_16w_decode_avx2_varB2<14>(pInData, inLength, pOutData, outCapacity); }

size_t rANS32x32_32blk_16w_encode_scalar_13(const uint8_t *pInData, const size_t length, uint8_t *pOutData, const size_t outCapacity, const hist_t *pHist) { return rANS32x32_32blk_16w_encode_scalar<13>(pInData, length, pOutData, outCapacity, pHist); }
size_t rANS32x32_32blk_16w_decode_scalar_13(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity) { return rANS32x32_32blk_16w_decode_scalar<13>(pInData, inLength, pOutData, outCapacity); }
size_t rANS32x32_32blk_16w_decode_avx2_varA_13(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity) { return rANS32x32_32blk_16w_decode_avx2_varA<13>(pInData, inLength, pOutData, outCapacity); }
size_t rANS32x32_32blk_16w_decode_avx2_varA2_13(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity) { return rANS32x32_32blk_16w_decode_avx2_varA2<13>(pInData, inLength, pOutData, outCapacity); }
size_t rANS32x32_32blk_16w_decode_avx2_varB_13(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity) { return rANS32x32_32blk_16w_decode_avx2_varB<13>(pInData, inLength, pOutData, outCapacity); }
size_t rANS32x32_32blk_16w_decode_avx2_varB2_13(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity) { return rANS32x32_32blk_16w_decode_avx2_varB2<13>(pInData, inLength, pOutData, outCapacity); }

size_t rANS32x32_32blk_16w_encode_scalar_12(const uint8_t *pInData, const size_t length, uint8_t *pOutData, const size_t outCapacity, const hist_t *pHist) { return rANS32x32_32blk_16w_encode_scalar<12>(pInData, length, pOutData, outCapacity, pHist); }
size_t rANS32x32_32blk_16w_decode_scalar_12(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity) { return rANS32x32_32blk_16w_decode_scalar<12>(pInData, inLength, pOutData, outCapacity); }
size_t rANS32x32_32blk_16w_decode_avx2_varA_12(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity) { return rANS32x32_32blk_16w_decode_avx2_varA<12>(pInData, inLength, pOutData, outCapacity); }
size_t rANS32x32_32blk_16w_decode_avx2_varA2_12(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity) { return rANS32x32_32blk_16w_decode_avx2_varA2<12>(pInData, inLength, pOutData, outCapacity); }
size_t rANS32x32_32blk_16w_decode_avx2_varB_12(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity) { return rANS32x32_32blk_16w_decode_avx2_varB<12>(pInData, inLength, pOutData, outCapacity); }
size_t rANS32x32_32blk_16w_decode_avx2_varB2_12(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity) { return rANS32x32_32blk_16w_decode_avx2_varB2<12>(pInData, inLength, pOutData, outCapacity); }
size_t rANS32x32_32blk_16w_decode_avx2_varC_12(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity) { return rANS32x32_32blk_16w_decode_avx2_varC<12>(pInData, inLength, pOutData, outCapacity); }
size_t rANS32x32_32blk_16w_decode_avx2_varC2_12(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity) { return rANS32x32_32blk_16w_decode_avx2_varC2<12>(pInData, inLength, pOutData, outCapacity); }

size_t rANS32x32_32blk_16w_encode_scalar_11(const uint8_t *pInData, const size_t length, uint8_t *pOutData, const size_t outCapacity, const hist_t *pHist) { return rANS32x32_32blk_16w_encode_scalar<11>(pInData, length, pOutData, outCapacity, pHist); }
size_t rANS32x32_32blk_16w_decode_scalar_11(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity) { return rANS32x32_32blk_16w_decode_scalar<11>(pInData, inLength, pOutData, outCapacity); }
size_t rANS32x32_32blk_16w_decode_avx2_varA_11(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity) { return rANS32x32_32blk_16w_decode_avx2_varA<11>(pInData, inLength, pOutData, outCapacity); }
size_t rANS32x32_32blk_16w_decode_avx2_varA2_11(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity) { return rANS32x32_32blk_16w_decode_avx2_varA2<11>(pInData, inLength, pOutData, outCapacity); }
size_t rANS32x32_32blk_16w_decode_avx2_varB_11(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity) { return rANS32x32_32blk_16w_decode_avx2_varB<11>(pInData, inLength, pOutData, outCapacity); }
size_t rANS32x32_32blk_16w_decode_avx2_varB2_11(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity) { return rANS32x32_32blk_16w_decode_avx2_varB2<11>(pInData, inLength, pOutData, outCapacity); }
size_t rANS32x32_32blk_16w_decode_avx2_varC_11(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity) { return rANS32x32_32blk_16w_decode_avx2_varC<11>(pInData, inLength, pOutData, outCapacity); }
size_t rANS32x32_32blk_16w_decode_avx2_varC2_11(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity) { return rANS32x32_32blk_16w_decode_avx2_varC2<11>(pInData, inLength, pOutData, outCapacity); }

size_t rANS32x32_32blk_16w_encode_scalar_10(const uint8_t *pInData, const size_t length, uint8_t *pOutData, const size_t outCapacity, const hist_t *pHist) { return rANS32x32_32blk_16w_encode_scalar<10>(pInData, length, pOutData, outCapacity, pHist); }
size_t rANS32x32_32blk_16w_decode_scalar_10(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity) { return rANS32x32_32blk_16w_decode_scalar<10>(pInData, inLength, pOutData, outCapacity); }
size_t rANS32x32_32blk_16w_decode_avx2_varA_10(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity) { return rANS32x32_32blk_16w_decode_avx2_varA<10>(pInData, inLength, pOutData, outCapacity); }
size_t rANS32x32_32blk_16w_decode_avx2_varA2_10(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity) { return rANS32x32_32blk_16w_decode_avx2_varA2<10>(pInData, inLength, pOutData, outCapacity); }
size_t rANS32x32_32blk_16w_decode_avx2_varB_10(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity) { return rANS32x32_32blk_16w_decode_avx2_varB<10>(pInData, inLength, pOutData, outCapacity); }
size_t rANS32x32_32blk_16w_decode_avx2_varB2_10(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity) { return rANS32x32_32blk_16w_decode_avx2_varB2<10>(pInData, inLength, pOutData, outCapacity); }
size_t rANS32x32_32blk_16w_decode_avx2_varC_10(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity) { return rANS32x32_32blk_16w_decode_avx2_varC<10>(pInData, inLength, pOutData, outCapacity); }
size_t rANS32x32_32blk_16w_decode_avx2_varC2_10(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity) { return rANS32x32_32blk_16w_decode_avx2_varC2<10>(pInData, inLength, pOutData, outCapacity); }
