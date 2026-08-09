#ifndef HELPER_FUNC_H
#define HELPER_FUNC_H

#include <filesystem>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
#include <Windows.h>
#include <setupapi.h>
#include <devguid.h> // HAS to be after windows.h
#include <spdlog/spdlog.h>

static constexpr std::size_t MEASUREMENT_PACKET_SIZE = 13;
static constexpr int INFINITE_OPERATION = 9999;
constexpr std::string_view DATA_FORMAT_ERR = "Failed to set proper measured data format before reading measurements";
constexpr std::string_view SLEEP_DISABLE_ERR = "Failed to disable Windows sleep while measurements are active";
constexpr std::string_view SLEEP_ENABLE_ERR = "Failed to re-enable windows sleep settings";


///
/// Run the given callable with Windows system sleep (and display timeout) disabled for its duration.
/// Sleep is always re-enabled afterwards, including when func throws.
/// @param func Callable to invoke while sleep is disabled.
/// @throws std::runtime_error if disabling or re-enabling sleep fails.
template<typename Func>
void runWithSleepDisabled(Func &&func) {
    if (SetThreadExecutionState(ES_CONTINUOUS | ES_SYSTEM_REQUIRED | ES_DISPLAY_REQUIRED) == 0) {
        SPDLOG_ERROR(SLEEP_DISABLE_ERR);
        throw std::runtime_error(std::string(SLEEP_DISABLE_ERR));
    }
    try {
        std::forward<Func>(func)();
    } catch (...) {
        SetThreadExecutionState(ES_CONTINUOUS);
        throw;
    }
    if (SetThreadExecutionState(ES_CONTINUOUS) == 0) {
        SPDLOG_ERROR(SLEEP_ENABLE_ERR);
        throw std::runtime_error(std::string(SLEEP_ENABLE_ERR));
    }
}

///
/// Get Path to the directory where user data is to be stored.
/// Do NOT change the name of the directory, it matches the value used in NPET_DP.
/// @return Path to the directory where user data should be stored
inline std::filesystem::path getUserFilesPath() noexcept {
    try {
        std::wstring appdata;
        if (const DWORD SIZE = GetEnvironmentVariableW(L"APPDATA", nullptr, 0); SIZE > 0) {
            appdata.resize(SIZE - 1);
            GetEnvironmentVariableW(L"APPDATA", appdata.data(), SIZE);
        }
        return !appdata.empty()
                   ? std::filesystem::path(appdata) / L"NPET"
                   : std::filesystem::path{L"NPET"};
    } catch (...) {
        return std::filesystem::path{L"NPET"};
    }
}

inline const std::filesystem::path USER_FILES = getUserFilesPath();


struct ISetupDiApi {
    virtual HDEVINFO getClassDevs(const GUID *classGuid, PCTSTR enumerator,
                                  HWND hwndParent, DWORD flags) = 0;

    virtual BOOL enumDeviceInfo(HDEVINFO devInfo, DWORD index,
                                PSP_DEVINFO_DATA devInfoData) = 0;

    virtual BOOL getDeviceRegistryProperty(HDEVINFO devInfo,
                                           PSP_DEVINFO_DATA devInfoData,
                                           DWORD property, PDWORD propertyRegDataType,
                                           PBYTE propertyBuffer, DWORD propertyBufferSize,
                                           PDWORD requiredSize) = 0;

    virtual BOOL destroyDeviceInfoList(HDEVINFO devInfo) = 0;

    ISetupDiApi() = default;

    virtual ~ISetupDiApi() = default;

    ISetupDiApi(const ISetupDiApi &) = delete;

    ISetupDiApi &operator=(const ISetupDiApi &) = delete;

    ISetupDiApi(ISetupDiApi &&) = delete;

    ISetupDiApi &operator=(ISetupDiApi &&) = delete;
};

struct Win32SetupDiApi : ISetupDiApi {
    HDEVINFO getClassDevs(const GUID *classGuid, const PCTSTR ENUMERATOR,
                          HWND hwndParent, const DWORD FLAGS) override {
        return SetupDiGetClassDevs(classGuid, ENUMERATOR, hwndParent, FLAGS);
    }

    BOOL enumDeviceInfo(const HDEVINFO DEV_INFO, const DWORD INDEX,
                        PSP_DEVINFO_DATA devInfoData) override {
        return SetupDiEnumDeviceInfo(DEV_INFO, INDEX, devInfoData);
    }

    BOOL getDeviceRegistryProperty(const HDEVINFO DEV_INFO, PSP_DEVINFO_DATA devInfoData,
                                   const DWORD PROPERTY, PDWORD propertyRegDataType,
                                   PBYTE propertyBuffer, const DWORD PROPERTY_BUFFER_SIZE,
                                   PDWORD requiredSize) override {
        return SetupDiGetDeviceRegistryProperty(DEV_INFO, devInfoData, PROPERTY,
                                                propertyRegDataType, propertyBuffer,
                                                PROPERTY_BUFFER_SIZE, requiredSize);
    }

    BOOL destroyDeviceInfoList(HDEVINFO devInfo) override {
        return SetupDiDestroyDeviceInfoList(devInfo);
    }
};


struct WinApiAdapter {
    virtual BOOL allocateAndInitializeSid(
        PSID_IDENTIFIER_AUTHORITY pIdentifierAuthority,
        BYTE nSubAuthorityCount,
        DWORD dwSubAuthority0, DWORD dwSubAuthority1,
        DWORD dwSubAuthority2, DWORD dwSubAuthority3,
        DWORD dwSubAuthority4, DWORD dwSubAuthority5,
        DWORD dwSubAuthority6, DWORD dwSubAuthority7,
        PSID *pSid) = 0;

    virtual BOOL checkTokenMembership(HANDLE TokenHandle, PSID SidToCheck, PBOOL IsMember) = 0;

    virtual PVOID freeSid(PSID pSid) = 0;

    WinApiAdapter() = default;

    virtual ~WinApiAdapter() = default;

    WinApiAdapter(const WinApiAdapter &) = delete;

    WinApiAdapter &operator=(const WinApiAdapter &) = delete;

    WinApiAdapter(WinApiAdapter &&) = delete;

    WinApiAdapter &operator=(WinApiAdapter &&) = delete;
};

struct RealWinApi : WinApiAdapter {
    BOOL allocateAndInitializeSid(
        PSID_IDENTIFIER_AUTHORITY pIdentifierAuthority,
        const BYTE N_SUB_AUTHORITY_COUNT,
        const DWORD DW_SUB_AUTHORITY0, const DWORD DW_SUB_AUTHORITY1,
        const DWORD DW_SUB_AUTHORITY2, const DWORD DW_SUB_AUTHORITY3,
        const DWORD DW_SUB_AUTHORITY4, const DWORD DW_SUB_AUTHORITY5,
        const DWORD DW_SUB_AUTHORITY6, const DWORD DW_SUB_AUTHORITY7,
        PSID *pSid) override {
        return ::AllocateAndInitializeSid(pIdentifierAuthority, N_SUB_AUTHORITY_COUNT,
                                          DW_SUB_AUTHORITY0, DW_SUB_AUTHORITY1, DW_SUB_AUTHORITY2, DW_SUB_AUTHORITY3,
                                          DW_SUB_AUTHORITY4, DW_SUB_AUTHORITY5, DW_SUB_AUTHORITY6, DW_SUB_AUTHORITY7,
                                          pSid);
    }

    BOOL checkTokenMembership(HANDLE TokenHandle, const PSID SID_TO_CHECK, PBOOL IsMember) override {
        return ::CheckTokenMembership(TokenHandle, SID_TO_CHECK, IsMember);
    }

    PVOID freeSid(const PSID P_SID) override {
        return ::FreeSid(P_SID);
    }
};

inline Win32SetupDiApi &getWin32Api() {
    static Win32SetupDiApi instance;
    return instance;
}

inline RealWinApi &getRealWinApi() {
    static RealWinApi instance;
    return instance;
}

///
/// Get available COM ports on the system and return the list as a vector.
/// Ports listed in excludedPorts are dropped from the result.
/// @param api The API interface to interact with the system's device information.
/// @param excludedPorts COM port numbers to exclude from the result (e.g. {5, 8}).
/// @return: A vector containing the names of the available COM ports.
std::vector<std::string> getComPorts(ISetupDiApi &api = getWin32Api(), const std::vector<int> &excludedPorts = {});

///
/// Extract the numeric COM port number from a friendly device name
/// (e.g. "USB Serial Port (COM8)" -> 8, "USB Serial Port (COM12)" -> 12).
/// @param port Friendly device name containing "COM" followed by one or more digits.
/// @return The COM port number.
/// @throws invalid_argument if the port name does not contain a "COM<digits>" sequence.
int extractComPortNumber(const std::string &port);

///
/// Check if the user has administrator privileges.
/// This function uses Windows API to determine if the current user is an administrator.
/// @return True if the user is an administrator, false otherwise.
bool isUserAdmin(WinApiAdapter &api = getRealWinApi());

#endif //HELPER_FUNC_H
