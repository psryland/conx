//**********************************************
// Console Extensions
//  Copyright (c) Rylogic Ltd 2004
//**********************************************
// ExtractFrames: Decode a video file and save frames as numbered PNGs.
// Uses Media Foundation IMFSourceReader for decoding and GDI+ for PNG output.
#include "src/forward.h"

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
	struct Cmd_ExtractFrames
	{
		void ShowHelp() const
		{
			std::cout <<
				"ExtractFrames: Extract PNG frames from a video file\n"
				" Syntax: Conx -extract_frames -i <video> -o <output-dir>\n"
				"         [-t <seconds>] [-from <seconds>] [-to <seconds>]\n"
				"         [-fps N] [-scale N]\n"
				"\n"
				"  Required:\n"
				"  -i        : Input video file path (e.g. .mp4).\n"
				"  -o        : Output directory for PNG files.\n"
				"\n"
				"  Time selection (pick one mode):\n"
				"  -t        : Extract a single frame at this time in seconds (e.g. 8 or 1.5).\n"
				"  -from/-to : Extract frames over a time range (seconds).\n"
				"              -from defaults to 0, -to defaults to end of video.\n"
				"              Combined with -fps to control sample rate.\n"
				"\n"
				"  Options:\n"
				"  -fps      : Frames per second to extract within a range (default: 1).\n"
				"              Ignored when -t is used (single frame mode).\n"
				"  -scale    : Scale factor for output images (e.g. 0.5 for half size).\n"
				"\n"
				"  Output files are named frame_0001.png, frame_0002.png, etc.\n"
				"  Timestamps are printed to stdout for each extracted frame.\n"
				"\n"
				"  Examples:\n"
				"    conx -extract_frames -i clip.mp4 -o C:\\tmp\\frames -t 8\n"
				"    conx -extract_frames -i clip.mp4 -o C:\\tmp\\frames -from 2 -to 10 -fps 5\n"
				"    conx -extract_frames -i clip.mp4 -o C:\\tmp\\frames -scale 0.5\n";
		}

		int Run(CmdLine const& args)
		{
			if (args.count("help") != 0)
				return ShowHelp(), 0;

			// Input video (required)
			std::filesystem::path input;
			if (args.count("i") != 0)
				input = args("i").as<std::string>();
			if (input.empty()) { std::cerr << "No input file provided (-i)\n"; return ShowHelp(), -1; }
			if (!std::filesystem::exists(input)) { std::cerr << std::format("Input file not found: {}\n", input.string()); return -1; }

			// Output directory (required)
			std::filesystem::path output;
			if (args.count("o") != 0)
				output = args("o").as<std::string>();
			if (output.empty()) { std::cerr << "No output directory provided (-o)\n"; return ShowHelp(), -1; }

			// Time selection
			bool single_frame = args.count("t") != 0;
			auto t_single = single_frame ? args("t").as<double>() : 0.0;
			auto t_from = args.count("from") != 0 ? args("from").as<double>() : 0.0;
			auto t_to = args.count("to") != 0 ? args("to").as<double>() : -1.0; // -1 means end of video
			auto fps = args.count("fps") != 0 ? args("fps").as<double>() : 1.0;
			auto scale = args.count("scale") != 0 ? args("scale").as<double>() : 1.0;

			fps = (std::max)(0.1, (std::min)(fps, 60.0));

			// Initialise Media Foundation
			auto hr = MFStartup(MF_VERSION);
			if (FAILED(hr)) { std::cerr << "MFStartup failed\n"; return -1; }

			// Initialise GDI+ for PNG saving
			Gdiplus::GdiplusStartupInput gdiplus_input;
			ULONG_PTR gdiplus_token = 0;
			Gdiplus::GdiplusStartup(&gdiplus_token, &gdiplus_input, nullptr);

			int result = ExtractFrames(input, output, single_frame, t_single, t_from, t_to, fps, scale);

			Gdiplus::GdiplusShutdown(gdiplus_token);
			MFShutdown();

			return result;
		}

	private:

		// Convert seconds to Media Foundation 100ns units
		static LONGLONG SecondsToMF(double seconds)
		{
			return static_cast<LONGLONG>(seconds * 10'000'000.0);
		}

		static double MFToSeconds(LONGLONG hns)
		{
			return static_cast<double>(hns) / 10'000'000.0;
		}

		static bool GetPngEncoderClsid(CLSID& clsid)
		{
			UINT num = 0, size = 0;
			Gdiplus::GetImageEncodersSize(&num, &size);
			if (size == 0) return false;

			auto buf = std::make_unique<uint8_t[]>(size);
			auto encoders = reinterpret_cast<Gdiplus::ImageCodecInfo*>(buf.get());
			Gdiplus::GetImageEncoders(num, size, encoders);

			for (UINT i = 0; i != num; ++i)
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

		// Scale a bitmap using GDI+. Returns a new Bitmap the caller owns.
		static std::unique_ptr<Gdiplus::Bitmap> ScaleBitmap(Gdiplus::Bitmap& src, double scale)
		{
			auto sw = (std::max)(1u, static_cast<UINT>(src.GetWidth() * scale));
			auto sh = (std::max)(1u, static_cast<UINT>(src.GetHeight() * scale));

			auto dst = std::make_unique<Gdiplus::Bitmap>(sw, sh, PixelFormat32bppARGB);
			Gdiplus::Graphics gfx(dst.get());
			gfx.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
			gfx.DrawImage(&src, 0, 0, static_cast<INT>(sw), static_cast<INT>(sh));
			return dst;
		}

		int ExtractFrames(
			std::filesystem::path const& input,
			std::filesystem::path const& output,
			bool single_frame,
			double t_single,
			double t_from,
			double t_to,
			double fps,
			double scale)
		{
			// Create the source reader with video processing enabled so MF can
			// decode and convert frames to RGB32 for us.
			IMFAttributes* reader_attrs = nullptr;
			MFCreateAttributes(&reader_attrs, 2);
			if (reader_attrs)
			{
				reader_attrs->SetUINT32(MF_READWRITE_ENABLE_HARDWARE_TRANSFORMS, TRUE);
				reader_attrs->SetUINT32(MF_SOURCE_READER_ENABLE_VIDEO_PROCESSING, TRUE);
			}

			IMFSourceReader* reader = nullptr;
			auto hr = MFCreateSourceReaderFromURL(input.wstring().c_str(), reader_attrs, &reader);
			if (reader_attrs) reader_attrs->Release();
			if (FAILED(hr) || !reader)
			{
				std::cerr << std::format("Failed to open video: {} (HRESULT: 0x{:08X})\n", input.string(), static_cast<unsigned>(hr));
				return -1;
			}

			// Configure the reader to decode to RGB32
			IMFMediaType* decode_type = nullptr;
			MFCreateMediaType(&decode_type);
			decode_type->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
			decode_type->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_RGB32);
			hr = reader->SetCurrentMediaType(static_cast<DWORD>(MF_SOURCE_READER_FIRST_VIDEO_STREAM), nullptr, decode_type);
			decode_type->Release();
			if (FAILED(hr))
			{
				std::cerr << "Failed to configure video decoder for RGB32 output\n";
				reader->Release();
				return -1;
			}

			// Get the video duration
			PROPVARIANT var;
			PropVariantInit(&var);
			double duration_sec = 0.0;
			hr = reader->GetPresentationAttribute(static_cast<DWORD>(MF_SOURCE_READER_MEDIASOURCE), MF_PD_DURATION, &var);
			if (SUCCEEDED(hr) && var.vt == VT_UI8)
				duration_sec = MFToSeconds(static_cast<LONGLONG>(var.uhVal.QuadPart));
			PropVariantClear(&var);

			// Get frame dimensions and stride from the current media type
			IMFMediaType* current_type = nullptr;
			UINT32 video_w = 0, video_h = 0;
			LONG default_stride = 0;
			hr = reader->GetCurrentMediaType(static_cast<DWORD>(MF_SOURCE_READER_FIRST_VIDEO_STREAM), &current_type);
			if (SUCCEEDED(hr))
			{
				MFGetAttributeSize(current_type, MF_MT_FRAME_SIZE, &video_w, &video_h);

				// MF_MT_DEFAULT_STRIDE is stored as UINT32 but represents a signed value
				// (negative = bottom-up). Reinterpret the bits as INT32.
				UINT32 stride_attr = 0;
				if (SUCCEEDED(current_type->GetUINT32(MF_MT_DEFAULT_STRIDE, &stride_attr)))
					default_stride = static_cast<LONG>(static_cast<INT32>(stride_attr));
				else
					default_stride = static_cast<LONG>(video_w * 4);

				current_type->Release();
			}

			if (video_w == 0 || video_h == 0)
			{
				std::cerr << "Failed to determine video dimensions\n";
				reader->Release();
				return -1;
			}

			std::cout << std::format("Video: {}x{}, {:.1f}s\n", video_w, video_h, duration_sec);

			// Create output directory
			std::filesystem::create_directories(output);

			// Build the list of timestamps to extract
			std::vector<double> timestamps;
			if (single_frame)
			{
				timestamps.push_back(t_single);
			}
			else
			{
				auto end_time = (t_to < 0.0) ? duration_sec : t_to;
				auto interval = 1.0 / fps;
				for (auto t = t_from; t <= end_time + 1e-6; t += interval)
					timestamps.push_back(t);
			}

			if (timestamps.empty())
			{
				std::cerr << "No frames to extract (check time range)\n";
				reader->Release();
				return -1;
			}

			std::cout << std::format("Extracting {} frame(s)...\n", timestamps.size());

			int extracted = 0;
			for (size_t idx = 0; idx != timestamps.size(); ++idx)
			{
				auto target_time = timestamps[idx];

				// Seek to the target time
				PROPVARIANT seek_pos;
				PropVariantInit(&seek_pos);
				seek_pos.vt = VT_I8;
				seek_pos.hVal.QuadPart = SecondsToMF(target_time);
				hr = reader->SetCurrentPosition(GUID_NULL, seek_pos);
				PropVariantClear(&seek_pos);
				if (FAILED(hr))
				{
					std::cerr << std::format("Failed to seek to {:.3f}s\n", target_time);
					continue;
				}

				// Read the next video sample after the seek
				DWORD stream_index = 0, flags = 0;
				LONGLONG sample_time = 0;
				IMFSample* sample = nullptr;
				hr = reader->ReadSample(static_cast<DWORD>(MF_SOURCE_READER_FIRST_VIDEO_STREAM), 0, &stream_index, &flags, &sample_time, &sample);
				if (FAILED(hr) || !sample || (flags & MF_SOURCE_READERF_ENDOFSTREAM))
				{
					if (sample) sample->Release();
					std::cerr << std::format("No frame available at {:.3f}s\n", target_time);
					continue;
				}

				// Get the buffer. Use GetBufferByIndex (not ConvertToContiguousBuffer)
				// to preserve the 2D layout needed for Lock2D.
				IMFMediaBuffer* buffer = nullptr;
				hr = sample->GetBufferByIndex(0, &buffer);
				if (FAILED(hr) || !buffer)
				{
					sample->Release();
					if (buffer) buffer->Release();
					std::cerr << std::format("Failed to get buffer for frame at {:.3f}s\n", target_time);
					continue;
				}

				BYTE* pixels = nullptr;
				LONG stride = 0;
				IMF2DBuffer* buffer_2d = nullptr;
				bool locked_2d = false;

				// Try IMF2DBuffer first — gives us the actual stride directly
				if (SUCCEEDED(buffer->QueryInterface(IID_PPV_ARGS(&buffer_2d))))
				{
					hr = buffer_2d->Lock2D(&pixels, &stride);
					if (SUCCEEDED(hr))
						locked_2d = true;
					else
						buffer_2d->Release(), buffer_2d = nullptr;
				}

				// Fallback: lock the flat buffer and compute stride from alignment.
				// Decoders commonly pad dimensions to multiples of 16.
				if (!locked_2d)
				{
					DWORD max_len = 0, cur_len = 0;
					hr = buffer->Lock(&pixels, &max_len, &cur_len);
					if (FAILED(hr))
					{
						buffer->Release();
						sample->Release();
						continue;
					}

					// Derive stride: both width and height may be padded to 16-pixel alignment
					auto aligned_w = (video_w + 15u) & ~15u;
					stride = static_cast<LONG>(aligned_w * 4);

					// Verify our guess: stride * aligned_height should equal the buffer size
					auto aligned_h = (video_h + 15u) & ~15u;
					if (stride * static_cast<LONG>(aligned_h) != static_cast<LONG>(cur_len))
					{
						// Guess didn't match — fall back to the media type default
						stride = default_stride;
					}
				}

				// MF RGB32 with video processing gives top-down frames (positive stride).
				// GDI+ Bitmap constructor takes the stride directly — positive = top-down.
				Gdiplus::Bitmap bmp(video_w, video_h, stride, PixelFormat32bppRGB, pixels);

				// Save (with optional scaling)
				auto filepath = output / std::format("frame_{:04d}.png", static_cast<int>(idx + 1));
				bool ok = false;
				if (scale > 0.0 && scale != 1.0)
				{
					auto scaled = ScaleBitmap(bmp, scale);
					ok = SaveBitmapAsPng(*scaled, filepath) == Gdiplus::Ok;
				}
				else
				{
					ok = SaveBitmapAsPng(bmp, filepath) == Gdiplus::Ok;
				}

				if (locked_2d)
					buffer_2d->Unlock2D(), buffer_2d->Release();
				else
					buffer->Unlock();
				buffer->Release();
				sample->Release();

				if (ok)
				{
					auto actual_sec = MFToSeconds(sample_time);
					std::cout << std::format("  {} (t={:.3f}s)\n", filepath.filename().string(), actual_sec);
					++extracted;
				}
				else
				{
					std::cerr << std::format("  Failed to save {}\n", filepath.filename().string());
				}
			}

			reader->Release();

			std::cout << std::format("{} frame(s) extracted to {}\n", extracted, output.string());
			return extracted > 0 ? 0 : -1;
		}
	};

	int ExtractFrames(CmdLine const& args)
	{
		Cmd_ExtractFrames cmd;
		return cmd.Run(args);
	}
}
