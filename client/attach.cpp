#include "attach.h"
#include "stdinput.h"
#include "dbg_format.h"

attach_client::attach_client(stdinput& io_)
	: poller()
	, io(io_)
	, base_type(&poller)
{
	net::socket::initialize();
}

bool attach_client::event_in()
{
	if (!base_type::event_in())
		return false;
	std::vector<char> tmp(base_type::recv_size());
	size_t len = base_type::recv(tmp.data(), tmp.size());
	if (len == 0)
		return true;
	io.output(tmp.data(), len);
	return true;
}

void attach_client::send(const vscode::rprotocol& rp)
{
	rapidjson::StringBuffer buffer;
	rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
	rp.Accept(writer);
	base_type::send(vscode::format("Content-Length: %d\r\n\r\n", buffer.GetSize()));
	base_type::send(buffer.GetString(), buffer.GetSize());
}

void attach_client::event_close()
{
	base_type::event_close();
	exit(0);
}

void attach_client::update()
{
	poller.wait(1000, 0);
}
