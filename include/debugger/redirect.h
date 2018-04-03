#pragma once

namespace vscode
{
	enum class std_fd {
		STDIN = 0,
		STDOUT,
		STDERR,
	};

	struct redirect_pipe {
		void* rd_;
		void* wr_;

		redirect_pipe();
		~redirect_pipe();
		bool create(const char* name);
	};

	class redirector
	{
	public:
		redirector();
		~redirector();

		void   open(const char* name, std_fd type);
		void   close();
		size_t peek();
		size_t read(char* buf, size_t len);

	private:
		redirect_pipe pipe_;
		void* old_;
		std_fd type_;
	};
}
