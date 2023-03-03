#include <autoattach/autoattach.h>
#include <util/log.h>

#include <bee/nonstd/filesystem.h>
#include <bee/utility/path_helper.h>
#ifndef _WIN32
#include <unistd.h>
#define DLLEXPORT __attribute__((visibility("default")))
#define DLLEXPORT_DECLARATION
#else
#define DLLEXPORT __declspec(dllexport)
#define DLLEXPORT_DECLARATION __cdecl
#endif
#include <string>
#include <atomic>

namespace luadebug::autoattach {
	static std::string readfile(const fs::path& filename) {
	#ifdef _WIN32
		FILE* f = _wfopen(filename.c_str(), L"rb");
	#else
		FILE* f = fopen(filename.c_str(), "rb");
	#endif
		if (!f) {
			return std::string();
		}
		fseek (f, 0, SEEK_END);
		long length = ftell(f);
		fseek(f, 0, SEEK_SET);
		std::string tmp;
		tmp.resize(length);
		fread(tmp.data(), 1, length, f);
		fclose(f);
		return tmp;
	}

	static int attach(lua::state L) {
		log::info("attach lua vm entry");
		auto r = bee::path_helper::dll_path();
		if (!r) {
			return -1;
		}
		auto root = r.value().parent_path().parent_path();
		auto buf = readfile(root / "script" / "attach.lua");
		if (lua::loadbuffer(L, buf.data(), buf.size(), "=(attach.lua)")) {
			/*
			TODO:
				attach.lua编译失败，这里应该用log::fatal将错误信息返回前端。
				但需要先将autoattach中止。
			*/
			log::error(lua::tostring(L, -1));
			lua::pop(L, 1);
			return -1;
		}
		lua::call<lua_pushstring>(L, root.generic_u8string().c_str());
		
	#ifdef _WIN32
		lua::call<lua_pushstring>(L, std::to_string(GetCurrentProcessId()).c_str());
	#else
		lua::call<lua_pushstring>(L, std::to_string(getpid()).c_str());
	#endif
		if (lua::pcall(L, 2, 0, 0)) {
			/*
				这里失败无法调用log::fatal，因为无法知道调试器已经加载到哪一步才失败的。
				所以调试器不应该把错误抛到这里。
			*/
			log::error(lua::tostring(L, -1));
			lua::pop(L, 1);
            return -2;
		}
        auto ec = lua::call<lua_toboolean>(L, -1) ? 0 : 1;
        lua::pop(L, 1);
        return ec;
	}
	static void start(bool ap) {
		static std::atomic_bool injected;
		bool test = false;
		if (injected.compare_exchange_strong(test, true, std::memory_order_acquire)) {
			luadebug::autoattach::initialize(luadebug::autoattach::attach, ap);
			injected.store(false, std::memory_order_release);
		}
	}
}


extern "C" {
DLLEXPORT void DLLEXPORT_DECLARATION launch() {
	luadebug::autoattach::start(false);
}

DLLEXPORT void DLLEXPORT_DECLARATION attach() {
	luadebug::autoattach::start(true);
}
}
