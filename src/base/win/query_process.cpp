#include <base/win/query_process.h>

namespace base { namespace win {

#define STATUS_INFO_LENGTH_MISMATCH      ((NTSTATUS)0xC0000004L)

	typedef enum _KWAIT_REASON
	{
		Executive,
		FreePage,
		PageIn,
		PoolAllocation,
		DelayExecution,
		Suspended,
		UserRequest,
		WrExecutive,
		WrFreePage,
		WrPageIn,
		WrPoolAllocation,
		WrDelayExecution,
		WrSuspended,
		WrUserRequest,
		WrEventPair,
		WrQueue,
		WrLpcReceive,
		WrLpcReply,
		WrVertualMemory,
		WrPageOut,
		WrRendezvous,
		Spare2,
		Spare3,
		Spare4,
		Spare5,
		Spare6,
		WrKernel
	}KWAIT_REASON;

	typedef enum _SYSTEM_INFORMATION_CLASS {
		SystemBasicInformation = 0,
		SystemPerformanceInformation = 2,
		SystemTimeOfDayInformation = 3,
		SystemProcessInformation = 5,
		SystemProcessorPerformanceInformation = 8,
		SystemInterruptInformation = 23,
		SystemExceptionInformation = 33,
		SystemRegistryQuotaInformation = 37,
		SystemLookasideInformation = 45,
		SystemPolicyInformation = 134,
	} SYSTEM_INFORMATION_CLASS;

	typedef struct _CLIENT_ID
	{
		HANDLE UniqueProcess;
		HANDLE UniqueThread;
	}CLIENT_ID;
	typedef CLIENT_ID *PCLIENT_ID;

	typedef struct _SYSTEM_THREAD_INFORMATION {
		LARGE_INTEGER           KernelTime;
		LARGE_INTEGER           UserTime;
		LARGE_INTEGER           CreateTime;
		ULONG                   WaitTime;
		PVOID                   StartAddress;
		CLIENT_ID               ClientId;
		KPRIORITY               Priority;
		LONG                    BasePriority;
		ULONG                   ContextSwitchCount;
		ULONG                   State;
		KWAIT_REASON            WaitReason;
	}SYSTEM_THREAD_INFORMATION, *PSYSTEM_THREAD_INFORMATION;

	typedef NTSTATUS(CALLBACK *PFN_NTQUERYSYSTEMINFORMATION)(SYSTEM_INFORMATION_CLASS SystemInformationClass, PVOID SystemInformation, ULONG SystemInformationLength, PULONG ReturnLength);

	bool query_process(std::function<bool(const SYSTEM_PROCESS_INFORMATION*)> NtCBProcessInformation) {
		static HMODULE hNtDll = LoadLibraryW(L"ntdll.dll");
		if (hNtDll == NULL) {
			return false;
		}
		static PFN_NTQUERYSYSTEMINFORMATION pfnNtQSI = (PFN_NTQUERYSYSTEMINFORMATION)GetProcAddress(hNtDll, "NtQuerySystemInformation");
		if (pfnNtQSI == NULL) {
			return false;
		}
		std::unique_ptr<uint8_t[]> buffer;
		ULONG size = 0x20000;
		ULONG returnedSize = 0;
		NTSTATUS status = -1;
		do {
			buffer.reset(new uint8_t[(size + 7) / 8 * 8]);
			status = pfnNtQSI(SystemProcessInformation, buffer.get(), size, (PULONG)&returnedSize);
			if (status == STATUS_INFO_LENGTH_MISMATCH) {
				size = returnedSize;
			}
		} while (status == STATUS_INFO_LENGTH_MISMATCH);

		if (status != 0) {
			return false;
		}
		for (ULONG offset = 0; offset < returnedSize;) {
			const SYSTEM_PROCESS_INFORMATION* ptr = (const SYSTEM_PROCESS_INFORMATION*)(buffer.get() + offset);
			if (ptr->ImageName.Buffer != 0) {
				if (NtCBProcessInformation(ptr)) {
					break;
				}
			}
			if (ptr->NextEntryOffset == 0)
			{
				break;
			}
			offset += ptr->NextEntryOffset;
		}
		return true;
	}

}}
