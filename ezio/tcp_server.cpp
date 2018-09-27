/*
 @ 0xCCCCCCCC
*/

#include "ezio/tcp_server.h"

#include <functional>

#include "kbase/error_exception_util.h"
#include "kbase/string_format.h"

#include "ezio/event_loop.h"

namespace ezio {

using namespace std::placeholders;

TCPServer::TCPServer(ezio::EventLoop* loop, const SocketAddress& addr, std::string name)
    : loop_(loop),
      listen_addr_(addr),
      name_(std::move(name)),
      acceptor_(loop, addr),
      next_conn_id_(0)
{
    acceptor_.set_on_new_connection(std::bind(&TCPServer::HandleNewConnection, this, _1, _2));
}

TCPServer::~TCPServer()
{
    ENSURE(CHECK, loop_->BelongsToCurrentThread()).Require();

    for (auto& conn_item : connections_) {
        TCPConnectionPtr conn(std::move(conn_item.second));
        loop_->RunTask(std::bind(&TCPConnection::MakeTeardown, conn));
    }
}

void TCPServer::Start()
{
    if (!started_.exchange(true, std::memory_order_acq_rel)) {
        ENSURE(CHECK, !acceptor_.listening()).Require();
        loop_->RunTask([this] {
            acceptor_.Listen();
        });
    }
}

void TCPServer::HandleNewConnection(ScopedSocket&& conn_sock, const SocketAddress& conn_addr)
{
    ENSURE(CHECK, loop_->BelongsToCurrentThread()).Require();

    auto conn_name = kbase::StringFormat("-{0}#{1}", listen_addr_.ToHostPort(), next_conn_id_);
    ++next_conn_id_;

    // TODO: Enable support of multi-looper.

    auto conn = std::make_shared<TCPConnection>(loop_,
                                                std::move(conn_name),
                                                std::move(conn_sock),
                                                listen_addr_,
                                                conn_addr);

    connections_.insert({conn->name(), conn});

    conn->set_on_connection(on_connection_);
    conn->set_on_message(on_message_);
    conn->set_on_close(std::bind(&TCPServer::RemoveConnection, this, _1));

    conn->MakeEstablished();
}

void TCPServer::RemoveConnection(const TCPConnectionPtr& conn)
{
    ENSURE(CHECK, loop_->BelongsToCurrentThread()).Require();

    auto removed_count = connections_.erase(conn->name());
    ENSURE(CHECK, removed_count == 1)(removed_count).Require();

    // TODO: Do we really need to use QueueTask() ?
    loop_->QueueTask(std::bind(&TCPConnection::MakeTeardown, conn));
}

}   // namespace ezio
