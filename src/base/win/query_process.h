#pragma once

#include <Windows.h>
#include <functional>

namespace base { namespace win {

	typedef struct _LSA_UNICODE_STRING
	{
		USHORT  Length;
		USHORT  MaximumLength;
		PWSTR   Buffer;
	}LSA_UNICODE_STRING, *PLSA_UNICODE_STRING;
	typedef LSA_UNICODE_STRING UNICODE_STRING, *PUNICODE_STRING;

	typedef struct _VM_COUNTERS
	{
		ULONG PeakVirtualSize;                  //虚拟存储峰值大小；
		ULONG VirtualSize;                      //虚拟存储大小；
		ULONG PageFaultCount;                   //页故障数目；
		ULONG PeakWorkingSetSize;               //工作集峰值大小；
		ULONG WorkingSetSize;                   //工作集大小；
		ULONG QuotaPeakPagedPoolUsage;          //分页池使用配额峰值；
		ULONG QuotaPagedPoolUsage;              //分页池使用配额；
		ULONG QuotaPeakNonPagedPoolUsage;       //非分页池使用配额峰值；
		ULONG QuotaNonPagedPoolUsage;           //非分页池使用配额；
		ULONG PagefileUsage;                    //页文件使用情况；
		ULONG PeakPagefileUsage;                //页文件使用峰值；
	}VM_COUNTERS, *PVM_COUNTERS;

	typedef LONG KPRIORITY;

	typedef struct _SYSTEM_PROCESS_INFORMATION {
		ULONG                     NextEntryOffset;
		ULONG                     NumberOfThreads;
		LARGE_INTEGER             Reserved[3];
		LARGE_INTEGER             CreateTime;
		LARGE_INTEGER             UserTime;
		LARGE_INTEGER             KernelTime;
		UNICODE_STRING            ImageName;
		KPRIORITY                 BasePriority;
		HANDLE                    ProcessId;
		HANDLE                    InheritedFromProcessId;
		ULONG                     HandleCount;
		ULONG                     Reserved2[2];
		ULONG                     PrivatePageCount;
		VM_COUNTERS               VirtualMemoryCounters;
		IO_COUNTERS               IoCounters;
		//SYSTEM_THREAD_INFORMATION Threads[0];
	} SYSTEM_PROCESS_INFORMATION, *PSYSTEM_PROCESS_INFORMATION;

	bool query_process(std::function<bool(const SYSTEM_PROCESS_INFORMATION*)> NtCBProcessInformation);
}}
