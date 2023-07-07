#include "mt_rANS32x32_16w.h"

#include "hist.h"
#include "simd_platform.h"
#include "block_codec32.h"

#include <string.h>
#include <math.h>

//////////////////////////////////////////////////////////////////////////

template <uint32_t TotalSymbolCountBits, rans32x32_decoder_type_t Impl, typename hist_type>
size_t mt_rANS32x32_16w_decode(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity)
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

  _rans_decode_state32_t<hist_type> decodeState;

  decodeState.pReadHead = reinterpret_cast<const uint16_t *>(pInData + inputIndex);
  const size_t outLengthInStates = expectedOutputLength - StateCount + 1;
  size_t i = 0;
  hist_t hist = {};

  do
  {
    const uint64_t blockSizeVal = *reinterpret_cast<const uint64_t *>(decodeState.pReadHead);
    decodeState.pReadHead += sizeof(uint64_t) / sizeof(uint16_t);

    if (blockSizeVal & ((uint64_t)1 << 63)) // Single symbol hist
    {
      const uint8_t symbol = (blockSizeVal >> 54) & 0xFF;
      const uint64_t blockSize = blockSizeVal & (((uint64_t)1 << 54) - 1);

      memset(pOutData + i, symbol, blockSize);

      i += blockSize;
    }
    else
    {
      const uint64_t readHeadBackOffset = *reinterpret_cast<const uint64_t *>(decodeState.pReadHead);
      decodeState.pReadHead += sizeof(uint64_t) / sizeof(uint16_t);
      const uint16_t *pReadHeadAfter = decodeState.pReadHead + readHeadBackOffset + 1;
      (void)readHeadBackOffset; // unused in single-threaded version.

      for (size_t j = 0; j < StateCount; j++)
      {
        decodeState.states[j] = *reinterpret_cast<const uint32_t *>(decodeState.pReadHead);
        decodeState.pReadHead += (sizeof(uint32_t) / sizeof(uint16_t));
      }

      for (size_t j = 0; j < 256; j++)
      {
        hist.symbolCount[j] = *decodeState.pReadHead;
        decodeState.pReadHead++;
      }

      if (!_init_from_hist(&decodeState.hist, &hist, TotalSymbolCountBits))
        return 0;

      uint64_t blockEndInStates = (i + blockSizeVal);

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

      decodeState.pReadHead = pReadHeadAfter;
    }

  } while (i < outLengthInStates);

  if (i < expectedOutputLength)
  {
    hist_dec_t<TotalSymbolCountBits> histDec;
    memcpy(&histDec.symbolCount, &hist.symbolCount, sizeof(hist.symbolCount)); // this is unreachable with single symbol hist at the end.

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

        if (state < DecodeConsumePoint16)
        {
          state = state << 16 | *decodeState.pReadHead;
          decodeState.pReadHead++;
        }

        decodeState.states[j] = state;
      }
    }
  }

  return expectedOutputLength;
}

//////////////////////////////////////////////////////////////////////////

template <uint32_t TotalSymbolCountBits, rans32x32_decoder_type_t Impl, typename hist_type>
size_t mt_rANS32x32_16w_decode_mt(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity, thread_pool *pThreadPool)
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

  _rans_decode_state32_t<hist_type> decodeState;

  decodeState.pReadHead = reinterpret_cast<const uint16_t *>(pInData + inputIndex);
  const size_t outLengthInStates = expectedOutputLength - StateCount + 1;
  size_t i = 0;
  hist_t hist = {};

  do
  {
    const uint64_t blockSizeVal = *reinterpret_cast<const uint64_t *>(decodeState.pReadHead);
    decodeState.pReadHead += sizeof(uint64_t) / sizeof(uint16_t);

    if (blockSizeVal & ((uint64_t)1 << 63)) // Single symbol hist
    {
      const uint8_t symbol = (blockSizeVal >> 54) & 0xFF;
      const uint64_t blockSize = blockSizeVal & (((uint64_t)1 << 54) - 1);

      memset(pOutData + i, symbol, blockSize); // let's hope this isn't the last block, because otherwise we'd delay starting tasks for other stuff, but otherwise `memset` is probably faster than starting a task.

      i += blockSize;
    }
    else
    {
      const uint64_t readHeadBackOffset = *reinterpret_cast<const uint64_t *>(decodeState.pReadHead);
      decodeState.pReadHead += sizeof(uint64_t) / sizeof(uint16_t);

      const uint16_t *pReadHeadAfter = decodeState.pReadHead + readHeadBackOffset + 1;

      for (size_t j = 0; j < StateCount; j++)
      {
        decodeState.states[j] = *reinterpret_cast<const uint32_t *>(decodeState.pReadHead);
        decodeState.pReadHead += (sizeof(uint32_t) / sizeof(uint16_t));
      }

      for (size_t j = 0; j < 256; j++)
      {
        hist.symbolCount[j] = *decodeState.pReadHead;
        decodeState.pReadHead++;
      }

      if (!_init_from_hist(&decodeState.hist, &hist, TotalSymbolCountBits))
        return 0;

      uint64_t blockEndInStates = (i + blockSizeVal);

      if (blockEndInStates > outLengthInStates)
        blockEndInStates = outLengthInStates;
      else if ((blockEndInStates & (StateCount - 1)) != 0)
        return 0;

      if (i + blockSizeVal > blockEndInStates)
      {
        i = rans32x32_16w_decoder<Impl, TotalSymbolCountBits, hist_type>::decode_section(&decodeState, pOutData, i, blockEndInStates);

        break;
      }
      else
      {
        thread_pool_add(pThreadPool, [=]() {
          auto decState = decodeState;
          rans32x32_16w_decoder<Impl, TotalSymbolCountBits, hist_type>::decode_section(&decState, pOutData, i, blockEndInStates);
          });

        i = blockEndInStates;
        decodeState.pReadHead = pReadHeadAfter;
      }
    }

  } while (i < outLengthInStates);

  if (i < expectedOutputLength)
  {
    hist_dec_t<TotalSymbolCountBits> histDec;
    memcpy(&histDec.symbolCount, &hist.symbolCount, sizeof(hist.symbolCount)); // this is unreachable with single symbol hist at the end.

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

        if (state < DecodeConsumePoint16)
        {
          state = state << 16 | *decodeState.pReadHead;
          decodeState.pReadHead++;
        }

        decodeState.states[j] = state;
      }
    }
  }

  thread_pool_await(pThreadPool);

  return expectedOutputLength;
}

//////////////////////////////////////////////////////////////////////////

template <uint32_t TotalSymbolCountBits>
static size_t mt_rANS32x32_decode_wrapper(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity, thread_pool *pThreadPool = nullptr)
{
  _DetectCPUFeatures();

  if (avx2Supported)
  {
    if constexpr (TotalSymbolCountBits >= 13)
    {
      if (pThreadPool)
        return mt_rANS32x32_16w_decode_mt<TotalSymbolCountBits, r32x32_dt_avx2_large_cache_15_to_13, hist_dec2_t<TotalSymbolCountBits>>(pInData, inLength, pOutData, outCapacity, pThreadPool);
      else
        return mt_rANS32x32_16w_decode<TotalSymbolCountBits, r32x32_dt_avx2_large_cache_15_to_13, hist_dec2_t<TotalSymbolCountBits>>(pInData, inLength, pOutData, outCapacity);
    }
    else
    {
      if (pThreadPool)
        return mt_rANS32x32_16w_decode_mt<TotalSymbolCountBits, r32x32_dt_avx2_large_cache_12_to_10, hist_dec_pack_t<TotalSymbolCountBits>>(pInData, inLength, pOutData, outCapacity, pThreadPool);
      else
        return mt_rANS32x32_16w_decode<TotalSymbolCountBits, r32x32_dt_avx2_large_cache_12_to_10, hist_dec_pack_t<TotalSymbolCountBits>>(pInData, inLength, pOutData, outCapacity);
    }
  }

  // Fallback.
  if (pThreadPool)
    return mt_rANS32x32_16w_decode_mt<TotalSymbolCountBits, r32x32_dt_scalar, hist_dec_t<TotalSymbolCountBits>>(pInData, inLength, pOutData, outCapacity, pThreadPool);
  else
    return mt_rANS32x32_16w_decode<TotalSymbolCountBits, r32x32_dt_scalar, hist_dec_t<TotalSymbolCountBits>>(pInData, inLength, pOutData, outCapacity);
}

//////////////////////////////////////////////////////////////////////////

size_t mt_rANS32x32_16w_decode_15(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity)
{
  return mt_rANS32x32_decode_wrapper<15>(pInData, inLength, pOutData, outCapacity);
}

size_t mt_rANS32x32_16w_decode_14(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity)
{
  return mt_rANS32x32_decode_wrapper<14>(pInData, inLength, pOutData, outCapacity);
}

size_t mt_rANS32x32_16w_decode_13(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity)
{
  return mt_rANS32x32_decode_wrapper<13>(pInData, inLength, pOutData, outCapacity);
}

size_t mt_rANS32x32_16w_decode_12(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity)
{
  return mt_rANS32x32_decode_wrapper<12>(pInData, inLength, pOutData, outCapacity);
}

size_t mt_rANS32x32_16w_decode_11(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity)
{
  return mt_rANS32x32_decode_wrapper<11>(pInData, inLength, pOutData, outCapacity);
}

size_t mt_rANS32x32_16w_decode_10(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity)
{
  return mt_rANS32x32_decode_wrapper<10>(pInData, inLength, pOutData, outCapacity);
}

//////////////////////////////////////////////////////////////////////////

size_t mt_rANS32x32_16w_decode_mt_15(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity, thread_pool *pThreadPool)
{
  return mt_rANS32x32_decode_wrapper<15>(pInData, inLength, pOutData, outCapacity, pThreadPool);
}

size_t mt_rANS32x32_16w_decode_mt_14(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity, thread_pool *pThreadPool)
{
  return mt_rANS32x32_decode_wrapper<14>(pInData, inLength, pOutData, outCapacity, pThreadPool);
}

size_t mt_rANS32x32_16w_decode_mt_13(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity, thread_pool *pThreadPool)
{
  return mt_rANS32x32_decode_wrapper<13>(pInData, inLength, pOutData, outCapacity, pThreadPool);
}

size_t mt_rANS32x32_16w_decode_mt_12(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity, thread_pool *pThreadPool)
{
  return mt_rANS32x32_decode_wrapper<12>(pInData, inLength, pOutData, outCapacity, pThreadPool);
}

size_t mt_rANS32x32_16w_decode_mt_11(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity, thread_pool *pThreadPool)
{
  return mt_rANS32x32_decode_wrapper<11>(pInData, inLength, pOutData, outCapacity, pThreadPool);
}

size_t mt_rANS32x32_16w_decode_mt_10(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity, thread_pool *pThreadPool)
{
  return mt_rANS32x32_decode_wrapper<10>(pInData, inLength, pOutData, outCapacity, pThreadPool);
}
