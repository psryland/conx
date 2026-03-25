# conx — Console Extensions

A single-binary Windows CLI tool for GUI automation, window management, screen capture, and shell utilities.

[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)

## Features

- **GUI Automation** — Send keyboard/mouse input, run scripted action sequences, with background mode (`-bg`) for focus-free automation via WM_ messages
- **UI Inspection** — Find elements by name, read text from windows, list windows
- **Screen Capture** — Capture windows to PNG with scaling and GPU-app support
- **Video Recording** — Record frame sequences as PNGs or H.265 MP4 video
- **Shell File Operations** — Copy/move/rename/delete via Windows Explorer shell
- **Utilities** — GUID generation, hashing, clipboard, process launching, and more
- **Self-documenting** — Built-in markdown reference via `conx -rtfm`
- **Proxy Mode** — Rename the exe + add a JSON config to create single-purpose launchers

## Installation

Download `conx.exe` from the [latest release](https://github.com/psryland/conx/releases/latest) and place it on your PATH.

## Quick Start

```powershell
# Generate a GUID
conx -guid

# Take a screenshot of Notepad
conx -screenshot -p notepad -o C:\screenshots

# Record a 5-second MP4 video of an app
conx -record -p myapp -o C:\tmp\clip.mp4 -fps 10 -duration 5 -bitblt

# Send keyboard input to an app
conx -send_keys "Hello World" -p notepad

# Send keyboard input without stealing focus (background mode)
conx -send_keys "Hello World" -p notepad -bg -c Edit

# Copy files using Windows Explorer shell (supports undo)
conx -shcopy "C:\src\file.txt" "C:\dst" -flags AllowUndo,NoConfirmMkDir

# Read text from a window via UI Automation
conx -read_text -p notepad

# View full built-in documentation
conx -rtfm
```

> **Note:** conx is a Windows subsystem application (no console window). When run from a terminal it attaches to the parent console automatically. To capture output programmatically, use `cmd /c`:
> ```powershell
> $output = & cmd /c "conx.exe -guid" 2>&1
> ```

## Commands

| Command | Description |
|---------|-------------|
| `automate` | Execute a script of mouse/keyboard commands |
| `clip` | Copy/paste text to/from the clipboard |
| `dirpath` | Open a directory picker dialog |
| `exec` | Launch another process |
| `find_element` | Find a UI element by name and return its bounds |
| `guid` | Generate a new GUID |
| `hash` | Hash a string (FNV-1a) |
| `hdata` | Convert a file into a C/C++ header |
| `list_windows` | List all windows of a process |
| `lwr` | Convert a string to lower case |
| `msgbox` | Display a message box |
| `read_dpi` | Report DPI scaling for a monitor |
| `read_text` | Read text from a window using UI Automation |
| `record` | Record frame sequences as numbered PNGs or H.265/H.264 MP4 video |
| `rtfm` | Output complete command reference in markdown |
| `screenshot` | Capture windows to PNG |
| `send_keys` | Send key presses to a window |
| `send_mouse` | Send mouse events to a window |
| `shcopy/shmove/shrename/shdelete` | Shell file operations |
| `shutdown_process` | Gracefully close a process |
| `wait` | Pause for a duration |
| `wait_window` | Wait for a window to appear |
| `version` | Display the version number |

Use `conx -<command> -help` for details on any command.

## Proxy Mode

Rename `conx.exe` (e.g., to `myapp.exe`) and place a JSON config alongside it:

```json
{
  "process": "C:\\Path\\To\\App.exe",
  "startdir": "C:\\Path\\To",
  "args": ["-flag", "value"]
}
```

The renamed exe will launch the configured process. Without a JSON file, the renamed exe acts as `conx -<exe_name>`.

## Building from Source

### Prerequisites

- Visual Studio 2022+ with C++ desktop workload (MSVC v143, C++20)
- Windows SDK 10.0

### Build

```powershell
msbuild conx.vcxproj /p:Configuration=Release /p:Platform=x64
```

Output: `obj/x64/Release/conx.exe`

A Release build automatically copies `conx.exe` to `%USERPROFILE%\.copilot\skills\conx\` for the Copilot CLI skill.

### Test

```powershell
dotnet-script tests/smoke.csx
```

Runs smoke tests for all commands.

## Copilot CLI Skill

This repo includes a [Copilot CLI skill](https://docs.github.com/copilot/how-tos/copilot-cli/customize-copilot/create-skills) at `%USERPROFILE%\.copilot\skills\conx\` that teaches GitHub Copilot CLI how to use conx for GUI automation tasks. The skill bundles the `conx.exe` binary alongside a `SKILL.md` reference.

## License

[MIT](LICENSE) — Copyright (c) Paul Ryland
