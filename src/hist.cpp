#include "hist.h"

#include <string.h>

//////////////////////////////////////////////////////////////////////////

void make_hist(hist_t *pHist, const uint8_t *pData, const size_t size, const size_t totalSymbolCountBits)
{
  uint32_t hist[256];
  memset(hist, 0, sizeof(hist));

  const uint32_t totalSymbolCount = ((uint32_t)1 << totalSymbolCountBits);

  for (size_t i = 0; i < size; i++)
    hist[pData[i]]++;

  uint32_t counter = 0;

  for (size_t i = 0; i < 256; i++)
    counter += hist[i];

  uint16_t capped[256];
  size_t cappedSum = 0;

  constexpr bool FloatingPointHistLimit = true;

  if constexpr (FloatingPointHistLimit)
  {
    const float mul = (float)totalSymbolCount / counter;

    for (size_t i = 0; i < 256; i++)
    {
      capped[i] = (uint16_t)(hist[i] * mul + 0.5f);

      if (capped[i] == 0 && hist[i])
        capped[i] = 1;

      cappedSum += capped[i];
    }
  }
  else
  {
    const uint32_t div = counter / totalSymbolCount;

    if (div)
    {
      const uint32_t add = div / 2;

      for (size_t i = 0; i < 256; i++)
      {
        capped[i] = (uint16_t)((hist[i] + add) / div);

        if (capped[i] == 0 && hist[i])
          capped[i] = 1;

        cappedSum += capped[i];
      }
    }
    else
    {
      const uint32_t mul = totalSymbolCount / counter;

      for (size_t i = 0; i < 256; i++)
      {
        capped[i] = (uint16_t)(hist[i] * mul);
        cappedSum += capped[i];
      }
    }
  }

  if (cappedSum != totalSymbolCount)
  {
    while (cappedSum > totalSymbolCount) // Start stealing.
    {
      size_t target = 2;

      while (true)
      {
        size_t found = totalSymbolCount;

        for (size_t i = 0; i < 256; i++)
          if (capped[i] > target && capped[i] < found)
            found = capped[i];

        if (found == totalSymbolCount)
          break;

        for (size_t i = 0; i < 256; i++)
        {
          if (capped[i] == found)
          {
            capped[i]--;
            cappedSum--;

            if (cappedSum == totalSymbolCount)
              goto hist_ready;
          }
        }

        target = found + 1;
      }
    }

    while (cappedSum < totalSymbolCount) // Start a charity.
    {
      size_t target = totalSymbolCount;

      while (true)
      {
        size_t found = 1;

        for (size_t i = 0; i < 256; i++)
          if (capped[i] < target && capped[i] > found)
            found = capped[i];

        if (found == 1)
          break;

        for (size_t i = 0; i < 256; i++)
        {
          if (capped[i] == found)
          {
            capped[i]++;
            cappedSum++;

            if (cappedSum == totalSymbolCount)
              goto hist_ready;
          }
        }

        target = found - 1;
      }
    }
  }

hist_ready:
  counter = 0;

  for (size_t i = 0; i < 256; i++)
  {
    pHist->cumul[i] = (uint16_t)counter;
    pHist->symbolCount[i] = capped[i];
    counter += capped[i];
  }
}

void make_enc_hist(hist_enc_t *pHistEnc, const hist_t *pHist)
{
  for (size_t i = 0; i < 256; i++)
    pHistEnc->symbols[i] = pHist->cumul[i] << 16 | pHist->symbolCount[i];
}

template <uint32_t TotalSymbolCountBits>
void make_dec_hist(hist_dec_t<TotalSymbolCountBits> *pHistDec, const hist_t *pHist)
{
  static_assert(TotalSymbolCountBits < 16);
  constexpr uint32_t TotalSymbolCount = ((uint32_t)1 << TotalSymbolCountBits);

  memcpy(pHistDec, pHist, sizeof(hist_t));

  uint8_t sym = 0;

  for (size_t i = 0; i < TotalSymbolCount; i++)
  {
    while (sym != 0xFF && (!pHist->symbolCount[sym] || pHist->cumul[sym + 1] <= i))
      sym++;

    pHistDec->cumulInv[i] = sym;
  }
}

template <uint32_t TotalSymbolCountBits>
void make_dec2_hist(hist_dec2_t<TotalSymbolCountBits> *pHistDec, const hist_t *pHist)
{
  static_assert(TotalSymbolCountBits < 16);
  constexpr uint32_t TotalSymbolCount = ((uint32_t)1 << TotalSymbolCountBits);

  for (size_t i = 0; i < 256; i++)
  {
    pHistDec->symbols[i].cumul = pHist->cumul[i];
    pHistDec->symbols[i].freq = pHist->symbolCount[i];
  }

  uint8_t sym = 0;

  for (size_t i = 0; i < TotalSymbolCount; i++)
  {
    while (sym != 0xFF && (!pHist->symbolCount[sym] || pHist->cumul[sym + 1] <= i))
      sym++;

    pHistDec->cumulInv[i] = sym;
  }
}

template <uint32_t TotalSymbolCountBits>
void make_dec3_hist(hist_dec3_t<TotalSymbolCountBits> *pHistDec, const hist_t *pHist)
{
  static_assert(TotalSymbolCountBits < 16);
  constexpr uint32_t TotalSymbolCount = ((uint32_t)1 << TotalSymbolCountBits);

  uint8_t sym = 0;

  for (size_t i = 0; i < TotalSymbolCount; i++)
  {
    while (sym != 0xFF && (!pHist->symbolCount[sym] || pHist->cumul[sym + 1] <= i))
      sym++;

    pHistDec->cumulInv[i] = sym;
    pHistDec->symbolFomCumul[i].cumul = pHist->cumul[sym];
    pHistDec->symbolFomCumul[i].freq = pHist->symbolCount[sym];
  }
}

template <uint32_t TotalSymbolCountBits>
void make_dec_pack_hist(hist_dec_pack_t<TotalSymbolCountBits> *pHistDec, const hist_t *pHist)
{
  static_assert(TotalSymbolCountBits <= 12);
  constexpr uint32_t TotalSymbolCount = ((uint32_t)1 << TotalSymbolCountBits);

  uint8_t sym = 0;

  for (size_t i = 0; i < TotalSymbolCount; i++)
  {
    while (sym != 0xFF && (!pHist->symbolCount[sym] || pHist->cumul[sym + 1] <= i))
      sym++;

    pHistDec->symbol[i] = sym | ((uint32_t)pHist->cumul[sym] << 8) | ((uint32_t)pHist->symbolCount[sym] << 20);
  }
}

bool inplace_complete_hist(hist_t *pHist, const size_t totalSymbolCountBits)
{
  uint16_t counter = 0;

  for (size_t i = 0; i < 256; i++)
  {
    pHist->cumul[i] = (uint16_t)counter;
    counter += pHist->symbolCount[i];
  }

  return (counter == 1 << totalSymbolCountBits);
}

template <uint32_t TotalSymbolCountBits>
bool inplace_make_hist_dec(hist_dec_t<TotalSymbolCountBits> *pHist)
{
  static_assert(TotalSymbolCountBits < 16);
  constexpr uint32_t TotalSymbolCount = ((uint32_t)1 << TotalSymbolCountBits);

  uint16_t counter = 0;

  for (size_t i = 0; i < 256; i++)
  {
    pHist->cumul[i] = (uint16_t)counter;
    counter += pHist->symbolCount[i];
  }

  if (counter != TotalSymbolCount)
    return false;

  uint8_t sym = 0;

  for (size_t i = 0; i < TotalSymbolCount; i++)
  {
    while (sym != 0xFF && (!pHist->symbolCount[sym] || pHist->cumul[sym + 1] <= i))
      sym++;

    pHist->cumulInv[i] = sym;
  }

  return true;
}

template <uint32_t TotalSymbolCountBits>
bool inplace_make_hist_dec2(hist_dec2_t<TotalSymbolCountBits> *pHist)
{
  static_assert(TotalSymbolCountBits < 16);
  constexpr uint32_t TotalSymbolCount = ((uint32_t)1 << TotalSymbolCountBits);

  uint16_t counter = 0;

  for (size_t i = 0; i < 256; i++)
  {
    pHist->symbols[i].cumul = (uint16_t)counter;
    counter += pHist->symbols[i].freq;
  }

  if (counter != TotalSymbolCount)
    return false;

  uint8_t sym = 0;

  for (size_t i = 0; i < TotalSymbolCount; i++)
  {
    while (sym != 0xFF && (!pHist->symbols[sym].freq || pHist->symbols[sym + 1].cumul <= i))
      sym++;

    pHist->cumulInv[i] = sym;
  }

  return true;
}

template void make_dec_hist(hist_dec_t<15> *pHistDec, const hist_t *pHist);
template void make_dec_hist(hist_dec_t<14> *pHistDec, const hist_t *pHist);
template void make_dec_hist(hist_dec_t<13> *pHistDec, const hist_t *pHist);
template void make_dec_hist(hist_dec_t<12> *pHistDec, const hist_t *pHist);
template void make_dec_hist(hist_dec_t<11> *pHistDec, const hist_t *pHist);
template void make_dec_hist(hist_dec_t<10> *pHistDec, const hist_t *pHist);

template void make_dec2_hist(hist_dec2_t<15> *pHistDec, const hist_t *pHist);
template void make_dec2_hist(hist_dec2_t<14> *pHistDec, const hist_t *pHist);
template void make_dec2_hist(hist_dec2_t<13> *pHistDec, const hist_t *pHist);
template void make_dec2_hist(hist_dec2_t<12> *pHistDec, const hist_t *pHist);
template void make_dec2_hist(hist_dec2_t<11> *pHistDec, const hist_t *pHist);
template void make_dec2_hist(hist_dec2_t<10> *pHistDec, const hist_t *pHist);

template void make_dec3_hist(hist_dec3_t<15> *pHistDec, const hist_t *pHist);
template void make_dec3_hist(hist_dec3_t<14> *pHistDec, const hist_t *pHist);
template void make_dec3_hist(hist_dec3_t<13> *pHistDec, const hist_t *pHist);
template void make_dec3_hist(hist_dec3_t<12> *pHistDec, const hist_t *pHist);
template void make_dec3_hist(hist_dec3_t<11> *pHistDec, const hist_t *pHist);
template void make_dec3_hist(hist_dec3_t<10> *pHistDec, const hist_t *pHist);

template bool inplace_make_hist_dec(hist_dec_t<15> *pHist);
template bool inplace_make_hist_dec(hist_dec_t<14> *pHist);
template bool inplace_make_hist_dec(hist_dec_t<13> *pHist);
template bool inplace_make_hist_dec(hist_dec_t<12> *pHist);
template bool inplace_make_hist_dec(hist_dec_t<11> *pHist);
template bool inplace_make_hist_dec(hist_dec_t<10> *pHist);

template bool inplace_make_hist_dec2(hist_dec2_t<15> *pHist);
template bool inplace_make_hist_dec2(hist_dec2_t<14> *pHist);
template bool inplace_make_hist_dec2(hist_dec2_t<13> *pHist);
template bool inplace_make_hist_dec2(hist_dec2_t<12> *pHist);
template bool inplace_make_hist_dec2(hist_dec2_t<11> *pHist);
template bool inplace_make_hist_dec2(hist_dec2_t<10> *pHist);

template void make_dec_pack_hist(hist_dec_pack_t<12> *pHistDec, const hist_t *pHist);
template void make_dec_pack_hist(hist_dec_pack_t<11> *pHistDec, const hist_t *pHist);
template void make_dec_pack_hist(hist_dec_pack_t<10> *pHistDec, const hist_t *pHist);
