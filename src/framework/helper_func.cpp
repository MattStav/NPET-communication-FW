#include "helper_func.h"

#include <array>
#include <rang.hpp>
#include <string>

#include <spdlog/spdlog.h>
#include <spdlog/fmt/ranges.h>  // enables formatting of vectors, arrays, etc.


///
/// Get available COM ports on the system and return the list as a vector.
/// @return: A vector containing the names of the available COM ports.
std::vector<std::string> getComPorts(ISetupDiApi &api) {
    SPDLOG_DEBUG("Getting available COM ports...");
    std::vector<std::string> com_ports;
    const HDEVINFO H_DEV_INFO = api.getClassDevs(&GUID_DEVCLASS_PORTS, nullptr, nullptr, DIGCF_PRESENT);
    if (H_DEV_INFO == INVALID_HANDLE_VALUE) {
        SPDLOG_ERROR("Failed to get available COM ports");
        throw std::runtime_error("Failed to get available COM ports");
    }
    auto device_info_scope = std::unique_ptr<void, std::function<void(HDEVINFO)> >(
        H_DEV_INFO, [&api](const HDEVINFO H) { api.destroyDeviceInfoList(H); });
    SP_DEVINFO_DATA dev_info_data = {.cbSize = sizeof(SP_DEVINFO_DATA)};
    for (DWORD i = 0; api.enumDeviceInfo(H_DEV_INFO, i, &dev_info_data) != 0; i++) {
        if (std::array<TCHAR, 256> device_name{}; api.getDeviceRegistryProperty(H_DEV_INFO, &dev_info_data,
                                                                 SPDRP_FRIENDLYNAME, nullptr,
                                                                 reinterpret_cast<PBYTE>(device_name.data()),
                                                                 sizeof(device_name), nullptr)) {
            com_ports.emplace_back(device_name.data());
        }
    } // end of for loop
    SPDLOG_INFO("Available COM ports: {}", com_ports);
    return com_ports;
} // end of get_com_ports function


///
/// Check if the user has administrator privileges.
/// This function uses Windows API to determine if the current user is an administrator.
/// @return True if the user is an administrator, false otherwise.
bool isUserAdmin(WinApiAdapter& api) {
    SPDLOG_DEBUG("Checking if user has administrator privileges...");
    BOOL is_admin = FALSE;
    PSID admin_group = nullptr;
    // Allocate and initialize a SID for the Administrators group.
    SID_IDENTIFIER_AUTHORITY nt_authority = SECURITY_NT_AUTHORITY;
    if (api.allocateAndInitializeSid(
        &nt_authority, 2, SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_ADMINS,
        0, 0, 0, 0, 0, 0, &admin_group) != 0) {
        // Check if the token is admin.
        if (api.checkTokenMembership(nullptr, admin_group, &is_admin) == 0) {
            is_admin = FALSE;
        }
        api.freeSid(admin_group);
    }
    SPDLOG_INFO("User has administrator privileges: {}", is_admin);
    return is_admin != FALSE;
} // end of is_user_admin function
