#pragma once

#include <windows.h>
#include <algorithm>
#include <string>
#include <utility>
#include <vector>

// Small, opt-in profile store. No process enumeration or background polling.
namespace DimProfiles {
struct Profile { std::wstring path; int level; };
inline std::vector<Profile> entries;
inline std::wstring storagePath;
inline bool dirty = false;
inline unsigned int persistedCount = 0;
constexpr size_t MaximumProfiles = 32;

inline bool EqualPath(const std::wstring& a, const std::wstring& b) {
    return CompareStringOrdinal(a.c_str(), -1, b.c_str(), -1, TRUE) == CSTR_EQUAL;
}

inline int Find(const std::wstring& path) {
    if (path.empty()) return -1;
    for (size_t i = 0; i < entries.size(); ++i)
        if (EqualPath(entries[i].path, path)) return static_cast<int>(i);
    return -1;
}

inline int Resolve(const std::wstring& path, int fallback) {
    const int found = Find(path);
    return found >= 0 ? entries[static_cast<size_t>(found)].level : std::clamp(fallback, 5, 100);
}

inline bool Set(const std::wstring& path, int level) {
    if (path.empty() || path.size() >= 32768) return false;
    level = std::clamp(level, 5, 100);
    const int found = Find(path);
    if (found >= 0) {
        auto& profile = entries[static_cast<size_t>(found)];
        if (profile.level != level) { profile.level = level; dirty = true; }
        return true;
    }
    if (entries.size() >= MaximumProfiles) return false;
    entries.push_back({path, level}); dirty = true;
    return true;
}

inline bool Remove(const std::wstring& path) {
    const int found = Find(path);
    if (found < 0) return false;
    entries.erase(entries.begin() + found); dirty = true;
    return true;
}

inline std::wstring Section(size_t index) { return L"App" + std::to_wstring(index); }

inline void Load(const std::wstring& path) {
    storagePath = path; entries.clear(); dirty = false;
    persistedCount = std::min<UINT>(GetPrivateProfileIntW(L"Meta", L"Count", 0, path.c_str()),
        static_cast<UINT>(MaximumProfiles));
    std::wstring buffer(32768, L'\0');
    for (unsigned int i = 0; i < persistedCount; ++i) {
        const std::wstring section = Section(i);
        const DWORD length = GetPrivateProfileStringW(section.c_str(), L"Path", L"", buffer.data(),
            static_cast<DWORD>(buffer.size()), path.c_str());
        const UINT level = GetPrivateProfileIntW(section.c_str(), L"Level", 0, path.c_str());
        if (!length || length >= buffer.size() - 1 || level < 5 || level > 100) continue;
        const std::wstring executable(buffer.data(), length);
        if (Find(executable) < 0) entries.push_back({executable, static_cast<int>(level)});
    }
}

inline bool Save() {
    if (!dirty) return true;
    if (storagePath.empty()) return false;
    // W profile APIs keep non-ASCII executable paths only in a Unicode INI.
    HANDLE file = CreateFileW(storagePath.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr,
        CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file != INVALID_HANDLE_VALUE) {
        const wchar_t marker = 0xFEFF; DWORD written = 0;
        const bool ok = WriteFile(file, &marker, sizeof(marker), &written, nullptr) && written == sizeof(marker);
        CloseHandle(file);
        if (!ok) return false;
    } else if (GetLastError() != ERROR_FILE_EXISTS && GetLastError() != ERROR_ALREADY_EXISTS) return false;
    bool ok = true;
    for (size_t i = 0; i < entries.size(); ++i) {
        const auto& profile = entries[i];
        const std::wstring section = Section(i), level = std::to_wstring(profile.level);
        ok = WritePrivateProfileStringW(section.c_str(), L"Path", profile.path.c_str(), storagePath.c_str()) && ok;
        ok = WritePrivateProfileStringW(section.c_str(), L"Level", level.c_str(), storagePath.c_str()) && ok;
    }
    for (size_t i = entries.size(); i < persistedCount; ++i)
        ok = WritePrivateProfileStringW(Section(i).c_str(), nullptr, nullptr, storagePath.c_str()) && ok;
    if (ok) {
        const std::wstring count = std::to_wstring(entries.size());
        ok = WritePrivateProfileStringW(L"Meta", L"Count", count.c_str(), storagePath.c_str()) != FALSE;
    }
    if (ok) { dirty = false; persistedCount = static_cast<unsigned int>(entries.size()); }
    return ok;
}

inline bool QueryApp(HWND window, std::wstring& path, std::wstring& name) {
    path.clear(); name.clear();
    if (!window || !IsWindowVisible(window)) return false;
    wchar_t klass[128] = {};
    GetClassNameW(window, klass, ARRAYSIZE(klass));
    for (const wchar_t* excluded : {L"Progman", L"WorkerW", L"Shell_TrayWnd", L"Shell_SecondaryTrayWnd",
        L"NotifyIconOverflowWindow", L"TopLevelWindowForOverflowXamlIsland", L"TaskListThumbnailWnd",
        L"MultitaskingViewFrame", L"DV2ControlHost"})
        if (CompareStringOrdinal(klass, -1, excluded, -1, TRUE) == CSTR_EQUAL) return false;
    DWORD pid = 0;
    GetWindowThreadProcessId(window, &pid);
    if (!pid || pid == GetCurrentProcessId()) return false;
    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!process) return false;
    wchar_t commonPath[512] = {};
    DWORD length = ARRAYSIZE(commonPath);
    BOOL queried = QueryFullProcessImageNameW(process, 0, commonPath, &length);
    std::wstring executable;
    if (queried) executable.assign(commonPath, length);
    else if (GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
        executable.resize(32768); length = static_cast<DWORD>(executable.size());
        queried = QueryFullProcessImageNameW(process, 0, executable.data(), &length);
        if (queried) executable.resize(length);
    }
    CloseHandle(process);
    if (!queried || !length) return false;
    const size_t separator = executable.find_last_of(L"\\/");
    const std::wstring basename = executable.substr(separator == std::wstring::npos ? 0 : separator + 1);
    for (const wchar_t* excluded : {L"StartMenuExperienceHost.exe", L"ShellExperienceHost.exe", L"SearchHost.exe",
        L"SearchApp.exe", L"LockApp.exe"})
        if (CompareStringOrdinal(basename.c_str(), -1, excluded, -1, TRUE) == CSTR_EQUAL) return false;
    path = std::move(executable); name = basename;
    if (name.size() > 4 && CompareStringOrdinal(name.c_str() + name.size() - 4, -1, L".exe", -1, TRUE) == CSTR_EQUAL)
        name.resize(name.size() - 4);
    return true;
}
} // namespace DimProfiles
