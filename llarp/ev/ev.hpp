#pragma once

#include <llarp/util/buffer.hpp>
#include <llarp/util/time.hpp>
#include <llarp/util/thread/threading.hpp>
#include <llarp/constants/evloop.hpp>
#include <llarp/net/interface_info.hpp>
#include <algorithm>
#include <deque>
#include <list>
#include <future>
#include <utility>


namespace llarp
{
  struct SockAddr;
  struct UDPHandle;

  namespace vpn
  {
    class NetworkInterface;
  }

  namespace net
  {
    class Platform;

    struct IPPacket;
  }  // namespace net
}

namespace llarp::eventloop
{
  /// distinct event loop waker upper; used to idempotently schedule a task on the next event loop
  ///
  /// Created via EventLoop::make_waker(...).
  class LappWakeup
  {
   public:
    /// Destructor: remove the task from the event loop task.  (Note that destruction here only
    /// initiates removal of the task from the underlying event loop: it is *possible* for the
    /// callback to fire again if already triggered depending on the underlying implementation).
    virtual ~LoopWakeup() = default;

    /// trigger this task to run on the next event loop iteration; does nothing if already
    /// triggered.
    virtual void
    Trigger() = 0;
  };

  /// holds a repeated task on the event loop; the task is removed on destruction
  class PeriodicTask
  {
   public:
    // Destructor: if the task has been started then it is removed from the event loop.  Note
    // that it is possible for a task to fire *after* destruction of this container;
    // destruction only initiates removal of the periodic task.
    virtual ~PeriodicTask() = default;

    // Starts the repeater to call `task` every `every` period.
    virtual void
    start(std::chrono::duration every, std::function<void()> task) = 0;
  };


template<typename Callable>
  class IdempotentCallable
  {
    public: 
    virtual void 
    reset_idempotency() = 0
  };

  /// @brief a threadsafe domain in which a callable can execute and modify state.
  class StatefulDomain 
  {

    protected:
    /// @brief determines if we are current executing in a way which we can directly execute a callable without
    virtual bool 
    in_our_domain() const =0;
        public: 

// Calls a function/lambda/etc.  If invoked from within the event loop itself this calls the
    // given lambda immediately; otherwise it passes it to `defer()` to be queued to run at the
    // next event loop iteration.
    template <typename Callable>
    void
  execute(Callable&& f)
    {
      if (not in_our_domain())
      {
        defer(std::forward<Callable>(f));
        return;
      }

      
        f();
        wakeup();

        
    }


    /// @brief convience warpper for execute()
    template <typename Callable>
    void operator()(Callable & f) 
    {
      execute(std::forward<Callable>(f));
    }

    // Queues a function to be called on the next event loop cycle and triggers it to be called as
    // soon as possible; can be called from any thread.  Note that, unlike `execute()`, this queues the
    // job even if called from the event loop thread itself and so you *usually* want to use
    // `execute()` instead.
    virtual void
    defer(std::function<void(void)> f) = 0;

    // Adds a timer to this statefule domain to invoke the given callback after a delay.
    // virtual void
    // defer_later(llarp_time_t delay_ms, std::function<void(void)> callback) = 0;

    // Created a repeated timer that fires ever `repeat` time unit.  Lifetime of the event
    // is tied to `owner`: callbacks will be invoked so long as `owner` remains alive, but
    // the first time it repeats after `owner` has been destroyed the internal timer object will
    // be destroyed and no more callbacks will be invoked.
    //
    // Intended to be used as:
    //
    //     loop->call_every(100ms, weak_from_this(), [this] { some_func(); });
    //
    // Alternative, when shared_from_this isn't available for a type, you can use a local member
    // shared_ptr (or even create a simple one, for more fine-grained control) to tie the lifetime:
    //
    //     m_keepalive = std::make_shared<int>(42);
    //     loop->call_every(100ms, m_keepalive, [this] { some_func(); });
    //
    template <typename Callable>  // Templated so that the compiler can inline the call
    void
    call_every(llarp_time_t repeat, std::weak_ptr<void> owner, Callable f)
    {
      auto repeater = make_repeater();
      auto& r = *repeater;  // reference *before* we pass ownership into the lambda below
      r.start(
          repeat,
          [repeater = std::move(repeater), owner = std::move(owner), f = std::move(f)]() mutable {
            if (auto ptr = owner.lock())
              f();
            else
              repeater.reset();  // Trigger timer removal on tied object destruction (we should be
                                 // the only thing holding the repeater; ideally it would be a
                                 // unique_ptr, but std::function says nuh-uh).
          });
    }

/// @brief wrap a callable so that it is called idempotently in our domain.
/// @tparam Callable 
/// @param f 
/// @return an IdempotentCallable 
template <typename Callable>
IdempotentCallable<Collable> 
idempotent_caller(Callable f)
{

}


    // Wraps a lambda with a lambda that triggers it to be called via domain->execute()
    // when invoked.  E.g.:
    //
    //     auto x = domain->make_caller([] (int a) { std::cerr << a; });
    //     x(42);
    //     x(99);
    //
    // will schedule two calls of the inner lambda (with different arguments) in the event loop.
    // Arguments are forwarded to the inner lambda (allowing moving arguments into it).
    template <typename Callable>
    auto
    make_caller(Callable f)
    {
      return [this, f = std::move(f)](auto&&... args) {
        if (inEventLoop())
          return f(std::forward<decltype(args)>(args)...);

        // This shared pointer in a pain in the ass but needed because this lambda is going into a
        // std::function that only accepts copyable lambdas.  I *want* to simply capture:
        // args=std::make_tuple(std::forward<decltype(args)>(args)...) but that fails if any given
        // arguments aren't copyable (because of std::function).  Dammit.
        auto args_tuple_ptr = std::make_shared<std::tuple<std::decay_t<decltype(args)>...>>(
            std::forward<decltype(args)>(args)...);
        call_soon([f, args = std::move(args_tuple_ptr)]() mutable {
          // Moving away the tuple args here is okay because this lambda will only be invoked once
          std::apply(f, std::move(*args));
        });
      };
    }
  };

  // this (nearly!) abstract base class
  // is overriden for each platform
  class MainLoop
  {
   public:
    // Runs the event loop. This does not return until sometime after `stop()` is called (and so
    // typically you want to run this in its own thread).
    virtual void
    run() = 0;

    virtual bool
    running() const = 0;

    // Returns a current steady clock time value representing the current time with event loop tick
    // granularity.  That is, the value is typically only updated at the beginning of an event loop
    // tick.
    virtual llarp_time_t
    time_now() const = 0;

    

    virtual bool
    add_network_interface(
        std::shared_ptr<vpn::NetworkInterface> netif,
        std::function<void(net::IPPacket)> packetHandler) = 0;

    virtual bool
    add_ticker(std::function<void(void)> ticker) = 0;

    virtual void
    stop() = 0;

    virtual ~EventLoop() = default;

    virtual const net::Platform*
    Net_ptr() const;

    using UDPReceiveFunc = std::function<void(UDPHandle&, SockAddr src, llarp::OwnedBuffer buf)>;

    // Constructs a UDP socket that can be used for sending and/or receiving
    virtual std::shared_ptr<UDPHandle>
    make_udp(UDPReceiveFunc on_recv) = 0;

    /// Make a thread-safe event loop waker (an "async" in libuv terminology) on this event loop;
    /// you can call `->Trigger()` on the returned shared pointer to fire the callback at the next
    /// available event loop iteration.  (Multiple Trigger calls invoked before the call is actually
    /// made are coalesced into one call).
    virtual std::shared_ptr<EventLoopWakeup>
    make_waker(std::function<void()> callback) = 0;

    // Initializes a new repeated task object. Note that the task is not actually added to the event
    // loop until you call start() on the returned object.  Typically invoked via call_every.
    virtual std::shared_ptr<EventLoopRepeater>
    make_repeater() = 0;

    // Constructs and initializes a new default (libuv) event loop
    static std::shared_ptr<EventLoop>
    create(size_t queueLength = event_loop_queue_size);

    // Returns true if called from within the event loop thread, false otherwise.
    virtual bool
    inEventLoop() const = 0;

    // Returns the uvw::Loop *if* this event loop is backed by a uvw event loop (i.e. the default),
    // nullptr otherwise.  (This base class default always returns nullptr).
    virtual std::shared_ptr<uvw::Loop>
    MaybeGetUVWLoop()
    {
      return nullptr;
    }

    // Triggers an event loop wakeup; use when something has been done that requires the event loop
    // to wake up (e.g. adding to queues).  This is called implicitly by call() and call_soon().
    // Idempotent and thread-safe.
    virtual void
    wakeup() = 0;
  };

  using EventLoop_ptr = std::shared_ptr<EventLoop>;

}  // namespace llarp
