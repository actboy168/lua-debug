#include <debugger/client/tcp_attach.h>
#include <debugger/client/stdinput.h>
#include <bee/utility/format.h>

tcp_attach::tcp_attach(stdinput& io_)
	: poller()
	, io(io_)
	, base_type(&poller)
{
	bee::net::socket::initialize();
}

bool tcp_attach::event_in()
{
	if (!base_type::event_in())
		return false;
	std::vector<char> tmp(base_type::recv_size());
	size_t len = base_type::recv(tmp.data(), tmp.size());
	if (len == 0)
		return true;
	io.raw_output(tmp.data(), len);
	return true;
}

void tcp_attach::send(const std::string& rp)
{
	base_type::send(bee::format("Content-Length: %d\r\n\r\n", rp.size()));
	base_type::send(rp.data(), rp.size());
}

void tcp_attach::send(const vscode::rprotocol& rp)
{
	rapidjson::StringBuffer buffer;
	rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
	rp.Accept(writer);
	base_type::send(bee::format("Content-Length: %d\r\n\r\n", buffer.GetSize()));
	base_type::send(buffer.GetString(), buffer.GetSize());
}

void tcp_attach::event_close()
{
	base_type::event_close();
	exit(0);
}

void tcp_attach::update()
{
	poller.wait(0);
	std::string buf;
	while (io.input(buf)) {
		send(buf);
	}
}
