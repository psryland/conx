//**********************************************
// Console Extensions
//  Copyright (c) Rylogic Ltd 2004
//**********************************************
#include "src/forward.h"

#include <objbase.h>
#pragma comment(lib, "ole32.lib")

namespace conx
{
	struct Cmd_Guid
	{
		void ShowHelp() const
		{
			std::cout <<
				"Generate a new GUID\n"
				" Syntax: Conx -guid\n";
		}

		int Run(pr::CmdLine const& args)
		{
			if (args.count("help") != 0)
				return ShowHelp(), 0;

			GUID guid;
			CoCreateGuid(&guid);
			char buf[64];
			snprintf(buf, sizeof(buf), "{%08lX-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X}",
				guid.Data1, guid.Data2, guid.Data3,
				guid.Data4[0], guid.Data4[1], guid.Data4[2], guid.Data4[3],
				guid.Data4[4], guid.Data4[5], guid.Data4[6], guid.Data4[7]);
			std::cout << buf;
			return 0;
		}
	};

	int Guid(pr::CmdLine const& args)
	{
		Cmd_Guid cmd;
		return cmd.Run(args);
	}
}
