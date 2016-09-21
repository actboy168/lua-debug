#pragma once

#include <string>

namespace vscode
{
	std::wstring u2w(const char* str);
	std::wstring u2w(const std::string& str);
	std::string w2u(const wchar_t* wstr);
	std::string w2u(const std::wstring& wstr);
}
