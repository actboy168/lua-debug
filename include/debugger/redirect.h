#pragma once

#if defined(_WIN32)

#include <stddef.h>

namespace vscode {
	enum class std_fd {
		STDIN = 0,
		STDOUT,
		STDERR,
	};

	class redirector {
	public:
		redirector();
		~redirector();

		void   open(std_fd type);
		void   close();
		size_t peek();
		size_t read(char* buf, size_t len);

	private:
        void*  m_rd;
        void*  m_wr;
		void*  m_old;
		std_fd m_type;
	};
}
#endif
