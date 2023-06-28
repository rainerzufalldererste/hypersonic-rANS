#include "hist.h"
#include "simd_platform.h"

#include <string.h>

constexpr size_t StateCount = 64; // Needs to be a power of two.
constexpr bool DecodeNoBranch = false;
constexpr bool EncodeNoBranch = false;

size_t rANS32x64_16w_capacity(const size_t inputSize)
{
  return inputSize + StateCount + sizeof(uint16_t) * 256 + sizeof(uint32_t) * StateCount + sizeof(uint64_t) * 2; // buffer + histogram + state
}

//////////////////////////////////////////////////////////////////////////

template <uint32_t TotalSymbolCountBits>
inline static uint8_t decode_symbol_scalar_32x64_16w(uint32_t *pState, const hist_dec_t<TotalSymbolCountBits> *pHist)
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
size_t rANS32x64_16w_encode_scalar(const uint8_t *pInData, const size_t length, uint8_t *pOutData, const size_t outCapacity, const hist_t *pHist)
{
  if (outCapacity < rANS32x64_16w_capacity(length))
    return 0;

  static_assert(TotalSymbolCountBits < 16);
  constexpr size_t EncodeEmitPoint = ((DecodeConsumePoint16 >> TotalSymbolCountBits) << 16);

  uint32_t states[StateCount];
  uint16_t *pEnd = reinterpret_cast<uint16_t *>(pOutData + outCapacity - sizeof(uint16_t));
  uint16_t *pStart = pEnd;

  // Init States.
  for (size_t i = 0; i < StateCount; i++)
    states[i] = DecodeConsumePoint16;

  const uint8_t idx2idx[] =
  {
    0x00, 0x01, 0x02, 0x03, 0x10, 0x11, 0x12, 0x13, 0x04, 0x05, 0x06, 0x07, 0x14, 0x15, 0x16, 0x17,
    0x08, 0x09, 0x0A, 0x0B, 0x18, 0x19, 0x1A, 0x1B, 0x0C, 0x0D, 0x0E, 0x0F, 0x1C, 0x1D, 0x1E, 0x1F,
    0x20, 0x21, 0x22, 0x23, 0x30, 0x31, 0x32, 0x33, 0x24, 0x25, 0x26, 0x27, 0x34, 0x35, 0x36, 0x37,
    0x28, 0x29, 0x2A, 0x2B, 0x38, 0x39, 0x3A, 0x3B, 0x2C, 0x2D, 0x2E, 0x2F, 0x3C, 0x3D, 0x3E, 0x3F,
  };

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
size_t rANS32x64_16w_decode_scalar(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity)
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

  const uint8_t idx2idx[] =
  {
    0x00, 0x01, 0x02, 0x03, 0x10, 0x11, 0x12, 0x13, 0x04, 0x05, 0x06, 0x07, 0x14, 0x15, 0x16, 0x17,
    0x08, 0x09, 0x0A, 0x0B, 0x18, 0x19, 0x1A, 0x1B, 0x0C, 0x0D, 0x0E, 0x0F, 0x1C, 0x1D, 0x1E, 0x1F,
    0x20, 0x21, 0x22, 0x23, 0x30, 0x31, 0x32, 0x33, 0x24, 0x25, 0x26, 0x27, 0x34, 0x35, 0x36, 0x37,
    0x28, 0x29, 0x2A, 0x2B, 0x38, 0x39, 0x3A, 0x3B, 0x2C, 0x2D, 0x2E, 0x2F, 0x3C, 0x3D, 0x3E, 0x3F,
  };

  static_assert(sizeof(idx2idx) == StateCount);

  const size_t outLengthInStates = expectedOutputLength - StateCount + 1;
  size_t i = 0;

  for (; i < outLengthInStates; i += StateCount)
  {
    for (size_t j = 0; j < StateCount; j++)
    {
      const uint8_t index = idx2idx[j];
      uint32_t state = states[j];

      pOutData[i + index] = decode_symbol_scalar_32x64_16w<TotalSymbolCountBits>(&state, &hist);

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

      pOutData[i + index] = decode_symbol_scalar_32x64_16w<TotalSymbolCountBits>(&state, &hist);

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

#define ___ 0xCC

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

template <uint32_t TotalSymbolCountBits, bool XmmShuffle, bool WriteAligned32 = false>
#ifndef _MSC_VER
__attribute__((target("avx2")))
#endif
size_t rANS32x64_16w_decode_avx2_varA(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity)
{
  if (inLength <= sizeof(uint64_t) * 2)
    return 0;

  if constexpr (!WriteAligned32)
    if ((reinterpret_cast<size_t>(pOutData) & (32 - 1)) == 0)
      return rANS32x64_16w_decode_avx2_varA<TotalSymbolCountBits, XmmShuffle, true>(pInData, inLength, pOutData, outCapacity);

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
  const __m128i shuffleDoubleMask = _mm_set_epi8(7, 7, 6, 6, 5, 5, 4, 4, 3, 3, 2, 2, 1, 1, 0, 0);
  const __m128i shuffleUpper16Bit = _mm_set1_epi16(0x0100);

  for (; i < outLengthInStates; i += StateCount)
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
    simd_t symbol0 = _mm256_i32gather_epi32(reinterpret_cast<const int32_t *>(&hist.cumulInv), slot0, sizeof(uint8_t));
    simd_t symbol1 = _mm256_i32gather_epi32(reinterpret_cast<const int32_t *>(&hist.cumulInv), slot1, sizeof(uint8_t));
    simd_t symbol2 = _mm256_i32gather_epi32(reinterpret_cast<const int32_t *>(&hist.cumulInv), slot2, sizeof(uint8_t));
    simd_t symbol3 = _mm256_i32gather_epi32(reinterpret_cast<const int32_t *>(&hist.cumulInv), slot3, sizeof(uint8_t));
    simd_t symbol4 = _mm256_i32gather_epi32(reinterpret_cast<const int32_t *>(&hist.cumulInv), slot4, sizeof(uint8_t));
    simd_t symbol5 = _mm256_i32gather_epi32(reinterpret_cast<const int32_t *>(&hist.cumulInv), slot5, sizeof(uint8_t));
    simd_t symbol6 = _mm256_i32gather_epi32(reinterpret_cast<const int32_t *>(&hist.cumulInv), slot6, sizeof(uint8_t));
    simd_t symbol7 = _mm256_i32gather_epi32(reinterpret_cast<const int32_t *>(&hist.cumulInv), slot7, sizeof(uint8_t));

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
    const simd_t pack0 = _mm256_i32gather_epi32(reinterpret_cast<const int32_t *>(&hist.symbols), symbol0, sizeof(uint32_t));
    const simd_t pack1 = _mm256_i32gather_epi32(reinterpret_cast<const int32_t *>(&hist.symbols), symbol1, sizeof(uint32_t));
    const simd_t pack2 = _mm256_i32gather_epi32(reinterpret_cast<const int32_t *>(&hist.symbols), symbol2, sizeof(uint32_t));
    const simd_t pack3 = _mm256_i32gather_epi32(reinterpret_cast<const int32_t *>(&hist.symbols), symbol3, sizeof(uint32_t));
    const simd_t pack4 = _mm256_i32gather_epi32(reinterpret_cast<const int32_t *>(&hist.symbols), symbol4, sizeof(uint32_t));
    const simd_t pack5 = _mm256_i32gather_epi32(reinterpret_cast<const int32_t *>(&hist.symbols), symbol5, sizeof(uint32_t));
    const simd_t pack6 = _mm256_i32gather_epi32(reinterpret_cast<const int32_t *>(&hist.symbols), symbol6, sizeof(uint32_t));
    const simd_t pack7 = _mm256_i32gather_epi32(reinterpret_cast<const int32_t *>(&hist.symbols), symbol7, sizeof(uint32_t));

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
    if constexpr (XmmShuffle)
    {
      // read input for blocks 0.
      const __m128i newWords0 = _mm_loadu_si128(reinterpret_cast<const __m128i *>(pReadHead));

      // (state < DecodeConsumePoint16) ? -1 : 0 | well, actually (DecodeConsumePoint16 > state) ? -1 : 0
      const simd_t cmp0 = _mm256_cmpgt_epi32(decodeConsumePoint, state0);
      const simd_t cmp1 = _mm256_cmpgt_epi32(decodeConsumePoint, state1);
      const simd_t cmp2 = _mm256_cmpgt_epi32(decodeConsumePoint, state2);
      const simd_t cmp3 = _mm256_cmpgt_epi32(decodeConsumePoint, state3);
      const simd_t cmp4 = _mm256_cmpgt_epi32(decodeConsumePoint, state4);
      const simd_t cmp5 = _mm256_cmpgt_epi32(decodeConsumePoint, state5);
      const simd_t cmp6 = _mm256_cmpgt_epi32(decodeConsumePoint, state6);
      const simd_t cmp7 = _mm256_cmpgt_epi32(decodeConsumePoint, state7);

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
      pReadHead += maskPop0;

      const __m128i newWords1 = _mm_loadu_si128(reinterpret_cast<const __m128i *>(pReadHead));

      const uint32_t maskPop1 = (uint32_t)__builtin_popcount(cmpMask1);
      pReadHead += maskPop1;

      const __m128i newWords2 = _mm_loadu_si128(reinterpret_cast<const __m128i *>(pReadHead));

      const uint32_t maskPop2 = (uint32_t)__builtin_popcount(cmpMask2);
      pReadHead += maskPop2;

      const __m128i newWords3 = _mm_loadu_si128(reinterpret_cast<const __m128i *>(pReadHead));

      const uint32_t maskPop3 = (uint32_t)__builtin_popcount(cmpMask3);
      pReadHead += maskPop3;

      const __m128i newWords4 = _mm_loadu_si128(reinterpret_cast<const __m128i *>(pReadHead));

      const uint32_t maskPop4 = (uint32_t)__builtin_popcount(cmpMask4);
      pReadHead += maskPop4;

      const __m128i newWords5 = _mm_loadu_si128(reinterpret_cast<const __m128i *>(pReadHead));

      const uint32_t maskPop5 = (uint32_t)__builtin_popcount(cmpMask5);
      pReadHead += maskPop5;

      const __m128i newWords6 = _mm_loadu_si128(reinterpret_cast<const __m128i *>(pReadHead));

      const uint32_t maskPop6 = (uint32_t)__builtin_popcount(cmpMask6);
      pReadHead += maskPop6;

      const __m128i newWords7 = _mm_loadu_si128(reinterpret_cast<const __m128i *>(pReadHead));

      const uint32_t maskPop7 = (uint32_t)__builtin_popcount(cmpMask7);
      pReadHead += maskPop7;

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
    else
    {
      // read input for blocks 0, 1.
      const simd_t newWords01 = _mm256_loadu_si256(reinterpret_cast<const simd_t *>(pReadHead));

      // (state < DecodeConsumePoint16) ? -1 : 0 | well, actually (DecodeConsumePoint16 > state) ? -1 : 0
      const simd_t cmp0 = _mm256_cmpgt_epi32(decodeConsumePoint, state0);
      const simd_t cmp1 = _mm256_cmpgt_epi32(decodeConsumePoint, state1);
      const simd_t cmp2 = _mm256_cmpgt_epi32(decodeConsumePoint, state2);
      const simd_t cmp3 = _mm256_cmpgt_epi32(decodeConsumePoint, state3);
      const simd_t cmp4 = _mm256_cmpgt_epi32(decodeConsumePoint, state4);
      const simd_t cmp5 = _mm256_cmpgt_epi32(decodeConsumePoint, state5);
      const simd_t cmp6 = _mm256_cmpgt_epi32(decodeConsumePoint, state6);
      const simd_t cmp7 = _mm256_cmpgt_epi32(decodeConsumePoint, state7);

      // get masks of those compares & start loading shuffle masks.
      const uint32_t cmpMask0 = (uint32_t)_mm256_movemask_ps(_mm256_castsi256_ps(cmp0));
      __m128i lutXmm0 = _mm_lddqu_si128(reinterpret_cast<const __m128i *>(&_ShuffleLutPerm32[cmpMask0 << 3])); // `* 8`.

      const uint32_t cmpMask1 = (uint32_t)_mm256_movemask_ps(_mm256_castsi256_ps(cmp1));
      __m128i lutXmm1 = _mm_lddqu_si128(reinterpret_cast<const __m128i *>(&_ShuffleLutPerm32[cmpMask1 << 3])); // `* 8`.

      const uint32_t cmpMask2 = (uint32_t)_mm256_movemask_ps(_mm256_castsi256_ps(cmp2));
      __m128i lutXmm2 = _mm_lddqu_si128(reinterpret_cast<const __m128i *>(&_ShuffleLutPerm32[cmpMask2 << 3])); // `* 8`.

      const uint32_t cmpMask3 = (uint32_t)_mm256_movemask_ps(_mm256_castsi256_ps(cmp3));
      __m128i lutXmm3 = _mm_lddqu_si128(reinterpret_cast<const __m128i *>(&_ShuffleLutPerm32[cmpMask3 << 3])); // `* 8`.

      const uint32_t cmpMask4 = (uint32_t)_mm256_movemask_ps(_mm256_castsi256_ps(cmp4));
      __m128i lutXmm4 = _mm_lddqu_si128(reinterpret_cast<const __m128i *>(&_ShuffleLutPerm32[cmpMask4 << 3])); // `* 8`.

      const uint32_t cmpMask5 = (uint32_t)_mm256_movemask_ps(_mm256_castsi256_ps(cmp5));
      __m128i lutXmm5 = _mm_lddqu_si128(reinterpret_cast<const __m128i *>(&_ShuffleLutPerm32[cmpMask5 << 3])); // `* 8`.

      const uint32_t cmpMask6 = (uint32_t)_mm256_movemask_ps(_mm256_castsi256_ps(cmp6));
      __m128i lutXmm6 = _mm_lddqu_si128(reinterpret_cast<const __m128i *>(&_ShuffleLutPerm32[cmpMask6 << 3])); // `* 8`.

      const uint32_t cmpMask7 = (uint32_t)_mm256_movemask_ps(_mm256_castsi256_ps(cmp7));
      __m128i lutXmm7 = _mm_lddqu_si128(reinterpret_cast<const __m128i *>(&_ShuffleLutPerm32[cmpMask7 << 3])); // `* 8`.

      // advance read head & read input for blocks 2, 3.
      const uint32_t maskPop0 = (uint32_t)__builtin_popcount(cmpMask0);
      const uint32_t maskPop1 = (uint32_t)__builtin_popcount(cmpMask1);
      pReadHead += maskPop0 + maskPop1;

      const simd_t newWords23 = _mm256_loadu_si256(reinterpret_cast<const simd_t *>(pReadHead));

      const uint32_t maskPop2 = (uint32_t)__builtin_popcount(cmpMask2);
      const uint32_t maskPop3 = (uint32_t)__builtin_popcount(cmpMask3);
      pReadHead += maskPop2 + maskPop3;

      // read input for blocks 4, 5
      const simd_t newWords45 = _mm256_loadu_si256(reinterpret_cast<const simd_t *>(pReadHead));

      // advance read head & read input for blocks 6, 7.
      const uint32_t maskPop4 = (uint32_t)__builtin_popcount(cmpMask4);
      const uint32_t maskPop5 = (uint32_t)__builtin_popcount(cmpMask5);
      pReadHead += maskPop4 + maskPop5;

      const simd_t newWords67 = _mm256_loadu_si256(reinterpret_cast<const simd_t *>(pReadHead));

      const uint32_t maskPop6 = (uint32_t)__builtin_popcount(cmpMask6);
      const uint32_t maskPop7 = (uint32_t)__builtin_popcount(cmpMask7);
      pReadHead += maskPop6 + maskPop7;

      // finalize lookups.
      const simd_t lut0 = _mm256_cvtepi8_epi32(lutXmm0);
      const simd_t lut2 = _mm256_cvtepi8_epi32(lutXmm2);
      const simd_t lut4 = _mm256_cvtepi8_epi32(lutXmm4);
      const simd_t lut6 = _mm256_cvtepi8_epi32(lutXmm6);
      const simd_t lut1 = _mm256_add_epi32(_mm256_cvtepi8_epi32(lutXmm1), _mm256_set1_epi32(maskPop0));
      const simd_t lut3 = _mm256_add_epi32(_mm256_cvtepi8_epi32(lutXmm3), _mm256_set1_epi32(maskPop2));
      const simd_t lut5 = _mm256_add_epi32(_mm256_cvtepi8_epi32(lutXmm5), _mm256_set1_epi32(maskPop4));
      const simd_t lut7 = _mm256_add_epi32(_mm256_cvtepi8_epi32(lutXmm7), _mm256_set1_epi32(maskPop6));

      // lut /= 2 for corresponding word-pair that will be permuted in place.
      const simd_t halfLut0 = _mm256_srli_epi32(lut0, 1);
      const simd_t halfLut2 = _mm256_srli_epi32(lut2, 1);
      const simd_t halfLut1 = _mm256_srli_epi32(lut1, 1);
      const simd_t halfLut3 = _mm256_srli_epi32(lut3, 1);
      const simd_t halfLut4 = _mm256_srli_epi32(lut4, 1);
      const simd_t halfLut6 = _mm256_srli_epi32(lut6, 1);
      const simd_t halfLut5 = _mm256_srli_epi32(lut5, 1);
      const simd_t halfLut7 = _mm256_srli_epi32(lut7, 1);

      // create `16` wherever we want to keep the lower word of the word pair.
      const simd_t shiftLutMask0 = _mm256_slli_epi32(_mm256_andnot_si256(lut0, _1), 4);
      const simd_t shiftLutMask1 = _mm256_slli_epi32(_mm256_andnot_si256(lut1, _1), 4);
      const simd_t shiftLutMask2 = _mm256_slli_epi32(_mm256_andnot_si256(lut2, _1), 4);
      const simd_t shiftLutMask3 = _mm256_slli_epi32(_mm256_andnot_si256(lut3, _1), 4);
      const simd_t shiftLutMask4 = _mm256_slli_epi32(_mm256_andnot_si256(lut4, _1), 4);
      const simd_t shiftLutMask5 = _mm256_slli_epi32(_mm256_andnot_si256(lut5, _1), 4);
      const simd_t shiftLutMask6 = _mm256_slli_epi32(_mm256_andnot_si256(lut6, _1), 4);
      const simd_t shiftLutMask7 = _mm256_slli_epi32(_mm256_andnot_si256(lut7, _1), 4);

      // select corresponding word-pair.
      const __m256i selectedWordPair0 = _mm256_permutevar8x32_epi32(newWords01, halfLut0);
      const __m256i selectedWordPair1 = _mm256_permutevar8x32_epi32(newWords01, halfLut1);
      const __m256i selectedWordPair2 = _mm256_permutevar8x32_epi32(newWords23, halfLut2);
      const __m256i selectedWordPair3 = _mm256_permutevar8x32_epi32(newWords23, halfLut3);
      const __m256i selectedWordPair4 = _mm256_permutevar8x32_epi32(newWords45, halfLut4);
      const __m256i selectedWordPair5 = _mm256_permutevar8x32_epi32(newWords45, halfLut5);
      const __m256i selectedWordPair6 = _mm256_permutevar8x32_epi32(newWords67, halfLut6);
      const __m256i selectedWordPair7 = _mm256_permutevar8x32_epi32(newWords67, halfLut7);

      // selectively shift up by the `shiftLutMask` wherever we want to keep the lower word and then shift all down to clear the upper word and select the correct one of the pair.
      // then `and` with `cmp` mask, because `permute` (unlike `shuffle`) doesn't set the values with indexes > 8 to 0...
      const simd_t newWord0 = _mm256_and_si256(cmp0, _mm256_srli_epi32(_mm256_sllv_epi32(selectedWordPair0, shiftLutMask0), 16));
      const simd_t newWord1 = _mm256_and_si256(cmp1, _mm256_srli_epi32(_mm256_sllv_epi32(selectedWordPair1, shiftLutMask1), 16));
      const simd_t newWord2 = _mm256_and_si256(cmp2, _mm256_srli_epi32(_mm256_sllv_epi32(selectedWordPair2, shiftLutMask2), 16));
      const simd_t newWord3 = _mm256_and_si256(cmp3, _mm256_srli_epi32(_mm256_sllv_epi32(selectedWordPair3, shiftLutMask3), 16));
      const simd_t newWord4 = _mm256_and_si256(cmp4, _mm256_srli_epi32(_mm256_sllv_epi32(selectedWordPair4, shiftLutMask4), 16));
      const simd_t newWord5 = _mm256_and_si256(cmp5, _mm256_srli_epi32(_mm256_sllv_epi32(selectedWordPair5, shiftLutMask5), 16));
      const simd_t newWord6 = _mm256_and_si256(cmp6, _mm256_srli_epi32(_mm256_sllv_epi32(selectedWordPair6, shiftLutMask6), 16));
      const simd_t newWord7 = _mm256_and_si256(cmp7, _mm256_srli_epi32(_mm256_sllv_epi32(selectedWordPair7, shiftLutMask7), 16));

      // matching: state << 16
      const simd_t matchShiftedState0 = _mm256_sllv_epi32(state0, _mm256_and_si256(cmp0, _16));
      const simd_t matchShiftedState1 = _mm256_sllv_epi32(state1, _mm256_and_si256(cmp1, _16));
      const simd_t matchShiftedState2 = _mm256_sllv_epi32(state2, _mm256_and_si256(cmp2, _16));
      const simd_t matchShiftedState3 = _mm256_sllv_epi32(state3, _mm256_and_si256(cmp3, _16));
      const simd_t matchShiftedState4 = _mm256_sllv_epi32(state4, _mm256_and_si256(cmp4, _16));
      const simd_t matchShiftedState5 = _mm256_sllv_epi32(state5, _mm256_and_si256(cmp5, _16));
      const simd_t matchShiftedState6 = _mm256_sllv_epi32(state6, _mm256_and_si256(cmp6, _16));
      const simd_t matchShiftedState7 = _mm256_sllv_epi32(state7, _mm256_and_si256(cmp7, _16));

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

  for (size_t j = 0; j < sizeof(statesX8) / sizeof(simd_t); j++)
    _mm256_storeu_si256(reinterpret_cast<simd_t *>(reinterpret_cast<uint8_t *>(states) + j * sizeof(simd_t)), statesX8[j]);

  const uint8_t idx2idx[] =
  {
    0x00, 0x01, 0x02, 0x03, 0x10, 0x11, 0x12, 0x13, 0x04, 0x05, 0x06, 0x07, 0x14, 0x15, 0x16, 0x17,
    0x08, 0x09, 0x0A, 0x0B, 0x18, 0x19, 0x1A, 0x1B, 0x0C, 0x0D, 0x0E, 0x0F, 0x1C, 0x1D, 0x1E, 0x1F,
    0x20, 0x21, 0x22, 0x23, 0x30, 0x31, 0x32, 0x33, 0x24, 0x25, 0x26, 0x27, 0x34, 0x35, 0x36, 0x37,
    0x28, 0x29, 0x2A, 0x2B, 0x38, 0x39, 0x3A, 0x3B, 0x2C, 0x2D, 0x2E, 0x2F, 0x3C, 0x3D, 0x3E, 0x3F,
  };

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

template <uint32_t TotalSymbolCountBits, bool XmmShuffle, bool WriteAligned32 = false>
#ifndef _MSC_VER
__attribute__((target("avx2")))
#endif
size_t rANS32x64_16w_decode_avx2_varB(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity)
{
  if (inLength <= sizeof(uint64_t) * 2)
    return 0;

  if constexpr (!WriteAligned32)
    if ((reinterpret_cast<size_t>(pOutData) & (32 - 1)) == 0)
      return rANS32x64_16w_decode_avx2_varB<TotalSymbolCountBits, XmmShuffle, true>(pInData, inLength, pOutData, outCapacity);

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
  const __m128i shuffleDoubleMask = _mm_set_epi8(7, 7, 6, 6, 5, 5, 4, 4, 3, 3, 2, 2, 1, 1, 0, 0);
  const __m128i shuffleUpper16Bit = _mm_set1_epi16(0x0100);

  for (; i < outLengthInStates; i += StateCount)
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
    simd_t symbol0 = _mm256_i32gather_epi32(reinterpret_cast<const int32_t *>(&histDec.cumulInv), slot0, sizeof(uint8_t));
    simd_t symbol1 = _mm256_i32gather_epi32(reinterpret_cast<const int32_t *>(&histDec.cumulInv), slot1, sizeof(uint8_t));
    simd_t symbol2 = _mm256_i32gather_epi32(reinterpret_cast<const int32_t *>(&histDec.cumulInv), slot2, sizeof(uint8_t));
    simd_t symbol3 = _mm256_i32gather_epi32(reinterpret_cast<const int32_t *>(&histDec.cumulInv), slot3, sizeof(uint8_t));
    simd_t symbol4 = _mm256_i32gather_epi32(reinterpret_cast<const int32_t *>(&histDec.cumulInv), slot4, sizeof(uint8_t));
    simd_t symbol5 = _mm256_i32gather_epi32(reinterpret_cast<const int32_t *>(&histDec.cumulInv), slot5, sizeof(uint8_t));
    simd_t symbol6 = _mm256_i32gather_epi32(reinterpret_cast<const int32_t *>(&histDec.cumulInv), slot6, sizeof(uint8_t));
    simd_t symbol7 = _mm256_i32gather_epi32(reinterpret_cast<const int32_t *>(&histDec.cumulInv), slot7, sizeof(uint8_t));

    // retrieve pack.
    const simd_t pack0 = _mm256_i32gather_epi32(reinterpret_cast<const int32_t *>(&histDec.symbolFomCumul), slot0, sizeof(uint32_t));
    const simd_t pack1 = _mm256_i32gather_epi32(reinterpret_cast<const int32_t *>(&histDec.symbolFomCumul), slot1, sizeof(uint32_t));
    const simd_t pack2 = _mm256_i32gather_epi32(reinterpret_cast<const int32_t *>(&histDec.symbolFomCumul), slot2, sizeof(uint32_t));
    const simd_t pack3 = _mm256_i32gather_epi32(reinterpret_cast<const int32_t *>(&histDec.symbolFomCumul), slot3, sizeof(uint32_t));
    const simd_t pack4 = _mm256_i32gather_epi32(reinterpret_cast<const int32_t *>(&histDec.symbolFomCumul), slot4, sizeof(uint32_t));
    const simd_t pack5 = _mm256_i32gather_epi32(reinterpret_cast<const int32_t *>(&histDec.symbolFomCumul), slot5, sizeof(uint32_t));
    const simd_t pack6 = _mm256_i32gather_epi32(reinterpret_cast<const int32_t *>(&histDec.symbolFomCumul), slot6, sizeof(uint32_t));
    const simd_t pack7 = _mm256_i32gather_epi32(reinterpret_cast<const int32_t *>(&histDec.symbolFomCumul), slot7, sizeof(uint32_t));

    // const uint32_t shiftedState = (state >> TotalSymbolCountBits);
    const simd_t shiftedState0 = _mm256_srli_epi32(statesX8[0], TotalSymbolCountBits);
    const simd_t shiftedState1 = _mm256_srli_epi32(statesX8[1], TotalSymbolCountBits);
    const simd_t shiftedState2 = _mm256_srli_epi32(statesX8[2], TotalSymbolCountBits);
    const simd_t shiftedState3 = _mm256_srli_epi32(statesX8[3], TotalSymbolCountBits);
    const simd_t shiftedState4 = _mm256_srli_epi32(statesX8[4], TotalSymbolCountBits);
    const simd_t shiftedState5 = _mm256_srli_epi32(statesX8[5], TotalSymbolCountBits);
    const simd_t shiftedState6 = _mm256_srli_epi32(statesX8[6], TotalSymbolCountBits);
    const simd_t shiftedState7 = _mm256_srli_epi32(statesX8[7], TotalSymbolCountBits);

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
    if constexpr (XmmShuffle)
    {
      // read input for blocks 0.
      const __m128i newWords0 = _mm_loadu_si128(reinterpret_cast<const __m128i *>(pReadHead));

      // (state < DecodeConsumePoint16) ? -1 : 0 | well, actually (DecodeConsumePoint16 > state) ? -1 : 0
      const simd_t cmp0 = _mm256_cmpgt_epi32(decodeConsumePoint, state0);
      const simd_t cmp1 = _mm256_cmpgt_epi32(decodeConsumePoint, state1);
      const simd_t cmp2 = _mm256_cmpgt_epi32(decodeConsumePoint, state2);
      const simd_t cmp3 = _mm256_cmpgt_epi32(decodeConsumePoint, state3);
      const simd_t cmp4 = _mm256_cmpgt_epi32(decodeConsumePoint, state4);
      const simd_t cmp5 = _mm256_cmpgt_epi32(decodeConsumePoint, state5);
      const simd_t cmp6 = _mm256_cmpgt_epi32(decodeConsumePoint, state6);
      const simd_t cmp7 = _mm256_cmpgt_epi32(decodeConsumePoint, state7);

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
      pReadHead += maskPop0;

      const __m128i newWords1 = _mm_loadu_si128(reinterpret_cast<const __m128i *>(pReadHead));

      const uint32_t maskPop1 = (uint32_t)__builtin_popcount(cmpMask1);
      pReadHead += maskPop1;

      const __m128i newWords2 = _mm_loadu_si128(reinterpret_cast<const __m128i *>(pReadHead));

      const uint32_t maskPop2 = (uint32_t)__builtin_popcount(cmpMask2);
      pReadHead += maskPop2;

      const __m128i newWords3 = _mm_loadu_si128(reinterpret_cast<const __m128i *>(pReadHead));

      const uint32_t maskPop3 = (uint32_t)__builtin_popcount(cmpMask3);
      pReadHead += maskPop3;

      const __m128i newWords4 = _mm_loadu_si128(reinterpret_cast<const __m128i *>(pReadHead));

      const uint32_t maskPop4 = (uint32_t)__builtin_popcount(cmpMask4);
      pReadHead += maskPop4;

      const __m128i newWords5 = _mm_loadu_si128(reinterpret_cast<const __m128i *>(pReadHead));

      const uint32_t maskPop5 = (uint32_t)__builtin_popcount(cmpMask5);
      pReadHead += maskPop5;

      const __m128i newWords6 = _mm_loadu_si128(reinterpret_cast<const __m128i *>(pReadHead));

      const uint32_t maskPop6 = (uint32_t)__builtin_popcount(cmpMask6);
      pReadHead += maskPop6;

      const __m128i newWords7 = _mm_loadu_si128(reinterpret_cast<const __m128i *>(pReadHead));

      const uint32_t maskPop7 = (uint32_t)__builtin_popcount(cmpMask7);
      pReadHead += maskPop7;

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
    else
    {
      // read input for blocks 0, 1.
      const simd_t newWords01 = _mm256_loadu_si256(reinterpret_cast<const simd_t *>(pReadHead));

      // (state < DecodeConsumePoint16) ? -1 : 0 | well, actually (DecodeConsumePoint16 > state) ? -1 : 0
      const simd_t cmp0 = _mm256_cmpgt_epi32(decodeConsumePoint, state0);
      const simd_t cmp1 = _mm256_cmpgt_epi32(decodeConsumePoint, state1);
      const simd_t cmp2 = _mm256_cmpgt_epi32(decodeConsumePoint, state2);
      const simd_t cmp3 = _mm256_cmpgt_epi32(decodeConsumePoint, state3);
      const simd_t cmp4 = _mm256_cmpgt_epi32(decodeConsumePoint, state4);
      const simd_t cmp5 = _mm256_cmpgt_epi32(decodeConsumePoint, state5);
      const simd_t cmp6 = _mm256_cmpgt_epi32(decodeConsumePoint, state6);
      const simd_t cmp7 = _mm256_cmpgt_epi32(decodeConsumePoint, state7);

      // get masks of those compares & start loading shuffle masks.
      const uint32_t cmpMask0 = (uint32_t)_mm256_movemask_ps(_mm256_castsi256_ps(cmp0));
      __m128i lutXmm0 = _mm_lddqu_si128(reinterpret_cast<const __m128i *>(&_ShuffleLutPerm32[cmpMask0 << 3])); // `* 8`.

      const uint32_t cmpMask1 = (uint32_t)_mm256_movemask_ps(_mm256_castsi256_ps(cmp1));
      __m128i lutXmm1 = _mm_lddqu_si128(reinterpret_cast<const __m128i *>(&_ShuffleLutPerm32[cmpMask1 << 3])); // `* 8`.

      const uint32_t cmpMask2 = (uint32_t)_mm256_movemask_ps(_mm256_castsi256_ps(cmp2));
      __m128i lutXmm2 = _mm_lddqu_si128(reinterpret_cast<const __m128i *>(&_ShuffleLutPerm32[cmpMask2 << 3])); // `* 8`.

      const uint32_t cmpMask3 = (uint32_t)_mm256_movemask_ps(_mm256_castsi256_ps(cmp3));
      __m128i lutXmm3 = _mm_lddqu_si128(reinterpret_cast<const __m128i *>(&_ShuffleLutPerm32[cmpMask3 << 3])); // `* 8`.

      const uint32_t cmpMask4 = (uint32_t)_mm256_movemask_ps(_mm256_castsi256_ps(cmp4));
      __m128i lutXmm4 = _mm_lddqu_si128(reinterpret_cast<const __m128i *>(&_ShuffleLutPerm32[cmpMask4 << 3])); // `* 8`.

      const uint32_t cmpMask5 = (uint32_t)_mm256_movemask_ps(_mm256_castsi256_ps(cmp5));
      __m128i lutXmm5 = _mm_lddqu_si128(reinterpret_cast<const __m128i *>(&_ShuffleLutPerm32[cmpMask5 << 3])); // `* 8`.

      const uint32_t cmpMask6 = (uint32_t)_mm256_movemask_ps(_mm256_castsi256_ps(cmp6));
      __m128i lutXmm6 = _mm_lddqu_si128(reinterpret_cast<const __m128i *>(&_ShuffleLutPerm32[cmpMask6 << 3])); // `* 8`.

      const uint32_t cmpMask7 = (uint32_t)_mm256_movemask_ps(_mm256_castsi256_ps(cmp7));
      __m128i lutXmm7 = _mm_lddqu_si128(reinterpret_cast<const __m128i *>(&_ShuffleLutPerm32[cmpMask7 << 3])); // `* 8`.

      // advance read head & read input for blocks 2, 3.
      const uint32_t maskPop0 = (uint32_t)__builtin_popcount(cmpMask0);
      const uint32_t maskPop1 = (uint32_t)__builtin_popcount(cmpMask1);
      pReadHead += maskPop0 + maskPop1;

      const simd_t newWords23 = _mm256_loadu_si256(reinterpret_cast<const simd_t *>(pReadHead));

      const uint32_t maskPop2 = (uint32_t)__builtin_popcount(cmpMask2);
      const uint32_t maskPop3 = (uint32_t)__builtin_popcount(cmpMask3);
      pReadHead += maskPop2 + maskPop3;

      // read input for blocks 4, 5
      const simd_t newWords45 = _mm256_loadu_si256(reinterpret_cast<const simd_t *>(pReadHead));

      // advance read head & read input for blocks 6, 7.
      const uint32_t maskPop4 = (uint32_t)__builtin_popcount(cmpMask4);
      const uint32_t maskPop5 = (uint32_t)__builtin_popcount(cmpMask5);
      pReadHead += maskPop4 + maskPop5;

      const simd_t newWords67 = _mm256_loadu_si256(reinterpret_cast<const simd_t *>(pReadHead));

      const uint32_t maskPop6 = (uint32_t)__builtin_popcount(cmpMask6);
      const uint32_t maskPop7 = (uint32_t)__builtin_popcount(cmpMask7);
      pReadHead += maskPop6 + maskPop7;

      // finalize lookups.
      const simd_t lut0 = _mm256_cvtepi8_epi32(lutXmm0);
      const simd_t lut2 = _mm256_cvtepi8_epi32(lutXmm2);
      const simd_t lut4 = _mm256_cvtepi8_epi32(lutXmm4);
      const simd_t lut6 = _mm256_cvtepi8_epi32(lutXmm6);
      const simd_t lut1 = _mm256_add_epi32(_mm256_cvtepi8_epi32(lutXmm1), _mm256_set1_epi32(maskPop0));
      const simd_t lut3 = _mm256_add_epi32(_mm256_cvtepi8_epi32(lutXmm3), _mm256_set1_epi32(maskPop2));
      const simd_t lut5 = _mm256_add_epi32(_mm256_cvtepi8_epi32(lutXmm5), _mm256_set1_epi32(maskPop4));
      const simd_t lut7 = _mm256_add_epi32(_mm256_cvtepi8_epi32(lutXmm7), _mm256_set1_epi32(maskPop6));

      // lut /= 2 for corresponding word-pair that will be permuted in place.
      const simd_t halfLut0 = _mm256_srli_epi32(lut0, 1);
      const simd_t halfLut2 = _mm256_srli_epi32(lut2, 1);
      const simd_t halfLut1 = _mm256_srli_epi32(lut1, 1);
      const simd_t halfLut3 = _mm256_srli_epi32(lut3, 1);
      const simd_t halfLut4 = _mm256_srli_epi32(lut4, 1);
      const simd_t halfLut6 = _mm256_srli_epi32(lut6, 1);
      const simd_t halfLut5 = _mm256_srli_epi32(lut5, 1);
      const simd_t halfLut7 = _mm256_srli_epi32(lut7, 1);

      // create `16` wherever we want to keep the lower word of the word pair.
      const simd_t shiftLutMask0 = _mm256_slli_epi32(_mm256_andnot_si256(lut0, _1), 4);
      const simd_t shiftLutMask1 = _mm256_slli_epi32(_mm256_andnot_si256(lut1, _1), 4);
      const simd_t shiftLutMask2 = _mm256_slli_epi32(_mm256_andnot_si256(lut2, _1), 4);
      const simd_t shiftLutMask3 = _mm256_slli_epi32(_mm256_andnot_si256(lut3, _1), 4);
      const simd_t shiftLutMask4 = _mm256_slli_epi32(_mm256_andnot_si256(lut4, _1), 4);
      const simd_t shiftLutMask5 = _mm256_slli_epi32(_mm256_andnot_si256(lut5, _1), 4);
      const simd_t shiftLutMask6 = _mm256_slli_epi32(_mm256_andnot_si256(lut6, _1), 4);
      const simd_t shiftLutMask7 = _mm256_slli_epi32(_mm256_andnot_si256(lut7, _1), 4);

      // select corresponding word-pair.
      const __m256i selectedWordPair0 = _mm256_permutevar8x32_epi32(newWords01, halfLut0);
      const __m256i selectedWordPair1 = _mm256_permutevar8x32_epi32(newWords01, halfLut1);
      const __m256i selectedWordPair2 = _mm256_permutevar8x32_epi32(newWords23, halfLut2);
      const __m256i selectedWordPair3 = _mm256_permutevar8x32_epi32(newWords23, halfLut3);
      const __m256i selectedWordPair4 = _mm256_permutevar8x32_epi32(newWords45, halfLut4);
      const __m256i selectedWordPair5 = _mm256_permutevar8x32_epi32(newWords45, halfLut5);
      const __m256i selectedWordPair6 = _mm256_permutevar8x32_epi32(newWords67, halfLut6);
      const __m256i selectedWordPair7 = _mm256_permutevar8x32_epi32(newWords67, halfLut7);

      // selectively shift up by the `shiftLutMask` wherever we want to keep the lower word and then shift all down to clear the upper word and select the correct one of the pair.
      // then `and` with `cmp` mask, because `permute` (unlike `shuffle`) doesn't set the values with indexes > 8 to 0...
      const simd_t newWord0 = _mm256_and_si256(cmp0, _mm256_srli_epi32(_mm256_sllv_epi32(selectedWordPair0, shiftLutMask0), 16));
      const simd_t newWord1 = _mm256_and_si256(cmp1, _mm256_srli_epi32(_mm256_sllv_epi32(selectedWordPair1, shiftLutMask1), 16));
      const simd_t newWord2 = _mm256_and_si256(cmp2, _mm256_srli_epi32(_mm256_sllv_epi32(selectedWordPair2, shiftLutMask2), 16));
      const simd_t newWord3 = _mm256_and_si256(cmp3, _mm256_srli_epi32(_mm256_sllv_epi32(selectedWordPair3, shiftLutMask3), 16));
      const simd_t newWord4 = _mm256_and_si256(cmp4, _mm256_srli_epi32(_mm256_sllv_epi32(selectedWordPair4, shiftLutMask4), 16));
      const simd_t newWord5 = _mm256_and_si256(cmp5, _mm256_srli_epi32(_mm256_sllv_epi32(selectedWordPair5, shiftLutMask5), 16));
      const simd_t newWord6 = _mm256_and_si256(cmp6, _mm256_srli_epi32(_mm256_sllv_epi32(selectedWordPair6, shiftLutMask6), 16));
      const simd_t newWord7 = _mm256_and_si256(cmp7, _mm256_srli_epi32(_mm256_sllv_epi32(selectedWordPair7, shiftLutMask7), 16));

      // matching: state << 16
      const simd_t matchShiftedState0 = _mm256_sllv_epi32(state0, _mm256_and_si256(cmp0, _16));
      const simd_t matchShiftedState1 = _mm256_sllv_epi32(state1, _mm256_and_si256(cmp1, _16));
      const simd_t matchShiftedState2 = _mm256_sllv_epi32(state2, _mm256_and_si256(cmp2, _16));
      const simd_t matchShiftedState3 = _mm256_sllv_epi32(state3, _mm256_and_si256(cmp3, _16));
      const simd_t matchShiftedState4 = _mm256_sllv_epi32(state4, _mm256_and_si256(cmp4, _16));
      const simd_t matchShiftedState5 = _mm256_sllv_epi32(state5, _mm256_and_si256(cmp5, _16));
      const simd_t matchShiftedState6 = _mm256_sllv_epi32(state6, _mm256_and_si256(cmp6, _16));
      const simd_t matchShiftedState7 = _mm256_sllv_epi32(state7, _mm256_and_si256(cmp7, _16));

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

  for (size_t j = 0; j < sizeof(statesX8) / sizeof(simd_t); j++)
    _mm256_storeu_si256(reinterpret_cast<simd_t *>(reinterpret_cast<uint8_t *>(states) + j * sizeof(simd_t)), statesX8[j]);

  const uint8_t idx2idx[] =
  {
    0x00, 0x01, 0x02, 0x03, 0x10, 0x11, 0x12, 0x13, 0x04, 0x05, 0x06, 0x07, 0x14, 0x15, 0x16, 0x17,
    0x08, 0x09, 0x0A, 0x0B, 0x18, 0x19, 0x1A, 0x1B, 0x0C, 0x0D, 0x0E, 0x0F, 0x1C, 0x1D, 0x1E, 0x1F,
    0x20, 0x21, 0x22, 0x23, 0x30, 0x31, 0x32, 0x33, 0x24, 0x25, 0x26, 0x27, 0x34, 0x35, 0x36, 0x37,
    0x28, 0x29, 0x2A, 0x2B, 0x38, 0x39, 0x3A, 0x3B, 0x2C, 0x2D, 0x2E, 0x2F, 0x3C, 0x3D, 0x3E, 0x3F,
  };

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

template <uint32_t TotalSymbolCountBits, bool XmmShuffle, bool WriteAligned32 = false>
#ifndef _MSC_VER
__attribute__((target("avx2")))
#endif
size_t rANS32x64_16w_decode_avx2_varC(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity)
{
  if (inLength <= sizeof(uint64_t) * 2)
    return 0;

  if constexpr (!WriteAligned32)
    if ((reinterpret_cast<size_t>(pOutData) & (32 - 1)) == 0)
      return rANS32x64_16w_decode_avx2_varC<TotalSymbolCountBits, XmmShuffle, true>(pInData, inLength, pOutData, outCapacity);

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
  const __m128i shuffleDoubleMask = _mm_set_epi8(7, 7, 6, 6, 5, 5, 4, 4, 3, 3, 2, 2, 1, 1, 0, 0);
  const __m128i shuffleUpper16Bit = _mm_set1_epi16(0x0100);

  for (; i < outLengthInStates; i += StateCount)
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
    const simd_t pack0 = _mm256_i32gather_epi32(reinterpret_cast<const int32_t *>(&histDec.symbol), slot0, sizeof(uint32_t));
    const simd_t pack1 = _mm256_i32gather_epi32(reinterpret_cast<const int32_t *>(&histDec.symbol), slot1, sizeof(uint32_t));
    const simd_t pack2 = _mm256_i32gather_epi32(reinterpret_cast<const int32_t *>(&histDec.symbol), slot2, sizeof(uint32_t));
    const simd_t pack3 = _mm256_i32gather_epi32(reinterpret_cast<const int32_t *>(&histDec.symbol), slot3, sizeof(uint32_t));
    const simd_t pack4 = _mm256_i32gather_epi32(reinterpret_cast<const int32_t *>(&histDec.symbol), slot4, sizeof(uint32_t));
    const simd_t pack5 = _mm256_i32gather_epi32(reinterpret_cast<const int32_t *>(&histDec.symbol), slot5, sizeof(uint32_t));
    const simd_t pack6 = _mm256_i32gather_epi32(reinterpret_cast<const int32_t *>(&histDec.symbol), slot6, sizeof(uint32_t));
    const simd_t pack7 = _mm256_i32gather_epi32(reinterpret_cast<const int32_t *>(&histDec.symbol), slot7, sizeof(uint32_t));

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
    if constexpr (XmmShuffle)
    {
      // read input for blocks 0.
      const __m128i newWords0 = _mm_loadu_si128(reinterpret_cast<const __m128i *>(pReadHead));

      // (state < DecodeConsumePoint16) ? -1 : 0 | well, actually (DecodeConsumePoint16 > state) ? -1 : 0
      const simd_t cmp0 = _mm256_cmpgt_epi32(decodeConsumePoint, state0);
      const simd_t cmp1 = _mm256_cmpgt_epi32(decodeConsumePoint, state1);
      const simd_t cmp2 = _mm256_cmpgt_epi32(decodeConsumePoint, state2);
      const simd_t cmp3 = _mm256_cmpgt_epi32(decodeConsumePoint, state3);
      const simd_t cmp4 = _mm256_cmpgt_epi32(decodeConsumePoint, state4);
      const simd_t cmp5 = _mm256_cmpgt_epi32(decodeConsumePoint, state5);
      const simd_t cmp6 = _mm256_cmpgt_epi32(decodeConsumePoint, state6);
      const simd_t cmp7 = _mm256_cmpgt_epi32(decodeConsumePoint, state7);

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
      pReadHead += maskPop0;

      const __m128i newWords1 = _mm_loadu_si128(reinterpret_cast<const __m128i *>(pReadHead));

      const uint32_t maskPop1 = (uint32_t)__builtin_popcount(cmpMask1);
      pReadHead += maskPop1;

      const __m128i newWords2 = _mm_loadu_si128(reinterpret_cast<const __m128i *>(pReadHead));

      const uint32_t maskPop2 = (uint32_t)__builtin_popcount(cmpMask2);
      pReadHead += maskPop2;

      const __m128i newWords3 = _mm_loadu_si128(reinterpret_cast<const __m128i *>(pReadHead));

      const uint32_t maskPop3 = (uint32_t)__builtin_popcount(cmpMask3);
      pReadHead += maskPop3;

      const __m128i newWords4 = _mm_loadu_si128(reinterpret_cast<const __m128i *>(pReadHead));

      const uint32_t maskPop4 = (uint32_t)__builtin_popcount(cmpMask4);
      pReadHead += maskPop4;

      const __m128i newWords5 = _mm_loadu_si128(reinterpret_cast<const __m128i *>(pReadHead));

      const uint32_t maskPop5 = (uint32_t)__builtin_popcount(cmpMask5);
      pReadHead += maskPop5;

      const __m128i newWords6 = _mm_loadu_si128(reinterpret_cast<const __m128i *>(pReadHead));

      const uint32_t maskPop6 = (uint32_t)__builtin_popcount(cmpMask6);
      pReadHead += maskPop6;

      const __m128i newWords7 = _mm_loadu_si128(reinterpret_cast<const __m128i *>(pReadHead));

      const uint32_t maskPop7 = (uint32_t)__builtin_popcount(cmpMask7);
      pReadHead += maskPop7;

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
    else
    {
      // read input for blocks 0, 1.
      const simd_t newWords01 = _mm256_loadu_si256(reinterpret_cast<const simd_t *>(pReadHead));

      // (state < DecodeConsumePoint16) ? -1 : 0 | well, actually (DecodeConsumePoint16 > state) ? -1 : 0
      const simd_t cmp0 = _mm256_cmpgt_epi32(decodeConsumePoint, state0);
      const simd_t cmp1 = _mm256_cmpgt_epi32(decodeConsumePoint, state1);
      const simd_t cmp2 = _mm256_cmpgt_epi32(decodeConsumePoint, state2);
      const simd_t cmp3 = _mm256_cmpgt_epi32(decodeConsumePoint, state3);
      const simd_t cmp4 = _mm256_cmpgt_epi32(decodeConsumePoint, state4);
      const simd_t cmp5 = _mm256_cmpgt_epi32(decodeConsumePoint, state5);
      const simd_t cmp6 = _mm256_cmpgt_epi32(decodeConsumePoint, state6);
      const simd_t cmp7 = _mm256_cmpgt_epi32(decodeConsumePoint, state7);

      // get masks of those compares & start loading shuffle masks.
      const uint32_t cmpMask0 = (uint32_t)_mm256_movemask_ps(_mm256_castsi256_ps(cmp0));
      __m128i lutXmm0 = _mm_lddqu_si128(reinterpret_cast<const __m128i *>(&_ShuffleLutPerm32[cmpMask0 << 3])); // `* 8`.

      const uint32_t cmpMask1 = (uint32_t)_mm256_movemask_ps(_mm256_castsi256_ps(cmp1));
      __m128i lutXmm1 = _mm_lddqu_si128(reinterpret_cast<const __m128i *>(&_ShuffleLutPerm32[cmpMask1 << 3])); // `* 8`.

      const uint32_t cmpMask2 = (uint32_t)_mm256_movemask_ps(_mm256_castsi256_ps(cmp2));
      __m128i lutXmm2 = _mm_lddqu_si128(reinterpret_cast<const __m128i *>(&_ShuffleLutPerm32[cmpMask2 << 3])); // `* 8`.

      const uint32_t cmpMask3 = (uint32_t)_mm256_movemask_ps(_mm256_castsi256_ps(cmp3));
      __m128i lutXmm3 = _mm_lddqu_si128(reinterpret_cast<const __m128i *>(&_ShuffleLutPerm32[cmpMask3 << 3])); // `* 8`.

      const uint32_t cmpMask4 = (uint32_t)_mm256_movemask_ps(_mm256_castsi256_ps(cmp4));
      __m128i lutXmm4 = _mm_lddqu_si128(reinterpret_cast<const __m128i *>(&_ShuffleLutPerm32[cmpMask4 << 3])); // `* 8`.

      const uint32_t cmpMask5 = (uint32_t)_mm256_movemask_ps(_mm256_castsi256_ps(cmp5));
      __m128i lutXmm5 = _mm_lddqu_si128(reinterpret_cast<const __m128i *>(&_ShuffleLutPerm32[cmpMask5 << 3])); // `* 8`.

      const uint32_t cmpMask6 = (uint32_t)_mm256_movemask_ps(_mm256_castsi256_ps(cmp6));
      __m128i lutXmm6 = _mm_lddqu_si128(reinterpret_cast<const __m128i *>(&_ShuffleLutPerm32[cmpMask6 << 3])); // `* 8`.

      const uint32_t cmpMask7 = (uint32_t)_mm256_movemask_ps(_mm256_castsi256_ps(cmp7));
      __m128i lutXmm7 = _mm_lddqu_si128(reinterpret_cast<const __m128i *>(&_ShuffleLutPerm32[cmpMask7 << 3])); // `* 8`.

      // advance read head & read input for blocks 2, 3.
      const uint32_t maskPop0 = (uint32_t)__builtin_popcount(cmpMask0);
      const uint32_t maskPop1 = (uint32_t)__builtin_popcount(cmpMask1);
      pReadHead += maskPop0 + maskPop1;

      const simd_t newWords23 = _mm256_loadu_si256(reinterpret_cast<const simd_t *>(pReadHead));

      const uint32_t maskPop2 = (uint32_t)__builtin_popcount(cmpMask2);
      const uint32_t maskPop3 = (uint32_t)__builtin_popcount(cmpMask3);
      pReadHead += maskPop2 + maskPop3;

      // read input for blocks 4, 5
      const simd_t newWords45 = _mm256_loadu_si256(reinterpret_cast<const simd_t *>(pReadHead));

      // advance read head & read input for blocks 6, 7.
      const uint32_t maskPop4 = (uint32_t)__builtin_popcount(cmpMask4);
      const uint32_t maskPop5 = (uint32_t)__builtin_popcount(cmpMask5);
      pReadHead += maskPop4 + maskPop5;

      const simd_t newWords67 = _mm256_loadu_si256(reinterpret_cast<const simd_t *>(pReadHead));

      const uint32_t maskPop6 = (uint32_t)__builtin_popcount(cmpMask6);
      const uint32_t maskPop7 = (uint32_t)__builtin_popcount(cmpMask7);
      pReadHead += maskPop6 + maskPop7;

      // finalize lookups.
      const simd_t lut0 = _mm256_cvtepi8_epi32(lutXmm0);
      const simd_t lut2 = _mm256_cvtepi8_epi32(lutXmm2);
      const simd_t lut4 = _mm256_cvtepi8_epi32(lutXmm4);
      const simd_t lut6 = _mm256_cvtepi8_epi32(lutXmm6);
      const simd_t lut1 = _mm256_add_epi32(_mm256_cvtepi8_epi32(lutXmm1), _mm256_set1_epi32(maskPop0));
      const simd_t lut3 = _mm256_add_epi32(_mm256_cvtepi8_epi32(lutXmm3), _mm256_set1_epi32(maskPop2));
      const simd_t lut5 = _mm256_add_epi32(_mm256_cvtepi8_epi32(lutXmm5), _mm256_set1_epi32(maskPop4));
      const simd_t lut7 = _mm256_add_epi32(_mm256_cvtepi8_epi32(lutXmm7), _mm256_set1_epi32(maskPop6));

      // lut /= 2 for corresponding word-pair that will be permuted in place.
      const simd_t halfLut0 = _mm256_srli_epi32(lut0, 1);
      const simd_t halfLut2 = _mm256_srli_epi32(lut2, 1);
      const simd_t halfLut1 = _mm256_srli_epi32(lut1, 1);
      const simd_t halfLut3 = _mm256_srli_epi32(lut3, 1);
      const simd_t halfLut4 = _mm256_srli_epi32(lut4, 1);
      const simd_t halfLut6 = _mm256_srli_epi32(lut6, 1);
      const simd_t halfLut5 = _mm256_srli_epi32(lut5, 1);
      const simd_t halfLut7 = _mm256_srli_epi32(lut7, 1);

      // create `16` wherever we want to keep the lower word of the word pair.
      const simd_t shiftLutMask0 = _mm256_slli_epi32(_mm256_andnot_si256(lut0, _1), 4);
      const simd_t shiftLutMask1 = _mm256_slli_epi32(_mm256_andnot_si256(lut1, _1), 4);
      const simd_t shiftLutMask2 = _mm256_slli_epi32(_mm256_andnot_si256(lut2, _1), 4);
      const simd_t shiftLutMask3 = _mm256_slli_epi32(_mm256_andnot_si256(lut3, _1), 4);
      const simd_t shiftLutMask4 = _mm256_slli_epi32(_mm256_andnot_si256(lut4, _1), 4);
      const simd_t shiftLutMask5 = _mm256_slli_epi32(_mm256_andnot_si256(lut5, _1), 4);
      const simd_t shiftLutMask6 = _mm256_slli_epi32(_mm256_andnot_si256(lut6, _1), 4);
      const simd_t shiftLutMask7 = _mm256_slli_epi32(_mm256_andnot_si256(lut7, _1), 4);

      // select corresponding word-pair.
      const __m256i selectedWordPair0 = _mm256_permutevar8x32_epi32(newWords01, halfLut0);
      const __m256i selectedWordPair1 = _mm256_permutevar8x32_epi32(newWords01, halfLut1);
      const __m256i selectedWordPair2 = _mm256_permutevar8x32_epi32(newWords23, halfLut2);
      const __m256i selectedWordPair3 = _mm256_permutevar8x32_epi32(newWords23, halfLut3);
      const __m256i selectedWordPair4 = _mm256_permutevar8x32_epi32(newWords45, halfLut4);
      const __m256i selectedWordPair5 = _mm256_permutevar8x32_epi32(newWords45, halfLut5);
      const __m256i selectedWordPair6 = _mm256_permutevar8x32_epi32(newWords67, halfLut6);
      const __m256i selectedWordPair7 = _mm256_permutevar8x32_epi32(newWords67, halfLut7);

      // selectively shift up by the `shiftLutMask` wherever we want to keep the lower word and then shift all down to clear the upper word and select the correct one of the pair.
      // then `and` with `cmp` mask, because `permute` (unlike `shuffle`) doesn't set the values with indexes > 8 to 0...
      const simd_t newWord0 = _mm256_and_si256(cmp0, _mm256_srli_epi32(_mm256_sllv_epi32(selectedWordPair0, shiftLutMask0), 16));
      const simd_t newWord1 = _mm256_and_si256(cmp1, _mm256_srli_epi32(_mm256_sllv_epi32(selectedWordPair1, shiftLutMask1), 16));
      const simd_t newWord2 = _mm256_and_si256(cmp2, _mm256_srli_epi32(_mm256_sllv_epi32(selectedWordPair2, shiftLutMask2), 16));
      const simd_t newWord3 = _mm256_and_si256(cmp3, _mm256_srli_epi32(_mm256_sllv_epi32(selectedWordPair3, shiftLutMask3), 16));
      const simd_t newWord4 = _mm256_and_si256(cmp4, _mm256_srli_epi32(_mm256_sllv_epi32(selectedWordPair4, shiftLutMask4), 16));
      const simd_t newWord5 = _mm256_and_si256(cmp5, _mm256_srli_epi32(_mm256_sllv_epi32(selectedWordPair5, shiftLutMask5), 16));
      const simd_t newWord6 = _mm256_and_si256(cmp6, _mm256_srli_epi32(_mm256_sllv_epi32(selectedWordPair6, shiftLutMask6), 16));
      const simd_t newWord7 = _mm256_and_si256(cmp7, _mm256_srli_epi32(_mm256_sllv_epi32(selectedWordPair7, shiftLutMask7), 16));

      // matching: state << 16
      const simd_t matchShiftedState0 = _mm256_sllv_epi32(state0, _mm256_and_si256(cmp0, _16));
      const simd_t matchShiftedState1 = _mm256_sllv_epi32(state1, _mm256_and_si256(cmp1, _16));
      const simd_t matchShiftedState2 = _mm256_sllv_epi32(state2, _mm256_and_si256(cmp2, _16));
      const simd_t matchShiftedState3 = _mm256_sllv_epi32(state3, _mm256_and_si256(cmp3, _16));
      const simd_t matchShiftedState4 = _mm256_sllv_epi32(state4, _mm256_and_si256(cmp4, _16));
      const simd_t matchShiftedState5 = _mm256_sllv_epi32(state5, _mm256_and_si256(cmp5, _16));
      const simd_t matchShiftedState6 = _mm256_sllv_epi32(state6, _mm256_and_si256(cmp6, _16));
      const simd_t matchShiftedState7 = _mm256_sllv_epi32(state7, _mm256_and_si256(cmp7, _16));

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

  for (size_t j = 0; j < sizeof(statesX8) / sizeof(simd_t); j++)
    _mm256_storeu_si256(reinterpret_cast<simd_t *>(reinterpret_cast<uint8_t *>(states) + j * sizeof(simd_t)), statesX8[j]);

  const uint8_t idx2idx[] =
  {
    0x00, 0x01, 0x02, 0x03, 0x10, 0x11, 0x12, 0x13, 0x04, 0x05, 0x06, 0x07, 0x14, 0x15, 0x16, 0x17,
    0x08, 0x09, 0x0A, 0x0B, 0x18, 0x19, 0x1A, 0x1B, 0x0C, 0x0D, 0x0E, 0x0F, 0x1C, 0x1D, 0x1E, 0x1F,
    0x20, 0x21, 0x22, 0x23, 0x30, 0x31, 0x32, 0x33, 0x24, 0x25, 0x26, 0x27, 0x34, 0x35, 0x36, 0x37,
    0x28, 0x29, 0x2A, 0x2B, 0x38, 0x39, 0x3A, 0x3B, 0x2C, 0x2D, 0x2E, 0x2F, 0x3C, 0x3D, 0x3E, 0x3F,
  };

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

template <uint32_t TotalSymbolCountBits, bool WriteAligned64 = false>
#ifndef _MSC_VER
#ifdef __llvm__
__attribute__((target("avx512bw")))
#else
__attribute__((target("avx512f", "avx512dq", "avx512bw")))
#endif
#endif
size_t rANS32x64_16w_decode_avx512fdqbw_varC(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity)
{
  if (inLength <= sizeof(uint64_t) * 2)
    return 0;

  if constexpr (!WriteAligned64)
    if ((reinterpret_cast<size_t>(pOutData) & (64 - 1)) == 0)
      return rANS32x64_16w_decode_avx512fdqbw_varC<TotalSymbolCountBits, true>(pInData, inLength, pOutData, outCapacity);

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

  typedef __m512i simd_t;
  simd_t statesX8[StateCount / (sizeof(simd_t) / sizeof(uint32_t))];

  for (size_t i = 0; i < sizeof(statesX8) / sizeof(simd_t); i++)
    statesX8[i] = _mm512_loadu_si512(reinterpret_cast<const simd_t *>(reinterpret_cast<const uint8_t *>(states) + i * sizeof(simd_t)));

  const size_t outLengthInStates = expectedOutputLength - StateCount + 1;
  size_t i = 0;

  const simd_t symCountMask = _mm512_set1_epi32(TotalSymbolCount - 1);
  const simd_t lower16 = _mm512_set1_epi32(0xFFFF);
  const simd_t lower12 = _mm512_set1_epi32((1 << 12) - 1);
  const simd_t lower8 = _mm512_set1_epi32(0xFF);
  const simd_t decodeConsumePoint = _mm512_set1_epi32(DecodeConsumePoint16);
  const simd_t symbolPermuteMask = _mm512_set_epi32(15, 7, 14, 6, 11, 3, 10, 2, 13, 5, 12, 4, 9, 1, 8, 0);

  for (; i < outLengthInStates; i += StateCount)
  {
#ifndef _MSC_VER
    __asm volatile("# LLVM-MCA-BEGIN");
#endif

    // const uint32_t slot = state & (TotalSymbolCount - 1);
    const simd_t slot0 = _mm512_and_si512(statesX8[0], symCountMask);
    const simd_t slot1 = _mm512_and_si512(statesX8[1], symCountMask);
    const simd_t slot2 = _mm512_and_si512(statesX8[2], symCountMask);
    const simd_t slot3 = _mm512_and_si512(statesX8[3], symCountMask);

    // retrieve pack.
    const simd_t pack0 = _mm512_i32gather_epi32(slot0, reinterpret_cast<const int32_t *>(&histDec.symbol), sizeof(uint32_t));
    const simd_t pack1 = _mm512_i32gather_epi32(slot1, reinterpret_cast<const int32_t *>(&histDec.symbol), sizeof(uint32_t));
    const simd_t pack2 = _mm512_i32gather_epi32(slot2, reinterpret_cast<const int32_t *>(&histDec.symbol), sizeof(uint32_t));
    const simd_t pack3 = _mm512_i32gather_epi32(slot3, reinterpret_cast<const int32_t *>(&histDec.symbol), sizeof(uint32_t));

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

    // pack symbols to one si256. (could possibly be `_mm256_cvtepi32_epi8` on avx-512f + avx-512-vl) (`_mm256_slli_epi32` + `_mm256_or_si256` packing is slower)
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

    // read input for blocks 0, 1.
    const __m512i newWords01 = _mm512_loadu_si512(reinterpret_cast<const simd_t *>(pReadHead));

    // (state < DecodeConsumePoint16) ? -1 : 0 | well, actually (DecodeConsumePoint16 > state) ? -1 : 0
    const __mmask16 cmpMask0 = _mm512_cmpgt_epi32_mask(decodeConsumePoint, state0);
    const __mmask16 cmpMask1 = _mm512_cmpgt_epi32_mask(decodeConsumePoint, state1);
    const __mmask16 cmpMask2 = _mm512_cmpgt_epi32_mask(decodeConsumePoint, state2);
    const __mmask16 cmpMask3 = _mm512_cmpgt_epi32_mask(decodeConsumePoint, state3);

    // get masks of those compares & start loading shuffle masks.
    const uint32_t cmpMask0a = cmpMask0 & 0xFF;
    const uint32_t cmpMask0b = cmpMask0 >> 8;
    __m128i lut0a = _mm_lddqu_si128(reinterpret_cast<const __m128i *>(&_ShuffleLutPerm32[cmpMask0a << 3])); // `* 8`.
    __m128i lut0b = _mm_lddqu_si128(reinterpret_cast<const __m128i *>(&_ShuffleLutPerm32[cmpMask0b << 3])); // `* 8`.

    const uint32_t cmpMask1a = cmpMask1 & 0xFF;
    const uint32_t cmpMask1b = cmpMask1 >> 8;
    __m128i lut1a = _mm_lddqu_si128(reinterpret_cast<const __m128i *>(&_ShuffleLutPerm32[cmpMask1a << 3])); // `* 8`.
    __m128i lut1b = _mm_lddqu_si128(reinterpret_cast<const __m128i *>(&_ShuffleLutPerm32[cmpMask1b << 3])); // `* 8`.

    const uint32_t cmpMask2a = cmpMask2 & 0xFF;
    const uint32_t cmpMask2b = cmpMask2 >> 8;
    __m128i lut2a = _mm_lddqu_si128(reinterpret_cast<const __m128i *>(&_ShuffleLutPerm32[cmpMask2a << 3])); // `* 8`.
    __m128i lut2b = _mm_lddqu_si128(reinterpret_cast<const __m128i *>(&_ShuffleLutPerm32[cmpMask2b << 3])); // `* 8`.

    const uint32_t cmpMask3a = cmpMask3 & 0xFF;
    const uint32_t cmpMask3b = cmpMask3 >> 8;
    __m128i lut3a = _mm_lddqu_si128(reinterpret_cast<const __m128i *>(&_ShuffleLutPerm32[cmpMask3a << 3])); // `* 8`.
    __m128i lut3b = _mm_lddqu_si128(reinterpret_cast<const __m128i *>(&_ShuffleLutPerm32[cmpMask3b << 3])); // `* 8`.

    // advance read head.
    const uint8_t maskPop0a = (uint8_t)__builtin_popcount(cmpMask0a);
    const uint8_t advance0 = (uint8_t)__builtin_popcount(cmpMask0);

    const uint8_t maskPop1a = (uint8_t)__builtin_popcount(cmpMask1a);
    const uint8_t advance1 = (uint8_t)__builtin_popcount(cmpMask1);

    pReadHead += advance0 + advance1;

    // read input for blocks 2, 3.
    const __m512i newWords23 = _mm512_loadu_si512(reinterpret_cast<const simd_t *>(pReadHead));

    // advance read head.
    const uint8_t maskPop2a = (uint8_t)__builtin_popcount(cmpMask2a);
    const uint8_t advance2 = (uint8_t)__builtin_popcount(cmpMask2);

    const uint8_t maskPop3a = (uint8_t)__builtin_popcount(cmpMask3a);
    const uint8_t advance3 = (uint8_t)__builtin_popcount(cmpMask3);

    pReadHead += advance2 + advance3;

    const __m128i nonShuf0 = _mm_unpacklo_epi64(lut0a, _mm_add_epi8(lut0b, _mm_set1_epi8(maskPop0a)));
    const __m128i nonShuf1 = _mm_add_epi8(_mm_unpacklo_epi64(lut1a, _mm_add_epi8(lut1b, _mm_set1_epi8(maskPop1a))), _mm_set1_epi8(advance0));
    const __m128i nonShuf2 = _mm_unpacklo_epi64(lut2a, _mm_add_epi8(lut2b, _mm_set1_epi8(maskPop2a)));
    const __m128i nonShuf3 = _mm_add_epi8(_mm_unpacklo_epi64(lut3a, _mm_add_epi8(lut3b, _mm_set1_epi8(maskPop3a))), _mm_set1_epi8(advance2));

    const simd_t lut0 = _mm512_cvtepu8_epi32(nonShuf0);
    const simd_t lut1 = _mm512_cvtepu8_epi32(nonShuf1);
    const simd_t lut2 = _mm512_cvtepu8_epi32(nonShuf2);
    const simd_t lut3 = _mm512_cvtepu8_epi32(nonShuf3);

    const simd_t selectedNewWord0 = _mm512_permutexvar_epi16(lut0, newWords01);
    const simd_t selectedNewWord1 = _mm512_permutexvar_epi16(lut1, newWords01);
    const simd_t selectedNewWord2 = _mm512_permutexvar_epi16(lut2, newWords23);
    const simd_t selectedNewWord3 = _mm512_permutexvar_epi16(lut3, newWords23);

    const simd_t newWord0 = _mm512_mask_and_epi32(_mm512_setzero_si512(), cmpMask0, selectedNewWord0, lower16);
    const simd_t newWord1 = _mm512_mask_and_epi32(_mm512_setzero_si512(), cmpMask1, selectedNewWord1, lower16);
    const simd_t newWord2 = _mm512_mask_and_epi32(_mm512_setzero_si512(), cmpMask2, selectedNewWord2, lower16);
    const simd_t newWord3 = _mm512_mask_and_epi32(_mm512_setzero_si512(), cmpMask3, selectedNewWord3, lower16);

    // matching: state << 16
    const simd_t matchShiftedState0 = _mm512_mask_slli_epi32(state0, cmpMask0, state0, 16);
    const simd_t matchShiftedState1 = _mm512_mask_slli_epi32(state1, cmpMask1, state1, 16);
    const simd_t matchShiftedState2 = _mm512_mask_slli_epi32(state2, cmpMask2, state2, 16);
    const simd_t matchShiftedState3 = _mm512_mask_slli_epi32(state3, cmpMask3, state3, 16);

    // state = state << 16 | newWord;
    statesX8[0] = _mm512_or_si512(matchShiftedState0, newWord0);
    statesX8[1] = _mm512_or_si512(matchShiftedState1, newWord1);
    statesX8[2] = _mm512_or_si512(matchShiftedState2, newWord2);
    statesX8[3] = _mm512_or_si512(matchShiftedState3, newWord3);

#ifndef _MSC_VER
    __asm volatile("# LLVM-MCA-END");
#endif
  }

  for (size_t j = 0; j < sizeof(statesX8) / sizeof(simd_t); j++)
    _mm512_storeu_si512(reinterpret_cast<simd_t *>(reinterpret_cast<uint8_t *>(states) + j * sizeof(simd_t)), statesX8[j]);

  const uint8_t idx2idx[] =
  {
    0x00, 0x01, 0x02, 0x03, 0x10, 0x11, 0x12, 0x13, 0x04, 0x05, 0x06, 0x07, 0x14, 0x15, 0x16, 0x17,
    0x08, 0x09, 0x0A, 0x0B, 0x18, 0x19, 0x1A, 0x1B, 0x0C, 0x0D, 0x0E, 0x0F, 0x1C, 0x1D, 0x1E, 0x1F,
    0x20, 0x21, 0x22, 0x23, 0x30, 0x31, 0x32, 0x33, 0x24, 0x25, 0x26, 0x27, 0x34, 0x35, 0x36, 0x37,
    0x28, 0x29, 0x2A, 0x2B, 0x38, 0x39, 0x3A, 0x3B, 0x2C, 0x2D, 0x2E, 0x2F, 0x3C, 0x3D, 0x3E, 0x3F,
  };

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

size_t rANS32x64_16w_decode_avx512_varC_12(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity) { return rANS32x64_16w_decode_avx512fdqbw_varC<12>(pInData, inLength, pOutData, outCapacity); }
size_t rANS32x64_16w_decode_avx512_varC_11(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity) { return rANS32x64_16w_decode_avx512fdqbw_varC<11>(pInData, inLength, pOutData, outCapacity); }
size_t rANS32x64_16w_decode_avx512_varC_10(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity) { return rANS32x64_16w_decode_avx512fdqbw_varC<10>(pInData, inLength, pOutData, outCapacity); }

//////////////////////////////////////////////////////////////////////////

size_t rANS32x64_16w_encode_scalar_15(const uint8_t *pInData, const size_t length, uint8_t *pOutData, const size_t outCapacity, const hist_t *pHist) { return rANS32x64_16w_encode_scalar<15>(pInData, length, pOutData, outCapacity, pHist); }
size_t rANS32x64_16w_decode_scalar_15(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity) { return rANS32x64_16w_decode_scalar<15>(pInData, inLength, pOutData, outCapacity); }
size_t rANS32x64_xmmShfl_16w_decode_avx2_varA_15(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity) { return rANS32x64_16w_decode_avx2_varA<15, true>(pInData, inLength, pOutData, outCapacity); }
size_t rANS32x64_xmmShfl_16w_decode_avx2_varB_15(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity) { return rANS32x64_16w_decode_avx2_varB<15, true>(pInData, inLength, pOutData, outCapacity); }

size_t rANS32x64_16w_encode_scalar_14(const uint8_t *pInData, const size_t length, uint8_t *pOutData, const size_t outCapacity, const hist_t *pHist) { return rANS32x64_16w_encode_scalar<14>(pInData, length, pOutData, outCapacity, pHist); }
size_t rANS32x64_16w_decode_scalar_14(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity) { return rANS32x64_16w_decode_scalar<14>(pInData, inLength, pOutData, outCapacity); }
size_t rANS32x64_xmmShfl_16w_decode_avx2_varA_14(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity) { return rANS32x64_16w_decode_avx2_varA<14, true>(pInData, inLength, pOutData, outCapacity); }
size_t rANS32x64_xmmShfl_16w_decode_avx2_varB_14(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity) { return rANS32x64_16w_decode_avx2_varB<14, true>(pInData, inLength, pOutData, outCapacity); }

size_t rANS32x64_16w_encode_scalar_13(const uint8_t *pInData, const size_t length, uint8_t *pOutData, const size_t outCapacity, const hist_t *pHist) { return rANS32x64_16w_encode_scalar<13>(pInData, length, pOutData, outCapacity, pHist); }
size_t rANS32x64_16w_decode_scalar_13(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity) { return rANS32x64_16w_decode_scalar<13>(pInData, inLength, pOutData, outCapacity); }
size_t rANS32x64_xmmShfl_16w_decode_avx2_varA_13(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity) { return rANS32x64_16w_decode_avx2_varA<13, true>(pInData, inLength, pOutData, outCapacity); }
size_t rANS32x64_xmmShfl_16w_decode_avx2_varB_13(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity) { return rANS32x64_16w_decode_avx2_varB<13, true>(pInData, inLength, pOutData, outCapacity); }

size_t rANS32x64_16w_encode_scalar_12(const uint8_t *pInData, const size_t length, uint8_t *pOutData, const size_t outCapacity, const hist_t *pHist) { return rANS32x64_16w_encode_scalar<12>(pInData, length, pOutData, outCapacity, pHist); }
size_t rANS32x64_16w_decode_scalar_12(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity) { return rANS32x64_16w_decode_scalar<12>(pInData, inLength, pOutData, outCapacity); }
size_t rANS32x64_xmmShfl_16w_decode_avx2_varA_12(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity) { return rANS32x64_16w_decode_avx2_varA<12, true>(pInData, inLength, pOutData, outCapacity); }
size_t rANS32x64_xmmShfl_16w_decode_avx2_varB_12(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity) { return rANS32x64_16w_decode_avx2_varB<12, true>(pInData, inLength, pOutData, outCapacity); }
size_t rANS32x64_xmmShfl_16w_decode_avx2_varC_12(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity) { return rANS32x64_16w_decode_avx2_varC<12, true>(pInData, inLength, pOutData, outCapacity); }

size_t rANS32x64_16w_encode_scalar_11(const uint8_t *pInData, const size_t length, uint8_t *pOutData, const size_t outCapacity, const hist_t *pHist) { return rANS32x64_16w_encode_scalar<11>(pInData, length, pOutData, outCapacity, pHist); }
size_t rANS32x64_16w_decode_scalar_11(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity) { return rANS32x64_16w_decode_scalar<11>(pInData, inLength, pOutData, outCapacity); }
size_t rANS32x64_xmmShfl_16w_decode_avx2_varA_11(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity) { return rANS32x64_16w_decode_avx2_varA<11, true>(pInData, inLength, pOutData, outCapacity); }
size_t rANS32x64_xmmShfl_16w_decode_avx2_varB_11(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity) { return rANS32x64_16w_decode_avx2_varB<11, true>(pInData, inLength, pOutData, outCapacity); }
size_t rANS32x64_xmmShfl_16w_decode_avx2_varC_11(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity) { return rANS32x64_16w_decode_avx2_varC<11, true>(pInData, inLength, pOutData, outCapacity); }

size_t rANS32x64_16w_encode_scalar_10(const uint8_t *pInData, const size_t length, uint8_t *pOutData, const size_t outCapacity, const hist_t *pHist) { return rANS32x64_16w_encode_scalar<10>(pInData, length, pOutData, outCapacity, pHist); }
size_t rANS32x64_16w_decode_scalar_10(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity) { return rANS32x64_16w_decode_scalar<10>(pInData, inLength, pOutData, outCapacity); }
size_t rANS32x64_xmmShfl_16w_decode_avx2_varA_10(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity) { return rANS32x64_16w_decode_avx2_varA<10, true>(pInData, inLength, pOutData, outCapacity); }
size_t rANS32x64_xmmShfl_16w_decode_avx2_varB_10(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity) { return rANS32x64_16w_decode_avx2_varB<10, true>(pInData, inLength, pOutData, outCapacity); }
size_t rANS32x64_xmmShfl_16w_decode_avx2_varC_10(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity) { return rANS32x64_16w_decode_avx2_varC<10, true>(pInData, inLength, pOutData, outCapacity); }

//////////////////////////////////////////////////////////////////////////

size_t rANS32x64_ymmPerm_16w_decode_avx2_varA_15(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity) { return rANS32x64_16w_decode_avx2_varA<15, false>(pInData, inLength, pOutData, outCapacity); }
size_t rANS32x64_ymmPerm_16w_decode_avx2_varB_15(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity) { return rANS32x64_16w_decode_avx2_varB<15, false>(pInData, inLength, pOutData, outCapacity); }

size_t rANS32x64_ymmPerm_16w_decode_avx2_varA_14(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity) { return rANS32x64_16w_decode_avx2_varA<14, false>(pInData, inLength, pOutData, outCapacity); }
size_t rANS32x64_ymmPerm_16w_decode_avx2_varB_14(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity) { return rANS32x64_16w_decode_avx2_varB<14, false>(pInData, inLength, pOutData, outCapacity); }

size_t rANS32x64_ymmPerm_16w_decode_avx2_varA_13(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity) { return rANS32x64_16w_decode_avx2_varA<13, false>(pInData, inLength, pOutData, outCapacity); }
size_t rANS32x64_ymmPerm_16w_decode_avx2_varB_13(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity) { return rANS32x64_16w_decode_avx2_varB<13, false>(pInData, inLength, pOutData, outCapacity); }

size_t rANS32x64_ymmPerm_16w_decode_avx2_varA_12(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity) { return rANS32x64_16w_decode_avx2_varA<12, false>(pInData, inLength, pOutData, outCapacity); }
size_t rANS32x64_ymmPerm_16w_decode_avx2_varB_12(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity) { return rANS32x64_16w_decode_avx2_varB<12, false>(pInData, inLength, pOutData, outCapacity); }
size_t rANS32x64_ymmPerm_16w_decode_avx2_varC_12(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity) { return rANS32x64_16w_decode_avx2_varC<12, false>(pInData, inLength, pOutData, outCapacity); }

size_t rANS32x64_ymmPerm_16w_decode_avx2_varA_11(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity) { return rANS32x64_16w_decode_avx2_varA<11, false>(pInData, inLength, pOutData, outCapacity); }
size_t rANS32x64_ymmPerm_16w_decode_avx2_varB_11(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity) { return rANS32x64_16w_decode_avx2_varB<11, false>(pInData, inLength, pOutData, outCapacity); }
size_t rANS32x64_ymmPerm_16w_decode_avx2_varC_11(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity) { return rANS32x64_16w_decode_avx2_varC<11, false>(pInData, inLength, pOutData, outCapacity); }

size_t rANS32x64_ymmPerm_16w_decode_avx2_varA_10(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity) { return rANS32x64_16w_decode_avx2_varA<10, false>(pInData, inLength, pOutData, outCapacity); }
size_t rANS32x64_ymmPerm_16w_decode_avx2_varB_10(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity) { return rANS32x64_16w_decode_avx2_varB<10, false>(pInData, inLength, pOutData, outCapacity); }
size_t rANS32x64_ymmPerm_16w_decode_avx2_varC_10(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity) { return rANS32x64_16w_decode_avx2_varC<10, false>(pInData, inLength, pOutData, outCapacity); }
