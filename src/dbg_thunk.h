#pragma once

namespace vscode {
	void* thunk_create(intptr_t dbg, intptr_t hook);
	void  thunk_destory(void* shellcode);
}
