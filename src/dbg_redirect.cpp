#include "dbg_redirect.h" 
#include "dbg_format.h"
#include <windows.h> 
#include <fcntl.h>
#include <io.h>

namespace vscode
{
	static bool redirect_handle(const char* name, FILE* handle, HANDLE& pipe_rd, HANDLE& pipe_wr)
	{
		std::wstring pipe_name = format(L"\\\\.\\pipe\\%s-redirector-%d", name, GetCurrentProcessId());
		SECURITY_ATTRIBUTES read_attr = { sizeof(SECURITY_ATTRIBUTES), 0, false };
		HANDLE rd = ::CreateNamedPipeW(
			pipe_name.c_str()
			, PIPE_ACCESS_INBOUND | FILE_FLAG_OVERLAPPED
			, PIPE_TYPE_BYTE | PIPE_WAIT
			, 1, 0, 1024 * 1024, 0, &read_attr
			);
		if (rd == INVALID_HANDLE_VALUE)  {
			return false;
		}
		SECURITY_ATTRIBUTES write_attr = { sizeof(SECURITY_ATTRIBUTES), 0, true };
		HANDLE wr = ::CreateFileW(
			pipe_name.c_str()
			, GENERIC_WRITE
			, 0, &write_attr
			, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, NULL
			);
		if (wr == INVALID_HANDLE_VALUE)  {
			::CloseHandle(rd);
			return false;
		}
		::ConnectNamedPipe(rd, NULL);
		SetStdHandle(STD_OUTPUT_HANDLE, wr);
		int fd = _open_osfhandle((intptr_t)wr, _O_WRONLY | _O_TEXT);
		FILE *fp = _fdopen(fd, "w");
		*handle = *fp;
		setvbuf(handle, NULL, _IONBF, 0);
		pipe_rd = rd;
		pipe_wr = wr;
		return true;
	}

	redirector::redirector()
		: out_rd_(INVALID_HANDLE_VALUE)
		, out_wr_(INVALID_HANDLE_VALUE)
		, err_rd_(INVALID_HANDLE_VALUE)
		, err_wr_(INVALID_HANDLE_VALUE)
	{
	}

	redirector::~redirector()
	{
		close();
	}

	void redirector::open()
	{
		redirect_handle("stdout", stdout, out_rd_, out_wr_);
		redirect_handle("stderr", stderr, err_rd_, err_wr_);
	}

	void redirector::close()
	{
		CloseHandle(out_rd_);
		CloseHandle(out_wr_);
		CloseHandle(err_rd_);
		CloseHandle(err_wr_);
		out_rd_ = INVALID_HANDLE_VALUE;
		out_wr_ = INVALID_HANDLE_VALUE;
		err_rd_ = INVALID_HANDLE_VALUE;
		err_wr_ = INVALID_HANDLE_VALUE;
	}

	size_t redirector::peek_stdout()
	{
		DWORD rlen = 0;
		if (!PeekNamedPipe(out_rd_, 0, 0, 0, &rlen, 0))
		{
			return 0;
		}
		return rlen;
	}

	size_t redirector::peek_stderr()
	{
		DWORD rlen = 0;
		if (!PeekNamedPipe(err_rd_, 0, 0, 0, &rlen, 0))
		{
			return 0;
		}
		return rlen;
	}

	size_t redirector::read_stdout(char* buf, size_t len)
	{
		if (!peek_stdout())
			return 0;
		DWORD rlen = 0;
		if (!ReadFile(out_rd_, buf, len, &rlen, 0))
		{
			return 0;
		}
		return rlen;
	}

	size_t redirector::read_stderr(char* buf, size_t len)
	{
		if (!peek_stderr())
			return 0;
		DWORD rlen = 0;
		if (!PeekNamedPipe(err_rd_, buf, len, &rlen, 0, 0))
		{
			return 0;
		}
		return rlen;
	}
}

