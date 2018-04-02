#include "server.h"

#define NETLOG_BACKEND NETLOG_EMPTY_BACKEND	
#include <memory>
#include <string>
#include <set>
#include <net/poller.h>
#include <net/tcp/listener.h>
#include <net/tcp/stream.h>

class server_impl;

class session
	: public net::tcp::stream
{
public:
	typedef net::tcp::stream base_type;

public:
	session(server_impl* server, net::poller_t* poll);
	std::string recv();

private:
	void event_close();

private:
	server_impl * server_;
};

class server_impl
	: public net::tcp::listener
{
public:
	friend class session;
	typedef net::tcp::listener base_type;

public:
	server_impl(const net::endpoint& ep);
	~server_impl();
	void      update();
	bool      listen();
	void      close_all();
	void      close_session(session* s);
	uint16_t  get_port() const;
	void      event_recv(server::fn_recv fn);

private:
	void event_accept(net::socket::fd_t fd, const net::endpoint& ep);

private:
	std::set<session*>                    sessions_;
	std::vector<std::unique_ptr<session>> clearlist_;
	net::endpoint                         endpoint_;
	server::fn_recv                       fn_recv_;
};

session::session(server_impl* server, net::poller_t* poll)
	: net::tcp::stream(poll)
	, server_(server)
{ }

std::string session::recv()
{
	std::string buf;
	buf.resize(base_type::recv_size());
	base_type::recv(buf.data(), buf.size());
	return buf;
}

void session::event_close()
{
	server_->close_session(this);
	base_type::event_close();
}

server_impl::server_impl(const net::endpoint& ep)
	: base_type(new net::poller_t)
	, sessions_()
	, endpoint_(ep)
{
	net::socket::initialize();
	base_type::open();
	listen();
}

server_impl::~server_impl()
{
	base_type::close();
	for (auto& s : sessions_) {
		s->close();
		delete s;
	}
	delete base_type::poller_;
}

void server_impl::update()
{
	if (!is_listening()) {
		listen();
	}
	for (auto& s = clearlist_.begin(); s != clearlist_.end();)
	{
		if ((*s)->sock == net::socket::retired_fd) {
			s = clearlist_.erase(s);
		}
		else {
			++s;
		}
	}
	base_type::poller_->wait(1000, 0);
}

bool server_impl::listen()
{
	return base_type::listen(endpoint_, false);
}

void server_impl::event_accept(net::socket::fd_t fd, const net::endpoint& ep)
{
	std::unique_ptr<session> s(new session(this, get_poller()));
	s->attach(fd, ep);
	sessions_.insert(s.release());
}

void server_impl::close_session(session* s)
{
	std::string msg = s->recv();
	if (fn_recv_) fn_recv_(msg);
	sessions_.erase(s);
	clearlist_.push_back(std::unique_ptr<session>(s));
}

void server_impl::close_all()
{
	base_type::close();
	for (auto& s : sessions_) {
		s->close();
		clearlist_.push_back(std::unique_ptr<session>(s));
	}
	sessions_.clear();
}

uint16_t server_impl::get_port() const
{
	if (sock == net::socket::retired_fd) {
		return 0;
	}
	sockaddr_in addr;
	int addrlen = sizeof(sockaddr_in);
	::getsockname(sock, (sockaddr*)&addr, &addrlen);
	return ::ntohs(addr.sin_port);
}

void server_impl::event_recv(server::fn_recv fn)
{
	fn_recv_ = fn;
}

server::server(const char* ip, uint16_t port)
	: impl_(new server_impl(net::endpoint(ip, port)))
{
}

server::~server()
{
	delete impl_;
}

void server::update()
{
	impl_->update();
}

void server::close()
{
	impl_->close_all();
}

uint16_t server::get_port() const
{
	return impl_->get_port();
}

void server::event_recv(fn_recv fn)
{
	impl_->event_recv(fn);
}
