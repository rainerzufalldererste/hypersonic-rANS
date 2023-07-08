// Improved Version of https://github.com/rainerzufalldererste/slapcodec/blob/master/slapcodec/src/threadpool.cpp

#include "thread_pool.h"

#include <thread>
#include <mutex>
#include <queue>
#include <atomic>
#include <condition_variable>

#ifdef _WIN32
#include <windows.h>
#else
#include <sched.h>
#include <pthread.h>
#endif

struct thread_pool
{
  std::queue<std::function<void(void)>> tasks;
  
  std::thread *pThreads;
  size_t threadCount;

  std::atomic<size_t> taskCount;
  std::atomic<bool> isRunning;
  std::mutex mutex;
  std::condition_variable condition_var;

  thread_pool(const size_t threadCount);
  ~thread_pool();
};

void thread_pool_ThreadFunc(thread_pool *pThreadPool, const size_t index)
{
#ifdef _WIN32
  SetThreadIdealProcessor(GetCurrentThread(), (DWORD)index);
  SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);
#else
  cpu_set_t cpuset;
  CPU_ZERO(&cpuset);
  CPU_SET((int32_t)index, &cpuset);

  pthread_t current_thread = pthread_self();
  pthread_setaffinity_np(current_thread, sizeof(cpu_set_t), &cpuset);
#endif

  while (pThreadPool->isRunning)
  {
    std::function<void(void)> task = nullptr;

    {
      std::unique_lock<std::mutex> lock(pThreadPool->mutex);
      pThreadPool->condition_var.wait_for(lock, std::chrono::milliseconds(1));

      if (!pThreadPool->tasks.empty())
      {
        task = pThreadPool->tasks.front();
        pThreadPool->tasks.pop();
      }
    }

    if (task)
    {
      task();
      --pThreadPool->taskCount;
      continue;
    }
  }
}

thread_pool::thread_pool(const size_t threads) :
  tasks(),
  pThreads(nullptr),
  threadCount(threads),
  taskCount(0),
  isRunning(true),
  mutex(),
  condition_var()
{
  pThreads = reinterpret_cast<std::thread *>(malloc(sizeof(std::thread) * threads));

  for (size_t i = 0; i < threads; i++)
    new (&pThreads[i]) std::thread(thread_pool_ThreadFunc, this, i);
}

thread_pool::~thread_pool()
{
  thread_pool_await(this);
   
  isRunning = false;
  condition_var.notify_all();

  for (size_t i = 0; i < threadCount; i++)
  {
    pThreads[i].join();
    pThreads[i].~thread();
  }

  free(pThreads);
}

thread_pool * thread_pool_new(const size_t threads)
{
  return new thread_pool(threads);
}

void thread_pool_destroy(thread_pool **ppThreadPool)
{
  if (ppThreadPool == nullptr || *ppThreadPool == nullptr)
    return;

  delete *ppThreadPool;
}

size_t thread_pool_thread_count(thread_pool *pPool)
{
  if (pPool == nullptr)
    return 1;

  return pPool->threadCount == 0 ? 1 : pPool->threadCount;
}

void thread_pool_add(thread_pool *pThreadPool, const std::function<void(void)> &task)
{
  pThreadPool->taskCount++;

  pThreadPool->mutex.lock();
  pThreadPool->tasks.push(task);
  pThreadPool->mutex.unlock();

  pThreadPool->condition_var.notify_one();
}

void thread_pool_await(thread_pool *pThreadPool)
{
  while (true)
  {
    std::function<void(void)> task = nullptr;

    // Locked by mutex.
    {
      pThreadPool->mutex.lock();

      if (!pThreadPool->tasks.empty())
      {
        task = pThreadPool->tasks.front();
        pThreadPool->tasks.pop();
      }

      pThreadPool->mutex.unlock();
    }

    if (task)
    {
      task();
      pThreadPool->taskCount--;
    }
    else
    {
      break;
    }
  }

  while (pThreadPool->taskCount > 0)
    std::this_thread::yield(); // Wait for all other threads to finish their tasks.
}

size_t thread_pool_max_threads()
{
  return std::thread::hardware_concurrency();
}
