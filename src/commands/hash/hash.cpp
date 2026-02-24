//**********************************************
// Console Extensions
//  Copyright (c) Rylogic Ltd 2004
//**********************************************
#include "src/forward.h"

namespace conx
{
	struct Cmd_Hash
	{
		std::string m_text;

		Cmd_Hash()
			:m_text()
		{}

		void ShowHelp() const
		{
			std::cout <<
				"Hash the given stdin data\n"
				" Syntax: Conx -hash data_to_hash...\n";
		}

		int Run(CmdLine const& args)
		{
			if (args.count("help") != 0)
				return ShowHelp(), 0;

			for (auto const& text : args("hash").values)
				m_text.append(text);

			// FNV-1a hash
			uint32_t hash = 2166136261u;
			for (auto ch : m_text)
			{
				hash ^= static_cast<uint32_t>(static_cast<unsigned char>(ch));
				hash *= 16777619u;
			}
			std::cout << std::format("{:08X}", hash);
			return 0;
		}
	};

	int Hash(CmdLine const& args)
	{
		Cmd_Hash cmd;
		return cmd.Run(args);
	}
}
