#include "hist.h"
#include "simd_platform.h"

#include <string.h>

constexpr size_t StateCount = 32;

size_t rANS32x32_capacity(const size_t inputSize)
{
  return inputSize + sizeof(uint16_t) * 256 + sizeof(uint32_t) * StateCount; // buffer + histogram + state
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

size_t rANS32x32_decode_avx2_basic(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outLength, const hist_dec2_t *pHist)
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
  }

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
