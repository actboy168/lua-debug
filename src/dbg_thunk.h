#pragma once

namespace vscode {
	void* thunk_create_hook(intptr_t dbg, intptr_t hook);
	void* thunk_create_panic(intptr_t dbg, intptr_t panic, intptr_t old_panic);
	void  thunk_destory(void* shellcode);
}
