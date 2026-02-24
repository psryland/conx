//**********************************************
// Console Extensions
//  Copyright (c) Rylogic Ltd 2004
//**********************************************
#include "src/forward.h"

namespace conx
{
	struct Cmd_Clip
	{
		std::string m_text;
		std::string m_newline;
		bool m_lwr, m_upr, m_fwdslash, m_bkslash, m_cstr, m_dopaste;
		
		Cmd_Clip()
			:m_text()
			,m_newline()
			,m_lwr(false)
			,m_upr(false)
			,m_fwdslash(false)
			,m_bkslash(false)
			,m_cstr(false)
			,m_dopaste(false)
		{}

		void ShowHelp() const
		{
			std::cout <<
				"Clip text to the system clipboard\n"
				" Syntax: Conx -clip \"text to copy\" [-lwr][-upr][-fwdslash][-bkslash][-cstr] [-crlf|cr|lf]\n"
				"  -lwr : converts copied text to lower case\n"
				"  -upr : converts copied text to upper case\n"
				"  -fwdslash : converts any directory marks to forward slashes\n"
				"  -bkslash : converts any directory marks to back slashes\n"
				"  -cstr : converts the copied text to a C\\C++ style string by adding escape characters\n"
				"  -crlf|cr|lf : convert newlines to the dos,mac,linux format\n"
				"\n"
				"Paste the clipboard contents to stdout\n"
				" Syntax: Conx -clip -paste\n"
				"   -paste : pastes the clipboard contents to stdout\n"
				"\n";
		}

		int Run(CmdLine const& args)
		{
			if (args.count("help") != 0)
				return ShowHelp(), 0;

			for (auto const& text : args("clip").values)
				m_text.append(text);

			m_lwr = args.count("lwr") != 0;
			m_upr = args.count("upr") != 0;
			m_fwdslash = args.count("fwdslash") != 0;
			m_bkslash = args.count("bkslash") != 0;
			m_cstr = args.count("cstr") != 0;
			m_dopaste = args.count("paste") != 0;
			m_newline =
				args.count("crlf") != 0 ? "\r\n" :
				args.count("cr") != 0 ? "\r" :
				args.count("lf") != 0 ? "\n" :
				"";

			if (m_dopaste)
			{
				std::string output;
				if (!OpenClipboard(nullptr)) return -1;
				auto h = GetClipboardData(CF_TEXT);
				if (h)
				{
					auto p = static_cast<char const*>(GlobalLock(h));
					if (p) output = p;
					GlobalUnlock(h);
				}
				CloseClipboard();
				std::cout << output;
				return 0;
			}

			// Perform optional conversions
			if (m_lwr)              { str::LowerCase(m_text); }
			if (m_upr)              { str::UpperCase(m_text); }
			if (m_fwdslash)         { str::Replace(m_text, "\\\\", "/");  str::Replace(m_text, "\\", "/"); }
			if (m_bkslash)          { str::Replace(m_text, "\\\\", "\\"); str::Replace(m_text, "/", "\\"); }
			if (!m_newline.empty()) { str::Replace(m_text, "\r\n", "\n"); str::Replace(m_text, "\r", "\n"); str::Replace(m_text, "\n", m_newline.c_str()); }
			if (m_cstr)             { m_text = str::StringToCString<std::string>(m_text); }
			auto SetClipBoardText = [](std::string const& text) -> bool
			{
				if (!OpenClipboard(nullptr)) return false;
				EmptyClipboard();
				auto h = GlobalAlloc(GMEM_MOVEABLE, text.size() + 1);
				if (!h) { CloseClipboard(); return false; }
				auto p = static_cast<char*>(GlobalLock(h));
				memcpy(p, text.c_str(), text.size() + 1);
				GlobalUnlock(h);
				SetClipboardData(CF_TEXT, h);
				CloseClipboard();
				return true;
			};
			return SetClipBoardText(m_text) ? 0 : -1;
		}
	};

	int Clip(CmdLine const& args)
	{
		Cmd_Clip cmd;
		return cmd.Run(args);
	}
}
