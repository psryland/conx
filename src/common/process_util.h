//**********************************************
// Console Extensions
//  Copyright (c) Rylogic Ltd 2004
//**********************************************
// Shared utilities for process/window enumeration
#pragma once
#include "src/forward.h"

namespace conx
{
	// Return the path to the current executable
	inline std::filesystem::path ExePath()
	{
		wchar_t buf[MAX_PATH + 1] = {};
		GetModuleFileNameW(nullptr, buf, MAX_PATH);
		return std::filesystem::path(buf);
	}

	// Find all process IDs whose name contains 'name' (case-insensitive substring match)
	inline std::vector<DWORD> FindProcesses(std::string const& name)
	{
		std::vector<DWORD> pids;
		auto snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
		if (snap == INVALID_HANDLE_VALUE)
			return pids;

		PROCESSENTRY32 pe = { .dwSize = sizeof(pe) };
		for (auto ok = Process32First(snap, &pe); ok; ok = Process32Next(snap, &pe))
		{
			std::string exe_name = pe.szExeFile;
			auto iname = name;
			auto iexe  = exe_name;
			std::transform(iname.begin(), iname.end(), iname.begin(), [](char c) { return static_cast<char>(tolower(c)); });
			std::transform(iexe.begin(),  iexe.end(),  iexe.begin(),  [](char c) { return static_cast<char>(tolower(c)); });
			if (iexe.find(iname) != std::string::npos)
				pids.push_back(pe.th32ProcessID);
		}
		CloseHandle(snap);
		return pids;
	}

	// Get the title text of a window
	inline std::string GetWindowTitle(HWND hwnd)
	{
		auto len = GetWindowTextLengthA(hwnd);
		if (len <= 0)
			return {};

		std::string title(len + 1, '\0');
		GetWindowTextA(hwnd, title.data(), static_cast<int>(title.size()));
		title.resize(len);
		return title;
	}

	// Enumerate windows belonging to the given PIDs
	inline std::vector<HWND> FindWindows(std::vector<DWORD> const& pids, bool include_hidden = false)
	{
		struct EnumData
		{
			std::vector<DWORD> const* pids;
			std::vector<HWND> windows;
			bool include_hidden;
		};
		EnumData data = { &pids, {}, include_hidden };

		EnumWindows([](HWND hwnd, LPARAM lparam) -> BOOL
		{
			auto& d = *reinterpret_cast<EnumData*>(lparam);

			if (!d.include_hidden && !IsWindowVisible(hwnd))
				return TRUE;

			DWORD pid = 0;
			GetWindowThreadProcessId(hwnd, &pid);
			if (std::find(d.pids->begin(), d.pids->end(), pid) == d.pids->end())
				return TRUE;

			RECT rc;
			if (!GetWindowRect(hwnd, &rc))
				return TRUE;
			if (rc.right - rc.left <= 0 || rc.bottom - rc.top <= 0)
				return TRUE;

			d.windows.push_back(hwnd);
			return TRUE;
		}, reinterpret_cast<LPARAM>(&data));

		return data.windows;
	}

	// Find a window of a process. If 'window_name' is provided, match by title (case-insensitive substring).
	// Otherwise, return the window with the largest area.
	inline HWND FindWindow(std::string const& process_name, std::string const& window_name = {})
	{
		auto pids = FindProcesses(process_name);
		if (pids.empty())
			return nullptr;

		auto windows = FindWindows(pids, true);
		if (windows.empty())
			return nullptr;

		// If a window name filter is given, find the first match
		if (!window_name.empty())
		{
			auto iwname = window_name;
			std::transform(iwname.begin(), iwname.end(), iwname.begin(), [](char c) { return static_cast<char>(tolower(c)); });

			for (auto hwnd : windows)
			{
				auto ititle = GetWindowTitle(hwnd);
				std::transform(ititle.begin(), ititle.end(), ititle.begin(), [](char c) { return static_cast<char>(tolower(c)); });
				if (ititle.find(iwname) != std::string::npos)
					return hwnd;
			}
			return nullptr;
		}

		// No window name given — return the largest window by area
		HWND best = nullptr;
		long best_area = 0;
		for (auto hwnd : windows)
		{
			RECT rc;
			if (!GetWindowRect(hwnd, &rc))
				continue;

			auto area = (rc.right - rc.left) * (rc.bottom - rc.top);
			if (area > best_area)
			{
				best_area = area;
				best = hwnd;
			}
		}
		return best;
	}

	// Convert client-area coordinates to normalised absolute screen coordinates for SendInput
	inline POINT ClientToAbsScreen(HWND hwnd, int client_x, int client_y)
	{
		POINT pt = { client_x, client_y };
		ClientToScreen(hwnd, &pt);

		// SendInput uses normalised coordinates: 0..65535 mapped to the virtual screen
		auto sx = GetSystemMetrics(SM_XVIRTUALSCREEN);
		auto sy = GetSystemMetrics(SM_YVIRTUALSCREEN);
		auto sw = GetSystemMetrics(SM_CXVIRTUALSCREEN);
		auto sh = GetSystemMetrics(SM_CYVIRTUALSCREEN);

		pt.x = static_cast<LONG>((pt.x - sx) * 65535LL / (sw - 1));
		pt.y = static_cast<LONG>((pt.y - sy) * 65535LL / (sh - 1));
		return pt;
	}

	// ── Background (WM_ message) helpers ──────────────────────────────────────────
	// These post WM_ messages directly to window handles, avoiding SendInput and
	// the need to bring the target to the foreground. Works for native Win32 apps;
	// Electron/Chromium apps should fall back to SendInput.

	// Walk ChildWindowFromPointEx recursively to find the deepest child at a point.
	// Coordinates are relative to 'parent's client area.
	inline HWND FindChildAtPoint(HWND parent, int client_x, int client_y)
	{
		POINT pt = { client_x, client_y };
		auto current = parent;
		for (;;)
		{
			auto child = ChildWindowFromPointEx(current, pt, CWP_SKIPINVISIBLE | CWP_SKIPDISABLED | CWP_SKIPTRANSPARENT);
			if (!child || child == current)
				break;

			// Convert the point from current's client space into child's client space
			MapWindowPoints(current, child, &pt, 1);
			current = child;
		}
		return current;
	}

	// Find the first visible child window whose class name contains 'class_name' (case-insensitive).
	inline HWND FindChildByClass(HWND parent, std::string const& class_name)
	{
		struct EnumData
		{
			std::string target;
			HWND result;
		};

		auto lower = class_name;
		std::transform(lower.begin(), lower.end(), lower.begin(), [](char c) { return static_cast<char>(tolower(c)); });

		EnumData data = { lower, nullptr };
		EnumChildWindows(parent, [](HWND hwnd, LPARAM lparam) -> BOOL
		{
			auto& d = *reinterpret_cast<EnumData*>(lparam);
			if (!IsWindowVisible(hwnd))
				return TRUE;

			char buf[256] = {};
			GetClassNameA(hwnd, buf, sizeof(buf));
			std::string cls = buf;
			std::transform(cls.begin(), cls.end(), cls.begin(), [](char c) { return static_cast<char>(tolower(c)); });

			if (cls.find(d.target) != std::string::npos)
			{
				d.result = hwnd;
				return FALSE; // stop enumeration
			}
			return TRUE;
		}, reinterpret_cast<LPARAM>(&data));
		return data.result;
	}

	// Post a mouse event via WM_ messages. Coordinates are relative to 'parent's client area.
	// Automatically resolves the deepest child at the point and converts coords.
	inline void PostMouseMsg(HWND parent, int client_x, int client_y, DWORD action)
	{
		POINT pt = { client_x, client_y };
		auto target = FindChildAtPoint(parent, client_x, client_y);

		// Convert from parent's client space to target's client space
		if (target != parent)
			MapWindowPoints(parent, target, &pt, 1);

		auto lparam = MAKELPARAM(static_cast<short>(pt.x), static_cast<short>(pt.y));

		// Map MOUSEEVENTF_* flags to WM_ messages
		if (action & MOUSEEVENTF_LEFTDOWN)   PostMessage(target, WM_LBUTTONDOWN, MK_LBUTTON, lparam);
		if (action & MOUSEEVENTF_LEFTUP)     PostMessage(target, WM_LBUTTONUP,   0,          lparam);
		if (action & MOUSEEVENTF_RIGHTDOWN)  PostMessage(target, WM_RBUTTONDOWN, MK_RBUTTON, lparam);
		if (action & MOUSEEVENTF_RIGHTUP)    PostMessage(target, WM_RBUTTONUP,   0,          lparam);
		if (action & MOUSEEVENTF_MIDDLEDOWN) PostMessage(target, WM_MBUTTONDOWN, MK_MBUTTON, lparam);
		if (action & MOUSEEVENTF_MIDDLEUP)   PostMessage(target, WM_MBUTTONUP,   0,          lparam);

		// If no button flags at all, treat as a move
		auto const button_mask = MOUSEEVENTF_LEFTDOWN | MOUSEEVENTF_LEFTUP
			| MOUSEEVENTF_RIGHTDOWN | MOUSEEVENTF_RIGHTUP
			| MOUSEEVENTF_MIDDLEDOWN | MOUSEEVENTF_MIDDLEUP;
		if ((action & button_mask) == 0)
			PostMessage(target, WM_MOUSEMOVE, 0, lparam);
	}

	// Post a WM_CHAR message for a single character.
	inline void PostCharMsg(HWND hwnd, char ch)
	{
		PostMessage(hwnd, WM_CHAR, static_cast<WPARAM>(ch), 1);
	}

	// Build the lParam for WM_KEYDOWN / WM_KEYUP from a virtual key code.
	inline LPARAM MakeKeyLParam(WORD vk, bool key_up)
	{
		auto scan = MapVirtualKey(vk, MAPVK_VK_TO_VSC);
		LPARAM lp = 1; // repeat count = 1
		lp |= (static_cast<LPARAM>(scan) & 0xFF) << 16;

		// Extended key flag for navigation/numpad keys
		bool extended = (vk >= VK_PRIOR && vk <= VK_DELETE) || vk == VK_DIVIDE || vk == VK_NUMLOCK
			|| vk == VK_RCONTROL || vk == VK_RMENU;
		if (extended)
			lp |= (1LL << 24);

		if (key_up)
		{
			lp |= (1LL << 30); // previous key state
			lp |= (1LL << 31); // transition state
		}
		return lp;
	}

	// Post WM_KEYDOWN for a virtual key.
	inline void PostVKeyDown(HWND hwnd, WORD vk)
	{
		PostMessage(hwnd, WM_KEYDOWN, vk, MakeKeyLParam(vk, false));
	}

	// Post WM_KEYUP for a virtual key.
	inline void PostVKeyUp(HWND hwnd, WORD vk)
	{
		PostMessage(hwnd, WM_KEYUP, vk, MakeKeyLParam(vk, true));
	}

	// Bring a window to the foreground, working around SetForegroundWindow restrictions.
	// If 'click' is true, a mouse click is sent to the centre of the client area to
	// ensure keyboard focus lands inside the window's content control.
	inline bool BringToForeground(HWND hwnd, bool click = false)
	{
		// Restore if minimised
		if (IsIconic(hwnd))
			ShowWindow(hwnd, SW_RESTORE);

		// Simulate an Alt key press. Windows only allows SetForegroundWindow to succeed
		// if the calling process received the last input event. Injecting a keypress
		// via SendInput satisfies this requirement.
		INPUT alt = {};
		alt.type = INPUT_KEYBOARD;
		alt.ki.wVk = VK_MENU;
		alt.ki.dwFlags = 0;
		SendInput(1, &alt, sizeof(INPUT));

		SetForegroundWindow(hwnd);

		// Release the Alt key
		alt.ki.dwFlags = KEYEVENTF_KEYUP;
		SendInput(1, &alt, sizeof(INPUT));

		// Wait for the window to come to the foreground
		for (int i = 0; i != 20; ++i)
		{
			if (GetForegroundWindow() == hwnd)
				break;
			Sleep(50);
		}

		// Allow the window time to fully activate and be ready for input
		Sleep(200);

		// Click the centre of the client area to ensure keyboard focus is inside the
		// window's content control (e.g. Notepad's RichEditD2DPT child).
		if (click)
		{
			RECT rc;
			GetClientRect(hwnd, &rc);
			auto cx = (rc.left + rc.right) / 2;
			auto cy = (rc.top + rc.bottom) / 2;

			auto pt = ClientToAbsScreen(hwnd, cx, cy);
			INPUT inputs[3] = {};

			// Move to centre
			inputs[0].type = INPUT_MOUSE;
			inputs[0].mi.dx = pt.x;
			inputs[0].mi.dy = pt.y;
			inputs[0].mi.dwFlags = MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_VIRTUALDESK | MOUSEEVENTF_MOVE;

			// Left button down
			inputs[1].type = INPUT_MOUSE;
			inputs[1].mi.dx = pt.x;
			inputs[1].mi.dy = pt.y;
			inputs[1].mi.dwFlags = MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_VIRTUALDESK | MOUSEEVENTF_LEFTDOWN;

			// Left button up
			inputs[2].type = INPUT_MOUSE;
			inputs[2].mi.dx = pt.x;
			inputs[2].mi.dy = pt.y;
			inputs[2].mi.dwFlags = MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_VIRTUALDESK | MOUSEEVENTF_LEFTUP;

			SendInput(3, inputs, sizeof(INPUT));
			Sleep(100);
		}

		return true;
	}
}
