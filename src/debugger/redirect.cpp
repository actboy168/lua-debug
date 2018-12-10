#if defined(_WIN32)

#include <debugger/redirect.h>
#include <windows.h>
#include <fcntl.h>
#include <io.h>
#include <stdio.h>

namespace vscode
{
	static DWORD handles[] = {
		STD_INPUT_HANDLE,
		STD_OUTPUT_HANDLE,
		STD_ERROR_HANDLE,
	};

	static FILE* files[] = {
		stdin,
		stdout,
		stderr,
	};

	redirect_pipe::redirect_pipe()
		: rd_(INVALID_HANDLE_VALUE)
		, wr_(INVALID_HANDLE_VALUE)
	{ }
	  
	redirect_pipe::~redirect_pipe() {
        close_rd();
        close_wr();
	}
    void redirect_pipe::close_rd() {
        if (rd_ != INVALID_HANDLE_VALUE) {
            ::CloseHandle(rd_);
        }
        rd_ = INVALID_HANDLE_VALUE;
    }
    void redirect_pipe::close_wr() {
        if (wr_ != INVALID_HANDLE_VALUE) {
            ::CloseHandle(wr_);
        }
        wr_ = INVALID_HANDLE_VALUE;
    }

	bool redirect_pipe::create(const char* name)
	{
		SECURITY_ATTRIBUTES attr = { sizeof(SECURITY_ATTRIBUTES), 0, true };
		return !!::CreatePipe(&rd_, &wr_, &attr, 0);
	}

	static void set_handle(std_fd type, HANDLE handle)
	{
		SetStdHandle(handles[(int)type], handle);
		int fd = _open_osfhandle((intptr_t)handle, type == std_fd::STDIN ? _O_RDONLY : _O_WRONLY);
		if (fd < 0) {
			return;
		}
		FILE *fp = _fdopen(fd, type == std_fd::STDIN ? "r": "w");
		if (fp) {
			_dup2(_fileno(fp), _fileno(files[(int)type]));
		}
	}

	redirector::redirector()
		: pipe_()
		, old_(INVALID_HANDLE_VALUE)
		, type_(std_fd::STDOUT)
	{
	}

	redirector::~redirector()
	{
		close();
	}

	void redirector::open(const char* name, std_fd type)
	{
		if (!pipe_.create(name)) {
			return ;
		}
		type_ = type;
		old_ = GetStdHandle(handles[(int)type]);
		set_handle(type, type == std_fd::STDIN ? pipe_.rd_: pipe_.wr_);
        if (type == std_fd::STDIN) {
            pipe_.rd_ = INVALID_HANDLE_VALUE;
        }
        else {
            pipe_.wr_ = INVALID_HANDLE_VALUE;
        }
	}

	void redirector::close()
	{
		if (old_ != INVALID_HANDLE_VALUE)
		{
            SetStdHandle(handles[(int)type_], old_);
		}
		old_ = INVALID_HANDLE_VALUE;
        pipe_.close_rd();
        pipe_.close_wr();
	}

	size_t redirector::peek()
	{
		DWORD rlen = 0;
		if (!PeekNamedPipe(pipe_.rd_, 0, 0, 0, &rlen, 0))
		{
			return 0;
		}
		return rlen;
	}

	size_t redirector::read(char* buf, size_t len)
	{
		if (!peek())
			return 0;
		DWORD rlen = 0;
		if (!ReadFile(pipe_.rd_, buf, static_cast<DWORD>(len), &rlen, 0))
		{
			return 0;
		}
		return rlen;
	}
}

#endif
