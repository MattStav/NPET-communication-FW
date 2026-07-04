#include "helper_func.h"

#include <rang.hpp>
#include <string>

#include <spdlog/spdlog.h>
#include <spdlog/fmt/ranges.h>  // enables formatting of vectors, arrays, etc.


///
/// Get available COM ports on the system and return the list as a vector.
/// @return: A vector containing the names of the available COM ports.
std::vector<std::string> get_com_ports(ISetupDiApi &api) {
    SPDLOG_DEBUG("Getting available COM ports...");
    std::vector<std::string> com_ports;
    const HDEVINFO hDevInfo = api.GetClassDevs(&GUID_DEVCLASS_PORTS, nullptr, nullptr, DIGCF_PRESENT);
    if (hDevInfo == INVALID_HANDLE_VALUE) {
        SPDLOG_ERROR("Failed to get available COM ports");
        throw std::runtime_error("Failed to get available COM ports");
    }
    auto device_info_scope = std::unique_ptr<void, std::function<void(HDEVINFO)> >(
        hDevInfo, [&api](const HDEVINFO h) { api.DestroyDeviceInfoList(h); });
    SP_DEVINFO_DATA devInfoData = {.cbSize = sizeof(SP_DEVINFO_DATA)};
    for (DWORD i = 0; api.EnumDeviceInfo(hDevInfo, i, &devInfoData); i++) {
        if (TCHAR deviceName[256]; api.GetDeviceRegistryProperty(hDevInfo, &devInfoData,
                                                                 SPDRP_FRIENDLYNAME, nullptr,
                                                                 reinterpret_cast<PBYTE>(deviceName),
                                                                 sizeof(deviceName), nullptr)) {
            com_ports.emplace_back(deviceName);
        }
    } // end of for loop
    SPDLOG_INFO("Available COM ports: {}", com_ports);
    return com_ports;
} // end of get_com_ports function


///
/// Check if the user has administrator privileges.
/// This function uses Windows API to determine if the current user is an administrator.
/// @return True if the user is an administrator, false otherwise.
bool is_user_admin(WinApiAdapter& api) {
    SPDLOG_DEBUG("Checking if user has administrator privileges...");
    BOOL is_admin = FALSE;
    PSID admin_group = nullptr;
    // Allocate and initialize a SID for the Administrators group.
    SID_IDENTIFIER_AUTHORITY nt_authority = SECURITY_NT_AUTHORITY;
    if (api.AllocateAndInitializeSid(
        &nt_authority, 2, SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_ADMINS,
        0, 0, 0, 0, 0, 0, &admin_group)) {
        // Check if the token is admin.
        if (!api.CheckTokenMembership(nullptr, admin_group, &is_admin)) {
            is_admin = FALSE;
        }
        api.FreeSid(admin_group);
    }
    SPDLOG_INFO("User has administrator privileges: {}", is_admin);
    return is_admin != FALSE;
} // end of is_user_admin function
