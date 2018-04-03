#include <base/file/stream.h>
#include <base/util/unicode.h>

namespace base { namespace file {

	stream::stream(const char* filename, std::ios_base::openmode mode)
	{
		file_.open(filename, std::ios::binary | mode);
	}

	stream::stream(const wchar_t* filename, std::ios_base::openmode mode)
	{
#if defined(_MSC_VER)
		file_.open(filename, std::ios::binary | mode);
#else
		file_.open(base::w2u(filename), std::ios::binary | mode);
#endif
	}

	stream::~stream()
	{
		file_.close();
	}

	bool stream::is_open() const
	{
		return !!file_;
	}

	write_stream::write_stream(const char* filename)
		: file_(filename, std::ios_base::out)
	{ }

	write_stream::write_stream(const std::string& filename)
		: file_(filename.c_str(), std::ios_base::out)
	{ }

	write_stream::write_stream(const wchar_t* filename)
		: file_(filename, std::ios_base::out)
	{ }

	write_stream::write_stream(const std::wstring& filename)
		: file_(filename.c_str(), std::ios_base::out)
	{ }

	read_stream::read_stream(const char* filename)
		: file_(filename, std::ios_base::in)
	{ }

	read_stream::read_stream(const std::string& filename)
		: file_(filename.c_str(), std::ios_base::in)
	{ }

	read_stream::read_stream(const wchar_t* filename)
		: file_(filename, std::ios_base::in)
	{ }

	read_stream::read_stream(const std::wstring& filename)
		: file_(filename.c_str(), std::ios_base::in)
	{ }
}}
