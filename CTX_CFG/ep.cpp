#include "stdafx.h"

void test_ctx(HANDLE hProcess)
{
	if (HANDLE hThread = CreateRemoteThread(hProcess, 0, 0, (PTHREAD_START_ROUTINE)Sleep, INVALID_HANDLE_VALUE, 0, 0))
	{
		CONTEXT MainContext = { };
		MainContext.ContextFlags = CONTEXT_CONTROL;
		NTSTATUS status = NtGetContextThread(NtCurrentThread(), &MainContext);
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
					if (0 <= (status = NtGetContextThread(hThread, &BackupContext)))
					{
						lpCaption = L"NtSetContextThread";
						status = NtSetContextThread(hThread, &MainContext);
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
	test_ctx(NtCurrentProcess());
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
				test_ctx(pi.hProcess);
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

