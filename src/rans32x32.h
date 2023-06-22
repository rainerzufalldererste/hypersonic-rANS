#ifndef rans32x32_h__
#define rans32x32_h__

#include "hist.h"

size_t rANS32x32_capacity(const size_t inputSize);

size_t rANS32x32_encode_basic(const uint8_t *pInData, const size_t length, uint8_t *pOutData, const size_t outCapacity, const hist_t *pHist);
size_t rANS32x32_decode_basic(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity);
size_t rANS32x32_decode_avx2_basic(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity);

#endif // rans32x32_h__
