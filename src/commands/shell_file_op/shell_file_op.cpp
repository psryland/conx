//**********************************************
// Console Extensions
//  Copyright (c) Rylogic Ltd 2004
//**********************************************
#include "src/forward.h"

namespace conx
{
	struct Cmd_ShFileOp
	{
		// Build a double-null-terminated path list from a comma-separated string
		static char const* BuildPathList(std::string const& arg, std::string& buf)
		{
			buf.clear();
			str::Split(arg, ",", [&](auto sub, int)
			{
				buf.append(std::filesystem::absolute(std::string(sub)).string());
				buf.push_back('\0');
			});
			buf.push_back('\0');
			return buf.c_str();
		}

		// Parse a comma-separated flags string into FILEOP_FLAGS
		static FILEOP_FLAGS ParseFlags(std::string const& arg)
		{
			FILEOP_FLAGS flags = 0;
			str::Split(arg, ",", [&](auto sub, int)
			{
				auto s = std::string(sub);
				if (str::EqualI(s, "AllowUndo"))             flags |= FOF_ALLOWUNDO;
				else if (str::EqualI(s, "FilesOnly"))        flags |= FOF_FILESONLY;
				else if (str::EqualI(s, "MultiDestFiles"))   flags |= FOF_MULTIDESTFILES;
				else if (str::EqualI(s, "NoConfirmation"))   flags |= FOF_NOCONFIRMATION;
				else if (str::EqualI(s, "NoConfirmMkDir"))   flags |= FOF_NOCONFIRMMKDIR;
				else if (str::EqualI(s, "NoConnectedElements")) flags |= FOF_NO_CONNECTED_ELEMENTS;
				else if (str::EqualI(s, "NoCopySecurityAttribs")) flags |= FOF_NOCOPYSECURITYATTRIBS;
				else if (str::EqualI(s, "NoErrorUI"))        flags |= FOF_NOERRORUI;
				else if (str::EqualI(s, "NoRecursion"))      flags |= FOF_NORECURSION;
				else if (str::EqualI(s, "NoUI"))             flags |= FOF_NO_UI;
				else if (str::EqualI(s, "RenameOnCollision")) flags |= FOF_RENAMEONCOLLISION;
				else if (str::EqualI(s, "Silent"))           flags |= FOF_SILENT;
				else if (str::EqualI(s, "SimpleProgress"))   flags |= FOF_SIMPLEPROGRESS;
				else if (str::EqualI(s, "WantNukeWarning"))  flags |= FOF_WANTNUKEWARNING;
				else std::cerr << std::format("Unknown flag: '{}'\n", s);
			});
			return flags;
		}

		void ShowHelp() const
		{
			std::cout <<
R"(Shell File Operation: Perform a file operation using the Windows Explorer shell
 Syntax: Conx -shcopy|-shmove|-shrename src,... dst,... [-flags flag,...] [-title "text"]
         Conx -shdelete src,... [-flags flag,...] [-title "text"]
  -shcopy   : Copy files from source(s) to destination(s)
  -shmove   : Move files from source(s) to destination(s)
  -shrename : Rename files
  -shdelete : Delete files (to recycle bin with AllowUndo flag)
  -flags    : Comma-separated flags: AllowUndo, FilesOnly, MultiDestFiles,
              NoConfirmation, NoConfirmMkDir, NoConnectedElements,
              NoCopySecurityAttribs, NoErrorUI, NoRecursion, NoUI,
              RenameOnCollision, Silent, SimpleProgress, WantNukeWarning
  -title    : Title for the progress dialog

 Returns 0 on success, 1 if aborted, or a SHFileOperation error code.
)";
		}

		int Run(CmdLine const& args)
		{
			if (args.count("help") != 0)
				return ShowHelp(), 0;

			SHFILEOPSTRUCTA fo = {};
			std::string src_buf, dst_buf;

			// Determine the operation
			if (args.count("shcopy") != 0)
			{
				fo.wFunc = FO_COPY;
				auto const& arg = args("shcopy");
				if (arg.num_values() < 2) { std::cerr << "-shcopy requires source and destination paths\n"; return ShowHelp(), -1; }
				fo.pFrom = BuildPathList(arg.values[0], src_buf);
				fo.pTo = BuildPathList(arg.values[1], dst_buf);
			}
			else if (args.count("shmove") != 0)
			{
				fo.wFunc = FO_MOVE;
				auto const& arg = args("shmove");
				if (arg.num_values() < 2) { std::cerr << "-shmove requires source and destination paths\n"; return ShowHelp(), -1; }
				fo.pFrom = BuildPathList(arg.values[0], src_buf);
				fo.pTo = BuildPathList(arg.values[1], dst_buf);
			}
			else if (args.count("shrename") != 0)
			{
				fo.wFunc = FO_RENAME;
				auto const& arg = args("shrename");
				if (arg.num_values() < 2) { std::cerr << "-shrename requires source and destination paths\n"; return ShowHelp(), -1; }
				fo.pFrom = BuildPathList(arg.values[0], src_buf);
				fo.pTo = BuildPathList(arg.values[1], dst_buf);
			}
			else if (args.count("shdelete") != 0)
			{
				fo.wFunc = FO_DELETE;
				auto const& arg = args("shdelete");
				if (arg.num_values() < 1) { std::cerr << "-shdelete requires source paths\n"; return ShowHelp(), -1; }
				fo.pFrom = BuildPathList(arg.values[0], src_buf);
			}
			else
			{
				std::cerr << "No shell operation specified\n";
				return ShowHelp(), -1;
			}

			// Parse optional flags and title
			if (args.count("flags") != 0)
				fo.fFlags = ParseFlags(args("flags").as<std::string>());

			std::string title;
			if (args.count("title") != 0)
			{
				title = args("title").as<std::string>();
				fo.lpszProgressTitle = title.c_str();
			}

			// Execute the operation.
			// Returns 0 for success, or an error code. See SHFileOperation docs.
			int res = SHFileOperationA(&fo);
			if (res == 0)
				res = fo.fAnyOperationsAborted ? 1 : 0;

			return res;
		}
	};

	int ShFileOp(CmdLine const& args)
	{
		Cmd_ShFileOp cmd;
		return cmd.Run(args);
	}
}
