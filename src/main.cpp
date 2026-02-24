//**********************************************
// Console Extensions
//  Copyright (c) Rylogic Ltd 2004
//**********************************************
#include "src/forward.h"
#include "src/commands/commands.h"
#include "src/common/process_util.h"
#include "src/common/cmd_line.h"

namespace conx
{
	static std::string_view Version = "1.0.0";

	struct Main
	{
		InitCom m_com;

		Main()
			:m_com(COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE)
		{}

		int Run(CmdLine& cmd_line)
		{
			// Get the name of this executable
			auto exepath = ExePath();
			auto path = exepath.parent_path();
			auto name = exepath.stem();
			auto extn = exepath.extension();

			// Look for a JSON file with the same name as this program in the local directory
			auto config = path / exepath.filename().replace_extension(L".json");
			if (std::filesystem::exists(config))
				return RunFromJson(config, cmd_line);

			// If the name of the exe is not 'conx', assume an implicit -exename as the first command line argument
			if (name != "conx")
				cmd_line.args.insert(begin(cmd_line.args), { std::format("{}", name.string()) });

			// Parse the command line
			auto IsOption = [&cmd_line](std::string_view option) -> bool
			{
				for (; !option.empty(); )
				{
					auto pos = option.find(',');

					if (cmd_line.count(std::string(option.substr(0, pos))) != 0)
						return true;

					if (pos == std::string::npos)
						break;

					option = option.substr(pos + 1);
				}
				return false;
			};

			// Handle -version before command dispatch
			if (cmd_line.count("version") != 0)
			{
				std::cout << Version << "\n";
				return 0;
			}

			// Forward to the appropriate command
			{
				#define CONX_CMD_OPTIONS(options, description, func) if (IsOption(options)) { return func(cmd_line); }
				CONX_CMD(CONX_CMD_OPTIONS);
				#undef CONX_CMD_OPTIONS
			}

			// If no commands given, display the command line help message
			std::cout << "\n"
				"  Console EXtensions (conx) v" << Version << "\n"
				"  Copyright (c) Rylogic 2004\n"
				"\n"
				"  Usage: conx -<command> [parameters]\n"
				"\n"
				"  Commands:\n"
				"\n";
			{
				// Find the longest command name for alignment
				size_t max_len = 0;
				#define CONX_CMD_MEASURE(options, description, func) max_len = (std::max)(max_len, std::string_view(options).size());
				CONX_CMD(CONX_CMD_MEASURE);
				#undef CONX_CMD_MEASURE

				#define CONX_CMD_OPTIONS(options, description, func) \
					std::cout << std::format("    {:<{}}  {}\n", options, max_len, description);
				CONX_CMD(CONX_CMD_OPTIONS);
				#undef CONX_CMD_OPTIONS
			}
			std::cout <<
				"\n"
				"  Use 'conx -<command> -help' for details on a specific command.\n"
				"\n"
				"  Proxy Mode:\n"
				"    Rename conx.exe and place a matching .json file alongside it:\n"
				"      { \"process\": \"...\", \"startdir\": \"...\", \"args\": [...] }\n"
				"    Or, without a .json file, the renamed exe acts as: conx -<exe_name>\n"
				"\n";
			return 0;
		}

		// Read 'config' and execute
		int RunFromJson(std::filesystem::path const& filepath, CmdLine& cmd_line)
		{
			try
			{
				// Load the file
				auto doc = json::Read(filepath, json::Options{ .AllowComments = true });
				auto root = doc.to_object();

				std::wstring process, startdir;

				// Read elements from the file
				if (auto jprocess = root.find("process"))
				{
					process = str::Widen(jprocess->to<std::string>());
				}
				if (auto jstartdir = root.find("startdir"))
				{
					startdir = str::Widen(jstartdir->to<std::string>());
				}
				if (auto jargs = root.find("args"))
				{
					for (auto& arg : jargs->to_array())
						cmd_line.args.push_back({ arg.to<std::string>() });
				}

				if (process.empty() || startdir.empty())
					throw std::runtime_error(std::format("JSON file '{}' must contain 'process' and 'startdir' elements", filepath.string()));

				// If a process name was given, execute it
				std::wstring args;
				for (auto& arg : cmd_line.args)
				{
					auto warg = str::Quotes<std::wstring>(str::Widen(arg.key), true);
					args.append(warg).append(L" ");
				}

				Process proc;
				if (proc.Start(process.c_str(), args.c_str(), startdir.c_str()))
					return proc.BlockTillExit();

				// Copy the error message before any other calls to Succeeded overwrite it
				auto err = Reason();

				ShowConsole();
				std::wcout << "Failed to start process: " << process.c_str() << "\n" << err.c_str() << "\n";
				return -1;
			}
			catch (std::exception const& ex)
			{
				std::wcout << "Failed to load " << filepath << std::endl << ex.what() << std::endl;
				return -1;
			}
		}
	};

}

// Run as a windows programso that the console window is not shown
int __stdcall wWinMain(HINSTANCE,HINSTANCE,LPWSTR lpCmdLine,int)
{
	using namespace conx;

	try
	{
		//MessageBox(0, "Paws'd", "Cex", MB_OK);

		// Connect stdout/stderr for command-line use.
		// If the parent set up stdout handles (via > redirect or Start-Process -RedirectStandardOutput),
		// wire the C runtime to those inherited handles. Otherwise, attach to the parent console.
		auto h_stdout = GetStdHandle(STD_OUTPUT_HANDLE);
		if (h_stdout != NULL && h_stdout != INVALID_HANDLE_VALUE)
		{
			// Stdout was redirected by the parent. Connect C runtime to the inherited handles.
			auto rewire = [](DWORD std_handle, FILE* stream, const char*)
			{
				auto h = GetStdHandle(std_handle);
				if (h == NULL || h == INVALID_HANDLE_VALUE) return;
				auto fd = _open_osfhandle(reinterpret_cast<intptr_t>(h), _O_TEXT);
				if (fd >= 0) _dup2(fd, _fileno(stream));
			};
			rewire(STD_OUTPUT_HANDLE, stdout, "w");
			rewire(STD_ERROR_HANDLE, stderr, "w");
			rewire(STD_INPUT_HANDLE, stdin, "r");
		}
		else if (AttachConsole(ATTACH_PARENT_PROCESS))
		{
			// No redirected handles. Attach to parent console for interactive use.
			(void)freopen("CONIN$", "r", stdin);
			(void)freopen("CONOUT$", "w", stdout);
			(void)freopen("CONOUT$", "w", stderr);
		}
		std::wcout.clear();
		std::cout.clear();
		std::wcerr.clear();
		std::cerr.clear();
		std::wcin.clear();
		std::cin.clear();

		// lpCmdLine doesn't include the program name, but CmdLine expects argv[0] to be the exe path
		auto cl = std::format("{} {}", ExePath().string(), str::Narrow(lpCmdLine));
		CmdLine cmd_line(cl);

		conx::Main m;
		return m.Run(cmd_line);
	}
	catch (std::exception const& ex)
	{
		std::cout << ex.what() << std::endl;
		return -1;
	}
}
int __cdecl main(int argc, char* argv[])
{
	using namespace conx;

	try
	{
		CmdLine cmd_line(argc, argv);

		conx::Main m;
		return m.Run(cmd_line);
	}
	catch (std::exception const& ex)
	{
		std::cout << ex.what() << std::endl;
		return -1;
	}
}