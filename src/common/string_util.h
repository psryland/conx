//**********************************************
// Console Extensions
//  Copyright (c) Rylogic Ltd 2004
//**********************************************
#pragma once
#include "src/forward.h"

namespace conx::str
{
	// Convert a string to lower case
	inline void LowerCase(std::string& s)
	{
		std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
	}

	// Convert a string to upper case
	inline void UpperCase(std::string& s)
	{
		std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
	}

	// Replace all occurrences of 'from' with 'to' in the string 's'
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

	// Case-insensitive string comparison
	inline bool EqualI(std::string_view a, std::string_view b)
	{
		if (a.size() != b.size()) return false;
		return std::equal(a.begin(), a.end(), b.begin(), [](unsigned char ca, unsigned char cb)
		{
			return std::tolower(ca) == std::tolower(cb);
		});
	}

	// Convert a UTF-8 string to wide characters
	inline std::wstring Widen(std::string_view s)
	{
		if (s.empty()) return {};
		auto len = MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
		std::wstring result(len, 0);
		MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), result.data(), len);
		return result;
	}

	// Convert a wide string to UTF-8
	inline std::string Narrow(std::wstring_view s)
	{
		if (s.empty()) return {};
		auto len = WideCharToMultiByte(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0, nullptr, nullptr);
		std::string result(len, 0);
		WideCharToMultiByte(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), result.data(), len, nullptr, nullptr);
		return result;
	}

	// Split a string by a delimiter, calling 'func' for each part with the part and its index
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

	// Escape special characters in a string for use in a C string literal
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

	// Add quotes around a string if it contains spaces, or if 'always' is true
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

	inline char* char_ptr(void* p) { return static_cast<char*>(p); }
}
