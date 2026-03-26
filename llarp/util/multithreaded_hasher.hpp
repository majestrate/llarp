#pragma once

#include <iterator>
#include <llarp/util/thread/queue.hpp>
#include <vector>
#include <functional>
#include <llarp/util/priority_queue.hpp>

namespace llarp::util
{
  namespace
  {
    template <typename Iter_t, typename Value_t>
    concept is_iterator_for =
#ifdef ANDROID
        std::is_same_v<Value_t, typename Iter_t::value_type>;
#else
        std::forward_iterator<Iter_t> and std::is_same_v<Value_t, typename Iter_t::value_type>;
#endif

    template <typename T>
    concept is_hashbuffer_compatible = std::is_move_constructible_v<T>;
  };  // namespace

  template <
      typename T,
      typename Hash_t,
      typename HashFunc_t = std::function<void(const uint8_t*, size_t, Hash_t&)>,
      typename GetHashRef_t = std::function<Hash_t&(T&)>>
    requires is_hashbuffer_compatible<T> and is_hashbuffer_compatible<Hash_t>
  struct MultithreadedHasher
  {
    struct HashedBuffer
    {
      T buf;

      const uint8_t*
      data() const
      {
        return buf.data();
      }

      size_t
      size() const
      {
        return buf.size();
      }
    };

    void
    async_hash_vec(std::vector<T> vec)
    {
      if (vec.empty())
        return;
      for (auto& v : vec)
        m_IngestData.pushBack(HashedBuffer{std::move(v)});
      vec.clear();
    }

    std::vector<T>
    poll_hashed_data()
    {
      std::vector<T> hashed;
      with_inplace_priority_queue<T>(hashed, [self = this](auto& queue) {
        do
        {
          auto maybe = self->m_HashedData.tryPopFront();
          if (not maybe)
            break;
          queue.emplace(std::move(*maybe));
        } while (true);
      });
      return hashed;
    }

    void
    start(
        size_t N_threads, std::function<void(void)> notify_ready = []() {})
    {
      if (not m_Threads.empty())
        return;
      m_Notify = notify_ready;
      while (N_threads > 0)
      {
        m_Threads.emplace_back([this]() { run_thread_worker(); });
        N_threads--;
      }
    }

    void
    stop()
    {
      m_IngestData.disable();
      for (auto& thread : m_Threads)
        thread.join();
      m_HashedData.disable();
      m_Threads.clear();
    }

    explicit MultithreadedHasher(HashFunc_t hash_func, GetHashRef_t get_hash)
        : m_HashFunc{std::move(hash_func)}, m_GetHashRef{std::move(get_hash)}
    {}

   private:
    thread::Queue<HashedBuffer> m_IngestData{128};
    thread::Queue<T> m_HashedData{128};
    std::vector<std::thread> m_Threads;
    const HashFunc_t m_HashFunc;
    std::function<void(void)> m_Notify;
    const GetHashRef_t m_GetHashRef;

    void
    run_thread_worker()
    {
      SetThreadName("llarp-hasher");
      do
      {
        auto maybe = m_IngestData.popFrontWithTimeout(50ms);
        if (not maybe and not m_IngestData.enabled())
          return;
        if (not maybe)
          continue;
        m_HashFunc(maybe->data(), maybe->size(), m_GetHashRef(maybe->buf));
        m_HashedData.pushBack(std::move(maybe->buf));
        m_Notify();
      } while (true);
    }
  };

}  // namespace llarp::util
