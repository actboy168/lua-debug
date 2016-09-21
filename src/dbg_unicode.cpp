#include "dbg_unicode.h"
#include <rapidjson/encodings.h>

namespace vscode
{
	template <class T>
	struct input_stream
	{
		typedef T Ch;
		input_stream(const T* t) : t_(t) { }
		T Take() { return *t_++; }
		const T* t_;
	};

	template <class T>
	struct output_stream
	{
		typedef typename T::value_type Ch;
		output_stream(T* t) : t_(t) { }
		void Put(Ch c) { t_->push_back(c); }
		T* t_;
	};


	std::wstring u2w(const char* str)
	{
		std::wstring wstr;
		output_stream<std::wstring> ostream(&wstr);
		for (input_stream<char> p(str);;)
		{
			unsigned codepoint = 0;
			bool suc = rapidjson::UTF8<>::Decode(p, &codepoint);
			if (suc && codepoint == 0)
				break;
			rapidjson::UTF16<>::Encode(ostream, suc ? codepoint : (unsigned)L"?");
		}
		return wstr;
	}

	std::wstring u2w(const std::string& str)
	{
		return u2w(str.c_str());
	}

	std::string w2u(const wchar_t* wstr)
	{
		std::string str;
		output_stream<std::string> ostream(&str);
		for (input_stream<wchar_t> p(wstr);;)
		{
			unsigned codepoint = 0;
			bool suc = rapidjson::UTF16<>::Decode(p, &codepoint);
			if (suc && codepoint == 0)
				break;
			rapidjson::UTF8<>::Encode(ostream, suc ? codepoint : (unsigned)"?");
		}
		return str;
	}

	std::string w2u(const std::wstring& wstr)
	{
		return w2u(wstr.c_str());
	}
}
