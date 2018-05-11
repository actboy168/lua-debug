#pragma once

#include <Windows.h>
#include <mutex>
#include <base/util/format.h>

namespace base { namespace win {
	struct process_switch {
		struct mutex {
			mutex(const std::wstring& name)
				: handle(CreateMutexW(0, FALSE, name.c_str())) {
				if (!handle) {
					throw std::runtime_error("processMutex create failed.");
				}
			}
			~mutex() {
				CloseHandle(handle);
				handle = NULL;
			}
			void lock() {
				DWORD ret = WaitForSingleObject(handle, INFINITE);
				if (ret != WAIT_OBJECT_0) {
					throw std::runtime_error("processMutex lock failed.");
				}
			}
			bool try_lock() {
				DWORD ret = WaitForSingleObject(handle, 0);
				if (ret == WAIT_OBJECT_0) {
					return true;
				}
				else if (ret == WAIT_TIMEOUT) {
					return false;
				}
				else {
					throw std::runtime_error("processMutex try_lock failed.");
				}
			}
			void unlock() {
				ReleaseMutex(handle);
			}
			HANDLE handle;
		};
		mutex m;

		process_switch(int pid, const wchar_t* name) : m(mutex_name(pid, name)) { m.lock(); }
		~process_switch() { m.unlock(); }

		static bool has(int pid, const wchar_t* name) {
			mutex m(mutex_name(pid, name));
			std::unique_lock<mutex> lock(m, std::try_to_lock_t());
			return !lock;
		}
		static std::wstring mutex_name(int pid, const wchar_t* name) {
			return base::format(L"base-process-switch-%s-%d", name, pid);
		}
	};
}}
