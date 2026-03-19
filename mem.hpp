#pragma once
#define NOMINMAX   // prevent windows.h from defining min/max macros
#include <windows.h>
#include <tlhelp32.h>
#include <string>
#include <vector>
#include <optional>
#include <stdexcept>
#include <cstdint>
#include <algorithm>

// ============================================================
//  mem.hpp — Typed process memory API
//
//  mem::ProcessMemory proc("game.exe");
//
//  auto val  = proc.read<float>(addr);          // returns optional
//  proc.write<float>(addr, 999.f);
//  uintptr_t addr = proc.resolvePointerChain(base + 0xDEAD, { 0x10, 0x4 });
//  uintptr_t base = proc.getModuleBase("game.exe");
//  uintptr_t hit  = proc.patternScan(base, 0x1000000, "48 8B ? ? ? 48 85");
// ============================================================

namespace mem {

class ProcessMemory {
public:

    // -- Attach by process name ------------------------------
    explicit ProcessMemory(const std::string& processName,
                           DWORD access = PROCESS_VM_READ   |
                                          PROCESS_VM_WRITE  |
                                          PROCESS_VM_OPERATION |
                                          PROCESS_QUERY_INFORMATION)
    {
        pid_ = findPID(processName);
        if (pid_ == 0)
            throw std::runtime_error("Process not found: " + processName);

        handle_ = OpenProcess(access, FALSE, pid_);
        if (!handle_ || handle_ == INVALID_HANDLE_VALUE)
            throw std::runtime_error("OpenProcess failed — try running as Administrator.");
    }

    // -- Attach by PID ---------------------------------------
    explicit ProcessMemory(DWORD pid,
                           DWORD access = PROCESS_VM_READ   |
                                          PROCESS_VM_WRITE  |
                                          PROCESS_VM_OPERATION |
                                          PROCESS_QUERY_INFORMATION)
        : pid_(pid)
    {
        handle_ = OpenProcess(access, FALSE, pid_);
        if (!handle_ || handle_ == INVALID_HANDLE_VALUE)
            throw std::runtime_error("OpenProcess failed.");
    }

    ~ProcessMemory() {
        if (handle_ && handle_ != INVALID_HANDLE_VALUE)
            CloseHandle(handle_);
    }

    ProcessMemory(const ProcessMemory&)            = delete;
    ProcessMemory& operator=(const ProcessMemory&) = delete;
    ProcessMemory(ProcessMemory&& o) noexcept
        : handle_(o.handle_), pid_(o.pid_) { o.handle_ = nullptr; }

    HANDLE handle() const { return handle_; }
    DWORD  pid()    const { return pid_;    }

    // -- read<T> ---------------------------------------------
    // Returns std::nullopt on failure — never crashes on bad addresses.

    template<typename T>
    std::optional<T> read(uintptr_t address) const {
        T value{};
        SIZE_T bytesRead = 0;
        if (ReadProcessMemory(handle_,
                              reinterpret_cast<LPCVOID>(address),
                              &value, sizeof(T), &bytesRead)
            && bytesRead == sizeof(T))
            return value;
        return std::nullopt;
    }

    // -- write<T> --------------------------------------------
    // Returns true on success.

    template<typename T>
    bool write(uintptr_t address, const T& value) const {
        SIZE_T written = 0;
        return WriteProcessMemory(handle_,
                                  reinterpret_cast<LPVOID>(address),
                                  &value, sizeof(T), &written)
               && written == sizeof(T);
    }

    // -- readBytes -------------------------------------------

    bool readBytes(uintptr_t address, void* buffer, size_t size) const {
        SIZE_T bytesRead = 0;
        return ReadProcessMemory(handle_,
                                 reinterpret_cast<LPCVOID>(address),
                                 buffer, size, &bytesRead)
               && bytesRead == size;
    }

    // -- resolvePointerChain ---------------------------------
    // Walks a multilevel pointer chain:
    //   addr = *(base) + offsets[0]
    //   addr = *(addr) + offsets[1]  ...
    // Returns 0 if any dereference fails.

    uintptr_t resolvePointerChain(uintptr_t base,
                                  const std::vector<uintptr_t>& offsets) const
    {
        uintptr_t addr = base;
        for (const uintptr_t offset : offsets) {
            auto ptr = read<uintptr_t>(addr);
            if (!ptr || *ptr == 0) return 0;
            addr = *ptr + offset;
        }
        return addr;
    }

    // -- getModuleBase ---------------------------------------
    // Returns load address of a module in the target process.
    // Returns 0 if the module is not loaded yet.

    uintptr_t getModuleBase(const std::string& moduleName) const {
        HANDLE snap = CreateToolhelp32Snapshot(
            TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid_);
        if (snap == INVALID_HANDLE_VALUE) return 0;

        MODULEENTRY32 me{};
        me.dwSize = sizeof(me);

        for (BOOL ok = Module32First(snap, &me); ok;
             ok = Module32Next(snap, &me))
        {
            if (_stricmp(me.szModule, moduleName.c_str()) == 0) {
                CloseHandle(snap);
                return reinterpret_cast<uintptr_t>(me.modBaseAddr);
            }
        }
        CloseHandle(snap);
        return 0;
    }

    // -- patternScan -----------------------------------------
    // Scans [start, start+length) for a byte pattern.
    // Use '?' or '??' as wildcards.
    // Returns address of first match, or 0.

    uintptr_t patternScan(uintptr_t start, size_t length,
                          const std::string& pattern) const
    {
        std::vector<uint8_t> bytes;
        std::vector<bool>    wildcard;

        for (size_t i = 0; i < pattern.size(); ) {
            if (pattern[i] == ' ') { ++i; continue; }
            if (pattern[i] == '?') {
                bytes.push_back(0x00);
                wildcard.push_back(true);
                if (i + 1 < pattern.size() && pattern[i+1] == '?') ++i;
                ++i;
            } else {
                uint8_t b = static_cast<uint8_t>(
                    std::stoul(pattern.substr(i, 2), nullptr, 16));
                bytes.push_back(b);
                wildcard.push_back(false);
                i += 2;
            }
        }
        if (bytes.empty()) return 0;

        constexpr size_t CHUNK = 4096;
        std::vector<uint8_t> buf(CHUNK + bytes.size());

        for (size_t offset = 0; offset < length; offset += CHUNK) {
            size_t toRead = std::min(CHUNK + bytes.size() - 1, length - offset);
            SIZE_T bytesRead = 0;
            if (!ReadProcessMemory(handle_,
                                   reinterpret_cast<LPCVOID>(start + offset),
                                   buf.data(), toRead, &bytesRead))
                continue;

            for (size_t i = 0; i + bytes.size() <= bytesRead; ++i) {
                bool match = true;
                for (size_t j = 0; j < bytes.size(); ++j) {
                    if (!wildcard[j] && buf[i + j] != bytes[j]) {
                        match = false; break;
                    }
                }
                if (match) return start + offset + i;
            }
        }
        return 0;
    }

private:
    HANDLE handle_ = nullptr;
    DWORD  pid_    = 0;

    static DWORD findPID(const std::string& name) {
        HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (snap == INVALID_HANDLE_VALUE) return 0;

        PROCESSENTRY32 pe{};
        pe.dwSize = sizeof(pe);
        DWORD found = 0;

        for (BOOL ok = Process32First(snap, &pe); ok;
             ok = Process32Next(snap, &pe))
        {
            if (_stricmp(pe.szExeFile, name.c_str()) == 0) {
                found = pe.th32ProcessID;
                break;
            }
        }
        CloseHandle(snap);
        return found;
    }
};

} // namespace mem