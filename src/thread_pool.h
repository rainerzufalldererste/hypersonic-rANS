#ifndef thread_pool_h__
#define thread_pool_h__

#include <stddef.h>
#include <functional>

struct thread_pool;

thread_pool * thread_pool_new(const size_t threads);
void thread_pool_destroy(thread_pool **ppThreadPool);

size_t thread_pool_thread_count(thread_pool *pThreadPool);

void thread_pool_add(thread_pool *pThreadPool, const std::function<void(void)> &func);
void thread_pool_await(thread_pool *pThreadPool);

size_t thread_pool_max_threads();

#endif // thread_pool_h__
