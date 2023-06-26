#include "hist.h"
#include "simd_platform.h"

#include <string.h>

constexpr size_t StateCount = 16; // Needs to be a power of two.
constexpr bool DecodeNoBranch = false;
constexpr bool EncodeNoBranch = false;

size_t rANS32x16_16w_capacity(const size_t inputSize)
{
  return inputSize + StateCount + sizeof(uint16_t) * 256 + sizeof(uint32_t) * StateCount + sizeof(uint64_t) * 2; // buffer + histogram + state
}

//////////////////////////////////////////////////////////////////////////

template <uint32_t TotalSymbolCountBits>
inline static uint8_t decode_symbol_scalar_32x16_16w(uint32_t *pState, const hist_dec_t<TotalSymbolCountBits> *pHist)
{
  constexpr uint32_t TotalSymbolCount = ((uint32_t)1 << TotalSymbolCountBits);

  const uint32_t state = *pState;
  const uint32_t slot = state & (TotalSymbolCount - 1);
  const uint8_t symbol = pHist->cumulInv[slot];
  const uint32_t previousState = (state >> TotalSymbolCountBits) * (uint32_t)pHist->symbolCount[symbol] + slot - (uint32_t)pHist->cumul[symbol];

  *pState = previousState;

  return symbol;
}

//////////////////////////////////////////////////////////////////////////

template <uint32_t TotalSymbolCountBits>
size_t rANS32x16_16w_encode_scalar(const uint8_t *pInData, const size_t length, uint8_t *pOutData, const size_t outCapacity, const hist_t *pHist)
{
  if (outCapacity < rANS32x16_16w_capacity(length))
    return 0;

  static_assert(TotalSymbolCountBits < 16);
  constexpr size_t EncodeEmitPoint = ((DecodeConsumePoint16 >> TotalSymbolCountBits) << 16);

  uint32_t states[StateCount];
  uint16_t *pEnd = reinterpret_cast<uint16_t *>(pOutData + outCapacity - sizeof(uint16_t));
  uint16_t *pStart = pEnd;

  // Init States.
  for (size_t i = 0; i < StateCount; i++)
    states[i] = DecodeConsumePoint16;

  const uint8_t idx2idx[] = { 0x00, 0x01, 0x02, 0x03, 0x08, 0x09, 0x0A, 0x0B, 0x04, 0x05, 0x06, 0x07, 0x0C, 0x0D, 0x0E, 0x0F };
  static_assert(sizeof(idx2idx) == StateCount);

  int64_t i = length - 1;
  i &= ~(size_t)(StateCount - 1);
  i += StateCount;

  for (int64_t j = StateCount - 1; j >= 0; j--)
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
        const bool write = state >= max;
        *pStart = (uint16_t)(state & 0xFFFF);
        *pStart -= (size_t)write;
        state = write ? state >> 16 : state;
      }
      else
      {
        if (state >= max)
        {
          *pStart = (uint16_t)(state & 0xFFFF);
          pStart--;
          state >>= 16;
        }
      }

      states[stateIndex] = ((state / symbolCount) << TotalSymbolCountBits) + (uint32_t)pHist->cumul[in] + (state % symbolCount);
    }
  }

  i -= StateCount;

  for (; i >= (int64_t)StateCount; i -= StateCount)
  {
    for (int64_t j = StateCount - 1; j >= 0; j--)
    {
      const uint8_t index = idx2idx[j];

      const uint8_t in = pInData[i - StateCount + index];
      const uint32_t symbolCount = pHist->symbolCount[in];
      const uint32_t max = EncodeEmitPoint * symbolCount;

      const size_t stateIndex = j;

      uint32_t state = states[stateIndex];

      if constexpr (EncodeNoBranch)
      {
        const bool write = state >= max;
        *pStart = (uint16_t)(state & 0xFFFF);
        *pStart -= (size_t)write;
        state = write ? state >> 16 : state;
      }
      else
      {
        if (state >= max)
        {
          *pStart = (uint16_t)(state & 0xFFFF);
          pStart--;
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

  const size_t size = (pEnd - pStart) * sizeof(uint16_t);

  memmove(pWrite + outIndex, pStart + 1, size);
  outIndex += size;

  *reinterpret_cast<uint64_t *>(pOutData + sizeof(uint64_t)) = outIndex; // write total output length.

  return outIndex;
}

template <uint32_t TotalSymbolCountBits>
size_t rANS32x16_16w_decode_scalar(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity)
{
  if (inLength < sizeof(uint64_t) * 2 + sizeof(uint32_t) * StateCount + sizeof(uint16_t) * 256)
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

  const uint16_t *pReadHead = reinterpret_cast<const uint16_t *>(pInData + inputIndex);

  const uint8_t idx2idx[] = { 0x00, 0x01, 0x02, 0x03, 0x08, 0x09, 0x0A, 0x0B, 0x04, 0x05, 0x06, 0x07, 0x0C, 0x0D, 0x0E, 0x0F };
  static_assert(sizeof(idx2idx) == StateCount);

  const size_t outLengthInStates = expectedOutputLength - StateCount + 1;
  size_t i = 0;

  for (; i < outLengthInStates; i += StateCount)
  {
    for (size_t j = 0; j < StateCount; j++)
    {
      const uint8_t index = idx2idx[j];
      uint32_t state = states[j];

      pOutData[i + index] = decode_symbol_scalar_32x16_16w<TotalSymbolCountBits>(&state, &hist);

      if constexpr (DecodeNoBranch)
      {
        const bool read = state < DecodeConsumePoint16;
        const uint32_t newState = state << 16 | *pReadHead;
        state = read ? newState : state;
        pReadHead += (size_t)read;
      }
      else
      {
        if (state < DecodeConsumePoint16)
        {
          state = state << 16 | *pReadHead;
          pReadHead++;
        }
      }
      
      states[j] = state;
    }
  }

  for (size_t j = 0; j < StateCount; j++)
  {
    const uint8_t index = idx2idx[j];

    if (i + index < expectedOutputLength)
    {
      uint32_t state = states[j];

      pOutData[i + index] = decode_symbol_scalar_32x16_16w<TotalSymbolCountBits>(&state, &hist);

      if constexpr (DecodeNoBranch)
      {
        const bool read = state < DecodeConsumePoint16;
        const uint32_t newState = state << 16 | *pReadHead;
        state = read ? newState : state;
        pReadHead += (size_t)read;
      }
      else
      {
        if (state < DecodeConsumePoint16)
        {
          state = state << 16 | *pReadHead;
          pReadHead++;
        }
      }

      states[j] = state;
    }
  }

  return expectedOutputLength;
}

//////////////////////////////////////////////////////////////////////////

#define ___ 0xEE

// Somewhat similar to the table used by Fabian Giesen, but byte wise, for 8 lanes, in steps of two and using 0xEE instead of -1 to allow for some more trickery.
#ifdef _MSC_VER
__declspec(align(32))
#else
__attribute__((aligned(32)))
#endif
static const uint8_t _ShuffleLutShfl32[256 * 8] = 
{
  ___, ___, ___, ___, ___, ___, ___, ___, // 0000 0000
  0x0, ___, ___, ___, ___, ___, ___, ___, // 1000 0000
  ___, 0x0, ___, ___, ___, ___, ___, ___, // 0100 0000
  0x0, 0x2, ___, ___, ___, ___, ___, ___, // 1100 0000
  ___, ___, 0x0, ___, ___, ___, ___, ___, // 0010 0000
  0x0, ___, 0x2, ___, ___, ___, ___, ___, // 1010 0000
  ___, 0x0, 0x2, ___, ___, ___, ___, ___, // 0110 0000
  0x0, 0x2, 0x4, ___, ___, ___, ___, ___, // 1110 0000
  ___, ___, ___, 0x0, ___, ___, ___, ___, // 0001 0000
  0x0, ___, ___, 0x2, ___, ___, ___, ___, // 1001 0000
  ___, 0x0, ___, 0x2, ___, ___, ___, ___, // 0101 0000
  0x0, 0x2, ___, 0x4, ___, ___, ___, ___, // 1101 0000
  ___, ___, 0x0, 0x2, ___, ___, ___, ___, // 0011 0000
  0x0, ___, 0x2, 0x4, ___, ___, ___, ___, // 1011 0000
  ___, 0x0, 0x2, 0x4, ___, ___, ___, ___, // 0111 0000
  0x0, 0x2, 0x4, 0x6, ___, ___, ___, ___, // 1111 0000
  ___, ___, ___, ___, 0x0, ___, ___, ___, // 0000 1000
  0x0, ___, ___, ___, 0x2, ___, ___, ___, // 1000 1000
  ___, 0x0, ___, ___, 0x2, ___, ___, ___, // 0100 1000
  0x0, 0x2, ___, ___, 0x4, ___, ___, ___, // 1100 1000
  ___, ___, 0x0, ___, 0x2, ___, ___, ___, // 0010 1000
  0x0, ___, 0x2, ___, 0x4, ___, ___, ___, // 1010 1000
  ___, 0x0, 0x2, ___, 0x4, ___, ___, ___, // 0110 1000
  0x0, 0x2, 0x4, ___, 0x6, ___, ___, ___, // 1110 1000
  ___, ___, ___, 0x0, 0x2, ___, ___, ___, // 0001 1000
  0x0, ___, ___, 0x2, 0x4, ___, ___, ___, // 1001 1000
  ___, 0x0, ___, 0x2, 0x4, ___, ___, ___, // 0101 1000
  0x0, 0x2, ___, 0x4, 0x6, ___, ___, ___, // 1101 1000
  ___, ___, 0x0, 0x2, 0x4, ___, ___, ___, // 0011 1000
  0x0, ___, 0x2, 0x4, 0x6, ___, ___, ___, // 1011 1000
  ___, 0x0, 0x2, 0x4, 0x6, ___, ___, ___, // 0111 1000
  0x0, 0x2, 0x4, 0x6, 0x8, ___, ___, ___, // 1111 1000
  ___, ___, ___, ___, ___, 0x0, ___, ___, // 0000 0100
  0x0, ___, ___, ___, ___, 0x2, ___, ___, // 1000 0100
  ___, 0x0, ___, ___, ___, 0x2, ___, ___, // 0100 0100
  0x0, 0x2, ___, ___, ___, 0x4, ___, ___, // 1100 0100
  ___, ___, 0x0, ___, ___, 0x2, ___, ___, // 0010 0100
  0x0, ___, 0x2, ___, ___, 0x4, ___, ___, // 1010 0100
  ___, 0x0, 0x2, ___, ___, 0x4, ___, ___, // 0110 0100
  0x0, 0x2, 0x4, ___, ___, 0x6, ___, ___, // 1110 0100
  ___, ___, ___, 0x0, ___, 0x2, ___, ___, // 0001 0100
  0x0, ___, ___, 0x2, ___, 0x4, ___, ___, // 1001 0100
  ___, 0x0, ___, 0x2, ___, 0x4, ___, ___, // 0101 0100
  0x0, 0x2, ___, 0x4, ___, 0x6, ___, ___, // 1101 0100
  ___, ___, 0x0, 0x2, ___, 0x4, ___, ___, // 0011 0100
  0x0, ___, 0x2, 0x4, ___, 0x6, ___, ___, // 1011 0100
  ___, 0x0, 0x2, 0x4, ___, 0x6, ___, ___, // 0111 0100
  0x0, 0x2, 0x4, 0x6, ___, 0x8, ___, ___, // 1111 0100
  ___, ___, ___, ___, 0x0, 0x2, ___, ___, // 0000 1100
  0x0, ___, ___, ___, 0x2, 0x4, ___, ___, // 1000 1100
  ___, 0x0, ___, ___, 0x2, 0x4, ___, ___, // 0100 1100
  0x0, 0x2, ___, ___, 0x4, 0x6, ___, ___, // 1100 1100
  ___, ___, 0x0, ___, 0x2, 0x4, ___, ___, // 0010 1100
  0x0, ___, 0x2, ___, 0x4, 0x6, ___, ___, // 1010 1100
  ___, 0x0, 0x2, ___, 0x4, 0x6, ___, ___, // 0110 1100
  0x0, 0x2, 0x4, ___, 0x6, 0x8, ___, ___, // 1110 1100
  ___, ___, ___, 0x0, 0x2, 0x4, ___, ___, // 0001 1100
  0x0, ___, ___, 0x2, 0x4, 0x6, ___, ___, // 1001 1100
  ___, 0x0, ___, 0x2, 0x4, 0x6, ___, ___, // 0101 1100
  0x0, 0x2, ___, 0x4, 0x6, 0x8, ___, ___, // 1101 1100
  ___, ___, 0x0, 0x2, 0x4, 0x6, ___, ___, // 0011 1100
  0x0, ___, 0x2, 0x4, 0x6, 0x8, ___, ___, // 1011 1100
  ___, 0x0, 0x2, 0x4, 0x6, 0x8, ___, ___, // 0111 1100
  0x0, 0x2, 0x4, 0x6, 0x8, 0xA, ___, ___, // 1111 1100
  ___, ___, ___, ___, ___, ___, 0x0, ___, // 0000 0010
  0x0, ___, ___, ___, ___, ___, 0x2, ___, // 1000 0010
  ___, 0x0, ___, ___, ___, ___, 0x2, ___, // 0100 0010
  0x0, 0x2, ___, ___, ___, ___, 0x4, ___, // 1100 0010
  ___, ___, 0x0, ___, ___, ___, 0x2, ___, // 0010 0010
  0x0, ___, 0x2, ___, ___, ___, 0x4, ___, // 1010 0010
  ___, 0x0, 0x2, ___, ___, ___, 0x4, ___, // 0110 0010
  0x0, 0x2, 0x4, ___, ___, ___, 0x6, ___, // 1110 0010
  ___, ___, ___, 0x0, ___, ___, 0x2, ___, // 0001 0010
  0x0, ___, ___, 0x2, ___, ___, 0x4, ___, // 1001 0010
  ___, 0x0, ___, 0x2, ___, ___, 0x4, ___, // 0101 0010
  0x0, 0x2, ___, 0x4, ___, ___, 0x6, ___, // 1101 0010
  ___, ___, 0x0, 0x2, ___, ___, 0x4, ___, // 0011 0010
  0x0, ___, 0x2, 0x4, ___, ___, 0x6, ___, // 1011 0010
  ___, 0x0, 0x2, 0x4, ___, ___, 0x6, ___, // 0111 0010
  0x0, 0x2, 0x4, 0x6, ___, ___, 0x8, ___, // 1111 0010
  ___, ___, ___, ___, 0x0, ___, 0x2, ___, // 0000 1010
  0x0, ___, ___, ___, 0x2, ___, 0x4, ___, // 1000 1010
  ___, 0x0, ___, ___, 0x2, ___, 0x4, ___, // 0100 1010
  0x0, 0x2, ___, ___, 0x4, ___, 0x6, ___, // 1100 1010
  ___, ___, 0x0, ___, 0x2, ___, 0x4, ___, // 0010 1010
  0x0, ___, 0x2, ___, 0x4, ___, 0x6, ___, // 1010 1010
  ___, 0x0, 0x2, ___, 0x4, ___, 0x6, ___, // 0110 1010
  0x0, 0x2, 0x4, ___, 0x6, ___, 0x8, ___, // 1110 1010
  ___, ___, ___, 0x0, 0x2, ___, 0x4, ___, // 0001 1010
  0x0, ___, ___, 0x2, 0x4, ___, 0x6, ___, // 1001 1010
  ___, 0x0, ___, 0x2, 0x4, ___, 0x6, ___, // 0101 1010
  0x0, 0x2, ___, 0x4, 0x6, ___, 0x8, ___, // 1101 1010
  ___, ___, 0x0, 0x2, 0x4, ___, 0x6, ___, // 0011 1010
  0x0, ___, 0x2, 0x4, 0x6, ___, 0x8, ___, // 1011 1010
  ___, 0x0, 0x2, 0x4, 0x6, ___, 0x8, ___, // 0111 1010
  0x0, 0x2, 0x4, 0x6, 0x8, ___, 0xA, ___, // 1111 1010
  ___, ___, ___, ___, ___, 0x0, 0x2, ___, // 0000 0110
  0x0, ___, ___, ___, ___, 0x2, 0x4, ___, // 1000 0110
  ___, 0x0, ___, ___, ___, 0x2, 0x4, ___, // 0100 0110
  0x0, 0x2, ___, ___, ___, 0x4, 0x6, ___, // 1100 0110
  ___, ___, 0x0, ___, ___, 0x2, 0x4, ___, // 0010 0110
  0x0, ___, 0x2, ___, ___, 0x4, 0x6, ___, // 1010 0110
  ___, 0x0, 0x2, ___, ___, 0x4, 0x6, ___, // 0110 0110
  0x0, 0x2, 0x4, ___, ___, 0x6, 0x8, ___, // 1110 0110
  ___, ___, ___, 0x0, ___, 0x2, 0x4, ___, // 0001 0110
  0x0, ___, ___, 0x2, ___, 0x4, 0x6, ___, // 1001 0110
  ___, 0x0, ___, 0x2, ___, 0x4, 0x6, ___, // 0101 0110
  0x0, 0x2, ___, 0x4, ___, 0x6, 0x8, ___, // 1101 0110
  ___, ___, 0x0, 0x2, ___, 0x4, 0x6, ___, // 0011 0110
  0x0, ___, 0x2, 0x4, ___, 0x6, 0x8, ___, // 1011 0110
  ___, 0x0, 0x2, 0x4, ___, 0x6, 0x8, ___, // 0111 0110
  0x0, 0x2, 0x4, 0x6, ___, 0x8, 0xA, ___, // 1111 0110
  ___, ___, ___, ___, 0x0, 0x2, 0x4, ___, // 0000 1110
  0x0, ___, ___, ___, 0x2, 0x4, 0x6, ___, // 1000 1110
  ___, 0x0, ___, ___, 0x2, 0x4, 0x6, ___, // 0100 1110
  0x0, 0x2, ___, ___, 0x4, 0x6, 0x8, ___, // 1100 1110
  ___, ___, 0x0, ___, 0x2, 0x4, 0x6, ___, // 0010 1110
  0x0, ___, 0x2, ___, 0x4, 0x6, 0x8, ___, // 1010 1110
  ___, 0x0, 0x2, ___, 0x4, 0x6, 0x8, ___, // 0110 1110
  0x0, 0x2, 0x4, ___, 0x6, 0x8, 0xA, ___, // 1110 1110
  ___, ___, ___, 0x0, 0x2, 0x4, 0x6, ___, // 0001 1110
  0x0, ___, ___, 0x2, 0x4, 0x6, 0x8, ___, // 1001 1110
  ___, 0x0, ___, 0x2, 0x4, 0x6, 0x8, ___, // 0101 1110
  0x0, 0x2, ___, 0x4, 0x6, 0x8, 0xA, ___, // 1101 1110
  ___, ___, 0x0, 0x2, 0x4, 0x6, 0x8, ___, // 0011 1110
  0x0, ___, 0x2, 0x4, 0x6, 0x8, 0xA, ___, // 1011 1110
  ___, 0x0, 0x2, 0x4, 0x6, 0x8, 0xA, ___, // 0111 1110
  0x0, 0x2, 0x4, 0x6, 0x8, 0xA, 0xC, ___, // 1111 1110
  ___, ___, ___, ___, ___, ___, ___, 0x0, // 0000 0001
  0x0, ___, ___, ___, ___, ___, ___, 0x2, // 1000 0001
  ___, 0x0, ___, ___, ___, ___, ___, 0x2, // 0100 0001
  0x0, 0x2, ___, ___, ___, ___, ___, 0x4, // 1100 0001
  ___, ___, 0x0, ___, ___, ___, ___, 0x2, // 0010 0001
  0x0, ___, 0x2, ___, ___, ___, ___, 0x4, // 1010 0001
  ___, 0x0, 0x2, ___, ___, ___, ___, 0x4, // 0110 0001
  0x0, 0x2, 0x4, ___, ___, ___, ___, 0x6, // 1110 0001
  ___, ___, ___, 0x0, ___, ___, ___, 0x2, // 0001 0001
  0x0, ___, ___, 0x2, ___, ___, ___, 0x4, // 1001 0001
  ___, 0x0, ___, 0x2, ___, ___, ___, 0x4, // 0101 0001
  0x0, 0x2, ___, 0x4, ___, ___, ___, 0x6, // 1101 0001
  ___, ___, 0x0, 0x2, ___, ___, ___, 0x4, // 0011 0001
  0x0, ___, 0x2, 0x4, ___, ___, ___, 0x6, // 1011 0001
  ___, 0x0, 0x2, 0x4, ___, ___, ___, 0x6, // 0111 0001
  0x0, 0x2, 0x4, 0x6, ___, ___, ___, 0x8, // 1111 0001
  ___, ___, ___, ___, 0x0, ___, ___, 0x2, // 0000 1001
  0x0, ___, ___, ___, 0x2, ___, ___, 0x4, // 1000 1001
  ___, 0x0, ___, ___, 0x2, ___, ___, 0x4, // 0100 1001
  0x0, 0x2, ___, ___, 0x4, ___, ___, 0x6, // 1100 1001
  ___, ___, 0x0, ___, 0x2, ___, ___, 0x4, // 0010 1001
  0x0, ___, 0x2, ___, 0x4, ___, ___, 0x6, // 1010 1001
  ___, 0x0, 0x2, ___, 0x4, ___, ___, 0x6, // 0110 1001
  0x0, 0x2, 0x4, ___, 0x6, ___, ___, 0x8, // 1110 1001
  ___, ___, ___, 0x0, 0x2, ___, ___, 0x4, // 0001 1001
  0x0, ___, ___, 0x2, 0x4, ___, ___, 0x6, // 1001 1001
  ___, 0x0, ___, 0x2, 0x4, ___, ___, 0x6, // 0101 1001
  0x0, 0x2, ___, 0x4, 0x6, ___, ___, 0x8, // 1101 1001
  ___, ___, 0x0, 0x2, 0x4, ___, ___, 0x6, // 0011 1001
  0x0, ___, 0x2, 0x4, 0x6, ___, ___, 0x8, // 1011 1001
  ___, 0x0, 0x2, 0x4, 0x6, ___, ___, 0x8, // 0111 1001
  0x0, 0x2, 0x4, 0x6, 0x8, ___, ___, 0xA, // 1111 1001
  ___, ___, ___, ___, ___, 0x0, ___, 0x2, // 0000 0101
  0x0, ___, ___, ___, ___, 0x2, ___, 0x4, // 1000 0101
  ___, 0x0, ___, ___, ___, 0x2, ___, 0x4, // 0100 0101
  0x0, 0x2, ___, ___, ___, 0x4, ___, 0x6, // 1100 0101
  ___, ___, 0x0, ___, ___, 0x2, ___, 0x4, // 0010 0101
  0x0, ___, 0x2, ___, ___, 0x4, ___, 0x6, // 1010 0101
  ___, 0x0, 0x2, ___, ___, 0x4, ___, 0x6, // 0110 0101
  0x0, 0x2, 0x4, ___, ___, 0x6, ___, 0x8, // 1110 0101
  ___, ___, ___, 0x0, ___, 0x2, ___, 0x4, // 0001 0101
  0x0, ___, ___, 0x2, ___, 0x4, ___, 0x6, // 1001 0101
  ___, 0x0, ___, 0x2, ___, 0x4, ___, 0x6, // 0101 0101
  0x0, 0x2, ___, 0x4, ___, 0x6, ___, 0x8, // 1101 0101
  ___, ___, 0x0, 0x2, ___, 0x4, ___, 0x6, // 0011 0101
  0x0, ___, 0x2, 0x4, ___, 0x6, ___, 0x8, // 1011 0101
  ___, 0x0, 0x2, 0x4, ___, 0x6, ___, 0x8, // 0111 0101
  0x0, 0x2, 0x4, 0x6, ___, 0x8, ___, 0xA, // 1111 0101
  ___, ___, ___, ___, 0x0, 0x2, ___, 0x4, // 0000 1101
  0x0, ___, ___, ___, 0x2, 0x4, ___, 0x6, // 1000 1101
  ___, 0x0, ___, ___, 0x2, 0x4, ___, 0x6, // 0100 1101
  0x0, 0x2, ___, ___, 0x4, 0x6, ___, 0x8, // 1100 1101
  ___, ___, 0x0, ___, 0x2, 0x4, ___, 0x6, // 0010 1101
  0x0, ___, 0x2, ___, 0x4, 0x6, ___, 0x8, // 1010 1101
  ___, 0x0, 0x2, ___, 0x4, 0x6, ___, 0x8, // 0110 1101
  0x0, 0x2, 0x4, ___, 0x6, 0x8, ___, 0xA, // 1110 1101
  ___, ___, ___, 0x0, 0x2, 0x4, ___, 0x6, // 0001 1101
  0x0, ___, ___, 0x2, 0x4, 0x6, ___, 0x8, // 1001 1101
  ___, 0x0, ___, 0x2, 0x4, 0x6, ___, 0x8, // 0101 1101
  0x0, 0x2, ___, 0x4, 0x6, 0x8, ___, 0xA, // 1101 1101
  ___, ___, 0x0, 0x2, 0x4, 0x6, ___, 0x8, // 0011 1101
  0x0, ___, 0x2, 0x4, 0x6, 0x8, ___, 0xA, // 1011 1101
  ___, 0x0, 0x2, 0x4, 0x6, 0x8, ___, 0xA, // 0111 1101
  0x0, 0x2, 0x4, 0x6, 0x8, 0xA, ___, 0xC, // 1111 1101
  ___, ___, ___, ___, ___, ___, 0x0, 0x2, // 0000 0011
  0x0, ___, ___, ___, ___, ___, 0x2, 0x4, // 1000 0011
  ___, 0x0, ___, ___, ___, ___, 0x2, 0x4, // 0100 0011
  0x0, 0x2, ___, ___, ___, ___, 0x4, 0x6, // 1100 0011
  ___, ___, 0x0, ___, ___, ___, 0x2, 0x4, // 0010 0011
  0x0, ___, 0x2, ___, ___, ___, 0x4, 0x6, // 1010 0011
  ___, 0x0, 0x2, ___, ___, ___, 0x4, 0x6, // 0110 0011
  0x0, 0x2, 0x4, ___, ___, ___, 0x6, 0x8, // 1110 0011
  ___, ___, ___, 0x0, ___, ___, 0x2, 0x4, // 0001 0011
  0x0, ___, ___, 0x2, ___, ___, 0x4, 0x6, // 1001 0011
  ___, 0x0, ___, 0x2, ___, ___, 0x4, 0x6, // 0101 0011
  0x0, 0x2, ___, 0x4, ___, ___, 0x6, 0x8, // 1101 0011
  ___, ___, 0x0, 0x2, ___, ___, 0x4, 0x6, // 0011 0011
  0x0, ___, 0x2, 0x4, ___, ___, 0x6, 0x8, // 1011 0011
  ___, 0x0, 0x2, 0x4, ___, ___, 0x6, 0x8, // 0111 0011
  0x0, 0x2, 0x4, 0x6, ___, ___, 0x8, 0xA, // 1111 0011
  ___, ___, ___, ___, 0x0, ___, 0x2, 0x4, // 0000 1011
  0x0, ___, ___, ___, 0x2, ___, 0x4, 0x6, // 1000 1011
  ___, 0x0, ___, ___, 0x2, ___, 0x4, 0x6, // 0100 1011
  0x0, 0x2, ___, ___, 0x4, ___, 0x6, 0x8, // 1100 1011
  ___, ___, 0x0, ___, 0x2, ___, 0x4, 0x6, // 0010 1011
  0x0, ___, 0x2, ___, 0x4, ___, 0x6, 0x8, // 1010 1011
  ___, 0x0, 0x2, ___, 0x4, ___, 0x6, 0x8, // 0110 1011
  0x0, 0x2, 0x4, ___, 0x6, ___, 0x8, 0xA, // 1110 1011
  ___, ___, ___, 0x0, 0x2, ___, 0x4, 0x6, // 0001 1011
  0x0, ___, ___, 0x2, 0x4, ___, 0x6, 0x8, // 1001 1011
  ___, 0x0, ___, 0x2, 0x4, ___, 0x6, 0x8, // 0101 1011
  0x0, 0x2, ___, 0x4, 0x6, ___, 0x8, 0xA, // 1101 1011
  ___, ___, 0x0, 0x2, 0x4, ___, 0x6, 0x8, // 0011 1011
  0x0, ___, 0x2, 0x4, 0x6, ___, 0x8, 0xA, // 1011 1011
  ___, 0x0, 0x2, 0x4, 0x6, ___, 0x8, 0xA, // 0111 1011
  0x0, 0x2, 0x4, 0x6, 0x8, ___, 0xA, 0xC, // 1111 1011
  ___, ___, ___, ___, ___, 0x0, 0x2, 0x4, // 0000 0111
  0x0, ___, ___, ___, ___, 0x2, 0x4, 0x6, // 1000 0111
  ___, 0x0, ___, ___, ___, 0x2, 0x4, 0x6, // 0100 0111
  0x0, 0x2, ___, ___, ___, 0x4, 0x6, 0x8, // 1100 0111
  ___, ___, 0x0, ___, ___, 0x2, 0x4, 0x6, // 0010 0111
  0x0, ___, 0x2, ___, ___, 0x4, 0x6, 0x8, // 1010 0111
  ___, 0x0, 0x2, ___, ___, 0x4, 0x6, 0x8, // 0110 0111
  0x0, 0x2, 0x4, ___, ___, 0x6, 0x8, 0xA, // 1110 0111
  ___, ___, ___, 0x0, ___, 0x2, 0x4, 0x6, // 0001 0111
  0x0, ___, ___, 0x2, ___, 0x4, 0x6, 0x8, // 1001 0111
  ___, 0x0, ___, 0x2, ___, 0x4, 0x6, 0x8, // 0101 0111
  0x0, 0x2, ___, 0x4, ___, 0x6, 0x8, 0xA, // 1101 0111
  ___, ___, 0x0, 0x2, ___, 0x4, 0x6, 0x8, // 0011 0111
  0x0, ___, 0x2, 0x4, ___, 0x6, 0x8, 0xA, // 1011 0111
  ___, 0x0, 0x2, 0x4, ___, 0x6, 0x8, 0xA, // 0111 0111
  0x0, 0x2, 0x4, 0x6, ___, 0x8, 0xA, 0xC, // 1111 0111
  ___, ___, ___, ___, 0x0, 0x2, 0x4, 0x6, // 0000 1111
  0x0, ___, ___, ___, 0x2, 0x4, 0x6, 0x8, // 1000 1111
  ___, 0x0, ___, ___, 0x2, 0x4, 0x6, 0x8, // 0100 1111
  0x0, 0x2, ___, ___, 0x4, 0x6, 0x8, 0xA, // 1100 1111
  ___, ___, 0x0, ___, 0x2, 0x4, 0x6, 0x8, // 0010 1111
  0x0, ___, 0x2, ___, 0x4, 0x6, 0x8, 0xA, // 1010 1111
  ___, 0x0, 0x2, ___, 0x4, 0x6, 0x8, 0xA, // 0110 1111
  0x0, 0x2, 0x4, ___, 0x6, 0x8, 0xA, 0xC, // 1110 1111
  ___, ___, ___, 0x0, 0x2, 0x4, 0x6, 0x8, // 0001 1111
  0x0, ___, ___, 0x2, 0x4, 0x6, 0x8, 0xA, // 1001 1111
  ___, 0x0, ___, 0x2, 0x4, 0x6, 0x8, 0xA, // 0101 1111
  0x0, 0x2, ___, 0x4, 0x6, 0x8, 0xA, 0xC, // 1101 1111
  ___, ___, 0x0, 0x2, 0x4, 0x6, 0x8, 0xA, // 0011 1111
  0x0, ___, 0x2, 0x4, 0x6, 0x8, 0xA, 0xC, // 1011 1111
  ___, 0x0, 0x2, 0x4, 0x6, 0x8, 0xA, 0xC, // 0111 1111
  0x0, 0x2, 0x4, 0x6, 0x8, 0xA, 0xC, 0xE, // 1111 1111
};

#undef ___

//////////////////////////////////////////////////////////////////////////

template <uint32_t TotalSymbolCountBits>
#ifndef _MSC_VER
__attribute__((target("avx2")))
#endif
size_t rANS32x16_xmmShfl_16w_decode_avx2_varA(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity)
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

  const uint16_t *pReadHead = reinterpret_cast<const uint16_t *>(pInData + inputIndex);

  typedef __m256i simd_t;
  simd_t statesX8[StateCount / (sizeof(simd_t) / sizeof(uint32_t))];

  for (size_t i = 0; i < sizeof(statesX8) / sizeof(simd_t); i++)
    statesX8[i] = _mm256_loadu_si256(reinterpret_cast<const simd_t *>(reinterpret_cast<const uint8_t *>(states) + i * sizeof(simd_t)));

  const size_t outLengthInStates = expectedOutputLength - StateCount + 1;
  size_t i = 0;

  const simd_t symCountMask = _mm256_set1_epi32(TotalSymbolCount - 1);
  const simd_t lower16 = _mm256_set1_epi32(0xFFFF);
  const simd_t lower8 = _mm256_set1_epi32(0xFF);
  const simd_t decodeConsumePoint = _mm256_set1_epi32(DecodeConsumePoint16);
  const simd_t _16 = _mm256_set1_epi32(16);
  const __m128i shuffleDoubleMask = _mm_set_epi8(7, 7, 6, 6, 5, 5, 4, 4, 3, 3, 2, 2, 1, 1, 0, 0);
  const __m128i shuffleUpper16Bit = _mm_set1_epi16(0x0100);

  for (; i < outLengthInStates; i += StateCount)
  {
    // const uint32_t slot = state & (TotalSymbolCount - 1);
    const simd_t slot0 = _mm256_and_si256(statesX8[0], symCountMask);
    const simd_t slot1 = _mm256_and_si256(statesX8[1], symCountMask);

    // const uint8_t symbol = pHist->cumulInv[slot];
    simd_t symbol0 = _mm256_i32gather_epi32(reinterpret_cast<const int32_t *>(&hist.cumulInv), slot0, sizeof(uint8_t));
    simd_t symbol1 = _mm256_i32gather_epi32(reinterpret_cast<const int32_t *>(&hist.cumulInv), slot1, sizeof(uint8_t));

    // since they were int32_t turn into uint8_t
    symbol0 = _mm256_and_si256(symbol0, lower8);
    symbol1 = _mm256_and_si256(symbol1, lower8);

    // pack symbols to one si128. (could possibly be `_mm256_cvtepi32_epi8` on avx-512f + avx-512-vl)
    const simd_t packed16 = _mm256_packus_epi32(symbol0, symbol1);
    const simd_t packed8 = _mm256_packus_epi16(packed16, packed16);
    const simd_t permuted = _mm256_permute4x64_epi64(packed8, 0b1000); // `abcdijklefghmnop`
    const __m128i permutedXmm = _mm256_castsi256_si128(permuted);

    // We intentionally encoded in a way to not have to do horrible things here. (TODO: make variant with `_mm256_stream_si256`/`_mm256_store_si256` if output buffer is 32 byte aligned)
    _mm_storeu_si128(reinterpret_cast<__m128i *>(pOutData + i), permutedXmm);

    // retrieve pack.
    const simd_t pack0 = _mm256_i32gather_epi32(reinterpret_cast<const int32_t *>(&hist.symbols), symbol0, sizeof(uint32_t));
    const simd_t pack1 = _mm256_i32gather_epi32(reinterpret_cast<const int32_t *>(&hist.symbols), symbol1, sizeof(uint32_t));

    // freq, cumul.
    const simd_t cumul0 = _mm256_srli_epi32(pack0, 16);
    const simd_t freq0 = _mm256_and_si256(pack0, lower16);
    const simd_t cumul1 = _mm256_srli_epi32(pack1, 16);
    const simd_t freq1 = _mm256_and_si256(pack1, lower16);

    // const uint32_t shiftedState = (state >> TotalSymbolCountBits);
    const simd_t shiftedState0 = _mm256_srli_epi32(statesX8[0], TotalSymbolCountBits);
    const simd_t shiftedState1 = _mm256_srli_epi32(statesX8[1], TotalSymbolCountBits);

    // const uint32_t freqScaled = shiftedState * freq;
    const __m256i freqScaled0 = _mm256_mullo_epi32(shiftedState0, freq0);
    const __m256i freqScaled1 = _mm256_mullo_epi32(shiftedState1, freq1);

    // state = freqScaled + slot - cumul;
    const simd_t state0 = _mm256_add_epi32(freqScaled0, _mm256_sub_epi32(slot0, cumul0));
    const simd_t state1 = _mm256_add_epi32(freqScaled1, _mm256_sub_epi32(slot1, cumul1));

    // now to the messy part...
    
    // read input for blocks 0.
    const __m128i newWords0 = _mm_loadu_si128(reinterpret_cast<const __m128i *>(pReadHead));

    // (state < DecodeConsumePoint16) ? -1 : 0 | well, actually (DecodeConsumePoint16 > state) ? -1 : 0
    const simd_t cmp0 = _mm256_cmpgt_epi32(decodeConsumePoint, state0);
    const simd_t cmp1 = _mm256_cmpgt_epi32(decodeConsumePoint, state1);

    // get masks of those compares & start loading shuffle masks. (TODO: test if `_mm_loadu_si128` or `_mm_set1_epi64x(_ShuffleLutShfl32[cmpMaskX << 3])` is faster, because the lut is probably not entirely cached)
    const uint32_t cmpMask0 = (uint32_t)_mm256_movemask_ps(_mm256_castsi256_ps(cmp0));
    __m128i lut0 = _mm_lddqu_si128(reinterpret_cast<const __m128i *>(&_ShuffleLutShfl32[cmpMask0 << 3])); // `* 8`.

    const uint32_t cmpMask1 = (uint32_t)_mm256_movemask_ps(_mm256_castsi256_ps(cmp1));
    __m128i lut1 = _mm_lddqu_si128(reinterpret_cast<const __m128i *>(&_ShuffleLutShfl32[cmpMask1 << 3])); // `* 8`.

    // advance read head & read input for blocks 1, 2, 3.
    const uint32_t maskPop0 = (uint32_t)__builtin_popcount(cmpMask0);
    pReadHead += maskPop0;

    const __m128i newWords1 = _mm_loadu_si128(reinterpret_cast<const __m128i *>(pReadHead));

    const uint32_t maskPop1 = (uint32_t)__builtin_popcount(cmpMask1);
    pReadHead += maskPop1;

    // finalize lookups.
    lut0 = _mm_or_si128(_mm_shuffle_epi8(lut0, shuffleDoubleMask), shuffleUpper16Bit);
    lut1 = _mm_or_si128(_mm_shuffle_epi8(lut1, shuffleDoubleMask), shuffleUpper16Bit);

    // matching: state << 16
    const simd_t matchShiftedState0 = _mm256_sllv_epi32(state0, _mm256_and_si256(cmp0, _16));
    const simd_t matchShiftedState1 = _mm256_sllv_epi32(state1, _mm256_and_si256(cmp1, _16));

    // shuffle new words in place.
    const __m128i newWordXmm0 = _mm_shuffle_epi8(newWords0, lut0);
    const __m128i newWordXmm1 = _mm_shuffle_epi8(newWords1, lut1);

    // expand new word.
    const __m256i newWord0 = _mm256_cvtepu16_epi32(newWordXmm0);
    const __m256i newWord1 = _mm256_cvtepu16_epi32(newWordXmm1);

    // state = state << 16 | newWord;
    statesX8[0] = _mm256_or_si256(matchShiftedState0, newWord0);
    statesX8[1] = _mm256_or_si256(matchShiftedState1, newWord1);
  }

  for (size_t j = 0; j < sizeof(statesX8) / sizeof(simd_t); j++)
    _mm256_storeu_si256(reinterpret_cast<simd_t *>(reinterpret_cast<uint8_t *>(states) + j * sizeof(simd_t)), statesX8[j]);

  const uint8_t idx2idx[] = { 0x00, 0x01, 0x02, 0x03, 0x08, 0x09, 0x0A, 0x0B, 0x04, 0x05, 0x06, 0x07, 0x0C, 0x0D, 0x0E, 0x0F };
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
        const uint32_t newState = state << 16 | *pReadHead;
        state = read ? newState : state;
        pReadHead += (size_t)read;
      }
      else
      {
        if (state < DecodeConsumePoint16)
        {
          state = state << 16 | *pReadHead;
          pReadHead++;
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
size_t rANS32x16_xmmShfl_16w_decode_avx2_varB(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity)
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

  const uint16_t *pReadHead = reinterpret_cast<const uint16_t *>(pInData + inputIndex);

  typedef __m256i simd_t;
  simd_t statesX8[StateCount / (sizeof(simd_t) / sizeof(uint32_t))];

  for (size_t i = 0; i < sizeof(statesX8) / sizeof(simd_t); i++)
    statesX8[i] = _mm256_loadu_si256(reinterpret_cast<const simd_t *>(reinterpret_cast<const uint8_t *>(states) + i * sizeof(simd_t)));

  const size_t outLengthInStates = expectedOutputLength - StateCount + 1;
  size_t i = 0;

  const simd_t symCountMask = _mm256_set1_epi32(TotalSymbolCount - 1);
  const simd_t lower16 = _mm256_set1_epi32(0xFFFF);
  const simd_t lower8 = _mm256_set1_epi32(0xFF);
  const simd_t decodeConsumePoint = _mm256_set1_epi32(DecodeConsumePoint16);
  const simd_t _16 = _mm256_set1_epi32(16);
  const __m128i shuffleDoubleMask = _mm_set_epi8(7, 7, 6, 6, 5, 5, 4, 4, 3, 3, 2, 2, 1, 1, 0, 0);
  const __m128i shuffleUpper16Bit = _mm_set1_epi16(0x0100);

  for (; i < outLengthInStates; i += StateCount)
  {
    // const uint32_t slot = state & (TotalSymbolCount - 1);
    const simd_t slot0 = _mm256_and_si256(statesX8[0], symCountMask);
    const simd_t slot1 = _mm256_and_si256(statesX8[1], symCountMask);

    // const uint8_t symbol = pHist->cumulInv[slot];
    simd_t symbol0 = _mm256_i32gather_epi32(reinterpret_cast<const int32_t *>(&histDec.cumulInv), slot0, sizeof(uint8_t));
    simd_t symbol1 = _mm256_i32gather_epi32(reinterpret_cast<const int32_t *>(&histDec.cumulInv), slot1, sizeof(uint8_t));

    // retrieve pack.
    const simd_t pack0 = _mm256_i32gather_epi32(reinterpret_cast<const int32_t *>(&histDec.symbolFomCumul), slot0, sizeof(uint32_t));
    const simd_t pack1 = _mm256_i32gather_epi32(reinterpret_cast<const int32_t *>(&histDec.symbolFomCumul), slot1, sizeof(uint32_t));

    // const uint32_t shiftedState = (state >> TotalSymbolCountBits);
    const simd_t shiftedState0 = _mm256_srli_epi32(statesX8[0], TotalSymbolCountBits);
    const simd_t shiftedState1 = _mm256_srli_epi32(statesX8[1], TotalSymbolCountBits);

    // since they were int32_t turn into uint8_t
    symbol0 = _mm256_and_si256(symbol0, lower8);
    symbol1 = _mm256_and_si256(symbol1, lower8);

    // pack symbols to one si128. (could possibly be `_mm256_cvtepi32_epi8` on avx-512f + avx-512-vl)
    const simd_t packed16 = _mm256_packus_epi32(symbol0, symbol1);
    const simd_t packed8 = _mm256_packus_epi16(packed16, packed16);
    const simd_t permuted = _mm256_permute4x64_epi64(packed8, 0b1000); // `abcdijklefghmnop`
    const __m128i permutedXmm = _mm256_castsi256_si128(permuted);

    // We intentionally encoded in a way to not have to do horrible things here. (TODO: make variant with `_mm256_stream_si256`/`_mm256_store_si256` if output buffer is 32 byte aligned)
    _mm_storeu_si128(reinterpret_cast<__m128i *>(pOutData + i), permutedXmm);

    // freq, cumul.
    const simd_t cumul0 = _mm256_srli_epi32(pack0, 16);
    const simd_t freq0 = _mm256_and_si256(pack0, lower16);
    const simd_t cumul1 = _mm256_srli_epi32(pack1, 16);
    const simd_t freq1 = _mm256_and_si256(pack1, lower16);

    // const uint32_t freqScaled = shiftedState * freq;
    const __m256i freqScaled0 = _mm256_mullo_epi32(shiftedState0, freq0);
    const __m256i freqScaled1 = _mm256_mullo_epi32(shiftedState1, freq1);

    // state = freqScaled + slot - cumul;
    const simd_t state0 = _mm256_add_epi32(freqScaled0, _mm256_sub_epi32(slot0, cumul0));
    const simd_t state1 = _mm256_add_epi32(freqScaled1, _mm256_sub_epi32(slot1, cumul1));

    // now to the messy part...
    
    // read input for blocks 0.
    const __m128i newWords0 = _mm_loadu_si128(reinterpret_cast<const __m128i *>(pReadHead));

    // (state < DecodeConsumePoint16) ? -1 : 0 | well, actually (DecodeConsumePoint16 > state) ? -1 : 0
    const simd_t cmp0 = _mm256_cmpgt_epi32(decodeConsumePoint, state0);
    const simd_t cmp1 = _mm256_cmpgt_epi32(decodeConsumePoint, state1);

    // get masks of those compares & start loading shuffle masks. (TODO: test if `_mm_loadu_si128` or `_mm_set1_epi64x(_ShuffleLutShfl32[cmpMaskX << 3])` is faster, because the lut is probably not entirely cached)
    const uint32_t cmpMask0 = (uint32_t)_mm256_movemask_ps(_mm256_castsi256_ps(cmp0));
    __m128i lut0 = _mm_lddqu_si128(reinterpret_cast<const __m128i *>(&_ShuffleLutShfl32[cmpMask0 << 3])); // `* 8`.

    const uint32_t cmpMask1 = (uint32_t)_mm256_movemask_ps(_mm256_castsi256_ps(cmp1));
    __m128i lut1 = _mm_lddqu_si128(reinterpret_cast<const __m128i *>(&_ShuffleLutShfl32[cmpMask1 << 3])); // `* 8`.

    // advance read head & read input for blocks 1, 2, 3.
    const uint32_t maskPop0 = (uint32_t)__builtin_popcount(cmpMask0);
    pReadHead += maskPop0;

    const __m128i newWords1 = _mm_loadu_si128(reinterpret_cast<const __m128i *>(pReadHead));

    const uint32_t maskPop1 = (uint32_t)__builtin_popcount(cmpMask1);
    pReadHead += maskPop1;

    // finalize lookups.
    lut0 = _mm_or_si128(_mm_shuffle_epi8(lut0, shuffleDoubleMask), shuffleUpper16Bit);
    lut1 = _mm_or_si128(_mm_shuffle_epi8(lut1, shuffleDoubleMask), shuffleUpper16Bit);

    // matching: state << 16
    const simd_t matchShiftedState0 = _mm256_sllv_epi32(state0, _mm256_and_si256(cmp0, _16));
    const simd_t matchShiftedState1 = _mm256_sllv_epi32(state1, _mm256_and_si256(cmp1, _16));

    // shuffle new words in place.
    const __m128i newWordXmm0 = _mm_shuffle_epi8(newWords0, lut0);
    const __m128i newWordXmm1 = _mm_shuffle_epi8(newWords1, lut1);

    // expand new word.
    const __m256i newWord0 = _mm256_cvtepu16_epi32(newWordXmm0);
    const __m256i newWord1 = _mm256_cvtepu16_epi32(newWordXmm1);

    // state = state << 16 | newWord;
    statesX8[0] = _mm256_or_si256(matchShiftedState0, newWord0);
    statesX8[1] = _mm256_or_si256(matchShiftedState1, newWord1);
  }

  for (size_t j = 0; j < sizeof(statesX8) / sizeof(simd_t); j++)
    _mm256_storeu_si256(reinterpret_cast<simd_t *>(reinterpret_cast<uint8_t *>(states) + j * sizeof(simd_t)), statesX8[j]);

  const uint8_t idx2idx[] = { 0x00, 0x01, 0x02, 0x03, 0x08, 0x09, 0x0A, 0x0B, 0x04, 0x05, 0x06, 0x07, 0x0C, 0x0D, 0x0E, 0x0F };
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
        const uint32_t newState = state << 16 | *pReadHead;
        state = read ? newState : state;
        pReadHead += (size_t)read;
      }
      else
      {
        if (state < DecodeConsumePoint16)
        {
          state = state << 16 | *pReadHead;
          pReadHead++;
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
size_t rANS32x16_xmmShfl_16w_decode_avx2_varC(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity)
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

  hist_dec_pack_t<TotalSymbolCountBits> histDec;
  make_dec_pack_hist<TotalSymbolCountBits>(&histDec, &hist);

  uint32_t states[StateCount];

  for (size_t i = 0; i < StateCount; i++)
  {
    states[i] = *reinterpret_cast<const uint32_t *>(pInData + inputIndex);
    inputIndex += sizeof(uint32_t);
  }

  const uint16_t *pReadHead = reinterpret_cast<const uint16_t *>(pInData + inputIndex);

  typedef __m256i simd_t;
  simd_t statesX8[StateCount / (sizeof(simd_t) / sizeof(uint32_t))];

  for (size_t i = 0; i < sizeof(statesX8) / sizeof(simd_t); i++)
    statesX8[i] = _mm256_loadu_si256(reinterpret_cast<const simd_t *>(reinterpret_cast<const uint8_t *>(states) + i * sizeof(simd_t)));

  const size_t outLengthInStates = expectedOutputLength - StateCount + 1;
  size_t i = 0;

  const simd_t symCountMask = _mm256_set1_epi32(TotalSymbolCount - 1);
  const simd_t lower12 = _mm256_set1_epi32((1 << 12) - 1);
  const simd_t lower8 = _mm256_set1_epi32(0xFF);
  const simd_t decodeConsumePoint = _mm256_set1_epi32(DecodeConsumePoint16);
  const simd_t _16 = _mm256_set1_epi32(16);
  const __m128i shuffleDoubleMask = _mm_set_epi8(7, 7, 6, 6, 5, 5, 4, 4, 3, 3, 2, 2, 1, 1, 0, 0);
  const __m128i shuffleUpper16Bit = _mm_set1_epi16(0x0100);

  for (; i < outLengthInStates; i += StateCount)
  {
    // const uint32_t slot = state & (TotalSymbolCount - 1);
    const simd_t slot0 = _mm256_and_si256(statesX8[0], symCountMask);
    const simd_t slot1 = _mm256_and_si256(statesX8[1], symCountMask);

    // retrieve pack.
    const simd_t pack0 = _mm256_i32gather_epi32(reinterpret_cast<const int32_t *>(&histDec.symbol), slot0, sizeof(uint32_t));
    const simd_t pack1 = _mm256_i32gather_epi32(reinterpret_cast<const int32_t *>(&histDec.symbol), slot1, sizeof(uint32_t));

    // const uint32_t shiftedState = (state >> TotalSymbolCountBits);
    const simd_t shiftedState0 = _mm256_srli_epi32(statesX8[0], TotalSymbolCountBits);
    const simd_t shiftedState1 = _mm256_srli_epi32(statesX8[1], TotalSymbolCountBits);

    // unpack symbol.
    const simd_t symbol0 = _mm256_and_si256(pack0, lower8);
    const simd_t symbol1 = _mm256_and_si256(pack1, lower8);

    // pack symbols to one si128. (could possibly be `_mm256_cvtepi32_epi8` on avx-512f + avx-512-vl)
    const simd_t packed16 = _mm256_packus_epi32(symbol0, symbol1);
    const simd_t packed8 = _mm256_packus_epi16(packed16, packed16);
    const simd_t permuted = _mm256_permute4x64_epi64(packed8, 0b1000); // `abcdijklefghmnop`
    const __m128i permutedXmm = _mm256_castsi256_si128(permuted);

    // We intentionally encoded in a way to not have to do horrible things here. (TODO: make variant with `_mm256_stream_si256`/`_mm256_store_si256` if output buffer is 32 byte aligned)
    _mm_storeu_si128(reinterpret_cast<__m128i *>(pOutData + i), permutedXmm);

    // unpack freq, cumul.
    const simd_t cumul0 = _mm256_and_si256(_mm256_srli_epi32(pack0, 8), lower12);
    const simd_t freq0 = _mm256_srli_epi32(pack0, 20);
    const simd_t cumul1 = _mm256_and_si256(_mm256_srli_epi32(pack1, 8), lower12);
    const simd_t freq1 = _mm256_srli_epi32(pack1, 20);

    // const uint32_t freqScaled = shiftedState * freq;
    const __m256i freqScaled0 = _mm256_mullo_epi32(shiftedState0, freq0);
    const __m256i freqScaled1 = _mm256_mullo_epi32(shiftedState1, freq1);

    // state = freqScaled + slot - cumul;
    const simd_t state0 = _mm256_add_epi32(freqScaled0, _mm256_sub_epi32(slot0, cumul0));
    const simd_t state1 = _mm256_add_epi32(freqScaled1, _mm256_sub_epi32(slot1, cumul1));

    // now to the messy part...
    
    // read input for blocks 0.
    const __m128i newWords0 = _mm_loadu_si128(reinterpret_cast<const __m128i *>(pReadHead));

    // (state < DecodeConsumePoint16) ? -1 : 0 | well, actually (DecodeConsumePoint16 > state) ? -1 : 0
    const simd_t cmp0 = _mm256_cmpgt_epi32(decodeConsumePoint, state0);
    const simd_t cmp1 = _mm256_cmpgt_epi32(decodeConsumePoint, state1);

    // get masks of those compares & start loading shuffle masks. (TODO: test if `_mm_loadu_si128` or `_mm_set1_epi64x(_ShuffleLutShfl32[cmpMaskX << 3])` is faster, because the lut is probably not entirely cached)
    const uint32_t cmpMask0 = (uint32_t)_mm256_movemask_ps(_mm256_castsi256_ps(cmp0));
    __m128i lut0 = _mm_lddqu_si128(reinterpret_cast<const __m128i *>(&_ShuffleLutShfl32[cmpMask0 << 3])); // `* 8`.

    const uint32_t cmpMask1 = (uint32_t)_mm256_movemask_ps(_mm256_castsi256_ps(cmp1));
    __m128i lut1 = _mm_lddqu_si128(reinterpret_cast<const __m128i *>(&_ShuffleLutShfl32[cmpMask1 << 3])); // `* 8`.

    // advance read head & read input for blocks 1, 2, 3.
    const uint32_t maskPop0 = (uint32_t)__builtin_popcount(cmpMask0);
    pReadHead += maskPop0;

    const __m128i newWords1 = _mm_loadu_si128(reinterpret_cast<const __m128i *>(pReadHead));

    const uint32_t maskPop1 = (uint32_t)__builtin_popcount(cmpMask1);
    pReadHead += maskPop1;

    // finalize lookups.
    lut0 = _mm_or_si128(_mm_shuffle_epi8(lut0, shuffleDoubleMask), shuffleUpper16Bit);
    lut1 = _mm_or_si128(_mm_shuffle_epi8(lut1, shuffleDoubleMask), shuffleUpper16Bit);

    // matching: state << 16
    const simd_t matchShiftedState0 = _mm256_sllv_epi32(state0, _mm256_and_si256(cmp0, _16));
    const simd_t matchShiftedState1 = _mm256_sllv_epi32(state1, _mm256_and_si256(cmp1, _16));

    // shuffle new words in place.
    const __m128i newWordXmm0 = _mm_shuffle_epi8(newWords0, lut0);
    const __m128i newWordXmm1 = _mm_shuffle_epi8(newWords1, lut1);

    // expand new word.
    const __m256i newWord0 = _mm256_cvtepu16_epi32(newWordXmm0);
    const __m256i newWord1 = _mm256_cvtepu16_epi32(newWordXmm1);

    // state = state << 16 | newWord;
    statesX8[0] = _mm256_or_si256(matchShiftedState0, newWord0);
    statesX8[1] = _mm256_or_si256(matchShiftedState1, newWord1);
  }

  for (size_t j = 0; j < sizeof(statesX8) / sizeof(simd_t); j++)
    _mm256_storeu_si256(reinterpret_cast<simd_t *>(reinterpret_cast<uint8_t *>(states) + j * sizeof(simd_t)), statesX8[j]);

  const uint8_t idx2idx[] = { 0x00, 0x01, 0x02, 0x03, 0x08, 0x09, 0x0A, 0x0B, 0x04, 0x05, 0x06, 0x07, 0x0C, 0x0D, 0x0E, 0x0F };
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
        const uint32_t newState = state << 16 | *pReadHead;
        state = read ? newState : state;
        pReadHead += (size_t)read;
      }
      else
      {
        if (state < DecodeConsumePoint16)
        {
          state = state << 16 | *pReadHead;
          pReadHead++;
        }
      }

      states[j] = state;
    }
  }

  return expectedOutputLength;
}

//////////////////////////////////////////////////////////////////////////

size_t rANS32x16_16w_encode_scalar_15(const uint8_t *pInData, const size_t length, uint8_t *pOutData, const size_t outCapacity, const hist_t *pHist) { return rANS32x16_16w_encode_scalar<15>(pInData, length, pOutData, outCapacity, pHist); }
size_t rANS32x16_16w_decode_scalar_15(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity) { return rANS32x16_16w_decode_scalar<15>(pInData, inLength, pOutData, outCapacity); }
size_t rANS32x16_xmmShfl_16w_decode_avx2_varA_15(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity) { return rANS32x16_xmmShfl_16w_decode_avx2_varA<15>(pInData, inLength, pOutData, outCapacity); }
size_t rANS32x16_xmmShfl_16w_decode_avx2_varB_15(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity) { return rANS32x16_xmmShfl_16w_decode_avx2_varB<15>(pInData, inLength, pOutData, outCapacity); }

size_t rANS32x16_16w_encode_scalar_14(const uint8_t *pInData, const size_t length, uint8_t *pOutData, const size_t outCapacity, const hist_t *pHist) { return rANS32x16_16w_encode_scalar<14>(pInData, length, pOutData, outCapacity, pHist); }
size_t rANS32x16_16w_decode_scalar_14(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity) { return rANS32x16_16w_decode_scalar<14>(pInData, inLength, pOutData, outCapacity); }
size_t rANS32x16_xmmShfl_16w_decode_avx2_varA_14(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity) { return rANS32x16_xmmShfl_16w_decode_avx2_varA<14>(pInData, inLength, pOutData, outCapacity); }
size_t rANS32x16_xmmShfl_16w_decode_avx2_varB_14(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity) { return rANS32x16_xmmShfl_16w_decode_avx2_varB<14>(pInData, inLength, pOutData, outCapacity); }

size_t rANS32x16_16w_encode_scalar_13(const uint8_t *pInData, const size_t length, uint8_t *pOutData, const size_t outCapacity, const hist_t *pHist) { return rANS32x16_16w_encode_scalar<13>(pInData, length, pOutData, outCapacity, pHist); }
size_t rANS32x16_16w_decode_scalar_13(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity) { return rANS32x16_16w_decode_scalar<13>(pInData, inLength, pOutData, outCapacity); }
size_t rANS32x16_xmmShfl_16w_decode_avx2_varA_13(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity) { return rANS32x16_xmmShfl_16w_decode_avx2_varA<13>(pInData, inLength, pOutData, outCapacity); }
size_t rANS32x16_xmmShfl_16w_decode_avx2_varB_13(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity) { return rANS32x16_xmmShfl_16w_decode_avx2_varB<13>(pInData, inLength, pOutData, outCapacity); }

size_t rANS32x16_16w_encode_scalar_12(const uint8_t *pInData, const size_t length, uint8_t *pOutData, const size_t outCapacity, const hist_t *pHist) { return rANS32x16_16w_encode_scalar<12>(pInData, length, pOutData, outCapacity, pHist); }
size_t rANS32x16_16w_decode_scalar_12(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity) { return rANS32x16_16w_decode_scalar<12>(pInData, inLength, pOutData, outCapacity); }
size_t rANS32x16_xmmShfl_16w_decode_avx2_varA_12(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity) { return rANS32x16_xmmShfl_16w_decode_avx2_varA<12>(pInData, inLength, pOutData, outCapacity); }
size_t rANS32x16_xmmShfl_16w_decode_avx2_varB_12(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity) { return rANS32x16_xmmShfl_16w_decode_avx2_varB<12>(pInData, inLength, pOutData, outCapacity); }
size_t rANS32x16_xmmShfl_16w_decode_avx2_varC_12(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity) { return rANS32x16_xmmShfl_16w_decode_avx2_varC<12>(pInData, inLength, pOutData, outCapacity); }

size_t rANS32x16_16w_encode_scalar_11(const uint8_t *pInData, const size_t length, uint8_t *pOutData, const size_t outCapacity, const hist_t *pHist) { return rANS32x16_16w_encode_scalar<11>(pInData, length, pOutData, outCapacity, pHist); }
size_t rANS32x16_16w_decode_scalar_11(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity) { return rANS32x16_16w_decode_scalar<11>(pInData, inLength, pOutData, outCapacity); }
size_t rANS32x16_xmmShfl_16w_decode_avx2_varA_11(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity) { return rANS32x16_xmmShfl_16w_decode_avx2_varA<11>(pInData, inLength, pOutData, outCapacity); }
size_t rANS32x16_xmmShfl_16w_decode_avx2_varB_11(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity) { return rANS32x16_xmmShfl_16w_decode_avx2_varB<11>(pInData, inLength, pOutData, outCapacity); }
size_t rANS32x16_xmmShfl_16w_decode_avx2_varC_11(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity) { return rANS32x16_xmmShfl_16w_decode_avx2_varC<11>(pInData, inLength, pOutData, outCapacity); }

size_t rANS32x16_16w_encode_scalar_10(const uint8_t *pInData, const size_t length, uint8_t *pOutData, const size_t outCapacity, const hist_t *pHist) { return rANS32x16_16w_encode_scalar<10>(pInData, length, pOutData, outCapacity, pHist); }
size_t rANS32x16_16w_decode_scalar_10(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity) { return rANS32x16_16w_decode_scalar<10>(pInData, inLength, pOutData, outCapacity); }
size_t rANS32x16_xmmShfl_16w_decode_avx2_varA_10(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity) { return rANS32x16_xmmShfl_16w_decode_avx2_varA<10>(pInData, inLength, pOutData, outCapacity); }
size_t rANS32x16_xmmShfl_16w_decode_avx2_varB_10(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity) { return rANS32x16_xmmShfl_16w_decode_avx2_varB<10>(pInData, inLength, pOutData, outCapacity); }
size_t rANS32x16_xmmShfl_16w_decode_avx2_varC_10(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity) { return rANS32x16_xmmShfl_16w_decode_avx2_varC<10>(pInData, inLength, pOutData, outCapacity); }

//////////////////////////////////////////////////////////////////////////

#define _ 0xEE

// Somewhat similar to the table used by Fabian Giesen, but byte wise, for 8 lanes, what `_` is defined to is absolutely irrelevant.
#ifdef _MSC_VER
__declspec(align(32))
#else
__attribute__((aligned(32)))
#endif
static const uint8_t _ShuffleLutPerm32[256 * 8] =
{
  _, _, _, _, _, _, _, _, // 0000 0000
  0, _, _, _, _, _, _, _, // 1000 0000
  _, 0, _, _, _, _, _, _, // 0100 0000
  0, 1, _, _, _, _, _, _, // 1100 0000
  _, _, 0, _, _, _, _, _, // 0010 0000
  0, _, 1, _, _, _, _, _, // 1010 0000
  _, 0, 1, _, _, _, _, _, // 0110 0000
  0, 1, 2, _, _, _, _, _, // 1110 0000
  _, _, _, 0, _, _, _, _, // 0001 0000
  0, _, _, 1, _, _, _, _, // 1001 0000
  _, 0, _, 1, _, _, _, _, // 0101 0000
  0, 1, _, 2, _, _, _, _, // 1101 0000
  _, _, 0, 1, _, _, _, _, // 0011 0000
  0, _, 1, 2, _, _, _, _, // 1011 0000
  _, 0, 1, 2, _, _, _, _, // 0111 0000
  0, 1, 2, 3, _, _, _, _, // 1111 0000
  _, _, _, _, 0, _, _, _, // 0000 1000
  0, _, _, _, 1, _, _, _, // 1000 1000
  _, 0, _, _, 1, _, _, _, // 0100 1000
  0, 1, _, _, 2, _, _, _, // 1100 1000
  _, _, 0, _, 1, _, _, _, // 0010 1000
  0, _, 1, _, 2, _, _, _, // 1010 1000
  _, 0, 1, _, 2, _, _, _, // 0110 1000
  0, 1, 2, _, 3, _, _, _, // 1110 1000
  _, _, _, 0, 1, _, _, _, // 0001 1000
  0, _, _, 1, 2, _, _, _, // 1001 1000
  _, 0, _, 1, 2, _, _, _, // 0101 1000
  0, 1, _, 2, 3, _, _, _, // 1101 1000
  _, _, 0, 1, 2, _, _, _, // 0011 1000
  0, _, 1, 2, 3, _, _, _, // 1011 1000
  _, 0, 1, 2, 3, _, _, _, // 0111 1000
  0, 1, 2, 3, 4, _, _, _, // 1111 1000
  _, _, _, _, _, 0, _, _, // 0000 0100
  0, _, _, _, _, 1, _, _, // 1000 0100
  _, 0, _, _, _, 1, _, _, // 0100 0100
  0, 1, _, _, _, 2, _, _, // 1100 0100
  _, _, 0, _, _, 1, _, _, // 0010 0100
  0, _, 1, _, _, 2, _, _, // 1010 0100
  _, 0, 1, _, _, 2, _, _, // 0110 0100
  0, 1, 2, _, _, 3, _, _, // 1110 0100
  _, _, _, 0, _, 1, _, _, // 0001 0100
  0, _, _, 1, _, 2, _, _, // 1001 0100
  _, 0, _, 1, _, 2, _, _, // 0101 0100
  0, 1, _, 2, _, 3, _, _, // 1101 0100
  _, _, 0, 1, _, 2, _, _, // 0011 0100
  0, _, 1, 2, _, 3, _, _, // 1011 0100
  _, 0, 1, 2, _, 3, _, _, // 0111 0100
  0, 1, 2, 3, _, 4, _, _, // 1111 0100
  _, _, _, _, 0, 1, _, _, // 0000 1100
  0, _, _, _, 1, 2, _, _, // 1000 1100
  _, 0, _, _, 1, 2, _, _, // 0100 1100
  0, 1, _, _, 2, 3, _, _, // 1100 1100
  _, _, 0, _, 1, 2, _, _, // 0010 1100
  0, _, 1, _, 2, 3, _, _, // 1010 1100
  _, 0, 1, _, 2, 3, _, _, // 0110 1100
  0, 1, 2, _, 3, 4, _, _, // 1110 1100
  _, _, _, 0, 1, 2, _, _, // 0001 1100
  0, _, _, 1, 2, 3, _, _, // 1001 1100
  _, 0, _, 1, 2, 3, _, _, // 0101 1100
  0, 1, _, 2, 3, 4, _, _, // 1101 1100
  _, _, 0, 1, 2, 3, _, _, // 0011 1100
  0, _, 1, 2, 3, 4, _, _, // 1011 1100
  _, 0, 1, 2, 3, 4, _, _, // 0111 1100
  0, 1, 2, 3, 4, 5, _, _, // 1111 1100
  _, _, _, _, _, _, 0, _, // 0000 0010
  0, _, _, _, _, _, 1, _, // 1000 0010
  _, 0, _, _, _, _, 1, _, // 0100 0010
  0, 1, _, _, _, _, 2, _, // 1100 0010
  _, _, 0, _, _, _, 1, _, // 0010 0010
  0, _, 1, _, _, _, 2, _, // 1010 0010
  _, 0, 1, _, _, _, 2, _, // 0110 0010
  0, 1, 2, _, _, _, 3, _, // 1110 0010
  _, _, _, 0, _, _, 1, _, // 0001 0010
  0, _, _, 1, _, _, 2, _, // 1001 0010
  _, 0, _, 1, _, _, 2, _, // 0101 0010
  0, 1, _, 2, _, _, 3, _, // 1101 0010
  _, _, 0, 1, _, _, 2, _, // 0011 0010
  0, _, 1, 2, _, _, 3, _, // 1011 0010
  _, 0, 1, 2, _, _, 3, _, // 0111 0010
  0, 1, 2, 3, _, _, 4, _, // 1111 0010
  _, _, _, _, 0, _, 1, _, // 0000 1010
  0, _, _, _, 1, _, 2, _, // 1000 1010
  _, 0, _, _, 1, _, 2, _, // 0100 1010
  0, 1, _, _, 2, _, 3, _, // 1100 1010
  _, _, 0, _, 1, _, 2, _, // 0010 1010
  0, _, 1, _, 2, _, 3, _, // 1010 1010
  _, 0, 1, _, 2, _, 3, _, // 0110 1010
  0, 1, 2, _, 3, _, 4, _, // 1110 1010
  _, _, _, 0, 1, _, 2, _, // 0001 1010
  0, _, _, 1, 2, _, 3, _, // 1001 1010
  _, 0, _, 1, 2, _, 3, _, // 0101 1010
  0, 1, _, 2, 3, _, 4, _, // 1101 1010
  _, _, 0, 1, 2, _, 3, _, // 0011 1010
  0, _, 1, 2, 3, _, 4, _, // 1011 1010
  _, 0, 1, 2, 3, _, 4, _, // 0111 1010
  0, 1, 2, 3, 4, _, 5, _, // 1111 1010
  _, _, _, _, _, 0, 1, _, // 0000 0110
  0, _, _, _, _, 1, 2, _, // 1000 0110
  _, 0, _, _, _, 1, 2, _, // 0100 0110
  0, 1, _, _, _, 2, 3, _, // 1100 0110
  _, _, 0, _, _, 1, 2, _, // 0010 0110
  0, _, 1, _, _, 2, 3, _, // 1010 0110
  _, 0, 1, _, _, 2, 3, _, // 0110 0110
  0, 1, 2, _, _, 3, 4, _, // 1110 0110
  _, _, _, 0, _, 1, 2, _, // 0001 0110
  0, _, _, 1, _, 2, 3, _, // 1001 0110
  _, 0, _, 1, _, 2, 3, _, // 0101 0110
  0, 1, _, 2, _, 3, 4, _, // 1101 0110
  _, _, 0, 1, _, 2, 3, _, // 0011 0110
  0, _, 1, 2, _, 3, 4, _, // 1011 0110
  _, 0, 1, 2, _, 3, 4, _, // 0111 0110
  0, 1, 2, 3, _, 4, 5, _, // 1111 0110
  _, _, _, _, 0, 1, 2, _, // 0000 1110
  0, _, _, _, 1, 2, 3, _, // 1000 1110
  _, 0, _, _, 1, 2, 3, _, // 0100 1110
  0, 1, _, _, 2, 3, 4, _, // 1100 1110
  _, _, 0, _, 1, 2, 3, _, // 0010 1110
  0, _, 1, _, 2, 3, 4, _, // 1010 1110
  _, 0, 1, _, 2, 3, 4, _, // 0110 1110
  0, 1, 2, _, 3, 4, 5, _, // 1110 1110
  _, _, _, 0, 1, 2, 3, _, // 0001 1110
  0, _, _, 1, 2, 3, 4, _, // 1001 1110
  _, 0, _, 1, 2, 3, 4, _, // 0101 1110
  0, 1, _, 2, 3, 4, 5, _, // 1101 1110
  _, _, 0, 1, 2, 3, 4, _, // 0011 1110
  0, _, 1, 2, 3, 4, 5, _, // 1011 1110
  _, 0, 1, 2, 3, 4, 5, _, // 0111 1110
  0, 1, 2, 3, 4, 5, 6, _, // 1111 1110
  _, _, _, _, _, _, _, 0, // 0000 0001
  0, _, _, _, _, _, _, 1, // 1000 0001
  _, 0, _, _, _, _, _, 1, // 0100 0001
  0, 1, _, _, _, _, _, 2, // 1100 0001
  _, _, 0, _, _, _, _, 1, // 0010 0001
  0, _, 1, _, _, _, _, 2, // 1010 0001
  _, 0, 1, _, _, _, _, 2, // 0110 0001
  0, 1, 2, _, _, _, _, 3, // 1110 0001
  _, _, _, 0, _, _, _, 1, // 0001 0001
  0, _, _, 1, _, _, _, 2, // 1001 0001
  _, 0, _, 1, _, _, _, 2, // 0101 0001
  0, 1, _, 2, _, _, _, 3, // 1101 0001
  _, _, 0, 1, _, _, _, 2, // 0011 0001
  0, _, 1, 2, _, _, _, 3, // 1011 0001
  _, 0, 1, 2, _, _, _, 3, // 0111 0001
  0, 1, 2, 3, _, _, _, 4, // 1111 0001
  _, _, _, _, 0, _, _, 1, // 0000 1001
  0, _, _, _, 1, _, _, 2, // 1000 1001
  _, 0, _, _, 1, _, _, 2, // 0100 1001
  0, 1, _, _, 2, _, _, 3, // 1100 1001
  _, _, 0, _, 1, _, _, 2, // 0010 1001
  0, _, 1, _, 2, _, _, 3, // 1010 1001
  _, 0, 1, _, 2, _, _, 3, // 0110 1001
  0, 1, 2, _, 3, _, _, 4, // 1110 1001
  _, _, _, 0, 1, _, _, 2, // 0001 1001
  0, _, _, 1, 2, _, _, 3, // 1001 1001
  _, 0, _, 1, 2, _, _, 3, // 0101 1001
  0, 1, _, 2, 3, _, _, 4, // 1101 1001
  _, _, 0, 1, 2, _, _, 3, // 0011 1001
  0, _, 1, 2, 3, _, _, 4, // 1011 1001
  _, 0, 1, 2, 3, _, _, 4, // 0111 1001
  0, 1, 2, 3, 4, _, _, 5, // 1111 1001
  _, _, _, _, _, 0, _, 1, // 0000 0101
  0, _, _, _, _, 1, _, 2, // 1000 0101
  _, 0, _, _, _, 1, _, 2, // 0100 0101
  0, 1, _, _, _, 2, _, 3, // 1100 0101
  _, _, 0, _, _, 1, _, 2, // 0010 0101
  0, _, 1, _, _, 2, _, 3, // 1010 0101
  _, 0, 1, _, _, 2, _, 3, // 0110 0101
  0, 1, 2, _, _, 3, _, 4, // 1110 0101
  _, _, _, 0, _, 1, _, 2, // 0001 0101
  0, _, _, 1, _, 2, _, 3, // 1001 0101
  _, 0, _, 1, _, 2, _, 3, // 0101 0101
  0, 1, _, 2, _, 3, _, 4, // 1101 0101
  _, _, 0, 1, _, 2, _, 3, // 0011 0101
  0, _, 1, 2, _, 3, _, 4, // 1011 0101
  _, 0, 1, 2, _, 3, _, 4, // 0111 0101
  0, 1, 2, 3, _, 4, _, 5, // 1111 0101
  _, _, _, _, 0, 1, _, 2, // 0000 1101
  0, _, _, _, 1, 2, _, 3, // 1000 1101
  _, 0, _, _, 1, 2, _, 3, // 0100 1101
  0, 1, _, _, 2, 3, _, 4, // 1100 1101
  _, _, 0, _, 1, 2, _, 3, // 0010 1101
  0, _, 1, _, 2, 3, _, 4, // 1010 1101
  _, 0, 1, _, 2, 3, _, 4, // 0110 1101
  0, 1, 2, _, 3, 4, _, 5, // 1110 1101
  _, _, _, 0, 1, 2, _, 3, // 0001 1101
  0, _, _, 1, 2, 3, _, 4, // 1001 1101
  _, 0, _, 1, 2, 3, _, 4, // 0101 1101
  0, 1, _, 2, 3, 4, _, 5, // 1101 1101
  _, _, 0, 1, 2, 3, _, 4, // 0011 1101
  0, _, 1, 2, 3, 4, _, 5, // 1011 1101
  _, 0, 1, 2, 3, 4, _, 5, // 0111 1101
  0, 1, 2, 3, 4, 5, _, 6, // 1111 1101
  _, _, _, _, _, _, 0, 1, // 0000 0011
  0, _, _, _, _, _, 1, 2, // 1000 0011
  _, 0, _, _, _, _, 1, 2, // 0100 0011
  0, 1, _, _, _, _, 2, 3, // 1100 0011
  _, _, 0, _, _, _, 1, 2, // 0010 0011
  0, _, 1, _, _, _, 2, 3, // 1010 0011
  _, 0, 1, _, _, _, 2, 3, // 0110 0011
  0, 1, 2, _, _, _, 3, 4, // 1110 0011
  _, _, _, 0, _, _, 1, 2, // 0001 0011
  0, _, _, 1, _, _, 2, 3, // 1001 0011
  _, 0, _, 1, _, _, 2, 3, // 0101 0011
  0, 1, _, 2, _, _, 3, 4, // 1101 0011
  _, _, 0, 1, _, _, 2, 3, // 0011 0011
  0, _, 1, 2, _, _, 3, 4, // 1011 0011
  _, 0, 1, 2, _, _, 3, 4, // 0111 0011
  0, 1, 2, 3, _, _, 4, 5, // 1111 0011
  _, _, _, _, 0, _, 1, 2, // 0000 1011
  0, _, _, _, 1, _, 2, 3, // 1000 1011
  _, 0, _, _, 1, _, 2, 3, // 0100 1011
  0, 1, _, _, 2, _, 3, 4, // 1100 1011
  _, _, 0, _, 1, _, 2, 3, // 0010 1011
  0, _, 1, _, 2, _, 3, 4, // 1010 1011
  _, 0, 1, _, 2, _, 3, 4, // 0110 1011
  0, 1, 2, _, 3, _, 4, 5, // 1110 1011
  _, _, _, 0, 1, _, 2, 3, // 0001 1011
  0, _, _, 1, 2, _, 3, 4, // 1001 1011
  _, 0, _, 1, 2, _, 3, 4, // 0101 1011
  0, 1, _, 2, 3, _, 4, 5, // 1101 1011
  _, _, 0, 1, 2, _, 3, 4, // 0011 1011
  0, _, 1, 2, 3, _, 4, 5, // 1011 1011
  _, 0, 1, 2, 3, _, 4, 5, // 0111 1011
  0, 1, 2, 3, 4, _, 5, 6, // 1111 1011
  _, _, _, _, _, 0, 1, 2, // 0000 0111
  0, _, _, _, _, 1, 2, 3, // 1000 0111
  _, 0, _, _, _, 1, 2, 3, // 0100 0111
  0, 1, _, _, _, 2, 3, 4, // 1100 0111
  _, _, 0, _, _, 1, 2, 3, // 0010 0111
  0, _, 1, _, _, 2, 3, 4, // 1010 0111
  _, 0, 1, _, _, 2, 3, 4, // 0110 0111
  0, 1, 2, _, _, 3, 4, 5, // 1110 0111
  _, _, _, 0, _, 1, 2, 3, // 0001 0111
  0, _, _, 1, _, 2, 3, 4, // 1001 0111
  _, 0, _, 1, _, 2, 3, 4, // 0101 0111
  0, 1, _, 2, _, 3, 4, 5, // 1101 0111
  _, _, 0, 1, _, 2, 3, 4, // 0011 0111
  0, _, 1, 2, _, 3, 4, 5, // 1011 0111
  _, 0, 1, 2, _, 3, 4, 5, // 0111 0111
  0, 1, 2, 3, _, 4, 5, 6, // 1111 0111
  _, _, _, _, 0, 1, 2, 3, // 0000 1111
  0, _, _, _, 1, 2, 3, 4, // 1000 1111
  _, 0, _, _, 1, 2, 3, 4, // 0100 1111
  0, 1, _, _, 2, 3, 4, 5, // 1100 1111
  _, _, 0, _, 1, 2, 3, 4, // 0010 1111
  0, _, 1, _, 2, 3, 4, 5, // 1010 1111
  _, 0, 1, _, 2, 3, 4, 5, // 0110 1111
  0, 1, 2, _, 3, 4, 5, 6, // 1110 1111
  _, _, _, 0, 1, 2, 3, 4, // 0001 1111
  0, _, _, 1, 2, 3, 4, 5, // 1001 1111
  _, 0, _, 1, 2, 3, 4, 5, // 0101 1111
  0, 1, _, 2, 3, 4, 5, 6, // 1101 1111
  _, _, 0, 1, 2, 3, 4, 5, // 0011 1111
  0, _, 1, 2, 3, 4, 5, 6, // 1011 1111
  _, 0, 1, 2, 3, 4, 5, 6, // 0111 1111
  0, 1, 2, 3, 4, 5, 6, 7, // 1111 1111
};

#undef _

//////////////////////////////////////////////////////////////////////////

template <uint32_t TotalSymbolCountBits>
#ifndef _MSC_VER
__attribute__((target("avx2")))
#endif
size_t rANS32x16_ymmPerm_16w_decode_avx2_varA(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity)
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

  const uint16_t *pReadHead = reinterpret_cast<const uint16_t *>(pInData + inputIndex);

  typedef __m256i simd_t;
  simd_t statesX8[StateCount / (sizeof(simd_t) / sizeof(uint32_t))];

  for (size_t i = 0; i < sizeof(statesX8) / sizeof(simd_t); i++)
    statesX8[i] = _mm256_loadu_si256(reinterpret_cast<const simd_t *>(reinterpret_cast<const uint8_t *>(states) + i * sizeof(simd_t)));

  const size_t outLengthInStates = expectedOutputLength - StateCount + 1;
  size_t i = 0;

  const simd_t symCountMask = _mm256_set1_epi32(TotalSymbolCount - 1);
  const simd_t lower16 = _mm256_set1_epi32(0xFFFF);
  const simd_t lower8 = _mm256_set1_epi32(0xFF);
  const simd_t decodeConsumePoint = _mm256_set1_epi32(DecodeConsumePoint16);
  const simd_t _16 = _mm256_set1_epi32(16);
  const simd_t _1 = _mm256_set1_epi32(1);

  for (; i < outLengthInStates; i += StateCount)
  {
    // const uint32_t slot = state & (TotalSymbolCount - 1);
    const simd_t slot0 = _mm256_and_si256(statesX8[0], symCountMask);
    const simd_t slot1 = _mm256_and_si256(statesX8[1], symCountMask);

    // const uint8_t symbol = pHist->cumulInv[slot];
    simd_t symbol0 = _mm256_i32gather_epi32(reinterpret_cast<const int32_t *>(&hist.cumulInv), slot0, sizeof(uint8_t));
    simd_t symbol1 = _mm256_i32gather_epi32(reinterpret_cast<const int32_t *>(&hist.cumulInv), slot1, sizeof(uint8_t));

    // since they were int32_t turn into uint8_t
    symbol0 = _mm256_and_si256(symbol0, lower8);
    symbol1 = _mm256_and_si256(symbol1, lower8);

    // pack symbols to one si128. (could possibly be `_mm256_cvtepi32_epi8` on avx-512f + avx-512-vl)
    const simd_t packed16 = _mm256_packus_epi32(symbol0, symbol1);
    const simd_t packed8 = _mm256_packus_epi16(packed16, packed16);
    const simd_t permuted = _mm256_permute4x64_epi64(packed8, 0b1000); // `abcdijklefghmnop`
    const __m128i permutedXmm = _mm256_castsi256_si128(permuted);

    // We intentionally encoded in a way to not have to do horrible things here. (TODO: make variant with `_mm256_stream_si256`/`_mm256_store_si256` if output buffer is 32 byte aligned)
    _mm_storeu_si128(reinterpret_cast<__m128i *>(pOutData + i), permutedXmm);

    // retrieve pack.
    const simd_t pack0 = _mm256_i32gather_epi32(reinterpret_cast<const int32_t *>(&hist.symbols), symbol0, sizeof(uint32_t));
    const simd_t pack1 = _mm256_i32gather_epi32(reinterpret_cast<const int32_t *>(&hist.symbols), symbol1, sizeof(uint32_t));

    // freq, cumul.
    const simd_t cumul0 = _mm256_srli_epi32(pack0, 16);
    const simd_t freq0 = _mm256_and_si256(pack0, lower16);
    const simd_t cumul1 = _mm256_srli_epi32(pack1, 16);
    const simd_t freq1 = _mm256_and_si256(pack1, lower16);

    // const uint32_t shiftedState = (state >> TotalSymbolCountBits);
    const simd_t shiftedState0 = _mm256_srli_epi32(statesX8[0], TotalSymbolCountBits);
    const simd_t shiftedState1 = _mm256_srli_epi32(statesX8[1], TotalSymbolCountBits);

    // const uint32_t freqScaled = shiftedState * freq;
    const __m256i freqScaled0 = _mm256_mullo_epi32(shiftedState0, freq0);
    const __m256i freqScaled1 = _mm256_mullo_epi32(shiftedState1, freq1);

    // state = freqScaled + slot - cumul;
    const simd_t state0 = _mm256_add_epi32(freqScaled0, _mm256_sub_epi32(slot0, cumul0));
    const simd_t state1 = _mm256_add_epi32(freqScaled1, _mm256_sub_epi32(slot1, cumul1));

    // now to the messy part...

    // read input for blocks 0, 1.
    const simd_t newWords01 = _mm256_loadu_si256(reinterpret_cast<const simd_t *>(pReadHead));

    // (state < DecodeConsumePoint16) ? -1 : 0 | well, actually (DecodeConsumePoint16 > state) ? -1 : 0
    const simd_t cmp0 = _mm256_cmpgt_epi32(decodeConsumePoint, state0);
    const simd_t cmp1 = _mm256_cmpgt_epi32(decodeConsumePoint, state1);

    // get masks of those compares & start loading shuffle masks. (TODO: test if `_mm_loadu_si128` or `_mm_set1_epi64x(_ShuffleLutPerm32[cmpMaskX << 3])` is faster, because the lut is probably not entirely cached)
    const uint32_t cmpMask0 = (uint32_t)_mm256_movemask_ps(_mm256_castsi256_ps(cmp0));
    __m128i lutXmm0 = _mm_lddqu_si128(reinterpret_cast<const __m128i *>(&_ShuffleLutPerm32[cmpMask0 << 3])); // `* 8`.

    const uint32_t cmpMask1 = (uint32_t)_mm256_movemask_ps(_mm256_castsi256_ps(cmp1));
    __m128i lutXmm1 = _mm_lddqu_si128(reinterpret_cast<const __m128i *>(&_ShuffleLutPerm32[cmpMask1 << 3])); // `* 8`.

    // advance read head & read input for blocks 2, 3.
    const uint32_t maskPop0 = (uint32_t)__builtin_popcount(cmpMask0);
    const uint32_t maskPop1 = (uint32_t)__builtin_popcount(cmpMask1);
    pReadHead += maskPop0 + maskPop1;

    // finalize lookups.
    const simd_t lut0 = _mm256_cvtepi8_epi32(lutXmm0);
    const simd_t lut1 = _mm256_add_epi32(_mm256_cvtepi8_epi32(lutXmm1), _mm256_set1_epi32(maskPop0));

    // lut /= 2 for corresponding word-pair that will be permuted in place.
    const simd_t halfLut0 = _mm256_srli_epi32(lut0, 1);
    const simd_t halfLut1 = _mm256_srli_epi32(lut1, 1);

    // create `16` wherever we want to keep the lower word of the word pair.
    const simd_t shiftLutMask0 = _mm256_slli_epi32(_mm256_andnot_si256(lut0, _1), 4);
    const simd_t shiftLutMask1 = _mm256_slli_epi32(_mm256_andnot_si256(lut1, _1), 4);

    // select corresponding word-pair.
    const __m256i selectedWordPair0 = _mm256_permutevar8x32_epi32(newWords01, halfLut0);
    const __m256i selectedWordPair1 = _mm256_permutevar8x32_epi32(newWords01, halfLut1);

    // selectively shift up by the `shiftLutMask` wherever we want to keep the lower word and then shift all down to clear the upper word and select the correct one of the pair.
    // then `and` with `cmp` mask, because `permute` (unlike `shuffle`) doesn't set the values with indexes > 8 to 0...
    const simd_t newWord0 = _mm256_and_si256(cmp0, _mm256_srli_epi32(_mm256_sllv_epi32(selectedWordPair0, shiftLutMask0), 16));
    const simd_t newWord1 = _mm256_and_si256(cmp1, _mm256_srli_epi32(_mm256_sllv_epi32(selectedWordPair1, shiftLutMask1), 16));

    // matching: state << 16
    const simd_t matchShiftedState0 = _mm256_sllv_epi32(state0, _mm256_and_si256(cmp0, _16));
    const simd_t matchShiftedState1 = _mm256_sllv_epi32(state1, _mm256_and_si256(cmp1, _16));

    // state = state << 16 | newWord;
    statesX8[0] = _mm256_or_si256(matchShiftedState0, newWord0);
    statesX8[1] = _mm256_or_si256(matchShiftedState1, newWord1);
  }

  for (size_t j = 0; j < sizeof(statesX8) / sizeof(simd_t); j++)
    _mm256_storeu_si256(reinterpret_cast<simd_t *>(reinterpret_cast<uint8_t *>(states) + j * sizeof(simd_t)), statesX8[j]);

  const uint8_t idx2idx[] = { 0x00, 0x01, 0x02, 0x03, 0x08, 0x09, 0x0A, 0x0B, 0x04, 0x05, 0x06, 0x07, 0x0C, 0x0D, 0x0E, 0x0F };
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
        const uint32_t newState = state << 16 | *pReadHead;
        state = read ? newState : state;
        pReadHead += (size_t)read;
      }
      else
      {
        if (state < DecodeConsumePoint16)
        {
          state = state << 16 | *pReadHead;
          pReadHead++;
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
size_t rANS32x16_ymmPerm_16w_decode_avx2_varB(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity)
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

  const uint16_t *pReadHead = reinterpret_cast<const uint16_t *>(pInData + inputIndex);

  typedef __m256i simd_t;
  simd_t statesX8[StateCount / (sizeof(simd_t) / sizeof(uint32_t))];

  for (size_t i = 0; i < sizeof(statesX8) / sizeof(simd_t); i++)
    statesX8[i] = _mm256_loadu_si256(reinterpret_cast<const simd_t *>(reinterpret_cast<const uint8_t *>(states) + i * sizeof(simd_t)));

  const size_t outLengthInStates = expectedOutputLength - StateCount + 1;
  size_t i = 0;

  const simd_t symCountMask = _mm256_set1_epi32(TotalSymbolCount - 1);
  const simd_t lower16 = _mm256_set1_epi32(0xFFFF);
  const simd_t lower8 = _mm256_set1_epi32(0xFF);
  const simd_t decodeConsumePoint = _mm256_set1_epi32(DecodeConsumePoint16);
  const simd_t _16 = _mm256_set1_epi32(16);
  const simd_t _1 = _mm256_set1_epi32(1);

  for (; i < outLengthInStates; i += StateCount)
  {
    // const uint32_t slot = state & (TotalSymbolCount - 1);
    const simd_t slot0 = _mm256_and_si256(statesX8[0], symCountMask);
    const simd_t slot1 = _mm256_and_si256(statesX8[1], symCountMask);

    // const uint8_t symbol = pHist->cumulInv[slot];
    simd_t symbol0 = _mm256_i32gather_epi32(reinterpret_cast<const int32_t *>(&histDec.cumulInv), slot0, sizeof(uint8_t));
    simd_t symbol1 = _mm256_i32gather_epi32(reinterpret_cast<const int32_t *>(&histDec.cumulInv), slot1, sizeof(uint8_t));

    // retrieve pack.
    const simd_t pack0 = _mm256_i32gather_epi32(reinterpret_cast<const int32_t *>(&histDec.symbolFomCumul), slot0, sizeof(uint32_t));
    const simd_t pack1 = _mm256_i32gather_epi32(reinterpret_cast<const int32_t *>(&histDec.symbolFomCumul), slot1, sizeof(uint32_t));

    // const uint32_t shiftedState = (state >> TotalSymbolCountBits);
    const simd_t shiftedState0 = _mm256_srli_epi32(statesX8[0], TotalSymbolCountBits);
    const simd_t shiftedState1 = _mm256_srli_epi32(statesX8[1], TotalSymbolCountBits);

    // since they were int32_t turn into uint8_t
    symbol0 = _mm256_and_si256(symbol0, lower8);
    symbol1 = _mm256_and_si256(symbol1, lower8);

    // pack symbols to one si128. (could possibly be `_mm256_cvtepi32_epi8` on avx-512f + avx-512-vl)
    const simd_t packed16 = _mm256_packus_epi32(symbol0, symbol1);
    const simd_t packed8 = _mm256_packus_epi16(packed16, packed16);
    const simd_t permuted = _mm256_permute4x64_epi64(packed8, 0b1000); // `abcdijklefghmnop`
    const __m128i permutedXmm = _mm256_castsi256_si128(permuted);

    // We intentionally encoded in a way to not have to do horrible things here. (TODO: make variant with `_mm256_stream_si256`/`_mm256_store_si256` if output buffer is 32 byte aligned)
    _mm_storeu_si128(reinterpret_cast<__m128i *>(pOutData + i), permutedXmm);

    // freq, cumul.
    const simd_t cumul0 = _mm256_srli_epi32(pack0, 16);
    const simd_t freq0 = _mm256_and_si256(pack0, lower16);
    const simd_t cumul1 = _mm256_srli_epi32(pack1, 16);
    const simd_t freq1 = _mm256_and_si256(pack1, lower16);

    // const uint32_t freqScaled = shiftedState * freq;
    const __m256i freqScaled0 = _mm256_mullo_epi32(shiftedState0, freq0);
    const __m256i freqScaled1 = _mm256_mullo_epi32(shiftedState1, freq1);

    // state = freqScaled + slot - cumul;
    const simd_t state0 = _mm256_add_epi32(freqScaled0, _mm256_sub_epi32(slot0, cumul0));
    const simd_t state1 = _mm256_add_epi32(freqScaled1, _mm256_sub_epi32(slot1, cumul1));

    // now to the messy part...

    // read input for blocks 0, 1.
    const simd_t newWords01 = _mm256_loadu_si256(reinterpret_cast<const simd_t *>(pReadHead));

    // (state < DecodeConsumePoint16) ? -1 : 0 | well, actually (DecodeConsumePoint16 > state) ? -1 : 0
    const simd_t cmp0 = _mm256_cmpgt_epi32(decodeConsumePoint, state0);
    const simd_t cmp1 = _mm256_cmpgt_epi32(decodeConsumePoint, state1);
    // get masks of those compares & start loading shuffle masks. (TODO: test if `_mm_loadu_si128` or `_mm_set1_epi64x(_ShuffleLutPerm32[cmpMaskX << 3])` is faster, because the lut is probably not entirely cached)
    const uint32_t cmpMask0 = (uint32_t)_mm256_movemask_ps(_mm256_castsi256_ps(cmp0));
    __m128i lutXmm0 = _mm_lddqu_si128(reinterpret_cast<const __m128i *>(&_ShuffleLutPerm32[cmpMask0 << 3])); // `* 8`.

    const uint32_t cmpMask1 = (uint32_t)_mm256_movemask_ps(_mm256_castsi256_ps(cmp1));
    __m128i lutXmm1 = _mm_lddqu_si128(reinterpret_cast<const __m128i *>(&_ShuffleLutPerm32[cmpMask1 << 3])); // `* 8`.

    // advance read head & read input for blocks 2, 3.
    const uint32_t maskPop0 = (uint32_t)__builtin_popcount(cmpMask0);
    const uint32_t maskPop1 = (uint32_t)__builtin_popcount(cmpMask1);
    pReadHead += maskPop0 + maskPop1;

    // finalize lookups.
    const simd_t lut0 = _mm256_cvtepi8_epi32(lutXmm0);
    const simd_t lut1 = _mm256_add_epi32(_mm256_cvtepi8_epi32(lutXmm1), _mm256_set1_epi32(maskPop0));

    // lut /= 2 for corresponding word-pair that will be permuted in place.
    const simd_t halfLut0 = _mm256_srli_epi32(lut0, 1);
    const simd_t halfLut1 = _mm256_srli_epi32(lut1, 1);

    // create `16` wherever we want to keep the lower word of the word pair.
    const simd_t shiftLutMask0 = _mm256_slli_epi32(_mm256_andnot_si256(lut0, _1), 4);
    const simd_t shiftLutMask1 = _mm256_slli_epi32(_mm256_andnot_si256(lut1, _1), 4);

    // select corresponding word-pair.
    const __m256i selectedWordPair0 = _mm256_permutevar8x32_epi32(newWords01, halfLut0);
    const __m256i selectedWordPair1 = _mm256_permutevar8x32_epi32(newWords01, halfLut1);

    // selectively shift up by the `shiftLutMask` wherever we want to keep the lower word and then shift all down to clear the upper word and select the correct one of the pair.
    // then `and` with `cmp` mask, because `permute` (unlike `shuffle`) doesn't set the values with indexes > 8 to 0...
    const simd_t newWord0 = _mm256_and_si256(cmp0, _mm256_srli_epi32(_mm256_sllv_epi32(selectedWordPair0, shiftLutMask0), 16));
    const simd_t newWord1 = _mm256_and_si256(cmp1, _mm256_srli_epi32(_mm256_sllv_epi32(selectedWordPair1, shiftLutMask1), 16));

    // matching: state << 16
    const simd_t matchShiftedState0 = _mm256_sllv_epi32(state0, _mm256_and_si256(cmp0, _16));
    const simd_t matchShiftedState1 = _mm256_sllv_epi32(state1, _mm256_and_si256(cmp1, _16));

    // state = state << 16 | newWord;
    statesX8[0] = _mm256_or_si256(matchShiftedState0, newWord0);
    statesX8[1] = _mm256_or_si256(matchShiftedState1, newWord1);
  }

  for (size_t j = 0; j < sizeof(statesX8) / sizeof(simd_t); j++)
    _mm256_storeu_si256(reinterpret_cast<simd_t *>(reinterpret_cast<uint8_t *>(states) + j * sizeof(simd_t)), statesX8[j]);

  const uint8_t idx2idx[] = { 0x00, 0x01, 0x02, 0x03, 0x08, 0x09, 0x0A, 0x0B, 0x04, 0x05, 0x06, 0x07, 0x0C, 0x0D, 0x0E, 0x0F };
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
        const uint32_t newState = state << 16 | *pReadHead;
        state = read ? newState : state;
        pReadHead += (size_t)read;
      }
      else
      {
        if (state < DecodeConsumePoint16)
        {
          state = state << 16 | *pReadHead;
          pReadHead++;
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
size_t rANS32x16_ymmPerm_16w_decode_avx2_varC(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity)
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

  hist_dec_pack_t<TotalSymbolCountBits> histDec;
  make_dec_pack_hist<TotalSymbolCountBits>(&histDec, &hist);

  uint32_t states[StateCount];

  for (size_t i = 0; i < StateCount; i++)
  {
    states[i] = *reinterpret_cast<const uint32_t *>(pInData + inputIndex);
    inputIndex += sizeof(uint32_t);
  }

  const uint16_t *pReadHead = reinterpret_cast<const uint16_t *>(pInData + inputIndex);

  typedef __m256i simd_t;
  simd_t statesX8[StateCount / (sizeof(simd_t) / sizeof(uint32_t))];

  for (size_t i = 0; i < sizeof(statesX8) / sizeof(simd_t); i++)
    statesX8[i] = _mm256_loadu_si256(reinterpret_cast<const simd_t *>(reinterpret_cast<const uint8_t *>(states) + i * sizeof(simd_t)));

  const size_t outLengthInStates = expectedOutputLength - StateCount + 1;
  size_t i = 0;

  const simd_t symCountMask = _mm256_set1_epi32(TotalSymbolCount - 1);
  const simd_t lower12 = _mm256_set1_epi32((1 << 12) - 1);
  const simd_t lower8 = _mm256_set1_epi32(0xFF);
  const simd_t decodeConsumePoint = _mm256_set1_epi32(DecodeConsumePoint16);
  const simd_t _16 = _mm256_set1_epi32(16);
  const simd_t _1 = _mm256_set1_epi32(1);

  for (; i < outLengthInStates; i += StateCount)
  {
    // const uint32_t slot = state & (TotalSymbolCount - 1);
    const simd_t slot0 = _mm256_and_si256(statesX8[0], symCountMask);
    const simd_t slot1 = _mm256_and_si256(statesX8[1], symCountMask);

    // retrieve pack.
    const simd_t pack0 = _mm256_i32gather_epi32(reinterpret_cast<const int32_t *>(&histDec.symbol), slot0, sizeof(uint32_t));
    const simd_t pack1 = _mm256_i32gather_epi32(reinterpret_cast<const int32_t *>(&histDec.symbol), slot1, sizeof(uint32_t));

    // const uint32_t shiftedState = (state >> TotalSymbolCountBits);
    const simd_t shiftedState0 = _mm256_srli_epi32(statesX8[0], TotalSymbolCountBits);
    const simd_t shiftedState1 = _mm256_srli_epi32(statesX8[1], TotalSymbolCountBits);

    // unpack symbol.
    const simd_t symbol0 = _mm256_and_si256(pack0, lower8);
    const simd_t symbol1 = _mm256_and_si256(pack1, lower8);

    // pack symbols to one si128. (could possibly be `_mm256_cvtepi32_epi8` on avx-512f + avx-512-vl)
    const simd_t packed16 = _mm256_packus_epi32(symbol0, symbol1);
    const simd_t packed8 = _mm256_packus_epi16(packed16, packed16);
    const simd_t permuted = _mm256_permute4x64_epi64(packed8, 0b1000); // `abcdijklefghmnop`
    const __m128i permutedXmm = _mm256_castsi256_si128(permuted);

    // We intentionally encoded in a way to not have to do horrible things here. (TODO: make variant with `_mm256_stream_si256`/`_mm256_store_si256` if output buffer is 32 byte aligned)
    _mm_storeu_si128(reinterpret_cast<__m128i *>(pOutData + i), permutedXmm);

    // unpack freq, cumul.
    const simd_t cumul0 = _mm256_and_si256(_mm256_srli_epi32(pack0, 8), lower12);
    const simd_t freq0 = _mm256_srli_epi32(pack0, 20);
    const simd_t cumul1 = _mm256_and_si256(_mm256_srli_epi32(pack1, 8), lower12);
    const simd_t freq1 = _mm256_srli_epi32(pack1, 20);

    // const uint32_t freqScaled = shiftedState * freq;
    const __m256i freqScaled0 = _mm256_mullo_epi32(shiftedState0, freq0);
    const __m256i freqScaled1 = _mm256_mullo_epi32(shiftedState1, freq1);

    // state = freqScaled + slot - cumul;
    const simd_t state0 = _mm256_add_epi32(freqScaled0, _mm256_sub_epi32(slot0, cumul0));
    const simd_t state1 = _mm256_add_epi32(freqScaled1, _mm256_sub_epi32(slot1, cumul1));

    // now to the messy part...

    // read input for blocks 0, 1.
    const simd_t newWords01 = _mm256_loadu_si256(reinterpret_cast<const simd_t *>(pReadHead));

    // (state < DecodeConsumePoint16) ? -1 : 0 | well, actually (DecodeConsumePoint16 > state) ? -1 : 0
    const simd_t cmp0 = _mm256_cmpgt_epi32(decodeConsumePoint, state0);
    const simd_t cmp1 = _mm256_cmpgt_epi32(decodeConsumePoint, state1);

    // get masks of those compares & start loading shuffle masks. (TODO: test if `_mm_loadu_si128` or `_mm_set1_epi64x(_ShuffleLutPerm32[cmpMaskX << 3])` is faster, because the lut is probably not entirely cached)
    const uint32_t cmpMask0 = (uint32_t)_mm256_movemask_ps(_mm256_castsi256_ps(cmp0));
    __m128i lutXmm0 = _mm_lddqu_si128(reinterpret_cast<const __m128i *>(&_ShuffleLutPerm32[cmpMask0 << 3])); // `* 8`.

    const uint32_t cmpMask1 = (uint32_t)_mm256_movemask_ps(_mm256_castsi256_ps(cmp1));
    __m128i lutXmm1 = _mm_lddqu_si128(reinterpret_cast<const __m128i *>(&_ShuffleLutPerm32[cmpMask1 << 3])); // `* 8`.

    // advance read head & read input for blocks 2, 3.
    const uint32_t maskPop0 = (uint32_t)__builtin_popcount(cmpMask0);
    const uint32_t maskPop1 = (uint32_t)__builtin_popcount(cmpMask1);
    pReadHead += maskPop0 + maskPop1;

    // finalize lookups.
    const simd_t lut0 = _mm256_cvtepi8_epi32(lutXmm0);
    const simd_t lut1 = _mm256_add_epi32(_mm256_cvtepi8_epi32(lutXmm1), _mm256_set1_epi32(maskPop0));

    // lut /= 2 for corresponding word-pair that will be permuted in place.
    const simd_t halfLut0 = _mm256_srli_epi32(lut0, 1);
    const simd_t halfLut1 = _mm256_srli_epi32(lut1, 1);

    // create `16` wherever we want to keep the lower word of the word pair.
    const simd_t shiftLutMask0 = _mm256_slli_epi32(_mm256_andnot_si256(lut0, _1), 4);
    const simd_t shiftLutMask1 = _mm256_slli_epi32(_mm256_andnot_si256(lut1, _1), 4);

    // select corresponding word-pair.
    const __m256i selectedWordPair0 = _mm256_permutevar8x32_epi32(newWords01, halfLut0);
    const __m256i selectedWordPair1 = _mm256_permutevar8x32_epi32(newWords01, halfLut1);

    // selectively shift up by the `shiftLutMask` wherever we want to keep the lower word and then shift all down to clear the upper word and select the correct one of the pair.
    // then `and` with `cmp` mask, because `permute` (unlike `shuffle`) doesn't set the values with indexes > 8 to 0...
    const simd_t newWord0 = _mm256_and_si256(cmp0, _mm256_srli_epi32(_mm256_sllv_epi32(selectedWordPair0, shiftLutMask0), 16));
    const simd_t newWord1 = _mm256_and_si256(cmp1, _mm256_srli_epi32(_mm256_sllv_epi32(selectedWordPair1, shiftLutMask1), 16));

    // matching: state << 16
    const simd_t matchShiftedState0 = _mm256_sllv_epi32(state0, _mm256_and_si256(cmp0, _16));
    const simd_t matchShiftedState1 = _mm256_sllv_epi32(state1, _mm256_and_si256(cmp1, _16));

    // state = state << 16 | newWord;
    statesX8[0] = _mm256_or_si256(matchShiftedState0, newWord0);
    statesX8[1] = _mm256_or_si256(matchShiftedState1, newWord1);
  }

  for (size_t j = 0; j < sizeof(statesX8) / sizeof(simd_t); j++)
    _mm256_storeu_si256(reinterpret_cast<simd_t *>(reinterpret_cast<uint8_t *>(states) + j * sizeof(simd_t)), statesX8[j]);

  const uint8_t idx2idx[] = { 0x00, 0x01, 0x02, 0x03, 0x08, 0x09, 0x0A, 0x0B, 0x04, 0x05, 0x06, 0x07, 0x0C, 0x0D, 0x0E, 0x0F };
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
        const uint32_t newState = state << 16 | *pReadHead;
        state = read ? newState : state;
        pReadHead += (size_t)read;
      }
      else
      {
        if (state < DecodeConsumePoint16)
        {
          state = state << 16 | *pReadHead;
          pReadHead++;
        }
      }

      states[j] = state;
    }
  }

  return expectedOutputLength;
}

//////////////////////////////////////////////////////////////////////////

size_t rANS32x16_ymmPerm_16w_decode_avx2_varA_15(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity) { return rANS32x16_ymmPerm_16w_decode_avx2_varA<15>(pInData, inLength, pOutData, outCapacity); }
size_t rANS32x16_ymmPerm_16w_decode_avx2_varB_15(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity) { return rANS32x16_ymmPerm_16w_decode_avx2_varB<15>(pInData, inLength, pOutData, outCapacity); }

size_t rANS32x16_ymmPerm_16w_decode_avx2_varA_14(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity) { return rANS32x16_ymmPerm_16w_decode_avx2_varA<14>(pInData, inLength, pOutData, outCapacity); }
size_t rANS32x16_ymmPerm_16w_decode_avx2_varB_14(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity) { return rANS32x16_ymmPerm_16w_decode_avx2_varB<14>(pInData, inLength, pOutData, outCapacity); }

size_t rANS32x16_ymmPerm_16w_decode_avx2_varA_13(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity) { return rANS32x16_ymmPerm_16w_decode_avx2_varA<13>(pInData, inLength, pOutData, outCapacity); }
size_t rANS32x16_ymmPerm_16w_decode_avx2_varB_13(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity) { return rANS32x16_ymmPerm_16w_decode_avx2_varB<13>(pInData, inLength, pOutData, outCapacity); }

size_t rANS32x16_ymmPerm_16w_decode_avx2_varA_12(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity) { return rANS32x16_ymmPerm_16w_decode_avx2_varA<12>(pInData, inLength, pOutData, outCapacity); }
size_t rANS32x16_ymmPerm_16w_decode_avx2_varB_12(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity) { return rANS32x16_ymmPerm_16w_decode_avx2_varB<12>(pInData, inLength, pOutData, outCapacity); }
size_t rANS32x16_ymmPerm_16w_decode_avx2_varC_12(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity) { return rANS32x16_ymmPerm_16w_decode_avx2_varC<12>(pInData, inLength, pOutData, outCapacity); }

size_t rANS32x16_ymmPerm_16w_decode_avx2_varA_11(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity) { return rANS32x16_ymmPerm_16w_decode_avx2_varA<11>(pInData, inLength, pOutData, outCapacity); }
size_t rANS32x16_ymmPerm_16w_decode_avx2_varB_11(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity) { return rANS32x16_ymmPerm_16w_decode_avx2_varB<11>(pInData, inLength, pOutData, outCapacity); }
size_t rANS32x16_ymmPerm_16w_decode_avx2_varC_11(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity) { return rANS32x16_ymmPerm_16w_decode_avx2_varC<11>(pInData, inLength, pOutData, outCapacity); }

size_t rANS32x16_ymmPerm_16w_decode_avx2_varA_10(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity) { return rANS32x16_ymmPerm_16w_decode_avx2_varA<10>(pInData, inLength, pOutData, outCapacity); }
size_t rANS32x16_ymmPerm_16w_decode_avx2_varB_10(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity) { return rANS32x16_ymmPerm_16w_decode_avx2_varB<10>(pInData, inLength, pOutData, outCapacity); }
size_t rANS32x16_ymmPerm_16w_decode_avx2_varC_10(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity) { return rANS32x16_ymmPerm_16w_decode_avx2_varC<10>(pInData, inLength, pOutData, outCapacity); }
