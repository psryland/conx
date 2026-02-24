///
/// Smoke tests for conx — exercises every command.
/// Run from the repo root:  dotnet-script tests/smoke.csx
///

using System.Diagnostics;

var exe = Path.GetFullPath(Args.Count > 0 ? Args[0] : Path.Combine(Path.GetDirectoryName(GetScriptPath()), "..", "obj", "x64", "Debug", "conx.exe"));
if (!File.Exists(exe))
{
	Console.ForegroundColor = ConsoleColor.Red;
	Console.WriteLine($"ERROR: conx.exe not found at {exe}");
	Console.ResetColor();
	Console.WriteLine("Build first:  msbuild conx.vcxproj /p:Configuration=Debug /p:Platform=x64");
	Environment.Exit(1);
}

Console.WriteLine($"Testing: {exe}\n");

int pass = 0, fail = 0;

void Test(string name, string args, int expected_exit = 0, string contains = null)
{
	var psi = new ProcessStartInfo("cmd.exe", $"/c \"\"{exe}\" {args}\"")
	{
		RedirectStandardOutput = true,
		RedirectStandardError = true,
		UseShellExecute = false,
		CreateNoWindow = true,
	};

	var proc = Process.Start(psi);
	var stdout = proc.StandardOutput.ReadToEnd();
	var stderr = proc.StandardError.ReadToEnd();
	proc.WaitForExit();

	if (proc.ExitCode != expected_exit)
	{
		Console.ForegroundColor = ConsoleColor.Red;
		Console.WriteLine($"  FAIL  {name}  (exit={proc.ExitCode}, expected={expected_exit})");
		if (!string.IsNullOrWhiteSpace(stderr))
			Console.WriteLine($"        stderr: {stderr.Trim()}");
		Console.ResetColor();
		fail++;
		return;
	}

	if (contains != null && !stdout.Contains(contains, StringComparison.Ordinal))
	{
		Console.ForegroundColor = ConsoleColor.Red;
		Console.WriteLine($"  FAIL  {name}  (output missing '{contains}')");
		Console.ResetColor();
		fail++;
		return;
	}

	Console.ForegroundColor = ConsoleColor.Green;
	Console.WriteLine($"  PASS  {name}");
	Console.ResetColor();
	pass++;
}

// ── Commands tested with real input ──────────────────────────────────

Test("no-args (help)", "", contains: "Console EXtensions");

Test("guid", "-guid", contains: "{");

Test("hash", "-hash hello", contains: "4F9F2CAB");

Test("lwr", "-lwr \"HELLO WORLD\"", contains: "hello world");

Test("wait 0s", "-wait 0");

Test("list_windows", "-list_windows", contains: "HWND=");

Test("read_dpi", "-read_dpi");

Test("rtfm", "-rtfm", contains: "CONX - Console EXtensions");

// hdata — binary
var hdata_in = Path.GetTempFileName();
var hdata_out = Path.GetTempFileName();
File.WriteAllText(hdata_in, "AB");
Test("hdata (binary)", $"-hdata -f \"{hdata_in}\" -o \"{hdata_out}\"");
File.Delete(hdata_in);
File.Delete(hdata_out);

// hdata — text
hdata_in = Path.GetTempFileName();
hdata_out = Path.GetTempFileName();
File.WriteAllText(hdata_in, "hello");
Test("hdata (text)", $"-hdata -f \"{hdata_in}\" -o \"{hdata_out}\" -t");
File.Delete(hdata_in);
File.Delete(hdata_out);

// clip — copy then paste round-trip
Test("clip (copy)", "-clip conx_test_string");
Test("clip (paste)", "-clip -paste", contains: "conx_test_string");

// shcopy — copy a temp file and verify it arrives
var sh_src = Path.GetTempFileName();
var sh_dst_dir = Path.Combine(Path.GetTempPath(), "conx_test_shcopy");
if (Directory.Exists(sh_dst_dir)) Directory.Delete(sh_dst_dir, true);
Directory.CreateDirectory(sh_dst_dir);
File.WriteAllText(sh_src, "shcopy_test");
Test("shcopy", $"-shcopy \"{sh_src}\" \"{sh_dst_dir}\" -flags NoUI,NoConfirmMkDir");
var sh_copied = Path.Combine(sh_dst_dir, Path.GetFileName(sh_src));
if (!File.Exists(sh_copied))
{
	Console.ForegroundColor = ConsoleColor.Red;
	Console.WriteLine("  FAIL  shcopy verify  (copied file not found)");
	Console.ResetColor();
	fail++;
}
else
{
	Console.ForegroundColor = ConsoleColor.Green;
	Console.WriteLine("  PASS  shcopy verify");
	Console.ResetColor();
	pass++;
}
File.Delete(sh_src);
if (Directory.Exists(sh_dst_dir)) Directory.Delete(sh_dst_dir, true);

// ── All commands tested via -help ────────────────────────────────────

var commands = new[]
{
	"automate", "clip", "dirpath", "exec", "find_element",
	"guid", "hash", "hdata", "list_windows", "lwr", "msgbox",
	"read_dpi", "read_text", "rtfm", "screenshot",
	"send_keys", "send_mouse", "shcopy", "shutdown_process",
	"wait", "wait_window",
};
foreach (var cmd in commands)
	Test($"{cmd} -help", $"-{cmd} -help", contains: "Syntax:");

// ── Summary ──────────────────────────────────────────────────────────

Console.WriteLine($"\n--- Results ---");
Console.ForegroundColor = ConsoleColor.Green;
Console.WriteLine($"  Passed: {pass}");
Console.ResetColor();

if (fail > 0)
{
	Console.ForegroundColor = ConsoleColor.Red;
	Console.WriteLine($"  Failed: {fail}");
	Console.ResetColor();
	Environment.Exit(1);
}

Console.ForegroundColor = ConsoleColor.Green;
Console.WriteLine("  All tests passed!");
Console.ResetColor();

// ── Helpers ──────────────────────────────────────────────────────────

string GetScriptPath([System.Runtime.CompilerServices.CallerFilePath] string path = null) => path;
