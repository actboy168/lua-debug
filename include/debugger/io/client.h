#pragma once

#include <debugger/io/server.h>

namespace vscode { namespace io {
	class client : public session
	{
	public:
        client(const bee::net::endpoint& ep);
		virtual ~client();
        void update(int ms);
        void event_connect(bee::net::socket::fd_t fd);
        void close();

    private:
        bee::net::tcp::connecter_t<bee::net::poller_t> connecter;
	};
}}
