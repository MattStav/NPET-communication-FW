#ifndef HELPER_FUNC_H
#define HELPER_FUNC_H

#include <filesystem>
#include <string>
#include <vector>
#include <Windows.h>
#include <setupapi.h>
#include <devguid.h> // HAS to be after windows.h


inline const char *appdata = std::getenv("APPDATA");
inline const std::filesystem::path USER_FILES = appdata
    ? std::filesystem::path(appdata) / "NPET_FW"
    : std::filesystem::path{"NPET_FW"};


struct ISetupDiApi {
    virtual HDEVINFO GetClassDevs(const GUID *classGuid, PCTSTR enumerator,
                                  HWND hwndParent, DWORD flags) = 0;

    virtual BOOL EnumDeviceInfo(HDEVINFO devInfo, DWORD index,
                                PSP_DEVINFO_DATA devInfoData) = 0;

    virtual BOOL GetDeviceRegistryProperty(HDEVINFO devInfo,
                                           PSP_DEVINFO_DATA devInfoData,
                                           DWORD property, PDWORD propertyRegDataType,
                                           PBYTE propertyBuffer, DWORD propertyBufferSize,
                                           PDWORD requiredSize) = 0;

    virtual BOOL DestroyDeviceInfoList(HDEVINFO devInfo) = 0;

    virtual ~ISetupDiApi() = default;
};

struct Win32SetupDiApi : ISetupDiApi {
    HDEVINFO GetClassDevs(const GUID *classGuid, const PCTSTR enumerator,
                          HWND hwndParent, const DWORD flags) override {
        return SetupDiGetClassDevs(classGuid, enumerator, hwndParent, flags);
    }

    BOOL EnumDeviceInfo(const HDEVINFO devInfo, const DWORD index,
                        PSP_DEVINFO_DATA devInfoData) override {
        return SetupDiEnumDeviceInfo(devInfo, index, devInfoData);
    }

    BOOL GetDeviceRegistryProperty(const HDEVINFO devInfo, PSP_DEVINFO_DATA devInfoData,
                                   const DWORD property, PDWORD propertyRegDataType,
                                   PBYTE propertyBuffer, const DWORD propertyBufferSize,
                                   PDWORD requiredSize) override {
        return SetupDiGetDeviceRegistryProperty(devInfo, devInfoData, property,
                                                propertyRegDataType, propertyBuffer,
                                                propertyBufferSize, requiredSize);
    }

    BOOL DestroyDeviceInfoList(HDEVINFO devInfo) override {
        return SetupDiDestroyDeviceInfoList(devInfo);
    }
};


struct WinApiAdapter {
    virtual BOOL AllocateAndInitializeSid(
        PSID_IDENTIFIER_AUTHORITY pIdentifierAuthority,
        BYTE nSubAuthorityCount,
        DWORD dwSubAuthority0, DWORD dwSubAuthority1,
        DWORD dwSubAuthority2, DWORD dwSubAuthority3,
        DWORD dwSubAuthority4, DWORD dwSubAuthority5,
        DWORD dwSubAuthority6, DWORD dwSubAuthority7,
        PSID *pSid) = 0;

    virtual BOOL CheckTokenMembership(HANDLE TokenHandle, PSID SidToCheck, PBOOL IsMember) = 0;

    virtual PVOID FreeSid(PSID pSid) = 0;

    virtual ~WinApiAdapter() = default;
};

struct RealWinApi : WinApiAdapter {
    BOOL AllocateAndInitializeSid(
        PSID_IDENTIFIER_AUTHORITY pIdentifierAuthority,
        const BYTE nSubAuthorityCount,
        const DWORD dwSubAuthority0, const DWORD dwSubAuthority1,
        const DWORD dwSubAuthority2, const DWORD dwSubAuthority3,
        const DWORD dwSubAuthority4, const DWORD dwSubAuthority5,
        const DWORD dwSubAuthority6, const DWORD dwSubAuthority7,
        PSID *pSid) override {
        return ::AllocateAndInitializeSid(pIdentifierAuthority, nSubAuthorityCount,
                                          dwSubAuthority0, dwSubAuthority1, dwSubAuthority2, dwSubAuthority3,
                                          dwSubAuthority4, dwSubAuthority5, dwSubAuthority6, dwSubAuthority7, pSid);
    }

    BOOL CheckTokenMembership(HANDLE TokenHandle, const PSID SidToCheck, PBOOL IsMember) override {
        return ::CheckTokenMembership(TokenHandle, SidToCheck, IsMember);
    }

    PVOID FreeSid(const PSID pSid) override {
        return ::FreeSid(pSid);
    }
};

inline Win32SetupDiApi g_win32_api;
inline RealWinApi g_real_win_api;

std::vector<std::string> get_com_ports(ISetupDiApi &api = g_win32_api);

bool is_user_admin(WinApiAdapter &api = g_real_win_api);

#endif //HELPER_FUNC_H
