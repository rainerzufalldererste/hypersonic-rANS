#ifndef hist_h__
#define hist_h__

#include "rans.h"

struct hist_t
{
  uint16_t symbolCount[256];
  uint16_t cumul[256];
};

typedef uint32_t enc_sym_t;

struct hist_enc_t
{
  enc_sym_t symbols[256];
};

struct hist_dec_t : hist_t
{
  uint8_t cumulInv[TotalSymbolCount];
};

struct dec_sym_t
{
  uint16_t freq, cumul;
};

static_assert(sizeof(dec_sym_t) == sizeof(uint32_t));

struct hist_dec2_t
{
  dec_sym_t symbols[256];
  uint8_t cumulInv[TotalSymbolCount];
};

//////////////////////////////////////////////////////////////////////////

void make_hist(hist_t *pHist, const uint8_t *pData, const size_t size);
void make_enc_hist(hist_enc_t *pHistEnc, const hist_t *pHist);
void make_dec_hist(hist_dec_t *pHistDec, const hist_t *pHist);
void make_dec2_hist(hist_dec2_t *pHistDec, const hist_t *pHist);

#endif // hist_h__
