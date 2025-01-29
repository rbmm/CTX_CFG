#include "stdafx.h"

ULONG _G_LowLimit;

NTSTATUS GetStackLimits(HANDLE hThread, PNT_TIB& tib, PVOID& StackBase, PVOID& StackLimit, PVOID& LowStackLimit)
{
	if (!_G_LowLimit)
	{
		return STATUS_NOT_FOUND;
	}

	THREAD_BASIC_INFORMATION tbi;
	NTSTATUS status = NtQueryInformationThread(hThread, ThreadBasicInformation, &tbi, sizeof(tbi), 0);
	if (0 <= status)
	{
		PNT_TIB NtTib = (PNT_TIB)tbi.TebBaseAddress;
		StackBase = NtTib->StackBase;
		StackLimit = NtTib->StackLimit;
		LowStackLimit = *(void**)RtlOffsetToPointer(NtTib, _G_LowLimit);
		tib = NtTib;
	}

	return status;
}

ULONG GetLowLimitOffset()
{
	ULONG_PTR LowLimit, HighLimit;
	GetCurrentThreadStackLimits(&LowLimit, &HighLimit);

	PNT_TIB NtTib = (PNT_TIB)NtCurrentTeb();

	if (NtTib->StackBase == (PVOID)HighLimit)
	{
		MEMORY_BASIC_INFORMATION mbi;
		if (VirtualQuery(NtTib, &mbi, sizeof(mbi)) &&
			MEM_COMMIT == mbi.State &&
			MEM_PRIVATE == mbi.Type &&
			PAGE_READWRITE == mbi.AllocationProtect &&
			NtTib == mbi.BaseAddress &&
			(mbi.RegionSize /= sizeof(PVOID)))
		{
			ULONG_PTR* pLowLimit = (ULONG_PTR*)NtTib;
			ULONG ofs = 0;
			do
			{
				if (*pLowLimit == LowLimit)
				{
					if (ofs)
					{
						return 0;
					}

					ofs = RtlPointerToOffset(NtTib, pLowLimit);
				}
			} while (++pLowLimit, --mbi.RegionSize);

			_G_LowLimit = ofs;

			return ofs;
		}
	}

	return 0;
}


void test_ctx(HANDLE hProcess, BOOL bCurrentProcess)
{
	if (HANDLE hThread = CreateRemoteThread(hProcess, 0, 0, (PTHREAD_START_ROUTINE)Sleep, INVALID_HANDLE_VALUE, 0, 0))
	{
		CONTEXT MainContext = { };
		MainContext.ContextFlags = CONTEXT_CONTROL;
		NTSTATUS status = NtGetContextThread(hThread, &MainContext);

		PNT_TIB NtTib = 0;
		PVOID StackBase = 0, StackLimit = 0, LowStackLimit = 0;

		if (bCurrentProcess)
		{
			GetStackLimits(hThread, NtTib, StackBase, StackLimit, LowStackLimit);
		}
		NtClose(hThread);

		if (0 <= status)
		{
			if (hThread = CreateRemoteThread(hProcess, 0, 0, (PTHREAD_START_ROUTINE)Sleep, INVALID_HANDLE_VALUE, 0, 0))
			{
				PCWSTR lpCaption = 0;
				CONTEXT BackupContext = { };
				BackupContext.ContextFlags = CONTEXT_CONTROL;

				if (0 <= (status = NtSuspendThread(hThread, 0)))
				{
					PVOID BackupStackBase = 0, BackupStackLimit = 0, BackupLowStackLimit = 0;

					if (bCurrentProcess)
					{
						GetStackLimits(hThread, NtTib = 0, BackupStackBase, BackupStackLimit, BackupLowStackLimit);
					}

					if (0 <= (status = NtGetContextThread(hThread, &BackupContext)))
					{
						lpCaption = L"NtSetContextThread";

						if (NtTib)
						{
							NtTib->StackBase = StackBase;
							NtTib->StackLimit = StackLimit;
							*(void**)RtlOffsetToPointer(NtTib, _G_LowLimit) = LowStackLimit;
						}

						status = NtSetContextThread(hThread, &MainContext);

						if (NtTib)
						{
							*(void**)RtlOffsetToPointer(NtTib, _G_LowLimit) = BackupLowStackLimit;
							NtTib->StackLimit = BackupStackLimit;
							NtTib->StackBase = BackupStackBase;
						}

						NtSetContextThread(hThread, &BackupContext);
					}

					NtResumeThread(hThread, 0);
				}

				NtClose(hThread);

				WCHAR sz[0x40];
				swprintf_s(sz, _countof(sz), L"[%08x] %p", status, hProcess);
				MessageBoxW(0, sz, lpCaption, MB_ICONINFORMATION);
			}
		}
	}
}

void test_ctx()
{
	GetLowLimitOffset();

	test_ctx(NtCurrentProcess(), TRUE);
	if (PWSTR lpApplicationName = new WCHAR[0x8000])
	{
		GetModuleFileNameW(0, lpApplicationName, 0x8000);
		if (GetLastError() == NOERROR)
		{
			PROCESS_INFORMATION pi;
			STARTUPINFOW si = { sizeof(si) };
			if (CreateProcessW(lpApplicationName, const_cast<PWSTR>(L"\n"), 0, 0, 0, 0, 0, 0, &si, &pi))
			{
				NtClose(pi.hThread);
				test_ctx(pi.hProcess, FALSE);
				NtClose(pi.hProcess);
			}
		}
		delete[] lpApplicationName;
	}
}

void WINAPI ep(void*)
{
	if ('\n' == *GetCommandLineW())
	{
		MessageBoxW(0, 0, L"copy of process", MB_ICONINFORMATION);
	}
	else
	{
		test_ctx();
	}
	ExitProcess(0);
}

