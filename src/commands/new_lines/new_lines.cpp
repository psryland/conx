//**********************************************
// Console Extensions
//  Copyright (c) Rylogic Ltd 2004
//**********************************************
#include "src/forward.h"

namespace conx
{
	struct Cmd_NewLines
	{
		std::filesystem::path m_infile;
		std::filesystem::path m_outfile;
		int m_min, m_max;
		std::string m_lineends;
		bool m_replace_infile;

		Cmd_NewLines()
			:m_infile()
			,m_outfile()
			,m_min(0)
			,m_max(std::numeric_limits<int>::max())
			,m_lineends()
			,m_replace_infile()
		{}

		void ShowHelp() const
		{
			std::cout <<
				"Add or remove new lines from a text file\n"
				" Syntax: conx -newlines -f 'FileToFormat' [-o 'OutputFilename'] [-limit min max] [-lineends end-style]\n"
				"    -f <filepath> : The file to format\n"
				"    -o <out-filepath> : Output filename\n"
				"    -limit min max : Set limits on the number of consecutive new lines\n"
				"    -lineends end-style : Replace line ends with CR, LF, CRLF, or LFCR\n";
		}

		int Run(CmdLine const& args)
		{
			using namespace std::filesystem;

			if (args.count("help") != 0)
				return ShowHelp(), 0;

			// Parse arguments
			if (args.count("f") != 0) { m_infile = args("f").as<std::string>(); }
			if (args.count("o") != 0) { m_outfile = args("o").as<std::string>(); }
			if (args.count("limit") != 0)
			{
				auto const& arg = args("limit");
				m_min = arg.as<int>(0);
				m_max = arg.as<int>(1);
			}
			if (args.count("lineends") != 0)
			{
				m_lineends = args("lineends").as<std::string>();
				str::LowerCase(m_lineends);
				str::Replace(m_lineends, "cr", "\r");
				str::Replace(m_lineends, "lf", "\n");
			}

			// Validate input
			m_replace_infile = m_outfile.empty();
			if (m_replace_infile)
				m_outfile = path(m_infile).concat(".tmp");

			if (!exists(m_infile))
				throw std::runtime_error(std::format("Input file '{}' doesn't exist", m_infile.string()));

			if (m_lineends.empty())
				m_lineends = "\n";

			// Read the input file and process newlines
			std::cout << "Running formatting...";

			std::ifstream ifile(m_infile, std::ios::binary);
			if (!ifile)
				throw std::runtime_error(std::format("Failed to open input file '{}'\n", m_infile.string()));

			std::ofstream ofile(m_outfile, std::ios::binary);
			if (!ofile)
				throw std::runtime_error(std::format("Failed to create output file '{}'\n", m_outfile.string()));

			// Process the file: read character by character, normalise line endings,
			// and clamp consecutive newlines to [m_min, m_max].
			int consecutive_newlines = 0;
			char ch;
			while (ifile.get(ch))
			{
				// Normalise CR/LF/CRLF to a single logical newline
				if (ch == '\r')
				{
					// Peek for a following LF (CRLF pair)
					if (ifile.peek() == '\n')
						ifile.get();

					++consecutive_newlines;
					continue;
				}
				if (ch == '\n')
				{
					++consecutive_newlines;
					continue;
				}

				// We have a non-newline character. Flush any pending newlines (clamped to [min, max]).
				if (consecutive_newlines > 0)
				{
					auto count = std::clamp(consecutive_newlines, m_min, m_max);
					for (int i = 0; i != count; ++i)
						ofile << m_lineends;

					consecutive_newlines = 0;
				}

				ofile.put(ch);
			}

			// Flush any trailing newlines
			if (consecutive_newlines > 0)
			{
				auto count = std::clamp(consecutive_newlines, m_min, m_max);
				for (int i = 0; i != count; ++i)
					ofile << m_lineends;
			}

			ofile.close();
			std::cout << "done\n";

			// If we're replacing the input file...
			if (m_replace_infile)
			{
				try { copy_file(m_outfile, m_infile, copy_options::overwrite_existing); }
				catch (filesystem_error const& ex)
				{
					std::cout << std::format("Failed to replace '{}' with '{}'\n{}\n", m_infile.string(), m_outfile.string(), ex.code().message());
				}

				// Clean up the temp file
				std::error_code ec;
				remove(m_outfile, ec);
			}

			return 0;
		}
	};

	int NewLines(CmdLine const& args)
	{
		Cmd_NewLines cmd;
		return cmd.Run(args);
	}
}
