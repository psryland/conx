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
#include <cstdio>
#include <charconv>
#include <stdexcept>
#include <array>

#include <sdkddkver.h>
#include <windows.h>
#include <process.h>
#include <shlobj.h>
#include <fcntl.h>
#include <io.h>
#include <objbase.h>
#include <shellapi.h>
#include <tlhelp32.h>
#include <gdiplus.h>

#include "common/json.h"
#include "common/cmd_line.h"
#include "common/string_util.h"
#include "common/com_init.h"
#include "common/process.h"
#include "common/console_util.h"

namespace conx
{
	// Get a string describing the last error code
	inline std::wstring Reason()
	{
		wchar_t buf[512] = {};
		auto err = GetLastError();
		FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, err, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), buf, 512, nullptr);
		return buf;
	}
}