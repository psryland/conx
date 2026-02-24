//**********************************************
// Console Extensions
//  Copyright (c) Rylogic Ltd 2004
//**********************************************
// Standalone process wrapper
#pragma once
#include "src/forward.h"

namespace conx
{
	struct Process
	{
		HANDLE m_process = nullptr;

		~Process()
		{
			if (m_process)
				CloseHandle(m_process);
		}

		bool Start(wchar_t const* exe, wchar_t const* args, wchar_t const* working_dir)
		{
			// Build a mutable command line: "exe" args
			std::wstring cmd;
			cmd += L'"';
			cmd += exe;
			cmd += L'"';
			if (args && args[0])
			{
				cmd += L' ';
				cmd += args;
			}

			STARTUPINFOW si = { sizeof(si) };
			PROCESS_INFORMATION pi = {};
			auto ok = CreateProcessW(
				nullptr,
				cmd.data(),
				nullptr,
				nullptr,
				FALSE,
				0,
				nullptr,
				working_dir,
				&si,
				&pi
			);
			if (!ok)
				return false;

			CloseHandle(pi.hThread);
			m_process = pi.hProcess;
			return true;
		}

		int BlockTillExit()
		{
			if (!m_process) return -1;
			WaitForSingleObject(m_process, INFINITE);
			DWORD exit_code = 0;
			GetExitCodeProcess(m_process, &exit_code);
			return static_cast<int>(exit_code);
		}
	};
}
