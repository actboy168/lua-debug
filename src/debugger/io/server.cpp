#include <debugger/io/server.h>

namespace vscode { namespace io {
    session::session(bee::net::poller_t* poller)
        : stream()
        , bee::net::tcp::stream(poller)
    { }

    session::~session()
    {
        bee::net::tcp::stream::close();
    }

    bool session::is_closed() const {
        return bee::net::tcp::stream::sock == bee::net::socket::retired_fd;
    }

    void session::close() {
        bee::net::tcp::stream::close();
    }

    size_t session::raw_peek() {
        if (is_closed()) return 0;
        return bee::net::tcp::stream::recv_size();
    }
    bool session::raw_recv(char* buf, size_t len) {
        if (is_closed()) return false;
        return len == bee::net::tcp::stream::recv(buf, len);
    }
    bool session::raw_send(const char* buf, size_t len) {
        if (is_closed()) return false;
        return len == bee::net::tcp::stream::send(buf, len);
    }

    bool session::event_in() {
        if (!bee::net::tcp::stream::event_in())
            return false;
        stream::update(10);
        return !is_closed();
    }

	server::server(const bee::net::endpoint& ep)
		: session_(0)
        , bee::net::tcp::listener(new bee::net::poller_t)
        , endpoint_(ep)
	{
        bee::net::socket::initialize();
        bee::net::tcp::listener::open(ep);
	}

	server::~server()
	{
        close();
        delete session_;
        session_ = 0;
	}

	void server::update(int ms)
	{
        if (!bee::net::tcp::listener::is_listening()) {
            bee::net::tcp::listener::listen(endpoint_);
        }
        get_poller()->wait(1000, ms);
        if (session_) session_->update(0);
	}

	void server::close()
	{
        bee::net::tcp::listener::close();
        if (session_) {
            session_->close();
            while (!session_->is_closed()) {
                get_poller()->wait(1000, 0);
                if (session_) session_->update(0);
            }
        }
	}

    uint16_t server::get_port() const  {
        if (is_closed()) {
            return 0;
        }
        bee::net::endpoint ep = bee::net::endpoint::from_empty();
        if (!bee::net::socket::getsockname(bee::net::tcp::listener::sock, ep)) {
            return false;
        }
        auto[ip, port] = ep.info();
        return port;
    }

    void server::event_accept(bee::net::socket::fd_t fd)
    {
        if (!is_closed()) {
            bee::net::socket::close(fd);
            return;
        }
        if (session_) delete session_;
        session_ = new session(get_poller());
        session_->attach(fd);
    }

    bool server::is_closed() const {
        if (!session_) return true;
        return session_->is_closed();
    }

    bool server::output(const char* buf, size_t len) {
        if (!session_) return false;
        return session_->output(buf, len);
    }
    bool server::input(std::string& buf) {
        if (!session_) return false;
        return session_->input(buf);
    }
}}
