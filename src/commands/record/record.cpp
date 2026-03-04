//**********************************************
// Console Extensions
//  Copyright (c) Rylogic Ltd 2004
//**********************************************
// Record: Capture a sequence of frames from a window or screen region.
// Outputs as numbered PNGs (to a directory) or as an MP4 video (to a .mp4 file).
#include "src/forward.h"
#include "src/common/process_util.h"

#include <gdiplus.h>
#pragma comment(lib, "gdiplus.lib")

#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <mferror.h>
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfuuid.lib")

namespace conx
{
	// -----------------------------------------------------------------------
	// Mp4Writer — Wraps IMFSinkWriter for encoding frames to an MP4 file.
	// -----------------------------------------------------------------------
	struct Mp4Writer
	{
		IMFSinkWriter* m_writer = nullptr;
		DWORD m_stream_index = 0;
		UINT32 m_width = 0;
		UINT32 m_height = 0;
		UINT32 m_fps = 0;
		LONGLONG m_frame_duration = 0;
		bool m_using_hevc = false;

		~Mp4Writer() { Close(); }

		bool Open(std::filesystem::path const& filepath, UINT32 width, UINT32 height, UINT32 fps)
		{
			auto hr = MFStartup(MF_VERSION);
			if (FAILED(hr)) { std::cerr << "MFStartup failed\n"; return false; }

			// Dimensions must be even for most video encoders
			m_width  = (width  + 1) & ~1u;
			m_height = (height + 1) & ~1u;
			m_fps = fps;
			m_frame_duration = 10'000'000LL / fps; // 100ns units

			// Create the sink writer
			IMFAttributes* attrs = nullptr;
			MFCreateAttributes(&attrs, 1);
			if (attrs)
			{
				attrs->SetUINT32(MF_READWRITE_ENABLE_HARDWARE_TRANSFORMS, TRUE);
			}

			auto wide_path = filepath.wstring();
			hr = MFCreateSinkWriterFromURL(wide_path.c_str(), nullptr, attrs, &m_writer);
			if (attrs) attrs->Release();
			if (FAILED(hr)) { std::cerr << "Failed to create sink writer\n"; return false; }

			// Configure the output stream — try H.265 (HEVC) first, fall back to H.264
			if (!ConfigureOutputStream(MFVideoFormat_HEVC))
			{
				if (!ConfigureOutputStream(MFVideoFormat_H264))
				{
					std::cerr << "Failed to configure video encoder (neither H.265 nor H.264 available)\n";
					return false;
				}
				std::cout << "Using H.264 encoder\n";
			}
			else
			{
				m_using_hevc = true;
				std::cout << "Using H.265 (HEVC) encoder\n";
			}

			// Configure the input stream — we feed raw BGRA frames
			IMFMediaType* input_type = nullptr;
			MFCreateMediaType(&input_type);
			input_type->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
			input_type->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_RGB32);
			MFSetAttributeSize(input_type, MF_MT_FRAME_SIZE, m_width, m_height);
			MFSetAttributeRatio(input_type, MF_MT_FRAME_RATE, m_fps, 1);
			input_type->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);

			hr = m_writer->SetInputMediaType(m_stream_index, input_type, nullptr);
			input_type->Release();
			if (FAILED(hr)) { std::cerr << "Failed to set input media type\n"; return false; }

			hr = m_writer->BeginWriting();
			if (FAILED(hr)) { std::cerr << "Failed to begin writing\n"; return false; }

			return true;
		}

		bool WriteFrame(HBITMAP hbm, LONGLONG timestamp)
		{
			if (!m_writer) return false;

			// Get bitmap pixel data
			BITMAP bm = {};
			GetObject(hbm, sizeof(bm), &bm);

			auto stride = static_cast<LONG>(m_width * 4);
			auto buffer_size = static_cast<DWORD>(stride * m_height);

			// Extract BGRA pixels from the HBITMAP
			BITMAPINFOHEADER bmi = {};
			bmi.biSize = sizeof(bmi);
			bmi.biWidth = static_cast<LONG>(m_width);
			bmi.biHeight = static_cast<LONG>(m_height); // Bottom-up (MF RGB32 expects this)
			bmi.biPlanes = 1;
			bmi.biBitCount = 32;
			bmi.biCompression = BI_RGB;

			auto pixels = std::make_unique<uint8_t[]>(buffer_size);
			auto hdc = GetDC(nullptr);
			GetDIBits(hdc, hbm, 0, m_height, pixels.get(), reinterpret_cast<BITMAPINFO*>(&bmi), DIB_RGB_COLORS);
			ReleaseDC(nullptr, hdc);

			// Create MF buffer and sample
			IMFMediaBuffer* buffer = nullptr;
			auto hr = MFCreateMemoryBuffer(buffer_size, &buffer);
			if (FAILED(hr)) return false;

			BYTE* buf_data = nullptr;
			buffer->Lock(&buf_data, nullptr, nullptr);
			memcpy(buf_data, pixels.get(), buffer_size);
			buffer->Unlock();
			buffer->SetCurrentLength(buffer_size);

			IMFSample* sample = nullptr;
			MFCreateSample(&sample);
			sample->AddBuffer(buffer);
			sample->SetSampleTime(timestamp);
			sample->SetSampleDuration(m_frame_duration);

			hr = m_writer->WriteSample(m_stream_index, sample);

			sample->Release();
			buffer->Release();

			return SUCCEEDED(hr);
		}

		void Close()
		{
			if (m_writer)
			{
				m_writer->Finalize();
				m_writer->Release();
				m_writer = nullptr;
			}
			MFShutdown();
		}

	private:

		bool ConfigureOutputStream(GUID const& subtype)
		{
			IMFMediaType* output_type = nullptr;
			MFCreateMediaType(&output_type);
			output_type->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
			output_type->SetGUID(MF_MT_SUBTYPE, subtype);
			output_type->SetUINT32(MF_MT_AVG_BITRATE, 5'000'000); // 5 Mbps
			MFSetAttributeSize(output_type, MF_MT_FRAME_SIZE, m_width, m_height);
			MFSetAttributeRatio(output_type, MF_MT_FRAME_RATE, m_fps, 1);
			output_type->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);

			auto hr = m_writer->AddStream(output_type, &m_stream_index);
			output_type->Release();
			return SUCCEEDED(hr);
		}
	};

	// -----------------------------------------------------------------------
	// Cmd_Record
	// -----------------------------------------------------------------------
	struct Cmd_Record
	{
		void ShowHelp() const
		{
			std::cout <<
				"Record: Capture a sequence of frames from a window or screen region\n"
				" Syntax: Conx -record -o <output> [-p <process-name>] [-rect x,y,w,h]\n"
				"         [-fps N] [-duration N] [-scale N] [-bitblt]\n"
				"\n"
				"  Target (one required):\n"
				"  -p        : Name (or partial name) of the process to capture.\n"
				"              Captures the main window of that process.\n"
				"  -rect     : Screen region to capture as x,y,w,h (absolute screen coordinates).\n"
				"\n"
				"  Output:\n"
				"  -o        : Output path (required).\n"
				"              If a directory or path without extension: saves numbered PNGs.\n"
				"              If path ends in .mp4: encodes video using H.265/H.264.\n"
				"\n"
				"  Options:\n"
				"  -fps      : Frames per second to capture (default: 10, range: 1-60).\n"
				"  -duration : Duration to record in seconds (default: 5).\n"
				"  -scale    : Scale factor for output images (e.g. 0.5 for half size).\n"
				"  -bitblt   : Capture from screen DC instead of PrintWindow.\n"
				"              Required for GPU-rendered apps (Electron, games, etc.).\n"
				"              The window must be visible and in the foreground.\n"
				"\n"
				"  PNG output: files are named frame_0001.png, frame_0002.png, etc.\n"
				"  MP4 output: single file, uses hardware H.265 encoding if available.\n"
				"  Reports actual capture rate on completion.\n";
		}

		int Run(CmdLine const& args)
		{
			if (args.count("help") != 0)
				return ShowHelp(), 0;

			// Output path (required)
			std::filesystem::path output;
			if (args.count("o") != 0)
				output = args("o").as<std::string>();

			if (output.empty()) { std::cerr << "No output path provided (-o)\n"; return ShowHelp(), -1; }

			// Determine output mode from the path
			auto ext = output.extension().string();
			for (auto& c : ext) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
			bool mp4_mode = (ext == ".mp4");

			// Target: process window or screen rect
			std::string process_name;
			if (args.count("p") != 0)
				process_name = args("p").as<std::string>();

			RECT capture_rect = {};
			bool use_rect = false;
			if (args.count("rect") != 0)
			{
				auto rect_str = args("rect").as<std::string>();
				if (!ParseRect(rect_str, capture_rect))
				{
					std::cerr << std::format("Invalid rect format '{}'. Expected: x,y,w,h\n", rect_str);
					return -1;
				}
				use_rect = true;
			}

			if (process_name.empty() && !use_rect)
			{
				std::cerr << "No capture target specified. Use -p <process> or -rect x,y,w,h\n";
				return ShowHelp(), -1;
			}

			auto fps        = args.count("fps") != 0 ? args("fps").as<int>() : 10;
			auto duration   = args.count("duration") != 0 ? args("duration").as<double>() : 5.0;
			auto scale      = args.count("scale") != 0 ? args("scale").as<double>() : 1.0;
			auto use_bitblt = args.count("bitblt") != 0;

			fps = (std::max)(1, (std::min)(fps, 60));

			// Resolve window handle if capturing a process
			HWND hwnd = nullptr;
			if (!process_name.empty())
			{
				hwnd = FindWindow(process_name, "");
				if (!hwnd)
				{
					std::cerr << std::format("No window found for '{}'\n", process_name);
					return -1;
				}
				std::cout << std::format("Recording '{}'\n", GetWindowTitle(hwnd));
			}

			// Get initial capture dimensions (needed for MP4 writer setup)
			RECT initial_rc;
			if (use_rect)
			{
				initial_rc = capture_rect;
			}
			else
			{
				if (!GetWindowRect(hwnd, &initial_rc))
				{
					std::cerr << "Failed to get window rect\n";
					return -1;
				}
			}

			auto frame_w = initial_rc.right - initial_rc.left;
			auto frame_h = initial_rc.bottom - initial_rc.top;
			if (scale > 0.0 && scale != 1.0)
			{
				frame_w = (std::max)(1L, static_cast<LONG>(frame_w * scale));
				frame_h = (std::max)(1L, static_cast<LONG>(frame_h * scale));
			}

			// Initialise GDI+ (needed for PNG mode)
			Gdiplus::GdiplusStartupInput gdiplus_input;
			ULONG_PTR gdiplus_token = 0;
			Gdiplus::GdiplusStartup(&gdiplus_token, &gdiplus_input, nullptr);
			struct GdiPlusGuard { ULONG_PTR token; ~GdiPlusGuard() { Gdiplus::GdiplusShutdown(token); } } gdiplus_guard{ gdiplus_token };

			// Initialise MP4 writer if needed
			Mp4Writer mp4;
			if (mp4_mode)
			{
				std::filesystem::create_directories(output.parent_path());
				if (!mp4.Open(output, static_cast<UINT32>(frame_w), static_cast<UINT32>(frame_h), static_cast<UINT32>(fps)))
				{
					std::cerr << "Failed to initialise MP4 encoder\n";
					return -1;
				}
			}
			else
			{
				std::filesystem::create_directories(output);
			}

			auto total_frames = static_cast<int>(fps * duration);
			auto frame_interval_ms = 1000.0 / fps;

			std::cout << std::format("Recording {} frames at {}fps for {:.1f}s ({})...\n",
				total_frames, fps, duration, mp4_mode ? "MP4" : "PNG");

			int captured = 0;
			auto start_time = GetTickCount64();

			for (int i = 0; i < total_frames; ++i)
			{
				auto frame_start = GetTickCount64();

				RECT rc;
				if (use_rect)
				{
					rc = capture_rect;
				}
				else
				{
					if (!hwnd || !GetWindowRect(hwnd, &rc))
					{
						std::cerr << std::format("Failed to get window rect at frame {}\n", i + 1);
						break;
					}
				}

				// Capture frame to HBITMAP
				auto hbm = CaptureFrame(hwnd, rc, use_bitblt || use_rect, scale);
				if (!hbm)
				{
					std::cerr << std::format("Failed to capture frame {}\n", i + 1);
					continue;
				}

				bool ok = false;
				if (mp4_mode)
				{
					auto timestamp = static_cast<LONGLONG>(i) * mp4.m_frame_duration;
					ok = mp4.WriteFrame(hbm, timestamp);
				}
				else
				{
					auto filepath = output / std::format("frame_{:04d}.png", i + 1);
					Gdiplus::Bitmap bmp(hbm, nullptr);
					ok = SaveBitmapAsPng(bmp, filepath) == Gdiplus::Ok;
				}

				DeleteObject(hbm);

				if (ok) ++captured;

				// Sleep to maintain target frame rate
				auto elapsed = GetTickCount64() - frame_start;
				auto sleep_ms = static_cast<DWORD>(frame_interval_ms - elapsed);
				if (sleep_ms > 0 && sleep_ms < 10000)
					Sleep(sleep_ms);
			}

			// Finalise MP4
			if (mp4_mode)
				mp4.Close();

			auto total_elapsed_ms = GetTickCount64() - start_time;
			auto actual_fps = captured > 0 ? (captured * 1000.0 / total_elapsed_ms) : 0.0;

			std::cout << std::format("{} frame(s) captured in {:.1f}s (actual rate: {:.1f}fps)\n",
				captured, total_elapsed_ms / 1000.0, actual_fps);

			if (mp4_mode && captured > 0)
			{
				auto file_size = std::filesystem::file_size(output);
				std::cout << std::format("Output: {} ({:.1f} KB)\n", output.filename().string(), file_size / 1024.0);
			}

			return captured > 0 ? 0 : -1;
		}

	private:

		static bool ParseRect(std::string const& str, RECT& rc)
		{
			int x = 0, y = 0, w = 0, h = 0;
			auto count = sscanf_s(str.c_str(), "%d,%d,%d,%d", &x, &y, &w, &h);
			if (count != 4 || w <= 0 || h <= 0) return false;

			rc.left   = x;
			rc.top    = y;
			rc.right  = x + w;
			rc.bottom = y + h;
			return true;
		}

		static bool GetPngEncoderClsid(CLSID& clsid)
		{
			UINT num = 0, size = 0;
			Gdiplus::GetImageEncodersSize(&num, &size);
			if (size == 0) return false;

			auto buf = std::make_unique<uint8_t[]>(size);
			auto encoders = reinterpret_cast<Gdiplus::ImageCodecInfo*>(buf.get());
			Gdiplus::GetImageEncoders(num, size, encoders);

			for (UINT i = 0; i < num; ++i)
			{
				if (wcscmp(encoders[i].MimeType, L"image/png") == 0)
				{
					clsid = encoders[i].Clsid;
					return true;
				}
			}
			return false;
		}

		static Gdiplus::Status SaveBitmapAsPng(Gdiplus::Bitmap& bmp, std::filesystem::path const& filepath)
		{
			CLSID clsid;
			if (!GetPngEncoderClsid(clsid))
				return Gdiplus::UnknownImageFormat;
			return bmp.Save(filepath.wstring().c_str(), &clsid, nullptr);
		}

		// Capture a frame as an HBITMAP. Caller must DeleteObject() the result.
		static HBITMAP CaptureFrame(HWND hwnd, RECT const& rc, bool use_bitblt, double scale)
		{
			auto w = rc.right  - rc.left;
			auto h = rc.bottom - rc.top;
			if (w <= 0 || h <= 0)
				return nullptr;

			auto hdc_screen = GetDC(nullptr);
			auto hdc_mem = CreateCompatibleDC(hdc_screen);
			auto hbm = CreateCompatibleBitmap(hdc_screen, w, h);
			auto old_bm = SelectObject(hdc_mem, hbm);

			bool captured = false;
			if (use_bitblt || !hwnd)
			{
				captured = BitBlt(hdc_mem, 0, 0, w, h, hdc_screen, rc.left, rc.top, SRCCOPY) != 0;
			}
			else
			{
				captured = PrintWindow(hwnd, hdc_mem, PW_RENDERFULLCONTENT) != 0;
				if (!captured)
				{
					auto hdc_wnd = GetDC(hwnd);
					captured = BitBlt(hdc_mem, 0, 0, w, h, hdc_wnd, 0, 0, SRCCOPY) != 0;
					ReleaseDC(hwnd, hdc_wnd);
				}
			}

			HBITMAP result = nullptr;
			if (captured)
			{
				if (scale > 0.0 && scale != 1.0)
				{
					auto sw = (std::max)(1, static_cast<int>(w * scale));
					auto sh = (std::max)(1, static_cast<int>(h * scale));

					auto hdc_scaled = CreateCompatibleDC(hdc_screen);
					auto hbm_scaled = CreateCompatibleBitmap(hdc_screen, sw, sh);
					auto old_scaled = SelectObject(hdc_scaled, hbm_scaled);

					SetStretchBltMode(hdc_scaled, HALFTONE);
					SetBrushOrgEx(hdc_scaled, 0, 0, nullptr);
					StretchBlt(hdc_scaled, 0, 0, sw, sh, hdc_mem, 0, 0, w, h, SRCCOPY);

					SelectObject(hdc_scaled, old_scaled);
					result = hbm_scaled;
				}
				else
				{
					// Detach the bitmap from the DC before returning it
					SelectObject(hdc_mem, old_bm);
					old_bm = nullptr;
					result = hbm;
					hbm = nullptr;
				}
			}

			if (old_bm) SelectObject(hdc_mem, old_bm);
			if (hbm) DeleteObject(hbm);
			DeleteDC(hdc_mem);
			ReleaseDC(nullptr, hdc_screen);

			return result;
		}
	};

	int Record(CmdLine const& args)
	{
		Cmd_Record cmd;
		return cmd.Run(args);
	}
}
