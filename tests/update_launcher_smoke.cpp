// Optional real Direct3D launcher startup/acknowledgement check. Not part of headless CTest.
#include <windows.h>
#include "community/installer.hpp"
#include "community/platform.hpp"
#include <iostream>
int wmain(int count, wchar_t **args) {
    try {
        if (count != 4)
            throw std::runtime_error(
                "Usage: update_launcher_smoke <portable-root> <data-dir> <capture.png>");
        auto root = cd::fs::canonical(args[1]);
        auto data = cd::fs::absolute(args[2]);
        auto capture = cd::fs::absolute(args[3]);
        cd::LauncherLock lock(root);
        HANDLE ready = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        if (!ready)
            throw std::runtime_error("Cannot create startup event");
        HANDLE handles[] = {ready, reinterpret_cast<HANDLE>(lock.handle())};
        for (auto h : handles)
            SetHandleInformation(h, HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT);
        SIZE_T bytes = 0;
        InitializeProcThreadAttributeList(nullptr, 1, 0, &bytes);
        std::vector<unsigned char> storage(bytes);
        auto attributes = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(storage.data());
        if (!InitializeProcThreadAttributeList(attributes, 1, 0, &bytes) ||
            !UpdateProcThreadAttribute(attributes, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST, handles,
                                       sizeof(handles), nullptr, nullptr))
            throw std::runtime_error("Cannot pass startup event");
        auto exe = root / "CommunityDivers.exe";
        auto q = [](const std::wstring &s) { return L"\"" + s + L"\""; };
        auto command = q(exe.wstring()) + L" --data-dir " + q(data.wstring()) + L" --capture " +
                       q(capture.wstring()) + L" --update-health-handle " +
                       std::to_wstring(reinterpret_cast<uintptr_t>(ready)) +
                       L" --launcher-lock-handle " + std::to_wstring(lock.handle());
        STARTUPINFOEXW startup{};
        startup.StartupInfo.cb = sizeof(startup);
        startup.lpAttributeList = attributes;
        startup.StartupInfo.dwFlags = STARTF_USESHOWWINDOW;
        startup.StartupInfo.wShowWindow = SW_HIDE;
        PROCESS_INFORMATION info{};
        if (!CreateProcessW(exe.c_str(), command.data(), nullptr, nullptr, TRUE,
                            EXTENDED_STARTUPINFO_PRESENT, nullptr, root.c_str(),
                            &startup.StartupInfo, &info))
            throw std::runtime_error("Cannot start real launcher");
        DeleteProcThreadAttributeList(attributes);
        CloseHandle(info.hThread);
        for (auto h : handles)
            SetHandleInformation(h, HANDLE_FLAG_INHERIT, 0);
        HANDLE waits[] = {ready, info.hProcess};
        auto result = WaitForMultipleObjects(2, waits, FALSE, 20000);
        if (result != WAIT_OBJECT_0) {
            if (WaitForSingleObject(info.hProcess, 0) == WAIT_TIMEOUT) {
                TerminateProcess(info.hProcess, 1);
                WaitForSingleObject(info.hProcess, 5000);
            }
            throw std::runtime_error("Real launcher did not acknowledge its rendered startup");
        }
        if (WaitForSingleObject(info.hProcess, 10000) != WAIT_OBJECT_0)
            throw std::runtime_error("Capture process did not finish");
        DWORD exit = 1;
        GetExitCodeProcess(info.hProcess, &exit);
        CloseHandle(info.hProcess);
        CloseHandle(ready);
        if (exit || !cd::fs::exists(capture))
            throw std::runtime_error("Real launcher startup/capture failed");
        std::cout << "PASS real launcher rendered, acknowledged startup and exited normally\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << "FAIL " << e.what() << '\n';
        return 1;
    }
}
