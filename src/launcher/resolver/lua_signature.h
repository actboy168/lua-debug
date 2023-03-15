#include <string>

namespace luadebug::autoattach {
	struct signture {
		std::string name;
		int32_t start_offset;
		int32_t end_offset;
		std::string pattern;
		int32_t pattern_offset;
	};
}