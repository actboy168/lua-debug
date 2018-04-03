#include <debugger/io/socket.h>

#define NETLOG_BACKEND NETLOG_EMPTY_BACKEND	
#include <iostream>
#include <memory>
#include <string>	
#include <thread> 	
#include <net/poller.h>
#include <net/tcp/connecter.h>
#include <net/tcp/listener.h> 
#include <net/tcp/stream.h>	
#include <base/util/format.h>

namespace vscode { namespace io {
	class server;
	class rprotocol;
	class wprotocol;

	class session
		: public net::tcp::stream
	{
	public:
		typedef net::tcp::stream base_type;

	public:
		session(server* server, net::poller_t* poll);
		bool      output(const char* buf, size_t len);
		bool      input(std::string& buf);

	private:
		void event_close();
		bool event_in();

	private:
		server* server_;
		std::string buf_;
		size_t      stat_;
		size_t      len_;
		net::tcp::buffer<std::string, 8> input_queue_;
	};

	class server
		: public net::tcp::listener
	{
	public:
		friend class session;
		typedef net::tcp::listener base_type;

	public:
		server(net::poller_t* poll, const net::endpoint& ep, bool rebind);
		~server();
		void      update();
		bool      listen();
		bool      output(const char* buf, size_t len);
		bool      input(std::string& buf);
		void      close_session(); 
		uint16_t  get_port() const;

	private:
		void event_accept(net::socket::fd_t fd, const net::endpoint& ep);
		void event_close();

	private:
		std::unique_ptr<session> session_;
		net::endpoint            endpoint_;
		std::vector<std::unique_ptr<session>> clearlist_;
		bool                     rebind_;
	};

	session::session(server* server, net::poller_t* poll)
		: net::tcp::stream(poll)
		, server_(server)
		, stat_(0)
		, buf_()
		, len_(0)
	{
	}

	bool session::output(const char* buf, size_t len)
	{
		auto l = ::base::format("Content-Length: %d\r\n\r\n", len);
		if (l.size() != send(l.data(), l.size())) {
			return false;
		}
		return len == send(buf, len);
	}

	bool session::input(std::string& buf)
	{
		if (input_queue_.empty())
			return false;
		buf = std::move(input_queue_.front());
		input_queue_.pop();
		return true;
	}

	void session::event_close()
	{
		base_type::event_close();
		server_->close_session();
	}

	bool session::event_in()
	{
		if (!base_type::event_in())
			return false;
		for (size_t n = base_type::recv_size(); n; --n)
		{
			char c = 0;
			base_type::recv(&c, 1);
			buf_.push_back(c);
			switch (stat_)
			{
			case 0:
				if (c == '\r') stat_ = 1;
				break;
			case 1:
				stat_ = 0;
				if (c == '\n')
				{
					if (buf_.substr(0, 16) != "Content-Length: ")
					{
						return false;
					}
					try {
						len_ = (size_t)std::stol(buf_.substr(16, buf_.size() - 18));
						stat_ = 2;
					}
					catch (...) {
						return false;
					}
					buf_.clear();
				}
				break;
			case 2:
				if (buf_.size() >= (len_ + 2))
				{
					input_queue_.push(buf_.substr(2, len_));
					buf_.clear();
					stat_ = 0;
				}
				break;
			}
		}
		return true;
	}

	server::server(net::poller_t* poll, const net::endpoint& ep, bool rebind)
		: base_type(poll)
		, session_()
		, endpoint_(ep)
		, rebind_(false)
	{
		net::socket::initialize();
		base_type::open();
	}

	server::~server()
	{
		base_type::close();
		if (session_)
			session_->close();
	}

	void server::update()
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
	}

	bool server::listen()
	{
		return base_type::listen(endpoint_, rebind_);
	}

	void server::event_accept(net::socket::fd_t fd, const net::endpoint& ep)
	{
		if (session_)
		{
			net::socket::close(fd);
			return;
		}
		session_.reset(new session(this, get_poller()));
		session_->attach(fd, ep);
	}

	void server::event_close()
	{
		session_.reset();
		base_type::event_close();
	}

	bool server::output(const char* buf, size_t len)
	{
		if (!session_)
			return false;
		return session_->output(buf, len);
	}

	bool server::input(std::string& buf)
	{
		if (!session_)
			return false;
		return session_->input(buf);
	}

	void server::close_session()
	{
		if (!session_)
			return;
		std::unique_ptr<session> s(session_.release());
		s->close();
		clearlist_.push_back(std::move(s));
	}

	uint16_t server::get_port() const
	{
		if (sock == net::socket::retired_fd) {
			return 0;
		}
		sockaddr_in addr;
		int addrlen = sizeof(sockaddr_in);
		::getsockname(sock, (sockaddr*)&addr, &addrlen);
		return ::ntohs(addr.sin_port);
	}

	socket::socket(const char* ip, uint16_t port, bool rebind)
		: poller_(new net::poller_t)
		, server_(new server(poller_, net::endpoint(ip, port), rebind))
	{
		server_->listen();
	}

	socket::~socket()
	{
		delete server_;
		delete poller_;
	}

	void socket::update(int ms)
	{
		server_->update();
		poller_->wait(1000, ms);
	}

	bool socket::output(const char* buf, size_t len)
	{
		if (!server_)
			return false;
		if (!server_->output(buf, len))
			return false;
		return true;
	}

	bool socket::input(std::string& buf)
	{
		if (!server_)
			return false;
		return server_->input(buf);
	}

	void socket::close()
	{
		if (server_)
			server_->close_session();
	}

	uint16_t socket::get_port() const
	{
		if (server_)
			return server_->get_port();
		return 0;
	}
}}
