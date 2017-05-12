#pragma once

#include <net/tcp/connecter.h>
#include <net/poller.h>
#include "dbg_protocol.h"	

class proxy_client
	: public net::tcp::connecter
{
	typedef net::tcp::connecter base_type;
public:
	proxy_client();
	bool event_in();
	void send(const vscode::rprotocol& rp);
	void event_close();
	void update();

private:
	net::poller_t poller;
};
