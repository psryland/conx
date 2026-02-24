//**********************************************
// Console Extensions
//  Copyright (c) Rylogic Ltd 2004
//**********************************************
// Standalone command-line parser
#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <stdexcept>
#include <type_traits>

namespace conx
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
