#pragma once
#include <string>
#include <Windows.h>

namespace Utils {

// Runs a command silently (no console window, no focus steal).
// Blocks until the process exits. Returns the process exit code, or -1 on failure.
inline int RunSilent(const std::string& executable, const std::string& args) {
    STARTUPINFOA si = { sizeof(si) };
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    PROCESS_INFORMATION pi = {};

    std::string cmdLine = executable + " " + args;

    if (!CreateProcessA(
            executable.c_str(),
            const_cast<char*>(cmdLine.c_str()),
            NULL, NULL, FALSE,
            CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        return -1;
    }

    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD exitCode = 0;
    GetExitCodeProcess(pi.hProcess, &exitCode);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return static_cast<int>(exitCode);
}

// Convenience: run a PowerShell command silently.
inline int RunPowerShell(const std::string& command) {
    std::string args = "-NoProfile -NonInteractive -ExecutionPolicy Bypass -Command \"" + command + "\"";
    return RunSilent("C:\\Windows\\System32\\WindowsPowerShell\\v1.0\\powershell.exe", args);
}

// Convenience: run a PowerShell script file silently with arguments.
inline int RunPowerShellScript(const std::string& scriptPath, const std::string& scriptArgs,
                               const std::string& logFile) {
    std::string args = "/c powershell.exe -NoProfile -ExecutionPolicy Bypass -File \""
                     + scriptPath + "\" " + scriptArgs + " > \"" + logFile + "\" 2>&1";
    return RunSilent("C:\\Windows\\System32\\cmd.exe", args);
}

// Convenience: expand a zip archive silently.
inline int ExpandArchive(const std::string& zipPath, const std::string& destPath) {
    std::string cmd = "try { Expand-Archive -LiteralPath '" + zipPath
                    + "' -DestinationPath '" + destPath + "' -Force; exit 0 } "
                    + "catch { Write-Error $_.Exception.Message; exit 1 }";
    return RunPowerShell(cmd);
}

} // namespace Utils
