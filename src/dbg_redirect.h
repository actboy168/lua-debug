#pragma once

namespace vscode
{
	class redirector
	{
	public:
		redirector();
		~redirector();

		void   open();
		void   close();
		size_t peek_stdout();
		size_t peek_stderr();
		size_t read_stdout(char* buf, size_t len);
		size_t read_stderr(char* buf, size_t len);

	private:
		void* out_rd_;
		void* out_wr_;
		void* err_rd_;
		void* err_wr_;
	};
}
