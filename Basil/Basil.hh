#include <Windows.h>
#include <utility>
#include <optional>
#include <string_view>
#include <unordered_map>
#include <array>

#undef max
#undef min

#define NOMINMAX

namespace Basil {


    namespace Detail {

        constexpr DWORD dwLocalPID = 0;
        constexpr DWORD dwPageSize = 0x1000;

        using Page = std::array<std::uint8_t, dwPageSize>;

        template <std::size_t nSeed = 0x543C730D, std::size_t nPrime = 0x1000931>
        struct Hasher {
            constexpr static std::size_t Get(const char* szKey, std::size_t nLength) {
                std::size_t nHash = nSeed;

                for (std::size_t iIdx = 0; iIdx < nLength; ++iIdx) {
                    const std::uint8_t u8Val = szKey[iIdx];
                    nHash ^= u8Val;
                    nHash *= nPrime;
                }

                return nHash;
            }
        };

    }


    namespace Implementation {

        template <typename T>
        [[nodiscard]] std::optional<std::pair<T, std::size_t>> ReadMemory(HANDLE hHandle, std::uintptr_t uAt) {
            T pPossibleResult { };
            SIZE_T nBytesRead = 0;

            if (!ReadProcessMemory(hHandle, (LPCVOID)(uAt), (LPVOID)(&pPossibleResult), sizeof(T), &nBytesRead))
                return std::nullopt;
            
            return std::make_pair(pPossibleResult, nBytesRead);
        }

        template <typename T>
        std::pair<bool, std::size_t> WriteMemory(HANDLE hHandle, std::uintptr_t uAt, const T pValue) {
            SIZE_T nBytesRead = 0;
            return std::make_pair(WriteProcessMemory(hHandle, (LPVOID)(uAt), &pValue, sizeof(T), &nBytesRead), nBytesRead);
        }

        template <std::size_t N>
        [[nodiscard]] std::optional<std::uintptr_t> PatternScanProcess(HANDLE hHandle, const std::array<int, N>& arrPattern, std::uintptr_t uStart, std::uintptr_t uEnd) {
            const std::size_t nArraySize = arrPattern.size();
            const std::uintptr_t uReach = uEnd - nArraySize;

            for (std::uintptr_t iIdx = uStart; iIdx < uReach; iIdx = min(iIdx + Detail::dwPageSize, uReach)) {
                const auto pRead = ReadMemory<Detail::Page>(hHandle, iIdx);
                if (!pRead.has_value())
                    continue;

                const auto& [nBytes, nBytesRead] = pRead.value();
                for (std::size_t jIdx = 0; jIdx < Detail::dwPageSize - nArraySize; ++jIdx) {
                    bool bFound = true;
                    for (std::size_t kIdx = 0; kIdx < nArraySize; ++kIdx) {
                        if (nBytes[jIdx + kIdx] != arrPattern[kIdx] && arrPattern[kIdx] != -1) {
                            bFound = false;
                            break;
                        }
                    }

                    if (bFound)
                        return iIdx + jIdx;
                }
            }

            return std::nullopt;
        }

    }

    struct Module_t {
        std::uint8_t*   m_u8Start;
        std::size_t     m_nSize;
    };

    struct Ctx_t {
        Ctx_t() = delete;
        [[nodiscard]] Ctx_t(std::string&& szName);
        ~Ctx_t();

        Ctx_t(const Ctx_t&) = default;
        Ctx_t(Ctx_t&&) = default;

        Ctx_t& operator=(const Ctx_t&) = default;
        Ctx_t& operator=(Ctx_t&&) = default;

        const std::string& GetName() const;
        std::optional<std::uint32_t> GetPID() const;
        std::optional<HANDLE> GetHandle() const;
        std::optional<HANDLE> GetHandleModulesSnapshot() const;
        std::optional<Module_t> GetModule(std::string&& szName) const;
        std::optional<Module_t> CaptureModule(std::string&& szName);

        void CaptureAllModules();

        template <typename T>
        [[nodiscard]] inline std::optional<std::pair<T, std::size_t>> ReadMemory(std::uintptr_t uAt) const;

        template <typename T>
        [[nodiscard]] inline std::optional<std::pair<T, std::size_t>> ReadModuleMemory(std::string&& szName, std::uintptr_t uAt) const;

        template <typename T>
        inline std::pair<bool, std::size_t> WriteMemory(std::uintptr_t uAt, const T pValue) const;

        template <typename T>
        inline std::pair<bool, std::size_t> WriteModuleMemory(std::string&& szName, std::uintptr_t uAt, const T pValue) const;

        template <std::size_t N>
        [[nodiscard]] inline std::optional<std::uintptr_t> PatternScan(const std::array<int, N>& arrPattern, std::uintptr_t uStart, std::uintptr_t uEnd) const;

        template <std::size_t N>
        [[nodiscard]] inline std::optional<std::uintptr_t> PatternScanModule(std::string&& szName, const std::array<int, N>& arrPattern) const;

    private:
        std::string m_szName;
        std::optional<std::uint32_t> m_u32Pid;
        std::optional<HANDLE> m_hHandle;
        std::optional<HANDLE> m_hHandleModulesSnapshot;
        std::unordered_map<std::size_t, Module_t> m_nModules;
    };

    template <typename T>
    std::optional<std::pair<T, std::size_t>> Ctx_t::ReadMemory(std::uintptr_t uAt) const {
        return Implementation::ReadMemory<T>(this->m_hHandle.value(), uAt);
    }

    template <typename T>
    std::optional<std::pair<T, std::size_t>> Ctx_t::ReadModuleMemory(std::string&& szName, std::uintptr_t uAt) const {
        const std::optional<Module_t> pModule = GetModule(std::move(szName));

        if (pModule.has_value()) {
            const std::uintptr_t uStart = (std::uintptr_t)(pModule.value().m_u8Start);
            return ReadMemory<T>(uStart + uAt);
        }

        return std::nullopt;
    }

    template <typename T>
    std::pair<bool, std::size_t> Ctx_t::WriteMemory(std::uintptr_t uAt, const T pValue) const {
        return Implementation::WriteMemory<T>(this->m_hHandle.value(), uAt, pValue);
    }

    template <typename T>
    std::pair<bool, std::size_t> Ctx_t::WriteModuleMemory(std::string&& szName, std::uintptr_t uAt, const T pValue) const {
        const std::optional<Module_t> pModule = GetModule(std::move(szName));

        if (pModule.has_value()) {
            const std::uintptr_t uStart = (std::uintptr_t)(pModule.value().m_u8Start);
            return WriteMemory<T>(uStart + uAt, pValue);
        }

        return std::make_pair(false, 0);
    }

    template <std::size_t N>
    std::optional<std::uintptr_t> Ctx_t::PatternScan(const std::array<int, N>& arrPattern, std::uintptr_t uStart, std::uintptr_t uEnd) const {
        return Implementation::PatternScanProcess<N>(this->m_hHandle.value(), arrPattern, uStart, uEnd);
    }

    template <std::size_t N>
    std::optional<std::uintptr_t> Ctx_t::PatternScanModule(std::string&& szName, const std::array<int, N>& arrPattern) const {
        const std::optional<Module_t> pModule = GetModule(std::move(szName));

        if (pModule.has_value()) {
            const auto& [uStart, uEnd] = pModule.value();
            const std::uintptr_t uStartAddress = (std::uintptr_t)(uStart);
            return PatternScan<N>(arrPattern, uStartAddress, uStartAddress + uEnd);
        }

        return std::nullopt;
    }

}