#pragma once
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX 1
#endif
#include <windows.h>

#include <DbgHelp.h>
#include <Psapi.h>
#pragma comment(lib, "dbghelp.lib")
#pragma comment(lib, "psapi.lib")

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
#include <optional>
#include <string>
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

namespace NativeInfo {
#ifdef _WIN32
	struct SymHandler {
		HANDLE hProcess = NULL;

		~SymHandler() {
			SymCleanup(hProcess);
			hProcess = NULL;
		}
		operator bool() const noexcept {
			return hProcess != NULL;
		}
	};
	inline std::string searchpath(bool SymBuildPath, bool SymUseSymSrv) {
		// Build the sym-path:
		if (SymBuildPath)
		{
			std::string searchpath;
			searchpath.reserve(4096);
			searchpath.append(".;");
			std::error_code ec;
			auto current_path = std::filesystem::current_path(ec);
			if (!ec) {
				searchpath.append(current_path.string().c_str());
				searchpath += ';';
			}
			const size_t nTempLen = 1024;
			char         szTemp[nTempLen];


			// Now add the path for the main-module:
			if (GetModuleFileNameA(NULL, szTemp, nTempLen) > 0)
			{
				std::filesystem::path path(szTemp);
				searchpath.append(path.parent_path().string());
				searchpath += ';';
			}
			if (GetEnvironmentVariableA("_NT_SYMBOL_PATH", szTemp, nTempLen) > 0)
			{
				szTemp[nTempLen - 1] = 0;
				searchpath.append(szTemp);
				searchpath += ';';
			}
			if (GetEnvironmentVariableA("_NT_ALTERNATE_SYMBOL_PATH", szTemp, nTempLen) > 0)
			{
				szTemp[nTempLen - 1] = 0;
				searchpath.append(szTemp);
				searchpath += ';';
			}
			if (GetEnvironmentVariableA("SYSTEMROOT", szTemp, nTempLen) > 0)
			{
				szTemp[nTempLen - 1] = 0;
				searchpath.append(szTemp);
				searchpath += ';';
				searchpath.append(szTemp);
				searchpath.append("\\system32;");
			}

			if (SymUseSymSrv)
			{
				if (GetEnvironmentVariableA("SYSTEMDRIVE", szTemp, nTempLen) > 0)
				{
					szTemp[nTempLen - 1] = 0;
					searchpath.append("SRV*");
					searchpath.append(szTemp);
					searchpath.append("\\websymbols*https://msdl.microsoft.com/download/symbols;");
				}
				else
					searchpath.append("SRV*c:\\websymbols*https://msdl.microsoft.com/download/symbols;");
			}
			return searchpath;
		} // if SymBuildPath
		return {};
	}

	inline HANDLE createSymHandler(bool SymBuildPath, bool SymUseSymSrv) {
		HANDLE proc = GetCurrentProcess();
		auto path = searchpath(SymBuildPath, SymUseSymSrv);
		if (!SymInitialize(proc, path.c_str(), TRUE)) {
			if (GetLastError() != 87) {
				return nullptr;
			}
		}
		DWORD symOptions = SymGetOptions(); // SymGetOptions
		symOptions |= SYMOPT_LOAD_LINES;
		symOptions |= SYMOPT_FAIL_CRITICAL_ERRORS;
		symOptions = SymSetOptions(symOptions);
		return proc;
	}

	inline SymHandler GetSymHandler() {
		static SymHandler handler{ createSymHandler(true, true) };
		return handler;
	}

	struct Symbol {
		std::string name;
		int32_t line_no;
		std::string file_name;
		std::string module_name;
	};

	inline std::optional<Symbol> Addr2Symbol(void* pObject)
	{
		static constexpr size_t max_sym_name =
#ifdef MAX_SYM_NAME
			MAX_SYM_NAME;
#else
			2000;
#endif // DEBUG
		struct MY_SYMBOL_INFO :SYMBOL_INFO
		{
			char name_buffer[MAX_SYM_NAME];
		};

		auto handler = GetSymHandler();
		if (!handler) {
			return std::nullopt;
		}
		using PTR_T = 
#ifdef _WIN64
			DWORD64;
#else
			DWORD;
#endif
		PTR_T            dwAddress = PTR_T(pObject);
		DWORD64            dwDisplacement = 0;

		MY_SYMBOL_INFO sym = {};
		sym.SizeOfStruct = sizeof(SYMBOL_INFO);
		sym.MaxNameLen = max_sym_name;
		if (!SymFromAddr(handler.hProcess, dwAddress, &dwDisplacement, &sym)) {
			return std::nullopt;
		}
		if (sym.Flags & SYMFLAG_PUBLIC_CODE) {
#if defined(_M_AMD64)
			uint8_t OP = *(uint8_t*)dwAddress;
			if (OP == 0xe9) {
				int32_t offset = *(int32_t*)((char*)dwAddress + 1);
				return Addr2Symbol((void*)(dwAddress + 5 + offset));
			}
#else
			//TODO ARM64/ARM64EC
#endif // _M_AMD64

		}
		Symbol sb;
		{
			char buffer[256];
			if (UnDecorateSymbolName(sym.Name, buffer, 256, UNDNAME_COMPLETE) != 0)
				sb.name = buffer;
			else
				sb.name = sym.Name;
		}
		IMAGEHLP_MODULE md = {};
		md.SizeOfStruct = sizeof(IMAGEHLP_MODULE);
		if (SymGetModuleInfo(handler.hProcess, dwAddress, &md)) {
#if defined(_WIN64)
			if (md.LineNumbers) {
#endif
				IMAGEHLP_LINE line = {};
				DWORD lineDisplacement = 0;
				if (SymGetLineFromAddr(handler.hProcess, dwAddress, &lineDisplacement, &line)) {
					sb.line_no = line.LineNumber;
					sb.file_name = line.FileName;
				}
#if defined(_WIN64)
			}
#endif
			sb.module_name = md.ModuleName;
		}

		// Object name output
		return std::move(sb);
	};
#else
	static std::string demangle_name(const char* name) {
#ifdef __GNUC__
		int status = 0;
		std::unique_ptr<char, decltype(&free)> realname(abi::__cxa_demangle(name, 0, 0, &status), free);
		return realname.get();
#endif
		return name;
	}
	static inline std::string shellcommand(const std::string& cmd) {
		try
		{
			std::string result;
			FILE* pipe = popen(cmd.c_str(), "r");
			if (pipe) {
				char buffer[256];
				fgets(buffer, 256, pipe);
				result.append(buffer);
				if (pclose(pipe) != 0) {
					return {};
				}
				return result;
			}
		}
		catch (const std::bad_alloc& e)
		{
		}
		return {};
	}
	static inline bool which_proc(const char* shell) {
#ifdef __linux__
		return system(shell) == 0;
#else
		FILE* pipe = popen(shell, "r");
		if (!pipe)
			return false;
		return pclose(pipe) == 0;
#endif
	}

#if USE_ATOS
	static std::string get_function_atos(void* ptr) {
		struct AtosInfo {
			pid_t pid;
			bool has_atos;
			AtosInfo() {
				pid = getpid();
				has_atos = which_proc("which atos");
			}
		};
		static AtosInfo atos_info;
		if (atos_info.has_atos) {
			auto funcinfo = shellcommand(std::format("atos -p {} {}", atos_info.pid, ptr));
			if (!(funcinfo[0] == '0' && funcinfo[1] == 'x')) {
				return funcinfo;
			}
		}
		return {};
	}
#endif

#if USE_ADDR2LINE
	static std::string get_function_addr2line(const char* fname, intptr_t offset) {
		static bool has_address2line = which_proc("which addr2line > /dev/null");
		if (has_address2line) {
			auto funcinfo = shellcommand(std::format("addr2line -e {} -f -p -C -s -i {:#x}", fname, offset));
			if (!funcinfo.empty() && funcinfo[0] != '?' && funcinfo[1] != '?') {
				return funcinfo;
			}
		}
		return {};
	}
#endif
#endif // _WIN32

	static std::string get_functioninfo(void* ptr) {
#ifdef _WIN32
		auto sym = NativeInfo::Addr2Symbol(ptr);
		return sym ? std::format("{} in {} ({}:{})",sym->name,sym->module_name,sym->file_name,sym->line_no) : "";
#else
		Dl_info info = {};
		if (dladdr(ptr, &info) == 0) {
			return {};
		}

		if (ptr > info.dli_fbase) {
			void* calc_address = info.dli_saddr == ptr ? info.dli_saddr : ptr;
			std::string funcinfo =
#if USE_ATOS
				get_function_atos(calc_address);
#elif USE_ADDR2LINE
				get_function_addr2line(info.dli_fname, (intptr_t)calc_address - (intptr_t)info.dli_fbase);
#else
			{}
#endif
			if (!funcinfo.empty()) {
				return funcinfo;
			}
		}

		if (info.dli_saddr != ptr) {
			return {};
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
				if (iter != infos.end()) {
					return &(iter->second);
				}
			}
			try
			{
				auto [iter, success] = infos.emplace(ptr, get_functioninfo(ptr));
				if (!success)
					return nullptr;
				return &(iter->second);
			}
			catch (const std::bad_alloc&)
			{
				return nullptr;
			}
		}

		std::unordered_map<void*, std::string> infos;
		std::mutex mtx;
	};
}
