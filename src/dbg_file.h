#pragma once

#include <fstream>	
#if defined(_MSC_VER)	   
#include "dbg_unicode.h"
#endif

namespace vscode
{
	class file
	{
	public:
		file(const char* filename, std::ios_base::openmode mode)
		{
#if defined(_MSC_VER)	   
			file_.open(base::u2w(filename), std::ios::binary | mode);
#else		 
			file_.open(filename, std::ios::binary | mode);
#endif
		}

		bool is_open() const
		{
			return !!file_;
		}

		template <class SequenceT>
		SequenceT read()
		{
			return std::move(SequenceT((std::istreambuf_iterator<char>(file_)), (std::istreambuf_iterator<char>())));
		}

		template <class SequenceT>
		void write(SequenceT buf)
		{
			std::copy(buf.begin(), buf.end(), std::ostreambuf_iterator<char>(file_));
		}

	private:
		file();
		file(file&);
		file(file&&);
		file& operator=(file&);
		file& operator=(file&&);

	private:
		std::fstream file_;
	};
}
