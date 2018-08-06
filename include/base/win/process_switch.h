#pragma once

#include <Windows.h>
#include <mutex>
#include <base/util/format.h>

namespace base { namespace win {
	struct process_switch {
		struct mutex {
			mutex(const std::wstring& name)
				: handle(CreateMutexW(0, FALSE, name.c_str()))
			    , owner(false) {
				if (!handle) {
					throw std::runtime_error("processMutex create failed.");
				}
			}
			~mutex() {
				CloseHandle(handle);
				handle = NULL;
			}
			void lock() {
				if (owner) {
					throw std::runtime_error("processMutex recursive.");
				}
				DWORD ret = WaitForSingleObject(handle, INFINITE);
				if (ret != WAIT_OBJECT_0) {
					throw std::runtime_error("processMutex lock failed.");
				}
				owner = true;
			}
			bool try_lock() {
				if (owner) {
					throw std::runtime_error("processMutex recursive.");
				}
				DWORD ret = WaitForSingleObject(handle, 0);
				if (ret == WAIT_OBJECT_0) {
					owner = true;
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
				if (owner) {
					ReleaseMutex(handle);
				}
				owner = false;
			}
			HANDLE handle;
			bool   owner;
		};
		mutex m;

		process_switch(int pid, const wchar_t* name, bool owner = true) : m(mutex_name(pid, name)) { if (owner) m.lock(); }
		~process_switch() { m.unlock(); }
		void unlock() {
			m.unlock();
		}
		bool has() {
			if (m.owner) return true;
			std::unique_lock<mutex> lock(m, std::try_to_lock_t());
			return !lock;
		}
		static bool has(int pid, const wchar_t* name) {
			return process_switch(pid, name, false).has();
		}
		static std::wstring mutex_name(int pid, const wchar_t* name) {
			return base::format(L"base-process-switch-%s-%d", name, pid);
		}
	};
}}
