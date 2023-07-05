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

//////////////////////////////////////////////////////////////////////////

template <uint32_t TotalSymbolCountBits>
static bool _init_from_hist(hist_dec_t<TotalSymbolCountBits> *pDecHist, hist_t *pIncompleteHist, const uint32_t totalSymbolCountBits)
{
  (void)totalSymbolCountBits;

  memcpy(&pDecHist->symbolCount, &pIncompleteHist->symbolCount, sizeof(pIncompleteHist->symbolCount));

  return inplace_make_hist_dec(pDecHist);
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

size_t block_rANS32x32_16w_decode_15(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity) { return block_rANS32x32_16w_decode<15, r32x32_dt_scalar, hist_dec_t<15>>(pInData, inLength, pOutData, outCapacity); }
size_t block_rANS32x32_16w_decode_14(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity) { return block_rANS32x32_16w_decode<14, r32x32_dt_scalar, hist_dec_t<14>>(pInData, inLength, pOutData, outCapacity); }
size_t block_rANS32x32_16w_decode_13(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity) { return block_rANS32x32_16w_decode<13, r32x32_dt_scalar, hist_dec_t<13>>(pInData, inLength, pOutData, outCapacity); }
size_t block_rANS32x32_16w_decode_12(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity) { return block_rANS32x32_16w_decode<12, r32x32_dt_scalar, hist_dec_t<12>>(pInData, inLength, pOutData, outCapacity); }
size_t block_rANS32x32_16w_decode_11(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity) { return block_rANS32x32_16w_decode<11, r32x32_dt_scalar, hist_dec_t<11>>(pInData, inLength, pOutData, outCapacity); }
size_t block_rANS32x32_16w_decode_10(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity) { return block_rANS32x32_16w_decode<10, r32x32_dt_scalar, hist_dec_t<10>>(pInData, inLength, pOutData, outCapacity); }
