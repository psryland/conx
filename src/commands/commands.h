//**********************************************
// Console Extensions
//  Copyright (c) Rylogic Ltd 2004
//**********************************************
#include "src/forward.h"

namespace conx
{
	// To add a new command:
	//  1. Add a line to the CONX_CMD macro
	//  2. Add a *.cpp file and implement the command entry point function.
	//      e.g. int MyCommand(CmdLine const& args) {}
	//  3. Command functions should return 0 on success.

	#define CONX_CMD(x)\
	x("automate", "Execute a script of mouse/keyboard commands", Automate)\
	x("clip", "Add text to the windows clipboard", Clip)\
	x("dirpath", "Open a dialog window for finding a path", DirPath)\
	x("exec", "Exec: execute another process", Exec)\
	x("find_element", "Find a UI element by name", FindElement)\
	x("guid", "Generate a new GUID", Guid)\
	x("hash", "Hash the given stdin data", Hash)\
	x("hdata", "Convert a source file into a C/C++ compatible header file", HData)\
	x("list_windows", "List all windows of a process", ListWindows)\
	x("lwr", "Convert a string to lower case", Lower)\
	x("msgbox", "Display a message box", MsgBox)\
	x("newlines", "Add or remove new lines from a text file", NewLines)\
	x("read_dpi", "Report the DPI scaling for a monitor", ReadDpi)\
	x("read_text", "Read text from a window using UI Automation", ReadText)\
	x("record", "Record a sequence of frames from a window or screen region", Record)\
	x("rtfm", "Output complete command reference in markdown", Rtfm)\
	x("screenshot", "Capture visible windows of a process to PNG", Screenshot)\
	x("send_keys", "Send key presses to a window", SendKeys)\
	x("send_mouse", "Send mouse events to a window", SendMouse)\
	x("shutdown_process", "Gracefully shut down a process", ShutdownProcess)\
	x("shcopy,shmove,shrename,shdelete", "Shell file operations (copy/move/rename/delete)", ShFileOp)\
	x("wait_window", "Wait for a window to appear", WaitWindow)\
	x("wait", "Wait for a specified length of time", Wait)

	// Forward declare command functions
	#define CONX_CMD_FUNCTION(option, description, func) int func(CmdLine const& args);
	CONX_CMD(CONX_CMD_FUNCTION);
	#undef CONX_CMD_FUNCTION
}