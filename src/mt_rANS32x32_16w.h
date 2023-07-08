#ifndef mt_rANS32x32_16w_h__
#define mt_rANS32x32_16w_h__

#include "hist.h"
#include "thread_pool.h"

size_t mt_rANS32x32_16w_capacity(const size_t inputSize);

size_t mt_rANS32x32_16w_encode_15(const uint8_t *pInData, const size_t length, uint8_t *pOutData, const size_t outCapacity);
size_t mt_rANS32x32_16w_encode_14(const uint8_t *pInData, const size_t length, uint8_t *pOutData, const size_t outCapacity);
size_t mt_rANS32x32_16w_encode_13(const uint8_t *pInData, const size_t length, uint8_t *pOutData, const size_t outCapacity);
size_t mt_rANS32x32_16w_encode_12(const uint8_t *pInData, const size_t length, uint8_t *pOutData, const size_t outCapacity);
size_t mt_rANS32x32_16w_encode_11(const uint8_t *pInData, const size_t length, uint8_t *pOutData, const size_t outCapacity);
size_t mt_rANS32x32_16w_encode_10(const uint8_t *pInData, const size_t length, uint8_t *pOutData, const size_t outCapacity);

size_t mt_rANS32x32_16w_decode_15(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity);
size_t mt_rANS32x32_16w_decode_14(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity);
size_t mt_rANS32x32_16w_decode_13(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity);
size_t mt_rANS32x32_16w_decode_12(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity);
size_t mt_rANS32x32_16w_decode_11(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity);
size_t mt_rANS32x32_16w_decode_10(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity);

size_t mt_rANS32x32_16w_decode_mt_15(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity, thread_pool *pThreadPool);
size_t mt_rANS32x32_16w_decode_mt_14(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity, thread_pool *pThreadPool);
size_t mt_rANS32x32_16w_decode_mt_13(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity, thread_pool *pThreadPool);
size_t mt_rANS32x32_16w_decode_mt_12(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity, thread_pool *pThreadPool);
size_t mt_rANS32x32_16w_decode_mt_11(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity, thread_pool *pThreadPool);
size_t mt_rANS32x32_16w_decode_mt_10(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity, thread_pool *pThreadPool);

#endif // mt_rANS32x32_16w_h__
