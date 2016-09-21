#include "dbg_network.h"
#include "dbg_message.h"

namespace vscode
{
	network::network(const char* ip, uint16_t port)
		: poller_(new net::poller_t)
		, server_(new server(poller_.get()))
	{
		net::socket::initialize();
		server_->listen(net::endpoint(ip, port));
	}

	void network::update(int ms)
	{
		poller_->wait(1000, ms);
	}

	bool network::output(const wprotocol& wp)
	{
		if (!server_)
			return false;
		if (!server_->output(wp))
			return false;
		return true;
	}

	rprotocol network::input()
	{
		if (!server_)
			return rprotocol();
		return server_->input();
	}

	bool network::input_empty() const
	{
		if (!server_)
			return false;
		return server_->input_empty();
	}

	void network::set_schema(const char* file)
	{
		if (!server_)
			return;
		return server_->set_schema(file);
	}
}
