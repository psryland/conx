//**********************************************
// Console Extensions
//  Copyright (c) Rylogic Ltd 2004
//**********************************************
#include "src/forward.h"
#include "src/common/process_util.h"

namespace conx
{
	struct Cmd_SendKeys
	{
		void ShowHelp() const
		{
			std::cout <<
				"SendKeys: Send key presses to a window\n"
				" Syntax: Conx -send_keys \"text\" -p <process-name> [-w <window-name>] [-rate <keys-per-second>] [-bg] [-c <class>]\n"
				"  -p    : Name (or partial name) of the target process\n"
				"  -w    : Title (or partial title) of the target window (default: largest)\n"
				"  -rate : Key press rate in keys per second (default: 10)\n"
				"  -bg   : Background mode — post WM_CHAR messages directly to the window\n"
				"           instead of using SendInput. Does not steal focus. Works for\n"
				"           native Win32 apps; use foreground mode for Electron/Chromium.\n"
				"  -c    : (With -bg) Target a child control by class name substring\n"
				"           (e.g., 'Edit', 'RichEdit'). Without this, sends to the window.\n"
				"\n"
				"  Without -bg, brings the window to the foreground and uses SendInput\n"
				"  for hardware-level key simulation. Works with all applications.\n";
		}

		int Run(CmdLine const& args)
		{
			if (args.count("help") != 0)
				return ShowHelp(), 0;

			std::string text;
			if (args.count("send_keys") != 0)
			{
				for (auto const& v : args("send_keys").values)
					text.append(v);
			}

			std::string process_name;
			if (args.count("p") != 0) { process_name = args("p").as<std::string>(); }

			std::string window_name;
			if (args.count("w") != 0) { window_name = args("w").as<std::string>(); }

			double rate = 10.0;
			if (args.count("rate") != 0) { rate = args("rate").as<double>(); }

			bool background = args.count("bg") != 0;

			std::string child_class;
			if (args.count("c") != 0) { child_class = args("c").as<std::string>(); }

			if (text.empty())         { std::cerr << "No text to send\n"; return ShowHelp(), -1; }
			if (process_name.empty()) { std::cerr << "No process name provided (-p)\n"; return ShowHelp(), -1; }
			if (rate <= 0)            { std::cerr << "Rate must be positive\n"; return -1; }

			auto hwnd = FindWindow(process_name, window_name);
			if (!hwnd)
			{
				auto target = window_name.empty() ? process_name : std::format("{}:{}", process_name, window_name);
				std::cerr << std::format("No window found for '{}'\n", target);
				return -1;
			}

			auto delay_ms = static_cast<DWORD>(1000.0 / rate);

			// Resolve the target for WM_ messages in background mode
			HWND key_target = hwnd;
			if (background && !child_class.empty())
			{
				key_target = FindChildByClass(hwnd, child_class);
				if (!key_target)
				{
					std::cerr << std::format("No child control matching class '{}' found\n", child_class);
					return -1;
				}
			}

			// Print status before any foreground activation
			auto mode = background ? "bg" : "fg";
			std::cout << std::format("Sending {} key(s) to '{}' [{}]\n", text.size(), GetWindowTitle(hwnd), mode);
			std::cout.flush();

			if (background)
			{
				// Background mode: post WM_CHAR messages directly
				for (auto ch : text)
				{
					PostCharMsg(key_target, ch);
					Sleep(delay_ms);
				}
			}
			else
			{
				// Foreground mode: bring to front and use SendInput
				BringToForeground(hwnd, true);

				for (auto ch : text)
				{
					INPUT inputs[2] = {};
					inputs[0].type = INPUT_KEYBOARD;
					inputs[0].ki.wScan = static_cast<WORD>(ch);
					inputs[0].ki.dwFlags = KEYEVENTF_UNICODE;

					inputs[1].type = INPUT_KEYBOARD;
					inputs[1].ki.wScan = static_cast<WORD>(ch);
					inputs[1].ki.dwFlags = KEYEVENTF_UNICODE | KEYEVENTF_KEYUP;

					SendInput(2, inputs, sizeof(INPUT));
					Sleep(delay_ms);
				}
			}

			return 0;
		}
	};

	int SendKeys(CmdLine const& args)
	{
		Cmd_SendKeys cmd;
		return cmd.Run(args);
	}
}
