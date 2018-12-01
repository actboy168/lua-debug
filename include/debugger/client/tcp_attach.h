#pragma once

#include <net/tcp/connecter.h>
#include <net/poller.h>
#include <debugger/protocol.h>

class stdinput;

class tcp_attach
	: public bee::net::tcp::connecter
{
	typedef bee::net::tcp::connecter base_type;
public:
	tcp_attach(stdinput& io);
	bool event_in();
	void send(const vscode::rprotocol& rp);
	void send(const std::string& rp);
	void event_close();
	void update();

private:
	bee::net::poller_t poller;
	stdinput&     io;
};
