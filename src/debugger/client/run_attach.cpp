#include <debugger/io/namedpipe.h>
#include <debugger/client/run.h>
#include <debugger/io/base.h>
#include <debugger/io/helper.h>
#include <debugger/client/stdinput.h>

static void sleep() {
	std::this_thread::sleep_for(std::chrono::milliseconds(10));
}

bool run_pipe_attach(stdinput& io, vscode::rprotocol& init, vscode::rprotocol& req, const std::wstring& pipename)
{
	vscode::io::namedpipe pipe;
	if (!pipe.open_client(pipename, 60000)) {
		return false;
	}
	io_output(&pipe, init);
	io_output(&pipe, req);
	for (;; sleep()) {
		io.update(10);
		pipe.update(10);
		std::string buf;
		while (io.input(buf)) {
			pipe.output(buf.data(), buf.size());
		}
		while (pipe.input(buf)) {
			io.output(buf.data(), buf.size());
		}
	}
	return true;
}
