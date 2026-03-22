//**********************************************
// Console Extensions
//  Copyright (c) Rylogic Ltd 2004
//**********************************************
// MoveWindow: Move a window to a new screen location.
#include "src/forward.h"
#include "src/common/process_util.h"

namespace conx
{
	struct Cmd_MoveWindow
	{
		void ShowHelp() const
		{
			std::cout <<
				"MoveWindow: Move a window to a new screen position\n"
				" Syntax: Conx -move_window -p <process-name> [-w <window-name>] -x <X> -y <Y> [-width <W>] [-height <H>]\n"
				"  -p      : Name (or partial name) of the target process\n"
				"  -w      : Title (or partial title) of the target window (default: largest window)\n"
				"  -x      : New X position in screen pixels\n"
				"  -y      : New Y position in screen pixels\n"
				"  -width  : New width in pixels (default: keep current)\n"
				"  -height : New height in pixels (default: keep current)\n"
				"\n"
				"  Finds a window matching the criteria and moves it to the specified position.\n"
				"  Returns 0 on success, non-zero on error.\n";
		}

		int Run(CmdLine const& args)
		{
			if (args.count("help") != 0)
				return ShowHelp(), 0;

			std::string process_name;
			if (args.count("p") != 0) { process_name = args("p").as<std::string>(); }

			std::string window_name;
			if (args.count("w") != 0) { window_name = args("w").as<std::string>(); }

			if (process_name.empty()) { std::cerr << "No process name provided (-p)\n"; return ShowHelp(), -1; }
			if (args.count("x") == 0) { std::cerr << "No X position provided (-x)\n"; return ShowHelp(), -1; }
			if (args.count("y") == 0) { std::cerr << "No Y position provided (-y)\n"; return ShowHelp(), -1; }

			auto x = args("x").as<int>();
			auto y = args("y").as<int>();

			auto hwnd = FindWindow(process_name, window_name);
			if (!hwnd)
			{
				std::cerr << std::format("No window found for process '{}'{}\n",
					process_name,
					window_name.empty() ? "" : std::format(" with title '{}'", window_name));
				return 1;
			}

			// Get the current window rect so we can preserve size if not overridden
			RECT rc;
			if (!GetWindowRect(hwnd, &rc))
			{
				std::cerr << "Failed to get window rect\n";
				return 1;
			}

			auto w = static_cast<int>(rc.right - rc.left);
			auto h = static_cast<int>(rc.bottom - rc.top);

			if (args.count("width") != 0) { w = args("width").as<int>(); }
			if (args.count("height") != 0) { h = args("height").as<int>(); }

			auto title = GetWindowTitle(hwnd);

			if (!SetWindowPos(hwnd, nullptr, x, y, w, h, SWP_NOZORDER | SWP_NOACTIVATE))
			{
				std::cerr << std::format("Failed to move window '{}'\n", title);
				return 1;
			}

			std::cout << std::format("Moved '{}' to ({},{}) size {}x{}\n", title, x, y, w, h);
			return 0;
		}
	};

	int MoveWindow(CmdLine const& args)
	{
		Cmd_MoveWindow cmd;
		return cmd.Run(args);
	}
}
