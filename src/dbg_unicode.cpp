#include "dbg_unicode.h"
#include <rapidjson/encodings.h>
#include <Windows.h>
#include <vector>

namespace base
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
			rapidjson::UTF16<>::Encode(ostream, suc ? codepoint : (unsigned int)L'?');
		}
		return wstr;
	}

	std::wstring u2w(const strview& str)
	{
		return u2w(str.data());
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
			rapidjson::UTF8<>::Encode(ostream, suc ? codepoint : (unsigned int)'?');
		}
		return str;
	}

	std::string w2u(const wstrview& wstr)
	{
		return w2u(wstr.data());
	}

	std::wstring a2w(const strview& str)
	{
		if (str.empty())
		{
			return L"";
		}
		int wlen = ::MultiByteToWideChar(CP_ACP, 0, str.data(), static_cast<int>(str.size()), NULL, 0);
		if (wlen <= 0)
		{
			return L"";
		}
		std::vector<wchar_t> result(wlen);
		::MultiByteToWideChar(CP_ACP, 0, str.data(), static_cast<int>(str.size()), result.data(), wlen);
		return std::wstring(result.data(), result.size());
	}

	std::string w2a(const wstrview& wstr)
	{
		if (wstr.empty())
		{
			return "";
		}
		int len = ::WideCharToMultiByte(CP_ACP, 0, wstr.data(), static_cast<int>(wstr.size()), NULL, 0, 0, 0);
		if (len <= 0)
		{
			return "";
		}
		std::vector<char> result(len);
		::WideCharToMultiByte(CP_ACP, 0, wstr.data(), static_cast<int>(wstr.size()), result.data(), len, 0, 0);
		return std::string(result.data(), result.size());
	}

	std::string a2u(const strview& str)
	{
		return w2u(a2w(str));
	}

	std::string u2a(const strview& str)
	{
		return w2a(u2w(str));
	}
}
