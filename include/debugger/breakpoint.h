#pragma once

#include <map>
#include <string>
#include <base/util/hybrid_array.h> 
#include <debugger/pathconvert.h>
#include <debugger/protocol.h>

struct lua_State;
namespace lua { struct Debug; }

namespace vscode
{
	template <class T> struct path_less;
	template <> struct path_less<char> {
		static char sep(char c)
		{
			return c == '/' ? '\\' : c;
		}
		bool operator()(const char& lft, const char& rht) const
		{
			return (tolower((int)(unsigned char)(sep(lft))) < tolower((int)(unsigned char)(sep(rht))));
		}
	};
	template <> struct path_less<std::string> {
		bool operator()(const std::string& lft, const std::string& rht) const
		{
			return std::lexicographical_compare(lft.begin(), lft.end(), rht.begin(), rht.end(), path_less<char>());
		}
	};

	struct bp {
		std::string cond;
		std::string hitcond;
		std::string log;
		int hit;
		bp(rapidjson::Value const& info, int h);

	};
	typedef std::map<size_t, bp> bp_source;

	class breakpoint
	{
	public:
		breakpoint();
		void clear();
		void clear(const std::string& client_path);
		void clear(intptr_t source_ref);
		void add(const std::string& client_path, size_t line, rapidjson::Value const& bp);
		void add(intptr_t source_ref, size_t line, rapidjson::Value const& bp);
		bool has(size_t line) const;
		bool has(bp_source* src, size_t line, lua_State* L, lua::Debug* ar) const;
		bp_source* get(const char* source, pathconvert& pathconvert);

	private:
		void clear(bp_source& src);
		void add(bp_source& src, size_t line, rapidjson::Value const& bp);
		bool evaluate_isok(lua_State* L, lua::Debug *ar, const std::string& script) const;

	private:
		std::map<std::string, bp_source, path_less<std::string>> files_;
		std::map<intptr_t, bp_source>    memorys_;
		base::hybrid_array<size_t, 1024> fast_table_;
	};
}
