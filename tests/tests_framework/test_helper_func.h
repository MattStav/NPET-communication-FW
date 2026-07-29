#ifndef TEST_HELPER_FUNC_H
#define TEST_HELPER_FUNC_H

#include <gmock/gmock.h>
#include "helper_func.h"


class MockSetupDiApi : public ISetupDiApi {
public:
    MOCK_METHOD(HDEVINFO, getClassDevs,
                (const GUID*, PCTSTR, HWND, DWORD), (override));
    MOCK_METHOD(BOOL, enumDeviceInfo,
                (HDEVINFO, DWORD, PSP_DEVINFO_DATA), (override));
    MOCK_METHOD(BOOL, getDeviceRegistryProperty,
                (HDEVINFO, PSP_DEVINFO_DATA, DWORD, PDWORD, PBYTE, DWORD, PDWORD),
                (override));
    MOCK_METHOD(BOOL, destroyDeviceInfoList, (HDEVINFO), (override));
};

class GetComPortsTest : public ::testing::Test {
protected:
    MockSetupDiApi api;


    void expectCleanup();
};

struct MockWinApi : WinApiAdapter {
    MOCK_METHOD(BOOL, allocateAndInitializeSid,
                (PSID_IDENTIFIER_AUTHORITY, BYTE, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, PSID*),
                (override));
    MOCK_METHOD(BOOL, checkTokenMembership, (HANDLE, PSID, PBOOL), (override));
    MOCK_METHOD(PVOID, freeSid, (PSID), (override));
};

class IsUserAdminTest : public ::testing::Test {
protected:
    MockWinApi api;

    void SetUp() override;

    void expectAllocSucceeds();

    // Convenience: make checkTokenMembership set *IsMember and return TRUE.
    void expectCheckToken(BOOL memberValue, BOOL returnValue = TRUE);

    void expectFreeSid();
};

#endif //TEST_HELPER_FUNC_H
