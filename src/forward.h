//**********************************************
// Console Extensions
//  Copyright (c) Rylogic Ltd 2004
//**********************************************

#pragma once

#include <string_view>
#include <vector>
#include <unordered_map>
#include <memory>
#include <iostream>
#include <string>
#include <sstream>
#include <fstream>
#include <filesystem>
#include <iterator>
#include <format>
#include <algorithm>
#include <cctype>
#include <charconv>
#include <stdexcept>
#include <array>

#define NOMINMAX
#include <sdkddkver.h>
#include <windows.h>
#include <process.h>
#include <shlobj.h>
#include <fcntl.h>
#include <io.h>
#include <shellapi.h>

#include "common/json.h"

// ── CmdLine ─────────────────────────────────────────────────────────
// Standalone command-line parser, replaces pr/common/command_line.h
namespace pr
{
	struct CmdLine
	{
		// A single parsed argument (flag + values)
		struct Arg
		{
			std::string key;
			std::vector<std::string> values;

			int num_values() const { return static_cast<int>(values.size()); }

			template <typename T>
			T as(int index = 0) const
			{
				if (index < 0 || index >= static_cast<int>(values.size()))
					throw std::runtime_error("Argument index out of range");
				return Convert<T>(values[index]);
			}

		private:
			template <typename T>
			static T Convert(std::string const& s)
			{
				if constexpr (std::is_same_v<T, std::string>)
					return s;
				else if constexpr (std::is_same_v<T, int>)
					return std::stoi(s);
				else if constexpr (std::is_same_v<T, unsigned int>)
					return static_cast<unsigned int>(std::stoul(s));
				else if constexpr (std::is_same_v<T, long>)
					return std::stol(s);
				else if constexpr (std::is_same_v<T, unsigned long>)
					return std::stoul(s);
				else if constexpr (std::is_same_v<T, double>)
					return std::stod(s);
				else if constexpr (std::is_same_v<T, float>)
					return std::stof(s);
				else if constexpr (std::is_same_v<T, bool>)
					return s == "true" || s == "1";
				else
					static_assert(!sizeof(T*), "Unsupported type");
			}
		};

		std::vector<Arg> args;
		std::unordered_map<std::string, int> m_map;

		CmdLine() = default;

		CmdLine(int argc, char* argv[])
		{
			Parse(argc, argv);
		}

		explicit CmdLine(std::string const& cmdline)
		{
			auto tokens = Tokenize(cmdline);
			std::vector<char*> ptrs;
			for (auto& t : tokens)
				ptrs.push_back(t.data());
			Parse(static_cast<int>(ptrs.size()), ptrs.data());
		}

		int count(std::string const& key) const
		{
			return m_map.count(key) ? 1 : 0;
		}

		Arg const& operator()(std::string const& key) const
		{
			auto it = m_map.find(key);
			if (it == m_map.end())
				throw std::runtime_error("Argument not found: " + key);
			return args[it->second];
		}

	private:

		void Parse(int argc, char* argv[])
		{
			for (int i = 1; i < argc; ++i)
			{
				std::string token = argv[i];

				if (!token.empty() && token[0] == '-')
				{
					auto key = token.substr(1);
					Arg arg;
					arg.key = key;

					// Collect all following non-dash tokens as values
					while (i + 1 < argc && argv[i + 1][0] != '-')
					{
						++i;
						arg.values.push_back(argv[i]);
					}

					auto idx = static_cast<int>(args.size());
					args.push_back(std::move(arg));
					m_map[key] = idx;
				}
				else
				{
					if (!args.empty())
						args.back().values.push_back(token);
					else
					{
						Arg arg;
						arg.key = token;
						auto idx = static_cast<int>(args.size());
						args.push_back(std::move(arg));
						m_map[token] = idx;
					}
				}
			}
		}

		static std::vector<std::string> Tokenize(std::string const& cmdline)
		{
			std::vector<std::string> tokens;
			std::string current;
			bool in_quotes = false;
			char quote_char = 0;

			for (size_t i = 0; i < cmdline.size(); ++i)
			{
				auto ch = cmdline[i];
				if (in_quotes)
				{
					if (ch == quote_char)
						in_quotes = false;
					else
						current += ch;
				}
				else if (ch == '"' || ch == '\'')
				{
					in_quotes = true;
					quote_char = ch;
				}
				else if (ch == ' ' || ch == '\t')
				{
					if (!current.empty())
					{
						tokens.push_back(std::move(current));
						current.clear();
					}
				}
				else
				{
					current += ch;
				}
			}
			if (!current.empty())
				tokens.push_back(std::move(current));

			return tokens;
		}
	};
}

// ── Win32 utilities ─────────────────────────────────────────────────
// Standalone replacements for pr/win32/win32.h
namespace pr
{
	namespace win32
	{
		inline std::filesystem::path ExePath()
		{
			wchar_t buf[MAX_PATH + 1] = {};
			GetModuleFileNameW(nullptr, buf, MAX_PATH);
			return std::filesystem::path(buf);
		}
	}

	inline std::wstring Widen(std::string_view s)
	{
		if (s.empty()) return {};
		auto len = MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
		std::wstring result(len, 0);
		MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), result.data(), len);
		return result;
	}

	inline std::string Narrow(std::wstring_view s)
	{
		if (s.empty()) return {};
		auto len = WideCharToMultiByte(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0, nullptr, nullptr);
		std::string result(len, 0);
		WideCharToMultiByte(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), result.data(), len, nullptr, nullptr);
		return result;
	}

	inline std::wstring Reason()
	{
		auto err = GetLastError();
		wchar_t buf[512] = {};
		FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
			nullptr, err, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), buf, 512, nullptr);
		return buf;
	}

	inline char* char_ptr(void* p) { return static_cast<char*>(p); }
}

// ── String utilities ────────────────────────────────────────────────
// Standalone replacements for pr/str/string_util.h
namespace pr::str
{
	inline void LowerCase(std::string& s)
	{
		std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
	}

	inline void UpperCase(std::string& s)
	{
		std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
	}

	inline void Replace(std::string& s, char const* from, char const* to)
	{
		std::string sfrom(from);
		std::string sto(to);
		if (sfrom.empty()) return;
		size_t pos = 0;
		while ((pos = s.find(sfrom, pos)) != std::string::npos)
		{
			s.replace(pos, sfrom.size(), sto);
			pos += sto.size();
		}
	}

	inline bool EqualI(std::string_view a, std::string_view b)
	{
		if (a.size() != b.size()) return false;
		return std::equal(a.begin(), a.end(), b.begin(), [](unsigned char ca, unsigned char cb)
		{
			return std::tolower(ca) == std::tolower(cb);
		});
	}

	template <typename Func>
	void Split(std::string_view s, std::string_view delim, Func&& func)
	{
		int index = 0;
		size_t pos = 0;
		while (pos <= s.size())
		{
			auto next = s.find(delim, pos);
			if (next == std::string_view::npos)
				next = s.size();
			func(s.substr(pos, next - pos), index++);
			pos = next + delim.size();
			if (next == s.size()) break;
		}
	}

	template <typename T>
	T StringToCString(std::string_view s)
	{
		T result;
		result.reserve(s.size());
		for (auto ch : s)
		{
			switch (ch)
			{
				case '\\': result += "\\\\"; break;
				case '"':  result += "\\\""; break;
				case '\n': result += "\\n";  break;
				case '\r': result += "\\r";  break;
				case '\t': result += "\\t";  break;
				default:   result += ch;     break;
			}
		}
		return result;
	}

	template <typename T>
	T Quotes(T const& s, bool always = false)
	{
		if (!always && s.find(' ') == T::npos)
			return s;
		T result;
		result.reserve(s.size() + 2);
		result += '"';
		result += s;
		result += '"';
		return result;
	}
}

// ── COM initialisation ──────────────────────────────────────────────
// Standalone replacement for pr/win32/windows_com.h
namespace pr
{
	struct InitCom
	{
		InitCom(DWORD flags = COINIT_APARTMENTTHREADED)
		{
			CoInitializeEx(nullptr, flags);
		}
		~InitCom()
		{
			CoUninitialize();
		}
		InitCom(InitCom const&) = delete;
		InitCom& operator=(InitCom const&) = delete;
	};
}

// ── Process wrapper ─────────────────────────────────────────────────
// Standalone replacement for pr/threads/process.h
namespace pr
{
	struct Process
	{
		HANDLE m_process = nullptr;

		~Process()
		{
			if (m_process)
				CloseHandle(m_process);
		}

		bool Start(wchar_t const* exe, wchar_t const* args, wchar_t const* working_dir)
		{
			// Build a mutable command line: "exe" args
			std::wstring cmd;
			cmd += L'"';
			cmd += exe;
			cmd += L'"';
			if (args && args[0])
			{
				cmd += L' ';
				cmd += args;
			}

			STARTUPINFOW si = { sizeof(si) };
			PROCESS_INFORMATION pi = {};
			auto ok = CreateProcessW(
				nullptr,
				cmd.data(),
				nullptr,
				nullptr,
				FALSE,
				0,
				nullptr,
				working_dir,
				&si,
				&pi
			);
			if (!ok)
				return false;

			CloseHandle(pi.hThread);
			m_process = pi.hProcess;
			return true;
		}

		int BlockTillExit()
		{
			if (!m_process) return -1;
			WaitForSingleObject(m_process, INFINITE);
			DWORD exit_code = 0;
			GetExitCodeProcess(m_process, &exit_code);
			return static_cast<int>(exit_code);
		}
	};
}

namespace conx
{
	// Show the console for this process
	void ShowConsole();

	// Add an environment variable
	void SetEnvVar(std::string_view env_var, std::string_view value);
}
