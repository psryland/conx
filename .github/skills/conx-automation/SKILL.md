---
name: conx-automation
description: >
  Skill for using the conx (Console Extensions) Windows CLI tool to automate GUI testing,
  window management, screenshots, keyboard/mouse simulation, and shell file operations.
  Use this when asked to automate Windows UI interactions, capture screenshots, send keys
  or mouse events to applications, find UI elements, read text from windows, or perform
  file operations via the Windows shell.
---

# conx — Console Extensions

conx is a single-binary Windows CLI tool for GUI automation, window management, and shell utilities.
The `conx.exe` executable is located in the same directory as this skill file (`.github/skills/conx-automation/conx.exe`).
A Release build of the project automatically copies the binary here.

> **Important:** conx is a Windows subsystem application. To capture its stdout, run it via `cmd /c`.
> The skill directory can be resolved relative to the repository root:
> ```powershell
> $conx = Join-Path $PWD ".github\skills\conx-automation\conx.exe"
> $output = & cmd /c "`"$conx`" -guid" 2>&1
> ```

## Command Reference

Every command is invoked as `conx -<command> [args...]`. Use `conx -<command> -help` for built-in help.

---

### Window Discovery & Targeting

Most UI commands accept `-p <process-name>` and `-w <window-name>` for targeting:
- `-p` matches process names via case-insensitive substring
- `-w` matches window titles via case-insensitive substring (defaults to largest window)

#### list_windows — Enumerate windows

```
conx -list_windows [-p <process-name>] [-all]
```
- Omit `-p` to list all processes' windows
- `-all` includes hidden/minimised windows
- Output: one line per window with HWND, size, visibility, process, and title

#### wait_window — Wait for a window to appear

```
conx -wait_window -p <process-name> [-w <window-name>] [-timeout <ms>]
```
- Returns 0 when found, 1 on timeout (default: 30000ms)
- Useful for waiting for application startup or dialogs

#### move_window — Move a window to a new position

```
conx -move_window -p <process-name> [-w <window-name>] -x <X> -y <Y> [-width <W>] [-height <H>]
```
- Moves (and optionally resizes) a window to the specified screen position
- `-width` / `-height` override the current size; omit to keep current dimensions

#### shutdown_process — Gracefully close a process

```
conx -shutdown_process -p <process-name> [-w <window-name>] [-timeout <ms>]
```
- Sends WM_CLOSE to the process's windows (like clicking the X button)
- `-timeout 0` sends WM_CLOSE without waiting

---

### GUI Automation

#### send_keys — Send keyboard input to a window

```
conx -send_keys "text" -p <process-name> [-w <window-name>] [-rate <keys-per-second>] [-bg] [-c <class>]
```
- Without `-bg`: brings window to foreground, uses SendInput for hardware-level key simulation
- With `-bg`: posts WM_CHAR messages directly — **does not steal focus**. Works for native Win32 apps.
- `-c <class>`: (with `-bg`) target a child control by class name substring (e.g., `Edit`, `RichEdit`)
- Characters are sent as raw unicode — this does **not** parse key-combo escape sequences like `{CTRL+A}`
- For key combos (ctrl+a, shift+delete, etc.), use the `automate` command with `key combo` instead
- Default rate: 10 keys/second

#### send_mouse — Send mouse events to a window

```
conx -send_mouse x,y -b <button-action> -p <process-name> [-w <window-name>] [-bg]
```
- Button actions: `LeftDown`, `LeftUp`, `LeftClick`, `RightDown`, `RightUp`, `RightClick`,
  `MiddleDown`, `MiddleUp`, `MiddleClick`, `Move`
- Coordinates are relative to the window's client area
- With `-bg`: posts WM_ messages directly — **does not steal focus**. Automatically resolves the
  deepest child window at the click point. Works for native Win32 apps.

**Drag sequence example** (button down → move → button up):
```powershell
& cmd /c "`"$conx`" -send_mouse 100,100 -b LeftDown -p MyApp"
& cmd /c "`"$conx`" -send_mouse 200,200 -b Move     -p MyApp"
& cmd /c "`"$conx`" -send_mouse 200,200 -b LeftUp   -p MyApp"
```

#### automate — Execute a script of mouse/keyboard commands

```
conx -automate -p <process-name> [-w <window-name>] [-f <script-file>] [-bg] [-c <class>]
```
- With `-bg`: posts WM_ messages directly — **does not steal focus**. All mouse and keyboard
  commands use background message posting instead of SendInput.
- `-c <class>`: (with `-bg`) target a child control by class name for keyboard commands
Script commands (one per line, `#` for comments):
- **Mouse:** `move x,y`, `click x,y [button]`, `down x,y [button]`, `up [button]`, `drag x1,y1 x2,y2 [N]`
- **Drawing:** `line x1,y1 x2,y2`, `circle cx,cy r [N]`, `arc cx,cy r a0 a1 [N]`, `fill_circle cx,cy r [N]`
- **Keyboard:** `type text...`, `key combo` (e.g., `ctrl+a`, `shift+delete`, `enter`, `f5`)
- **Timing:** `delay ms`
- Without `-f`, reads from stdin

#### find_element — Find UI elements by name

```
conx -find_element -name <text> -p <process-name> [-w <window-name>] [-depth N]
```
- Searches the UI Automation tree (case-insensitive substring match)
- Outputs control type, name, and bounding rectangle (screen + client coords) including center point
- Use the returned center coordinates with `send_mouse` or `automate` to click elements by name
- Default depth: 8

#### read_text — Read text from a window

```
conx -read_text -p <process-name> [-w <window-name>] [-depth N]
```
- Reads text from UI elements via Windows UI Automation API
- Outputs element tree with names, control types, and text values
- Default depth: 5

---

### Screen Capture

#### screenshot — Capture windows to PNG

```
conx -screenshot -p <process-name> -o <output-directory> [-all] [-bitblt] [-scale N]
```
- `-all` captures hidden/minimised windows
- `-bitblt` uses screen DC capture (required for GPU-rendered apps like Electron/Chromium)
- `-scale` scales output (e.g., `0.25` for quarter size)
- Output files: `<process>.<window-title>.png`

#### read_dpi — Report monitor DPI

```
conx -read_dpi [-monitor <index>]
```
- Output: `dpi_x dpi_y scale_percent` (e.g., `144 144 150`)

---

### Shell File Operations

#### shcopy / shmove / shrename / shdelete

Perform file operations via the Windows Explorer shell (SHFileOperation).

```
conx -shcopy src,... dst,... [-flags flag,...] [-title "text"]
conx -shmove src,... dst,... [-flags flag,...] [-title "text"]
conx -shrename src,... dst,... [-flags flag,...] [-title "text"]
conx -shdelete src,... [-flags flag,...] [-title "text"]
```
- Multiple paths separated by commas
- Flags: `AllowUndo`, `FilesOnly`, `MultiDestFiles`, `NoConfirmation`, `NoConfirmMkDir`,
  `NoConnectedElements`, `NoCopySecurityAttribs`, `NoErrorUI`, `NoRecursion`, `NoUI`,
  `RenameOnCollision`, `Silent`, `SimpleProgress`, `WantNukeWarning`
- Returns 0 on success, 1 if aborted

---

### Text & Clipboard Utilities

#### clip — Copy/paste clipboard text

```
conx -clip "text to copy" [-lwr] [-upr] [-fwdslash] [-bkslash] [-cstr] [-crlf|-cr|-lf]
conx -clip -paste
```
- `-lwr`/`-upr` converts case
- `-fwdslash`/`-bkslash` normalises path separators
- `-cstr` escapes for C/C++ string literal
- `-paste` outputs clipboard contents to stdout

#### lwr — Convert string to lower case

```
conx -lwr "text"
```

#### guid — Generate a new GUID

```
conx -guid
```
Output: `{xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx}`

#### hash — Hash a string (FNV-1a)

```
conx -hash data_to_hash...
```

---

### Process & File Utilities

#### exec — Launch a process

```
conx -exec [-async] [-cwd working_dir] -p exe_path [args...]
```
- `-async` returns immediately instead of waiting for the process to complete
- `-cwd` sets the working directory

#### hdata — Convert a file to a C/C++ header

```
conx -hdata -f src_file -o output_header [-t] [-v]
```
- `-t` outputs text data instead of binary
- `-v` enables verbose output

#### dirpath — Open a directory picker dialog

```
conx -dirpath env_variable_name [-msg "Message"]
```
- Stores the selected path in the specified environment variable

#### msgbox — Display a message box

```
conx -msgbox -title "title text" -body "body text" -style style_id
```

#### wait — Pause for a duration

```
conx -wait <seconds> -msg "message"
```

#### rtfm — Output built-in documentation

```
conx -rtfm
```
To discover all commands and their full options, dump the built-in manual:
```powershell
& cmd /c "`"$conx`" -rtfm" > conx-manual.md
```
Then read `conx-manual.md` for complete markdown documentation of every command.

---

## Common Patterns

### Background Mode (`-bg`) — Automation Without Stealing Focus

Add `-bg` to `send_keys`, `send_mouse`, or `automate` to post WM_ messages directly to
window handles instead of using SendInput. This **does not steal keyboard/mouse focus**
from the user.

**When to use `-bg`:**
- Native Win32 applications (Notepad, Explorer, MFC, WinForms, WPF)
- Any time the user is working and you don't want to interrupt them

**When to use foreground mode (no `-bg`):**
- Electron/Chromium apps (VS Code, Discord, Slack, Chrome) — they don't use standard Win32 controls
- UWP/WinUI apps that don't process posted WM_ messages
- Key combos with modifiers (ctrl+a, shift+delete, etc.) — modifier key state isn't
  propagated by PostMessage, so most controls won't recognise the modifier
- When `-bg` mode doesn't produce the expected result

```powershell
# Background: type into Notepad without stealing focus
& cmd /c "`"$conx`" -send_keys `"Hello World`" -p notepad -bg -c Edit"

# Background: click a button in a Win32 app
& cmd /c "`"$conx`" -send_mouse 150,300 -b LeftClick -p MyApp -bg"

# Background: run an automation script
@"
click 30,10
delay 500
type test_document.txt
key enter
"@ | & cmd /c "`"$conx`" -automate -p notepad -bg -c Edit"
```

### Visual Verification Workflow

Copilot can close the loop on GUI automation by capturing and viewing screenshots:
1. Perform an action (e.g., `send_keys` to type into an app)
2. Capture a screenshot of the target window (`-screenshot`)
3. View the PNG with the `view` tool to visually confirm the result

This is useful for debugging GUI interactions without requiring the user to manually verify.

```powershell
# 1. Type into the app
& cmd /c "`"$conx`" -send_keys `"Hello World`" -p notepad"
# 2. Capture
& cmd /c "`"$conx`" -screenshot -p notepad -o C:\tmp\verify"
# 3. View the image (use Copilot's view tool on the PNG)
```

### Wait for an app to start, then interact with it

```powershell
$conx = Join-Path $PWD ".github\skills\conx-automation\conx.exe"

# Launch the app
Start-Process "MyApp.exe"

# Wait for its main window
& cmd /c "`"$conx`" -wait_window -p MyApp -timeout 10000"

# Send keyboard input
& cmd /c "`"$conx`" -send_keys `"Hello World`" -p MyApp"
```

### Take a screenshot of a running application

```powershell
& cmd /c "`"$conx`" -screenshot -p notepad -o C:\screenshots"
```

### Automate a sequence of GUI actions

```powershell
@"
# Click the File menu
click 30,10
delay 500
# Type a filename
type test_document.txt
delay 200
key enter
"@ | & cmd /c "`"$conx`" -automate -p notepad"
```

### Copy files with Explorer shell (supports undo)

```powershell
& cmd /c "`"$conx`" -shcopy `"C:\src\file.txt`" `"C:\dst`" -flags AllowUndo,NoConfirmMkDir"
```

### Read text from a window for verification

```powershell
$text = & cmd /c "`"$conx`" -read_text -p notepad" 2>&1
if ($text -match "Expected Content") { Write-Host "Found it" }
```

### Find a UI element and click it

```powershell
$elem = & cmd /c "`"$conx`" -find_element -name `"Save`" -p MyApp" 2>&1
# Parse the client coordinates from the output, then click
& cmd /c "`"$conx`" -send_mouse 150,300 -b LeftClick -p MyApp"
```

## Building conx

```powershell
msbuild conx.vcxproj /p:Configuration=Release /p:Platform=x64
```

A Release build automatically copies `conx.exe` into `.github/skills/conx-automation/`.

## Testing

```powershell
dotnet-script tests/smoke.csx
```

Runs smoke tests for all commands including real output validation and `-help` checks.
