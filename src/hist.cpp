#include "hist.h"

#include <string.h>

//////////////////////////////////////////////////////////////////////////

void make_hist(hist_t *pHist, const uint8_t *pData, const size_t size)
{
  uint32_t hist[256];
  memset(hist, 0, sizeof(hist));

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
    const float mul = (float)TotalSymbolCount / counter;

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
    const uint32_t div = counter / TotalSymbolCount;

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
      const uint32_t mul = TotalSymbolCount / counter;

      for (size_t i = 0; i < 256; i++)
      {
        capped[i] = (uint16_t)(hist[i] * mul);
        cappedSum += capped[i];
      }
    }
  }

  if (cappedSum != TotalSymbolCount)
  {
    while (cappedSum > TotalSymbolCount) // Start stealing.
    {
      size_t target = 2;

      while (true)
      {
        size_t found = TotalSymbolCount;

        for (size_t i = 0; i < 256; i++)
          if (capped[i] > target && capped[i] < found)
            found = capped[i];

        if (found == TotalSymbolCount)
          break;

        for (size_t i = 0; i < 256; i++)
        {
          if (capped[i] == found)
          {
            capped[i]--;
            cappedSum--;

            if (cappedSum == TotalSymbolCount)
              goto hist_ready;
          }
        }

        target = found + 1;
      }
    }

    while (cappedSum < TotalSymbolCount) // Start a charity.
    {
      size_t target = TotalSymbolCount;

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

            if (cappedSum == TotalSymbolCount)
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

void make_dec_hist(hist_dec_t *pHistDec, const hist_t *pHist)
{
  memcpy(pHistDec, pHist, sizeof(hist_t));

  uint8_t sym = 0;

  for (size_t i = 0; i < TotalSymbolCount; i++)
  {
    while (sym != 0xFF && (!pHist->symbolCount[sym] || pHist->cumul[sym + 1] <= i))
      sym++;

    pHistDec->cumulInv[i] = sym;
  }
}

void make_dec2_hist(hist_dec2_t *pHistDec, const hist_t *pHist)
{
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

bool inplace_make_hist_dec(hist_dec_t *pHist)
{
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

bool inplace_make_hist_dec2(hist_dec2_t *pHist)
{
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
