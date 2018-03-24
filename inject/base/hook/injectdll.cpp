
#if !defined(_M_X64)

#include <base/hook/injectdll.h>
#include <windows.h>
#include <stdint.h>
#include <wow64ext.h>

namespace base { namespace hook {
	static bool is_process64(HANDLE hProcess) {
		BOOL is_x64 = FALSE;
		if (!IsWow64Process(hProcess, &is_x64)) {
			return false;
		}
		return !is_x64;
	}

	bool injectdll_x64(const PROCESS_INFORMATION& pi, const fs::path& dll) {
		static unsigned char sc[] = {
			0x9c,                                                                   // pushfq
			0x50,                                                                   // push rax
			0x51,                                                                   // push rcx
			0x52,                                                                   // push rdx
			0x53,                                                                   // push rbx
			0x55,                                                                   // push rbp
			0x56,                                                                   // push rsi
			0x57,                                                                   // push rdi
			0x41, 0x50,                                                             // push r8
			0x41, 0x51,                                                             // push r9
			0x41, 0x52,                                                             // push r10
			0x41, 0x53,                                                             // push r11
			0x41, 0x54,                                                             // push r12
			0x41, 0x55,                                                             // push r13
			0x41, 0x56,                                                             // push r14
			0x41, 0x57,                                                             // push r15
			0x48, 0x83, 0xEC, 0x28,                                                 // sub rsp, 0x28
			0x49, 0xB9, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,             // mov  r9, 0  // DllHandle
			0x49, 0xB8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,             // mov  r8, 0  // DllPath
			0x48, 0xB8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,             // mov  rax,0  // LdrLoadDll
			0x48, 0x31,                                                             // xor  rcx,rcx
			0xC9, 0x48,                                                             // xor  rdx,rdx
			0xFF, 0xD0,                                                             // call rax   LdrLoadDll
			0x48, 0x83, 0xC4, 0x28,                                                 // add rsp, 0x28
			0x41, 0x5F,                                                             // pop r15
			0x41, 0x5E,                                                             // pop r14
			0x41, 0x5D,                                                             // pop r13
			0x41, 0x5C,                                                             // pop r12
			0x41, 0x5B,                                                             // pop r11
			0x41, 0x5A,                                                             // pop r10
			0x41, 0x59,                                                             // pop r9
			0x41, 0x58,                                                             // pop r8
			0x5F,                                                                   // pop rdi
			0x5E,                                                                   // pop rsi
			0x5D,                                                                   // pop rbp
			0x5B,                                                                   // pop rbx
			0x5A,                                                                   // pop rdx
			0x59,                                                                   // pop rcx
			0x58,                                                                   // pop rax
			0x9D,                                                                   // popfq
			0xFF, 0x25, 0x00, 0x00, 0x00, 0x00,                                     // jmp offset
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00                          // rip
		};
		
		InitWow64ext();
		DWORD64 pfLoadLibrary = (DWORD64)GetProcAddress64(GetModuleHandle64(L"ntdll.dll"), "LdrLoadDll");
		if (!pfLoadLibrary) {
			return false;
		}

		struct UNICODE_STRING {
			USHORT    Length;
			USHORT    MaximumLength;
			DWORD64   Buffer;
		};
		SIZE_T memsize = sizeof(DWORD64) + sizeof(UNICODE_STRING) + (dll.wstring().size() + 1) * sizeof(wchar_t);
		DWORD64 memory = VirtualAllocEx64(pi.hProcess, NULL, memsize, MEM_COMMIT, PAGE_READWRITE);
		if (!memory) {
			return false;
		}
		DWORD64 shellcode = VirtualAllocEx64(pi.hProcess, NULL, sizeof(sc), MEM_COMMIT, PAGE_EXECUTE_READWRITE);
		if (!shellcode) {
			return false;
		}

		UNICODE_STRING us;
		us.Length = (USHORT)(dll.wstring().size() * sizeof(wchar_t));
		us.MaximumLength = us.Length + sizeof(wchar_t);
		us.Buffer = memory + sizeof(UNICODE_STRING);

		SIZE_T written = 0;
		BOOL ok = FALSE;
		ok = WriteProcessMemory64(pi.hProcess, memory, &us, sizeof(UNICODE_STRING), &written);
		if (!ok || written != sizeof(UNICODE_STRING)) {
			return false;
		}
		ok = WriteProcessMemory64(pi.hProcess, us.Buffer, dll.wstring().data(), us.MaximumLength, &written);
		if (!ok || written != us.MaximumLength) {
			return false;
		}

		_CONTEXT64 ctx = { 0 };
		ctx.ContextFlags = CONTEXT_CONTROL;
		if (!GetThreadContext64(pi.hThread, &ctx)) {
			return false;
		}

		DWORD64 handle = us.Buffer + us.MaximumLength;
		memcpy(sc + 30, &handle, sizeof(handle));
		memcpy(sc + 40, &memory, sizeof(memory));
		memcpy(sc + 50, &pfLoadLibrary, sizeof(pfLoadLibrary));
		memcpy(sc + 98, &ctx.Rip, sizeof(ctx.Rip));
		ok = WriteProcessMemory64(pi.hProcess, shellcode, &sc, sizeof(sc), &written);
		if (!ok || written != sizeof(sc)) {
			return false;
		}

		ctx.ContextFlags = CONTEXT_CONTROL;
		ctx.Rip = shellcode;
		if (!SetThreadContext64(pi.hThread, &ctx)) {
			return false;
		}
		return true;
	}

	bool injectdll_x86(const PROCESS_INFORMATION& pi, const fs::path& dll) {
		static unsigned char sc[] = {
			0x68, 0x00, 0x00, 0x00, 0x00,	// push eip
			0x9C,							// pushfd
			0x60,							// pushad
			0x68, 0x00, 0x00, 0x00, 0x00,	// push DllPath
			0xB8, 0x00, 0x00, 0x00, 0x00,	// mov eax, LoadLibraryW
			0xFF, 0xD0,						// call eax
			0x61,							// popad
			0x9D,							// popfd
			0xC3							// ret
		};

		DWORD pfLoadLibrary = (DWORD)::GetProcAddress(::GetModuleHandleW(L"Kernel32"), "LoadLibraryW");
		if (!pfLoadLibrary) {
			return false;
		}

		SIZE_T memsize = (dll.wstring().size() + 1) * sizeof(wchar_t);
		LPVOID memory = VirtualAllocEx(pi.hProcess, NULL, memsize, MEM_COMMIT, PAGE_READWRITE);
		if (!memory) {
			return false;
		}
		LPVOID shellcode = VirtualAllocEx(pi.hProcess, NULL, sizeof(sc), MEM_COMMIT, PAGE_EXECUTE_READWRITE);
		if (!shellcode) {
			return false;
		}
		SIZE_T written = 0;
		BOOL ok = FALSE;
		ok = WriteProcessMemory(pi.hProcess, memory, dll.wstring().data(), memsize, &written);
		if (!ok || written != memsize) {
			return false;
		}
		CONTEXT ctx = { 0 };
		ctx.ContextFlags = CONTEXT_FULL;
		if (!::GetThreadContext(pi.hThread, &ctx)) {
			return false;
		}
		memcpy(sc + 1, &ctx.Eip, sizeof(ctx.Eip));
		memcpy(sc + 8, &memory, sizeof(memory));
		memcpy(sc + 13, &pfLoadLibrary, sizeof(pfLoadLibrary));
		ok = WriteProcessMemory(pi.hProcess, shellcode, &sc, sizeof(sc), &written);
		if (!ok || written != sizeof(sc)) {
			return false;
		}

		ctx.ContextFlags = CONTEXT_CONTROL;
		ctx.Eip = (DWORD)shellcode;
		if (!::SetThreadContext(pi.hThread, &ctx)) {
			return false;
		}
		return true;
	}

	bool injectdll(const PROCESS_INFORMATION& pi, const fs::path& x86dll, const fs::path& x64dll) {
		if (is_process64(pi.hProcess)) {
			return injectdll_x64(pi, x64dll);
		}
		else {
			return injectdll_x86(pi, x86dll);
		}
	}
}}

#endif
