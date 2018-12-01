#include <debugger/io/socket_impl.h>

#define NETLOG_BACKEND NETLOG_EMPTY_BACKEND	
#include <iostream>
#include <memory>
#include <string>	
#include <thread> 	
#include <net/poller.h>
#include <net/tcp/connecter.h>
#include <net/tcp/listener.h> 
#include <net/tcp/stream.h>	
#include <bee/utility/format.h>
#include <debugger/io/stream.h>

namespace vscode { namespace io {
	class sock_server;
	class sock_session;

	typedef std::function<bool()> EventIn;
	typedef std::function<void()> EventClose;

	class sock_session
		: public bee::net::tcp::stream
	{
	public:
		typedef bee::net::tcp::stream base_type;

	public:
		sock_session(EventClose event_close, EventIn event_in, bee::net::poller_t* poll);
		bool output(const char* buf, size_t len);
		bool input(std::string& buf);

	private:
		void event_close();
		bool event_in();

	private:
		EventClose   event_close_;
		EventIn      event_in_;
	};

	class sock_server
		: public bee::net::tcp::listener
	{
	public:
		friend class sock_session;
		typedef bee::net::tcp::listener base_type;

	public:
		sock_server(bee::net::poller_t* poll, sock_stream& stream, const bee::net::endpoint& ep);
		~sock_server();
		void      update();
		bool      listen();
		void      close_session(); 
		uint16_t  get_port() const;
		bool      stream_update();

	private:
		void event_accept(bee::net::socket::fd_t fd);
		void event_close();

	private:
		std::unique_ptr<sock_session> session_;
		bee::net::endpoint            endpoint_;
		std::vector<std::unique_ptr<sock_session>> clearlist_;
		sock_stream&             stream_;
	};

	sock_stream::sock_stream()
		: s(nullptr)
	{ }
	size_t sock_stream::raw_peek() {
		if (is_closed()) return 0;
		return s->recv_size();
	}
	bool sock_stream::raw_recv(char* buf, size_t len) {
		if (is_closed()) return false;
		return len == s->recv(buf, len);
	}
	bool sock_stream::raw_send(const char* buf, size_t len) {
		if (is_closed()) return false;
		return len == s->send(buf, len);
	}
	void sock_stream::open(sock_session* s) {
		this->s = s;
	}
	void sock_stream::close() {
		s = nullptr;
		clear();
	}
	bool sock_stream::is_closed() const {
		return s == nullptr;
	}

	sock_session::sock_session(EventClose event_close, EventIn event_in, bee::net::poller_t* poll)
		: bee::net::tcp::stream(poll)
		, event_close_(event_close)
		, event_in_(event_in)
	{ }

	void sock_session::event_close()
	{
		base_type::event_close();
		event_close_();
	}

	bool sock_session::event_in()
	{
		if (!base_type::event_in())
			return false;
		return event_in_();
	}

	sock_server::sock_server(bee::net::poller_t* poll, sock_stream& stream, const bee::net::endpoint& ep)
		: base_type(poll)
		, session_()
		, endpoint_(ep)
		, stream_(stream)
	{
		bee::net::socket::initialize();
		base_type::open();
	}

	sock_server::~sock_server()
	{
		base_type::close();
		if (session_)
			session_->close();
	}

	void sock_server::update()
	{
		if (!is_listening()) {
			listen();
		}
		for (auto s = clearlist_.begin(); s != clearlist_.end();)
		{
			if ((*s)->sock == bee::net::socket::retired_fd) {
				s = clearlist_.erase(s);
			}
			else {
				++s;
			}
		}
	}

	bool sock_server::listen()
	{
		return base_type::listen(endpoint_);
	}

	void sock_server::event_accept(bee::net::socket::fd_t fd)
	{
		if (session_)
		{
			bee::net::socket::close(fd);
			return;
		}
		session_.reset(new sock_session([this]() { close_session(); }, std::bind(&sock_server::stream_update, this), get_poller()));
		session_->attach(fd);
		stream_.open(session_.get());
	}

	void sock_server::event_close()
	{
		session_.reset();
		base_type::event_close();
	}

	void sock_server::close_session()
	{
		if (!session_)
			return;
		std::unique_ptr<sock_session> s(session_.release());
		s->close();
		clearlist_.push_back(std::move(s));
	}

	uint16_t sock_server::get_port() const
	{
		if (sock == bee::net::socket::retired_fd) {
			return 0;
		}
		sockaddr_in addr;
#if defined _WIN32
		int addrlen = sizeof(sockaddr_in);
#else
		socklen_t addrlen = sizeof(sockaddr_in);
#endif
		::getsockname(sock, (sockaddr*)&addr, &addrlen);
		return ntohs(addr.sin_port);
	}

	bool sock_server::stream_update()
	{
		stream_.sock_stream::update(10);
		return !stream_.is_closed();
	}

	socket_s::socket_s(const bee::net::endpoint& ep)
		: sock_stream()
		, poller_(new bee::net::poller_t)
		, server_(new sock_server(poller_, *this, ep))
	{
		server_->listen();
	}

	socket_s::~socket_s()
	{
		delete server_;
		delete poller_;
	}

	void socket_s::update(int ms)
	{
		server_->update();
		poller_->wait(1000, ms);
	}

	void socket_s::close()
	{
		sock_stream::close();
		if (server_)
			server_->close_session();
	}

	uint16_t socket_s::get_port() const
	{
		if (server_)
			return server_->get_port();
		return 0;
	}

	class sock_client
		: public bee::net::tcp::connecter_t<bee::net::poller_t>
	{
	public:
		friend class sock_session;
		typedef bee::net::tcp::connecter_t<bee::net::poller_t> base_type;

	public:
		sock_client(bee::net::poller_t* poll, sock_stream& stream, const bee::net::endpoint& ep);

	private:
		void event_connect(bee::net::socket::fd_t fd);
		bool stream_update();

	private:
		bee::net::endpoint endpoint_;
		sock_stream&  stream_;
	};

	sock_client::sock_client(bee::net::poller_t* poll, sock_stream& stream, const bee::net::endpoint& ep)
		: base_type(poll)
		, endpoint_(ep)
		, stream_(stream)
	{
		bee::net::socket::initialize();
		connect(endpoint_, std::bind(&sock_client::event_connect, this, std::placeholders::_1));
	}

	void sock_client::event_connect(bee::net::socket::fd_t fd)
	{
		stream_.open(new sock_session([this]() { }, std::bind(&sock_client::stream_update, this), get_poller()));
	}

	bool sock_client::stream_update()
	{
		stream_.sock_stream::update(10);
		return !stream_.is_closed();
	}

	socket_c::socket_c(const bee::net::endpoint& ep)
		: sock_stream()
		, poller_(new bee::net::poller_t)
		, client_(new sock_client(poller_, *this, ep))
	{
	}

	socket_c::~socket_c()
	{
		delete client_;
		delete poller_;
	}

	void socket_c::update(int ms)
	{
		poller_->wait(1000, ms);
	}

	void socket_c::close()
	{
		sock_stream::close();
	}
}}
