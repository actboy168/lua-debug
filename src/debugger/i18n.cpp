#include <debugger/impl.h>
#include <map>
#include <string_view>

#pragma execution_character_set("utf-8")

namespace vscode {
	static std::map<std::string_view, translator_t> translators = {
		{ "zh-cn",{
			{ "All Exceptions", "所有异常" },
			{ "Uncaught Exceptions", "未捕获异常" },
		} }
	};

	void debugger_impl::setlang(const std::string_view& locale) {
		auto it = translators.find(locale);
		translator_ = it == translators.end() ? &it->second : nullptr;
	}

	std::string_view debugger_impl::LANG(const std::string_view& text) {
		if (!translator_) return text;
		auto it = translator_->find(text);
		if (it == translator_->end()) return text;
		return it->second;
	}
}