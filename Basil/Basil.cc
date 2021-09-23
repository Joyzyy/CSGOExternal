#include "Basil.hh"
#include <TlHelp32.h>
#include <stdexcept>

using namespace Basil;

Ctx_t::Ctx_t(std::string&& szName) {
    if (!szName.ends_with(".exe")) {
        throw std::runtime_error("Extension not supported for process context.");
        return;
    }

    if (szName.empty()) {
        throw std::runtime_error("Empty name.");
        return;
    }

    this->m_szName = std::move(szName);
    const std::size_t nNameHash = Detail::Hasher<>::Get(this->m_szName.c_str(), this->m_szName.size());

    HANDLE hLocalSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPALL, Detail::dwLocalPID);
    if (hLocalSnapshot == INVALID_HANDLE_VALUE) {
        throw std::runtime_error("Invalid handle value on local snapshot.");
        return;
    }

    PROCESSENTRY32 p32 = PROCESSENTRY32 {.dwSize = sizeof(PROCESSENTRY32)};
    for (bool bCopied = Process32First(hLocalSnapshot, &p32); bCopied; bCopied = Process32Next(hLocalSnapshot, &p32)) {
        const std::size_t nCurrentHash = Detail::Hasher<>::Get(p32.szExeFile, strlen(p32.szExeFile));
        if (nNameHash == nCurrentHash) {
            this->m_u32Pid = p32.th32ProcessID;
        }
    }

    CloseHandle(hLocalSnapshot);

    if (!this->m_u32Pid.has_value()) {
        throw std::runtime_error("Failed finding process " + this->m_szName + " in process list.");
        return;
    } else {
        if (this->m_u32Pid.value() == 0) {
            throw std::runtime_error("Process " + this->m_szName + "'s PID is 0. Exiting.");
            return;
        }
    }

    this->m_hHandleModulesSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, this->m_u32Pid.value());
    if (!this->m_hHandleModulesSnapshot.has_value()) {
        throw std::runtime_error("Failed opening handle to " + this->m_szName + " in process list.");
        return;
    } else {
        if (this->m_hHandleModulesSnapshot.value() == INVALID_HANDLE_VALUE) {
            throw std::runtime_error("Invalid handle value to " + this->m_szName + " in process list.");
            return;
        }
    }
}

Ctx_t::~Ctx_t() {
    if (this->m_hHandle.has_value()) {
        CloseHandle(this->m_hHandle.value());
    }

    if (this->m_hHandleModulesSnapshot.has_value()) {
        CloseHandle(this->m_hHandleModulesSnapshot.value());
    }
}

const std::string& Ctx_t::GetName() const {
    return this->m_szName;
}

std::optional<std::uint32_t> Ctx_t::GetPID() const {
    return this->m_u32Pid;
}

std::optional<HANDLE> Ctx_t::GetHandle() const {
    return this->m_hHandle;
}

std::optional<HANDLE> Ctx_t::GetHandleModulesSnapshot() const {
    return this->m_hHandleModulesSnapshot;
}

std::optional<Module_t> Ctx_t::GetModule(std::string&& szName) const {
    const std::size_t nHash = Detail::Hasher<>::Get(szName.c_str(), szName.size());

    if (this->m_nModules.contains(nHash)) {
        return this->m_nModules.at(nHash);
    }

    return std::nullopt;
}

std::optional<Module_t> Ctx_t::CaptureModule(std::string&& szName) {
    std::size_t nNameHash = Detail::Hasher<>::Get(szName.c_str(), szName.size());

    MODULEENTRY32 m32{.dwSize = sizeof(MODULEENTRY32)};
    if (GetHandleModulesSnapshot().has_value()) {
        const HANDLE& hSnapshot = GetHandleModulesSnapshot().value();
        for (bool bCopied = Module32First(hSnapshot, &m32); bCopied; bCopied = Module32Next(hSnapshot, &m32)) {
            const std::size_t nCurrentHash = Detail::Hasher<>::Get(m32.szModule, strlen(m32.szModule));

            if (nNameHash = nCurrentHash) {
                this->m_nModules[nNameHash] = Module_t{m32.modBaseAddr, m32.modBaseSize};
                return this->m_nModules[nNameHash];
            }
        }
    }

    return std::nullopt;
}

void Ctx_t::CaptureAllModules() {
    MODULEENTRY32 m32{.dwSize = sizeof(MODULEENTRY32)};
    if (GetHandleModulesSnapshot().has_value()) {
        const HANDLE& hSnapshot = GetHandleModulesSnapshot().value();
        for (bool bCopied = Module32First(hSnapshot, &m32); bCopied; bCopied = Module32Next(hSnapshot, &m32)) {
            const std::size_t nCurrentHash = Detail::Hasher<>::Get(m32.szModule, strlen(m32.szModule));
            this->m_nModules[nCurrentHash] = Module_t{m32.modBaseAddr, m32.modBaseSize};
        }
    } else {
        throw std::runtime_error("No available handle to the modules snapshot.");
    }
}