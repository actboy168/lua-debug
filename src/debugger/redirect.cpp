#if defined(_WIN32)

#include <debugger/redirect.h>
#include <windows.h>
#include <fcntl.h>
#include <io.h>
#include <stdio.h>

namespace vscode {
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

	static void set_handle(std_fd type, HANDLE handle) {
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
        : m_rd(INVALID_HANDLE_VALUE)
        , m_wr(INVALID_HANDLE_VALUE)
		, m_old(INVALID_HANDLE_VALUE)
		, m_type(std_fd::STDOUT)
	{ }

	redirector::~redirector() {
		close();
	}

	void redirector::open(std_fd type) {
        SECURITY_ATTRIBUTES attr = { sizeof(SECURITY_ATTRIBUTES), 0, true };
		if (!::CreatePipe(&m_rd, &m_wr, &attr, 0)) {
			return ;
		}
		m_type = type;
		m_old = GetStdHandle(handles[(int)type]);
        if (type == std_fd::STDIN) {
            set_handle(type, m_rd);
            m_rd = INVALID_HANDLE_VALUE;
        }
        else {
            set_handle(type, m_wr);
            m_wr = INVALID_HANDLE_VALUE;
        }
	}

	void redirector::close() {
		if (m_old != INVALID_HANDLE_VALUE) {
            SetStdHandle(handles[(int)m_type], m_old);
		}
		m_old = INVALID_HANDLE_VALUE;

        if (m_rd != INVALID_HANDLE_VALUE) {
            ::CloseHandle(m_rd);
        }
        m_rd = INVALID_HANDLE_VALUE;

        if (m_wr != INVALID_HANDLE_VALUE) {
            ::CloseHandle(m_wr);
        }
        m_wr = INVALID_HANDLE_VALUE;
	}

	size_t redirector::peek() {
		DWORD rlen = 0;
		if (!PeekNamedPipe(m_rd, 0, 0, 0, &rlen, 0)) {
			return 0;
		}
		return rlen;
	}

	size_t redirector::read(char* buf, size_t len) {
        if (!peek()) {
            return 0;
        }
		DWORD rlen = 0;
		if (!ReadFile(m_rd, buf, static_cast<DWORD>(len), &rlen, 0)) {
			return 0;
		}
		return rlen;
	}
}

#endif
