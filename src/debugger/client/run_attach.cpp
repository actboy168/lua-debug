#include <debugger/io/namedpipe.h>
#include <debugger/client/run.h>
#include <debugger/io/base.h>
#include <debugger/io/helper.h>
#include <debugger/client/stdinput.h>
#include <base/win/process_switch.h>
#include <base/hook/injectdll.h>

void event_terminated(stdinput& io);
void event_exited(stdinput& io, uint32_t exitcode);

static void sleep() {
	std::this_thread::sleep_for(std::chrono::milliseconds(10));
}

base::subprocess::process openprocess(int pid) {
	PROCESS_INFORMATION info = { 0 };
	if (!base::hook::openprocess(pid, PROCESS_QUERY_INFORMATION, 0, info)) {
		assert(false);
	}
	return base::subprocess::process(info);
}


bool run_pipe_attach(stdinput& io, vscode::rprotocol& init, vscode::rprotocol& req, const std::wstring& pipename, process_opt& process, base::win::process_switch* m)
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
					process = openprocess(body["processId"].GetInt());
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
	if (process && !(*process).is_running()) {
		event_terminated(io);
		event_exited(io, (*process).wait());
	}
	return true;
}
