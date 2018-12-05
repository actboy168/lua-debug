#include <debugger/client/get_unix_path.h>
#include <bee/utility/path_helper.h>
#include <bee/utility/format.h>
#include <filesystem>

std::string get_unix_path(int pid) {
	auto path = bee::path_helper::dll_path().value().parent_path().parent_path() / "tmp";
	std::filesystem::create_directories(path);
	return bee::w2u((path / bee::format("pid_%d.tmp", pid)).wstring());
}
