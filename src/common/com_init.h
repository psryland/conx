//**********************************************
// Console Extensions
//  Copyright (c) Rylogic Ltd 2004
//**********************************************
// Standalone COM initialisation wrapper
#pragma once
#include "src/forward.h"

namespace conx
{
	struct InitCom
	{
		InitCom(DWORD flags = COINIT_APARTMENTTHREADED)
		{
			auto r = CoInitializeEx(nullptr, flags);
			if (FAILED(r))
				throw std::runtime_error("Failed to initialize COM");
		}
		~InitCom()
		{
			CoUninitialize();
		}
		InitCom(InitCom const&) = delete;
		InitCom& operator=(InitCom const&) = delete;
	};
}
