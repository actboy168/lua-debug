
#if !defined(_M_X64)

#include <base/hook/detail/inject_dll.h>
#include <base/hook/assembler/writer.h>
#include <windows.h>
#include <stdint.h>

namespace base { namespace hook { namespace detail {
	static bool is_process64(HANDLE hProcess) {
		BOOL is_x64 = FALSE;
		if (!IsWow64Process(hProcess, &is_x64)) {
			return FALSE;
		}
		return !is_x64;
	}

	bool injectdll_x64(HANDLE process, HANDLE thread, const fs::path& dll) {
		return false;
	}

	bool injectdll_x86(HANDLE process, HANDLE thread, const fs::path& dll) {
		uintptr_t pfLoadLibrary = (uintptr_t)::GetProcAddress(::GetModuleHandleW(L"Kernel32"), "LoadLibraryW");
		if (!pfLoadLibrary) {
			return false;
		}

		CONTEXT cxt = { 0 };
		cxt.ContextFlags = CONTEXT_FULL;
		if (!::GetThreadContext(thread, &cxt)) {
			return false;
		}

		assembler::dynwriter code(128 + dll.wstring().size() * sizeof(wchar_t));
		uintptr_t code_base = (ctx.Esp - sizeof(code)) & ~0x1Fu;
		code.push(code_base + 100);
		code.call((uintptr_t)pfLoadLibrary, code_base + code._size());
		code.mov(assembler::eax, ctx.Eax);
		code.mov(assembler::ebx, ctx.Ebx);
		code.mov(assembler::ecx, ctx.Ecx);
		code.mov(assembler::edx, ctx.Edx);
		code.mov(assembler::esi, ctx.Esi);
		code.mov(assembler::edi, ctx.Edi);
		code.mov(assembler::ebp, ctx.Ebp);
		code.mov(assembler::esp, ctx.Esp);
		code.jmp(ctx.Eip, code_base + code._size());
		code._seek(100);
		code.emit(dll.wstring().data(), (dll.wstring().size() + 1) * sizeof(wchar_t));
		ctx.Esp = code_base - 4;
		ctx.Eip = code_base;

		DWORD nProtect = 0;
		if (!::VirtualProtectEx(process, (void*)code_base, code._maxsize(), PAGE_EXECUTE_READWRITE, &nProtect)) {
			return false;
		}
		DWORD nWritten = 0;
		if (!::WriteProcessMemory(process, (void*)code_base, code._data(), code._maxsize(), &nWritten)) {
			return false;
		}
		if (!::FlushInstructionCache(process, (void*)code_base, code._maxsize())) {
			return false;
		}
		if (!::SetThreadContext(thread, &ctx)) {
			return false;
		}
		return true;
	}

	bool injectdll(HANDLE process, HANDLE thread, const fs::path& x86dll, const fs::path& x64dll) {
		if (is_process64(process)) {
			return injectdll_x64(process, thread, x64dll);
		}
		else {
			return injectdll_x86(process, thread, x86dll);
		}
	}
}}}

#endif
