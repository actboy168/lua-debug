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
#include <signal.h>
#endif
#ifdef __GNUC__
#include <cxxabi.h>
#endif
#ifdef __APPLE__
#include <TargetConditionals.h>
#include <unistd.h>
#endif
#include <optional>
#include <string>
#include <bee/filesystem.h>
#include <bee/format.h>
#include <memory>
#ifdef __linux__
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <unistd.h>
#endif

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
	static inline std::optional<std::string> shellcommand(const char* prog_name, const char* argp[]) {
		try
		{
			int pdes[2] = {};
			if (::pipe(pdes) < 0) {
         		return std::nullopt;
       		}

			pid_t pid = ::fork();
			switch (pid) {
			case -1:
				// Failed...
				::close(pdes[0]);
				::close(pdes[1]);
				return std::nullopt;

			case 0:
				// We are the child.
				::close(STDERR_FILENO);
				::close(pdes[0]);
				if (pdes[1] != STDOUT_FILENO) {
					::dup2(pdes[1], STDOUT_FILENO);
				}

				// Do not use `execlp()`, `execvp()`, and `execvpe()` here!
				// `exec*p*` functions are vulnerable to PATH variable evaluation attacks.
				::execv(prog_name, (char *const *) argp);
				::_exit(127);
			}

			::FILE* p = ::fdopen(pdes[0], "r");
			::close(pdes[1]);
			if (!p) {
				return std::nullopt;
			}
			std::string res;
			char data[32];
            while (!::feof(p)) {
                if (::fgets(data, sizeof(data), p)) {
                    res += data;
                } else {
                    break;
                }
            }
			fclose(p);

			int pstat = 0;
			::kill(pid, SIGKILL);
			::waitpid(pid, &pstat, 0);

			while (!res.empty() && (res[res.size() - 1] == '\n' || res[res.size() - 1] == '\r')) {
				res.erase(res.size() - 1);
			}
			return res;
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
	static std::optional<std::string> get_function_atos(void* ptr) {
		struct AtosInfo {
			std::string pid;
			bool has_atos;
			AtosInfo() {
				pid = std::format("{}", getpid());
				has_atos = which_proc("which atos");
			}
		};
		static AtosInfo atos_info;
		if (atos_info.has_atos) {
			auto ptr_s = std::format("{}", ptr);
			const char* args[] = {
                "atos",
				"-p",
				atos_info.pid.c_str(),
				ptr_s.c_str(),
				nullptr,
			};
			auto funcinfo = shellcommand("/usr/bin/atos", args).value_or("");
			if (!(funcinfo[0] == '0' && funcinfo[1] == 'x')) {
				return funcinfo;
			}
		}
		return std::nullopt;
	}
#endif

#if USE_ADDR2LINE
	static std::optional<std::string> get_function_addr2line(const char* fname, intptr_t offset) {
		static bool has_address2line = which_proc("which addr2line > /dev/null");
		if (has_address2line) {
			auto offset_x = std::format("{:#x}", offset);
			const char* args[] = {
                "addr2line",
				"-e",
				fname,
				"-fpCsi",
				offset_x.c_str(),
				nullptr,
			};
			auto res = shellcommand("/usr/bin/addr2line", args);
			if (!res)
				return std::nullopt;
			auto funcinfo = *res;
			if (!funcinfo.empty() && funcinfo[0] != '?' && funcinfo[1] != '?') {
				return funcinfo;
			}
		}
		return std::nullopt;
	}
#endif
#endif // _WIN32

	static std::optional<std::string> get_functioninfo(void* ptr) {
        if (!ptr) {
            return std::nullopt;
        }
#ifdef _WIN32
		auto sym = NativeInfo::Addr2Symbol(ptr);
		if (sym){
			return std::format("{} in {} ({}:{})",sym->name,sym->module_name,sym->file_name,sym->line_no);
		}
		return std::nullopt;
#else
		Dl_info info = {};
		if (dladdr(ptr, &info) == 0) {
			return std::nullopt;
		}

		if (ptr > info.dli_fbase) {
			void* calc_address = info.dli_saddr == ptr ? info.dli_saddr : ptr;
			std::optional<std::string> funcinfo
#if USE_ATOS
				= get_function_atos(calc_address);
#elif USE_ADDR2LINE
				= get_function_addr2line(info.dli_fname, (intptr_t)calc_address - (intptr_t)info.dli_fbase);
#else
				;
#endif
			if (funcinfo.has_value()) {
				return funcinfo;
			}
		}

		if (info.dli_saddr != ptr) {
			return std::nullopt;
		}

		std::string filename = fs::path(info.dli_fname).filename();
		std::string realname = demangle_name(info.dli_sname);
		auto addr = info.dli_saddr;

		return std::format("`{}`{} {}", filename, realname, addr);
#endif
	}
}
