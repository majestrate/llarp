#pragma once

#include <functional>
#include <llarp/net/sock_addr.hpp>
#include <llarp/util/buffer.hpp>

namespace llarp
{

  class TCPConnectionPool;
  class TCPAcceptor;

  class TCPConnection : public std::enable_shared_from_this<TCPConnection>
  {
   public:
    using RecvHandler = std::function<void(std::optional<OwnedBuffer>, std::error_code)>;
    using SendCompletionHandler = std::function<void(std::error_code)>;

    virtual ~TCPConnection() = default;

    virtual void
    Send(OwnedBuffer buffer, SendCompletionHandler completionHandler = nullptr) = 0;

    virtual void
    Close() = 0;

    virtual SockAddr
    LocalAddr() const = 0;

    virtual SockAddr
    RemoteAddr() const = 0;

    virtual int
    Bind(SockAddr laddr) = 0;

    virtual bool
    Start() = 0;

    void
    SetRecvHandler(RecvHandler h);

   protected:
    RecvHandler m_RecvHandler;
    TCPConnectionPool& m_Pool;

    TCPConnection(RecvHandler recvHandler, TCPConnectionPool& pool);

    // untrack from connection pool.
    void
    Untrack();
  };

  class TCPConnectionPool
  {
   public:
    friend class TCPConnection;
    friend class TCPAcceptor;

    virtual ~TCPConnectionPool() = default;

    using CompletionHandler = std::function<void(std::shared_ptr<TCPConnection>, std::error_code)>;
    virtual void
    Connect(
        SockAddr remote_addr,
        CompletionHandler completion_hander,
        TCPConnection::RecvHandler recv_handler,
        std::optional<SockAddr> local_addr = std::nullopt) = 0;

    using AcceptHandler = std::function<void(std::shared_ptr<TCPConnection>, std::error_code)>;
    virtual std::shared_ptr<TCPAcceptor>
    CreateAcceptor(AcceptHandler accept_handler) = 0;

    virtual void
    CloseAll() = 0;

   protected:

    std::vector<std::shared_ptr<TCPConnection>> m_Connections;
    std::vector<std::shared_ptr<TCPAcceptor>> m_Acceptors;

    virtual std::shared_ptr<TCPConnection>
    MakeConnection(std::optional<SockAddr> local_addr, TCPConnection::RecvHandler recv_handler) = 0;

    void
    RemoveConn(std::shared_ptr<TCPConnection> conn);

    void
    RemoveConn(std::shared_ptr<TCPAcceptor> acceptor);
  };

  class TCPAcceptor : public std::enable_shared_from_this<TCPAcceptor>
  {
   public:
    using AcceptHandler = TCPConnectionPool::AcceptHandler;

    virtual ~TCPAcceptor() = default;

    virtual bool
    Bind(SockAddr addr) = 0;

    /// get locally bound address, nullopt if not currently bound yet.
    virtual std::optional<SockAddr>
    LocalAddr() const = 0;

    /// close this acceptor.
    virtual void
    Close() = 0;

   protected:
    AcceptHandler m_AcceptHandler;
    TCPConnectionPool& m_Pool;

    TCPAcceptor(AcceptHandler handler, TCPConnectionPool& pool);

    std::shared_ptr<TCPConnection>
    MakeConn();

    void
    Untrack();
  };

}  // namespace llarp