#include <Windows.h>
#include "dbg_thunk.h"

namespace vscode {

	void* thunk_create(intptr_t dbg, intptr_t hook)
	{
		static unsigned char sc[] = {
#if defined(_M_X64)
			0x57,                                                       // push rdi
			0x50,                                                       // push rax
			0x48, 0x83, 0xec, 0x28,                                     // sub rsp, 40
			0x48, 0x8b, 0xfc,                                           // mov rdi, rsp
			0x4c, 0x8b, 0xc2,                                           // mov r8, rdx
			0x48, 0x8b, 0xd1,                                           // mov rdx, rcx
			0x48, 0xb9, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // mov rcx, dbg
			0x48, 0xb8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // mov rax, hook
			0xff, 0xd0,                                                 // call rax
			0x48, 0x83, 0xc4, 0x28,                                     // add rsp, 40
			0x58,                                                       // pop rax
			0x5f,                                                       // pop rdi
			0xc3,                                                       // ret
#else
			0xff, 0x74, 0x24, 0x08,       // push [esp+8]
			0xff, 0x74, 0x24, 0x08,       // push [esp+8]
			0x68, 0x00, 0x00, 0x00, 0x00, // push dbg
			0xe8, 0x00, 0x00, 0x00, 0x00, // call hook
			0x83, 0xc4, 0x0c,             // add esp, 12
			0xc3,                         // ret

#endif
		};
		LPVOID shellcode = VirtualAllocEx(GetCurrentProcess(), NULL, sizeof(sc), MEM_COMMIT, PAGE_EXECUTE_READWRITE);
		if (!shellcode) {
			return 0;
		}
#if defined(_M_X64)
		memcpy(sc + 17, &dbg, sizeof(dbg));
		memcpy(sc + 27, &hook, sizeof(hook));
#else
		memcpy(sc + 9, &dbg, sizeof(dbg));
		hook = hook - ((intptr_t)shellcode + 18);
		memcpy(sc + 14, &hook, sizeof(hook));
#endif
		SIZE_T written = 0;
		BOOL ok = WriteProcessMemory(GetCurrentProcess(), shellcode, &sc, sizeof(sc), &written);
		if (!ok || written != sizeof(sc)) {
			thunk_destory(shellcode);
			return 0;
		}
		return shellcode;
	}

	void thunk_destory(void* shellcode)
	{
		if (shellcode) {
			VirtualFreeEx(GetCurrentProcess(), shellcode, 0, MEM_RELEASE);
		}
	}
}
