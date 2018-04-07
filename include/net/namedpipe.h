#pragma once

#include <Windows.h>
#include <string>

namespace net {
	class namedpipe {
	public:
		namedpipe();
		virtual ~namedpipe();
		bool open_server(std::wstring const& pipename);
		bool open_client(std::wstring const& pipename, int timeout);
		size_t peek() const;
		bool recv(char* str, size_t& n);
		bool send(const char* str, size_t& n) const;
		void close();
		std::wstring make_name(std::wstring const& pipename);

	private:
		HANDLE pipefd;
		bool   is_open;
		bool   is_bind;
	};

	namedpipe::namedpipe()
		: is_open(false)
		, is_bind(false)
		, pipefd(INVALID_HANDLE_VALUE)
	{ }

	namedpipe::~namedpipe() {
		close();
	}

	std::wstring namedpipe::make_name(std::wstring const& pipename)
	{
		if (pipename.find(L"\\") != 0) {
			return std::wstring(L"\\\\.\\pipe\\") + pipename;
		}
		return pipename;
	}

	bool namedpipe::open_server(std::wstring const& pipename) {
		std::wstring name = make_name(pipename);
		pipefd = CreateNamedPipeW(name.c_str(), PIPE_ACCESS_DUPLEX, PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT, PIPE_UNLIMITED_INSTANCES, 2048, 2048, 0, NULL);
		if (pipefd == INVALID_HANDLE_VALUE) {
			return false;
		}
		is_bind = is_open = ConnectNamedPipe(pipefd, NULL) ? true : (GetLastError() == ERROR_PIPE_CONNECTED);
		return is_open;
	}

	bool namedpipe::open_client(std::wstring const& pipename, int timeout) {
		std::wstring name = make_name(pipename);
		for (uint64_t endtick = timeout + GetTickCount64();  GetTickCount64() < endtick;) {
			pipefd = CreateFileW(name.c_str(), GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
			if (pipefd != INVALID_HANDLE_VALUE) {
				is_open = true;
				break;
			}
			if (GetLastError() == ERROR_PIPE_BUSY) {
				if (!WaitNamedPipeW(name.c_str(), 20000)) {
					is_open = false;
					return false;
				}
			}
			else {
				Sleep(1000);
			}
		}
		if (is_open) {
			DWORD mode = PIPE_READMODE_MESSAGE;
			is_open = !!SetNamedPipeHandleState(pipefd, &mode, NULL, NULL);
		}
		return is_open;
	}

	size_t namedpipe::peek() const {
		DWORD rlen = 0;
		if (!PeekNamedPipe(pipefd, NULL, 0, NULL, &rlen, NULL)) {
			return 0;
		}
		return (size_t)rlen;
	}

	bool namedpipe::recv(char* buf, size_t& len) {
		DWORD rlen = 0;
		if (!ReadFile(pipefd, buf, (DWORD)len, &rlen, NULL)) {
			return false;
		}
		len = rlen;
		return true;
	}

	bool namedpipe::send(const char* buf, size_t& len) const {
		DWORD wlen = 0;
		if (!WriteFile(pipefd, buf, (DWORD)len, &wlen, NULL)) {
			return false;
		}
		len = wlen;
		return true;
	}

	void namedpipe::close() {
		if (is_open) {
			FlushFileBuffers(pipefd);
			if (is_bind) {
				DisconnectNamedPipe(pipefd);
			}
		}
		if (pipefd != INVALID_HANDLE_VALUE) {
			CloseHandle(pipefd);
		}
		is_open = is_bind = false;
		pipefd = INVALID_HANDLE_VALUE;
	}
}