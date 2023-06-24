#include "hist.h"
#include "simd_platform.h"

#include <string.h>

//////////////////////////////////////////////////////////////////////////

size_t rANS32x1_capacity(const size_t inputSize)
{
  return inputSize + sizeof(uint16_t) * 256 + sizeof(uint32_t); // buffer + histogram + state
}

//////////////////////////////////////////////////////////////////////////
//
//inline uint32_t encode_symbol_scalar(const uint8_t symbol, const hist_t *pHist, const uint32_t state)
//{
//  const uint32_t symbolCount = pHist->symbolCount[symbol];
//
//  return ((state / symbolCount) << TotalSymbolCountBits) + pHist->cumul[symbol] + (state % symbolCount);
//}
//
//inline uint8_t decode_symbol_scalar(uint32_t *pState, const hist_dec_t *pHist)
//{
//  const uint32_t state = *pState;
//  const uint32_t slot = state & (TotalSymbolCount - 1);
//  const uint8_t symbol = pHist->cumulInv[slot];
//  const uint32_t previousState = (state >> TotalSymbolCountBits) * (uint32_t)pHist->symbolCount[symbol] + slot - (uint32_t)pHist->cumul[symbol];
//
//  *pState = previousState;
//
//  return symbol;
//}
//
////////////////////////////////////////////////////////////////////////////
//
//size_t rANS32x1_encode(const uint8_t *pInData, const size_t length, uint8_t *pOutData, const size_t outCapacity, const hist_enc_t *pHist)
//{
//  if (outCapacity < rANS32x1_capacity(length))
//    return 0;
//
//  uint32_t state = DecodeConsumePoint; // technically `n * TotalSymbolCount`.
//  uint8_t *pWrite = pOutData + outCapacity - 1;
//
//  for (int64_t i = length - 1; i >= 0; i--)
//  {
//    const uint8_t in = pInData[i];
//    const enc_sym_t pack = pHist->symbols[in];
//
//    const uint32_t freq = pack & 0xFFFF;
//    const uint32_t cumul = pack >> 16;
//    const uint32_t max = EncodeEmitPoint * freq;
//
//    while (state >= max)
//    {
//      *pWrite = (uint8_t)(state & 0xFF);
//      pWrite--;
//      state >>= 8;
//    }
//
//#ifndef _MSC_VER
//    const uint32_t stateDivFreq = state / freq;
//    const uint32_t stateModFreq = state % freq;
//#else
//    uint32_t stateDivFreq, stateModFreq;
//    stateDivFreq = _udiv64(state, freq, &stateModFreq);
//#endif
//
//    state = (stateDivFreq << TotalSymbolCountBits) + cumul + stateModFreq;
//  }
//
//  *reinterpret_cast<uint32_t *>(pOutData) = state;
//
//  const size_t writtenSize = (pOutData + outCapacity - 1) - pWrite;
//  memmove(pOutData + sizeof(uint32_t), pWrite + 1, writtenSize);
//
//  return writtenSize + sizeof(uint32_t);
//}
//
//size_t rANS32x1_decode(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outLength, const hist_dec2_t *pHist)
//{
//  if (inLength == 0)
//    return 0;
//
//  uint32_t state = *reinterpret_cast<const uint32_t *>(pInData);
//  size_t inIndex = sizeof(uint32_t);
//
//  for (size_t i = 0; i < outLength; i++)
//  {
//    const uint32_t slot = state & (TotalSymbolCount - 1);
//    const uint8_t symbol = pHist->cumulInv[slot];
//
//    const dec_sym_t pack = pHist->symbols[symbol];
//    const uint32_t freq = pack.freq;
//    const uint32_t cumul = pack.cumul;
//
//    state = (state >> TotalSymbolCountBits) * freq + slot - cumul;
//
//    pOutData[i] = symbol;
//
//    while (state < DecodeConsumePoint)
//      state = state << 8 | pInData[inIndex++];
//  }
//
//  return outLength;
//}
//
////////////////////////////////////////////////////////////////////////////
//
//size_t rANS32x1_encode_scalar(const uint8_t *pInData, const size_t length, uint8_t *pOutData, const size_t outCapacity, const hist_t *pHist)
//{
//  if (outCapacity < rANS32x1_capacity(length))
//    return 0;
//
//  uint32_t state = DecodeConsumePoint; // technically `n * TotalSymbolCount`.
//  uint8_t *pWrite = pOutData + outCapacity - 1;
//
//  for (int64_t i = length - 1; i >= 0; i--)
//  {
//    const uint8_t in = pInData[i];
//    const uint32_t symbolCount = pHist->symbolCount[in];
//    const uint32_t max = EncodeEmitPoint * symbolCount;
//
//    while (state >= max)
//    {
//      *pWrite = (uint8_t)(state & 0xFF);
//      pWrite--;
//      state >>= 8;
//    }
//
//    state = ((state / symbolCount) << TotalSymbolCountBits) + (uint32_t)pHist->cumul[in] + (state % symbolCount);
//  }
//
//  *reinterpret_cast<uint32_t *>(pOutData) = state;
//
//  const size_t writtenSize = (pOutData + outCapacity - 1) - pWrite;
//  memmove(pOutData + sizeof(uint32_t), pWrite + 1, writtenSize);
//
//  return writtenSize + sizeof(uint32_t);
//}
//
//size_t rANS32x1_decode_scalar(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outLength, const hist_dec_t *pHist)
//{
//  if (inLength == 0)
//    return 0;
//
//  uint32_t state = *reinterpret_cast<const uint32_t *>(pInData);
//  size_t inIndex = sizeof(uint32_t);
//
//  for (size_t i = 0; i < outLength; i++)
//  {
//    pOutData[i] = decode_symbol_scalar(&state, pHist);
//
//    while (state < DecodeConsumePoint)
//      state = state << 8 | pInData[inIndex++];
//  }
//
//  return outLength;
//}
