#pragma once 

#include <string>

namespace vscode { namespace path {
	std::string normalize(const std::string& path);
	std::string filename(const std::string& path);
	int tochar(char c);

	template <class T> struct less;
	template <> struct less<char> {
		bool operator()(const char& lft, const char& rht) const {
			return tochar(lft) < tochar(rht);
		}
	};
	template <> struct less<std::string> {
		bool operator()(const std::string& lft, const std::string& rht) const {
			return std::lexicographical_compare(lft.begin(), lft.end(), rht.begin(), rht.end(), less<char>());
		}
	};
}}
