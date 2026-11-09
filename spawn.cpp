// Запуск процессов-касс. Студентам этот файл менять не нужно.
#include "spawn.h"

#include <filesystem>
#include <format>
#include <stdexcept>
#include <system_error>
#include <thread>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#else
#include <csignal>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>
#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif
extern char** environ;
#endif

namespace
{

using Clock = std::chrono::steady_clock;

std::vector<std::string> OfficeArgs(int id, const std::vector<std::string>& extraArgs)
{
    std::vector<std::string> args = {"--office", "--id", std::to_string(id)};
    args.insert(args.end(), extraArgs.begin(), extraArgs.end());
    return args;
}

#ifdef _WIN32

std::filesystem::path CurrentExecutable()
{
    std::wstring buffer(MAX_PATH, L'\0');
    for (;;)
    {
        const DWORD length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
        if (length == 0)
            throw std::system_error(static_cast<int>(GetLastError()), std::system_category(), "GetModuleFileNameW");
        if (length < buffer.size())
        {
            buffer.resize(length);
            return buffer;
        }
        buffer.resize(buffer.size() * 2);
    }
}

struct Child
{
    HANDLE process = nullptr;
};

Child Start(const std::filesystem::path& exe, const std::vector<std::string>& args)
{
    std::wstring commandLine = L"\"" + exe.wstring() + L"\"";
    for (const std::string& arg : args)
        commandLine += L" " + std::wstring(arg.begin(), arg.end());  // аргументы — латиница и цифры

    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION info{};
    if (!CreateProcessW(nullptr, commandLine.data(), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &startup,
                        &info))
        throw std::system_error(static_cast<int>(GetLastError()), std::system_category(), "CreateProcessW");
    CloseHandle(info.hThread);
    return {info.hProcess};
}

// true — процесс закончился (exitCode заполнен), false — время вышло.
bool WaitUntil(Child& child, Clock::time_point deadline, int& exitCode)
{
    const auto left = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - Clock::now());
    const DWORD ms = left.count() > 0 ? static_cast<DWORD>(left.count()) : 0;
    if (WaitForSingleObject(child.process, ms) != WAIT_OBJECT_0)
        return false;
    DWORD code = 0;
    GetExitCodeProcess(child.process, &code);
    exitCode = static_cast<int>(code);
    CloseHandle(child.process);
    return true;
}

void Kill(Child& child)
{
    TerminateProcess(child.process, 1);
    WaitForSingleObject(child.process, INFINITE);
    CloseHandle(child.process);
}

std::string Describe(int exitCode)
{
    return std::format("завершилась с кодом {}", exitCode);  // std::abort() на Windows даёт код 3
}

#else

std::filesystem::path CurrentExecutable()
{
#ifdef __APPLE__
    uint32_t size = 0;
    _NSGetExecutablePath(nullptr, &size);
    std::string buffer(size, '\0');
    if (_NSGetExecutablePath(buffer.data(), &size) != 0)
        throw std::runtime_error("_NSGetExecutablePath");
    return std::filesystem::canonical(buffer.c_str());
#else
    return std::filesystem::read_symlink("/proc/self/exe");
#endif
}

struct Child
{
    pid_t pid = 0;
};

Child Start(const std::filesystem::path& exe, const std::vector<std::string>& args)
{
    const std::string path = exe.string();
    std::vector<char*> argv;
    argv.push_back(const_cast<char*>(path.c_str()));
    for (const std::string& arg : args)
        argv.push_back(const_cast<char*>(arg.c_str()));
    argv.push_back(nullptr);

    pid_t pid = 0;
    const int error = posix_spawn(&pid, path.c_str(), nullptr, nullptr, argv.data(), environ);
    if (error != 0)
        throw std::system_error(error, std::generic_category(), "posix_spawn");
    return {pid};
}

// Код возврата; если процесс убит сигналом — 128 + номер сигнала, как в shell.
int ExitCode(int status)
{
    if (WIFEXITED(status))
        return WEXITSTATUS(status);
    if (WIFSIGNALED(status))
        return 128 + WTERMSIG(status);
    return -1;
}

bool WaitUntil(Child& child, Clock::time_point deadline, int& exitCode)
{
    for (;;)
    {
        int status = 0;
        const pid_t done = waitpid(child.pid, &status, WNOHANG);
        if (done == child.pid)
        {
            exitCode = ExitCode(status);
            return true;
        }
        if (Clock::now() >= deadline)
            return false;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

void Kill(Child& child)
{
    kill(child.pid, SIGKILL);
    int status = 0;
    waitpid(child.pid, &status, 0);
}

std::string Describe(int exitCode)
{
    if (exitCode > 128)
        return std::format("упала (сигнал {}{})", exitCode - 128, exitCode - 128 == SIGABRT ? ", std::abort" : "");
    return std::format("завершилась с кодом {}", exitCode);
}

#endif

} // namespace

std::vector<OfficeResult> RunOffices(int count, const std::vector<std::string>& extraArgs,
                                     std::chrono::milliseconds timeout)
{
    const std::filesystem::path exe = CurrentExecutable();
    const auto deadline = Clock::now() + timeout;

    std::vector<Child> children;
    for (int id = 1; id <= count; ++id)
        children.push_back(Start(exe, OfficeArgs(id, extraArgs)));

    std::vector<OfficeResult> results;
    for (int i = 0; i < count; ++i)
    {
        OfficeResult result;
        result.id = i + 1;
        if (WaitUntil(children[i], deadline, result.exitCode))
        {
            if (result.exitCode != 0)
                result.failure = Describe(result.exitCode);
        }
        else
        {
            Kill(children[i]);
            result.hung = true;
            result.failure = "зависла — не закончила вовремя, убита";
        }
        results.push_back(result);
    }
    return results;
}
