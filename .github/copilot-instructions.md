# Copilot Instructions for CONX (Console EXtensions)

## Project Overview

CONX is a standalone Windows C++ command-line multi-tool (`conx.exe`). It provides a collection of subcommands for automation, clipboard manipulation, process interaction, screenshots, and other Windows utilities.

## Build

This is an MSVC project (`.vcxproj`), fully standalone with no external dependencies.

```powershell
# Debug build
msbuild conx.vcxproj /p:Configuration=Debug /p:Platform=x64

# Release build
msbuild conx.vcxproj /p:Configuration=Release /p:Platform=x64
```

Output goes to `obj\x64\Debug\conx.exe` or `obj\x64\Release\conx.exe`. There are no tests or linters.

## Architecture

### Command dispatch via X-macro

All commands are registered in a single X-macro `CONX_CMD` in `src/commands/commands.h`:

```cpp
#define CONX_CMD(x)\
x("automate", "Execute a script of mouse/keyboard commands", Automate)\
x("clip", "Add text to the windows clipboard", Clip)\
...
```

`main.cpp` iterates this macro to match the command-line argument to a handler function. Each command has a free function (e.g. `int Automate(pr::CmdLine const&)`) that is forward-declared by the same macro.

### Adding a new command

1. Add a line to `CONX_CMD` in `src/commands/commands.h` — the format is `x("cli-name", "description", FunctionName)`.
2. Create `src/commands/<name>/<name>.cpp` implementing `int FunctionName(pr::CmdLine const& args)`.
3. Add the `.cpp` to `conx.vcxproj` (under `<ClCompile>`) and `conx.vcxproj.filters`.
4. Return 0 on success, non-zero on error.

### Command implementation pattern

Every command follows the same structure:

```cpp
namespace conx
{
    struct Cmd_Name
    {
        // Member variables prefixed with m_
        std::string m_text;

        void ShowHelp() const { /* print usage to stdout */ }

        int Run(pr::CmdLine const& args)
        {
            if (args.count("help") != 0)
                return ShowHelp(), 0;
            // ... implementation ...
            return 0;
        }
    };

    int Name(pr::CmdLine const& args)
    {
        Cmd_Name cmd;
        return cmd.Run(args);
    }
}
```

### Key files

- `src/forward.h` — Common includes, and standalone implementations of `CmdLine`, `Process`, string utilities, COM init, and `Widen`/`Narrow` helpers. All live in the `pr` namespace for historical reasons.
- `src/pr/storage/json.h` — Standalone JSON DOM parser (only file remaining from the `pr/` library).
- `src/commands/process_util.h` — `FindProcesses`, `FindWindows`, `FindWindow`, `BringToForeground`, `ClientToAbsScreen`. Used by commands that interact with other processes/windows.
- `src/commands/commands.h` — The `CONX_CMD` X-macro and forward declarations.

### Proxy mode

If the executable is renamed from `conx.exe` to something else (e.g. `mytool.exe`), it behaves as if `-mytool` was passed as the first argument. Alternatively, a `.json` sidecar file can configure a process to launch.

## Conventions

- All source lives under `src/`; each command gets its own subdirectory under `src/commands/`.
- Use `snake_case` for variables and members; prefix class members with `m_`.
- Use `PascalCase` for class names, method names, and free functions.
- Use tabs for indentation.
- All command code is in the `conx` namespace. Utility types (`CmdLine`, `Process`, etc.) are in the `pr` namespace.
- The `pr::CmdLine` class handles argument parsing — use `args.count("flag")` to check presence and `args("flag").as<T>()` to read values.
- The app is a Windows subsystem application (no console window). `main.cpp` handles attaching/rewiring stdout to the parent console or redirected handles.
- DPI-aware via Per-Monitor V2 manifest (`src/app.manifest`).
