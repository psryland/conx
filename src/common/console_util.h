//**********************************************
// Console Extensions
//  Copyright (c) Rylogic Ltd 2004
//**********************************************
// Console utility functions
#pragma once
#include "src/forward.h"

namespace conx
{
	// Show the console for this process
	inline void ShowConsole()
	{
		// Attach to the current console
		if (AttachConsole((DWORD)-1) || AllocConsole())
		{
			// Redirect the CRT standard input, output, and error handles to the console
			freopen("CONIN$", "r", stdin);
			freopen("CONOUT$", "w", stdout);
			freopen("CONOUT$", "w", stderr);

			// Clear the error state for each of the C++ standard stream objects. We need to do this, as
			// attempts to access the standard streams before they refer to a valid target will cause the
			// 'iostream' objects to enter an error state. In versions of Visual Studio after 2005, this seems
			// to always occur during startup regardless of whether anything has been read from or written to
			// the console or not.
			std::wcout.clear();
			std::cout.clear();
			std::wcerr.clear();
			std::cerr.clear();
			std::wcin.clear();
			std::cin.clear();
		}
	}

	// Add an environment variable
	inline void SetEnvVar(std::string_view env_var, std::string_view value)
	{
		try
		{
			std::ofstream file("~conx.bat");
			file << std::format("@echo off\nset {}={}\n", env_var, value);
		}
		catch (std::exception const& ex)
		{
			std::cerr << "Failed to create '~conx.bat' file\n" << ex.what();
		}
	}
}
