#ifndef TEST_HELPER_FUNC_H
#define TEST_HELPER_FUNC_H

#include <gmock/gmock.h>
#include "helper_func.h"


class MockSetupDiApi : public ISetupDiApi {
public:
    MOCK_METHOD(HDEVINFO, GetClassDevs,
                (const GUID*, PCTSTR, HWND, DWORD), (override));
    MOCK_METHOD(BOOL, EnumDeviceInfo,
                (HDEVINFO, DWORD, PSP_DEVINFO_DATA), (override));
    MOCK_METHOD(BOOL, GetDeviceRegistryProperty,
                (HDEVINFO, PSP_DEVINFO_DATA, DWORD, PDWORD, PBYTE, DWORD, PDWORD),
                (override));
    MOCK_METHOD(BOOL, DestroyDeviceInfoList, (HDEVINFO), (override));
};

class GetComPortsTest : public ::testing::Test {
protected:
    MockSetupDiApi api;


    void ExpectCleanup();
};

struct MockWinApi : WinApiAdapter {
    MOCK_METHOD(BOOL, AllocateAndInitializeSid,
                (PSID_IDENTIFIER_AUTHORITY, BYTE, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, PSID*),
                (override));
    MOCK_METHOD(BOOL, CheckTokenMembership, (HANDLE, PSID, PBOOL), (override));
    MOCK_METHOD(PVOID, FreeSid, (PSID), (override));
};

class IsUserAdminTest : public ::testing::Test {
protected:
    MockWinApi api;

    void SetUp() override;

    void ExpectAllocSucceeds();

    // Convenience: make CheckTokenMembership set *IsMember and return TRUE.
    void ExpectCheckToken(BOOL memberValue, BOOL returnValue = TRUE);

    void ExpectFreeSid();
};

#endif //TEST_HELPER_FUNC_H
