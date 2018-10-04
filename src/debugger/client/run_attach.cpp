#include <debugger/io/namedpipe.h>
#include <debugger/client/run.h>
#include <debugger/io/base.h>
#include <debugger/io/helper.h>
#include <debugger/client/stdinput.h>
#include <base/win/process_switch.h>

void event_terminated(stdinput& io);
void event_exited(stdinput& io, uint32_t exitcode);

static void sleep() {
	std::this_thread::sleep_for(std::chrono::milliseconds(10));
}

bool run_pipe_attach(stdinput& io, vscode::rprotocol& init, vscode::rprotocol& req, const std::wstring& pipename, base::win::process& process, base::win::process_switch* m)
{
	vscode::io::namedpipe pipe;
	if (!pipe.open_client(pipename, 60000)) {
		return false;
	}
	io_output(&pipe, init);
	io_output(&pipe, req);
	for (; !pipe.is_closed(); sleep()) {
		io.update(10);
		pipe.update(10);
		std::string buf;

		for (;;) {
			vscode::rprotocol req = vscode::io_input(&io);
			if (req.IsNull()) {
				break;
			}
			if (req["type"] == "response" && req["command"] == "runInTerminal") {
				auto& body = req["body"];
				if (body.HasMember("processId") && body["processId"].IsInt()) {
					int pid = body["processId"].GetInt();
					process = base::win::process(pid);
				}
			}
			else {
				io_output(&pipe, req);
			}
		}
		while (pipe.input(buf)) {
			io.output(buf.data(), buf.size());
			if (m) {
				m->unlock();
				m = nullptr;
			}
		}
	}
	if (!process.is_running()) {
		event_terminated(io);
		event_exited(io, process.wait());
	}
	return true;
}
