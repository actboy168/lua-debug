#pragma once
#ifdef WIN32
#else
#include <dlfcn.h>
#include <execinfo.h>
#endif
#ifdef __GNUC__
#include <cxxabi.h>
#endif
#ifdef __APPLE__
#include <TargetConditionals.h>
#endif
#include <bee/filesystem.h>
#include <bee/format.h>
#include <memory>
#include <unordered_map>
#include <mutex>


#if defined(TARGET_OS_MAC)
#include <unistd.h>
#endif

#ifdef __GNUC__
#define DEMANGLE_NAME 1
#endif

#ifdef TARGET_OS_MAC
#define USE_ATOS 1
#define USE_ADDR2LINE 0
#elif defined(__linux__)
#define USE_ATOS 0
#define USE_ADDR2LINE 1
#endif

static std::string demangle_name(const char* name){
#ifdef DEMANGLE_NAME
	int status = 0;
	std::unique_ptr<char, decltype(&free)> realname(abi::__cxa_demangle(name, 0, 0, &status), free);
	return realname.get();
#else
	return name;
#endif
}
static inline std::string shellcommand(const std::string& cmd){
	try
	{
		std::string result;
		FILE* pipe = popen(cmd.c_str(), "r");
		if (pipe) {
			char buffer[256];
			fgets(buffer, 256, pipe);
			result.append(buffer);
			if (pclose(pipe) != 0){
				return {};
			}
			return result;	
		}
	}
	catch(const std::bad_alloc& e)
	{
	}
	return {};
}
static inline bool which_proc(const char* shell){
	FILE* pipe = popen(shell, "r");
	if (!pipe) return false;
	return pclose(pipe) == 0;
}

#if USE_ATOS
static std::string get_function_atos(void* ptr){
	struct AtosInfo{
		pid_t pid;
		bool has_atos;
		AtosInfo(){
			pid = getpid();
			has_atos = which_proc("which atos");
		}
	};
	static AtosInfo atos_info;
	if (atos_info.has_atos) {
		auto funcinfo = shellcommand(std::format("atos -p {} {}", atos_info.pid, ptr));
		if (!(funcinfo[0] == '0' && funcinfo[1] == 'x')){
			return funcinfo;
		}
	}
	return {};
}
#endif

#if USE_ADDR2LINE
static std::string get_function_addr2line(const Dl_info& info){
	static bool has_address2line = which_proc("which addr2line");
	if (has_address2line){
		auto offset = info.dli_saddr - info.dli_fbase;
		auto funcinfo = shellcommand(std::format("addr2line {} -e {} -f -p -C -s", offset, info.dli_fname));
		if (!funcinfo.empty() && funcinfo != "??:0"){
			return funcinfo;
		}
	}
	return {};
}
#endif

static std::string get_functioninfo(void* ptr){
#ifdef WIN32
	// TODO: support Windows
	return {};
#else
	Dl_info info = {};
	if (dladdr(ptr, &info) == 0 || info.dli_saddr != ptr) {
		return {};
	}

	std::string funcinfo = 
#if USE_ATOS
		get_function_atos(info.dli_saddr);
#elif USE_ADDR2LINE
		get_function_addr2line(info);
#else
		{}
#endif
	if (!funcinfo.empty()){
		return funcinfo;
	}

	std::string filename = fs::path(info.dli_fname).filename();
	std::string realname = demangle_name(info.dli_sname);
	auto addr = info.dli_saddr;

	return std::format("`{}`{} {}", filename, realname, addr);
#endif
}

struct LuaCFunctionInfo {
	const std::string* const operator[] (void* ptr) {
		if (ptr == nullptr) return nullptr;
		std::lock_guard guard(mtx);
		{
			auto iter = infos.find(ptr);
			if (iter != infos.end()){
				return &(iter->second);
			}
		}
		try
		{
			auto [iter,success] = infos.emplace(ptr, get_functioninfo(ptr));
			if (!success)
				return nullptr;
			return &(iter->second);
		}
		catch(const std::bad_alloc& e)
		{
			return nullptr;
		}
	}

	std::unordered_map<void*, std::string> infos;
	std::mutex mtx;
};

