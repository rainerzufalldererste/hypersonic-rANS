#ifndef block_rANS32x64_16w_h__
#define block_rANS32x64_16w_h__

#include "hist.h"

size_t block_rANS32x64_16w_capacity(const size_t inputSize);

size_t block_rANS32x64_16w_encode_15(const uint8_t *pInData, const size_t length, uint8_t *pOutData, const size_t outCapacity);
size_t block_rANS32x64_16w_encode_14(const uint8_t *pInData, const size_t length, uint8_t *pOutData, const size_t outCapacity);
size_t block_rANS32x64_16w_encode_13(const uint8_t *pInData, const size_t length, uint8_t *pOutData, const size_t outCapacity);
size_t block_rANS32x64_16w_encode_12(const uint8_t *pInData, const size_t length, uint8_t *pOutData, const size_t outCapacity);
size_t block_rANS32x64_16w_encode_11(const uint8_t *pInData, const size_t length, uint8_t *pOutData, const size_t outCapacity);
size_t block_rANS32x64_16w_encode_10(const uint8_t *pInData, const size_t length, uint8_t *pOutData, const size_t outCapacity);

size_t block_rANS32x64_16w_decode_15(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity);
size_t block_rANS32x64_16w_decode_14(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity);
size_t block_rANS32x64_16w_decode_13(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity);
size_t block_rANS32x64_16w_decode_12(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity);
size_t block_rANS32x64_16w_decode_11(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity);
size_t block_rANS32x64_16w_decode_10(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity);

#endif // block_rANS32x64_16w_h__
