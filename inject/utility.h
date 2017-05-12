#pragma once

#include <base/filesystem.h>

fs::path get_self_path();

template <typename CharT, size_t MaxSize>
class cstring_t {
public:
	cstring_t()
		: m_str()
		, m_len(0)
	{ }

	const CharT* data() const { return m_str; }
	size_t size() const { return m_len; }
	bool assign(const CharT* str, size_t len) {
		if (len + 1 > MaxSize) { return false; }
		memcpy(m_str, str, len);
		m_str[len] = L'\0';
		m_len = len;
		return true;
	}
private:
	CharT  m_str[MaxSize];
	size_t m_len = 0;
};

template <class T>
struct eat_helper
{
	static uintptr_t real;
	static void init(const wchar_t* module_name, const char* api_name)
	{
		real = base::hook::eat(module_name, api_name, (uintptr_t)T::fake);
	}
};
template <class T>
uintptr_t eat_helper<T>::real = 0;
