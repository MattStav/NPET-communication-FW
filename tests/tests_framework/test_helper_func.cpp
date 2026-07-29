#include "test_helper_func.h"

#include <gtest/gtest.h>
#include <cstring>
#include <filesystem>
#include <cstdlib>


using ::testing::_;
using ::testing::DoAll;
using ::testing::Return;
using ::testing::SetArrayArgument;
using ::testing::SetArgPointee;
using ::testing::InSequence;


/// The NPET data processing program shares this app data location
/// DO NOT CHANGE THE NAME
TEST(UserFilesPathTest, EndsWithNPET) {
    EXPECT_EQ(USER_FILES.filename(), "NPET");
}

TEST(UserFilesPathTest, IsAbsolutePath) {
    EXPECT_TRUE(USER_FILES.is_absolute());
}

TEST(UserFilesPathTest, StartsWithAppdata) {
    const char *appdata = std::getenv("APPDATA");
    ASSERT_NE(appdata, nullptr);
    EXPECT_NE(USER_FILES.string().find(appdata), std::string::npos);
}

TEST(UserFilesPathTest, AppdataIsNotNull) {
    EXPECT_NE(std::getenv("APPDATA"), nullptr);
}

static const HDEVINFO K_FAKE_HANDLE = reinterpret_cast<HDEVINFO>(0xDEADBEEF);


// Every test expects exactly one destroyDeviceInfoList call (RAII cleanup).
void GetComPortsTest::expectCleanup() {
    EXPECT_CALL(api, destroyDeviceInfoList(K_FAKE_HANDLE))
            .Times(1)
            .WillOnce(Return(TRUE));
}

/// SetupDigetClassDevs returns INVALID_HANDLE_VALUE → exception thrown
TEST_F(GetComPortsTest, ThrowsWhenHandleIsInvalid) {
    EXPECT_CALL(api, getClassDevs(_, _, _, _))
            .WillOnce(Return(INVALID_HANDLE_VALUE));

    EXPECT_THROW(getComPorts(api), std::runtime_error);
}

/// No devices enumerated → empty vector returned
TEST_F(GetComPortsTest, ReturnsEmptyVectorWhenNoDevicesFound) {
    EXPECT_CALL(api, getClassDevs(_, _, _, _)).WillOnce(Return(K_FAKE_HANDLE));
    EXPECT_CALL(api, enumDeviceInfo(K_FAKE_HANDLE, 0, _)).WillOnce(Return(FALSE));
    expectCleanup();

    const auto PORTS = getComPorts(api);

    EXPECT_TRUE(PORTS.empty());
}

/// One device, property fetch succeeds → single entry returned
TEST_F(GetComPortsTest, ReturnsSinglePort) {
    EXPECT_CALL(api, getClassDevs(_, _, _, _)).WillOnce(Return(K_FAKE_HANDLE)); {
        InSequence seq;
        EXPECT_CALL(api, enumDeviceInfo(K_FAKE_HANDLE, 0, _)).WillOnce(Return(TRUE));
        EXPECT_CALL(api, enumDeviceInfo(K_FAKE_HANDLE, 1, _)).WillOnce(Return(FALSE));
    }
    EXPECT_CALL(api, getDeviceRegistryProperty(K_FAKE_HANDLE, _, SPDRP_FRIENDLYNAME, _, _, _, _))
            .WillOnce(DoAll(
                [](HDEVINFO, PSP_DEVINFO_DATA, DWORD, PDWORD, PBYTE buf, DWORD, PDWORD) {
                    const char *name = "COM3 (USB Serial)";
                    memcpy(buf, name, strlen(name) + 1);
                },
                Return(TRUE)));
    expectCleanup();

    const auto PORTS = getComPorts(api);

    ASSERT_EQ(PORTS.size(), 1U);
    EXPECT_EQ(PORTS.at(0), "COM3 (USB Serial)");
}

/// Multiple devices → all entries returned in enumeration order
TEST_F(GetComPortsTest, ReturnsMultiplePorts) {
    EXPECT_CALL(api, getClassDevs(_, _, _, _)).WillOnce(Return(K_FAKE_HANDLE)); {
        InSequence seq;
        EXPECT_CALL(api, enumDeviceInfo(K_FAKE_HANDLE, 0, _)).WillOnce(Return(TRUE));
        EXPECT_CALL(api, enumDeviceInfo(K_FAKE_HANDLE, 1, _)).WillOnce(Return(TRUE));
        EXPECT_CALL(api, enumDeviceInfo(K_FAKE_HANDLE, 2, _)).WillOnce(Return(FALSE));
    }
    EXPECT_CALL(api, getDeviceRegistryProperty(K_FAKE_HANDLE, _, SPDRP_FRIENDLYNAME, _, _, _, _))
            .WillOnce(DoAll(
                [](HDEVINFO, PSP_DEVINFO_DATA, DWORD, PDWORD, PBYTE buf, DWORD, PDWORD) {
                    const char *name = "COM1 (Serial Port)";
                    memcpy(buf, name, strlen(name) + 1);
                },
                Return(TRUE)))
            .WillOnce(DoAll(
                [](HDEVINFO, PSP_DEVINFO_DATA, DWORD, PDWORD, PBYTE buf, DWORD, PDWORD) {
                    const char *name = "COM7 (Bluetooth)";
                    memcpy(buf, name, strlen(name) + 1);
                },
                Return(TRUE)));
    expectCleanup();

    const auto PORTS = getComPorts(api);

    ASSERT_EQ(PORTS.size(), 2U);
    EXPECT_EQ(PORTS.at(0), "COM1 (Serial Port)");
    EXPECT_EQ(PORTS.at(1), "COM7 (Bluetooth)");
}

/// Property fetch fails for a device → that device is silently skipped
TEST_F(GetComPortsTest, SkipsDeviceWhenPropertyFetchFails) {
    EXPECT_CALL(api, getClassDevs(_, _, _, _)).WillOnce(Return(K_FAKE_HANDLE)); {
        InSequence seq;
        EXPECT_CALL(api, enumDeviceInfo(K_FAKE_HANDLE, 0, _)).WillOnce(Return(TRUE));
        EXPECT_CALL(api, enumDeviceInfo(K_FAKE_HANDLE, 1, _)).WillOnce(Return(TRUE));
        EXPECT_CALL(api, enumDeviceInfo(K_FAKE_HANDLE, 2, _)).WillOnce(Return(FALSE));
    }
    EXPECT_CALL(api, getDeviceRegistryProperty(K_FAKE_HANDLE, _, SPDRP_FRIENDLYNAME, _, _, _, _))
            .WillOnce(Return(FALSE))
            .WillOnce(DoAll(
                [](HDEVINFO, PSP_DEVINFO_DATA, DWORD, PDWORD, PBYTE buf, DWORD, PDWORD) {
                    const char *name = "COM5 (PCI Port)";
                    memcpy(buf, name, strlen(name) + 1);
                },
                Return(TRUE)));
    expectCleanup();

    const auto PORTS = getComPorts(api);

    ASSERT_EQ(PORTS.size(), 1U);
    EXPECT_EQ(PORTS.at(0), "COM5 (PCI Port)");
}

/// Cleanup is called even when an exception is thrown (RAII guarantee)
TEST_F(GetComPortsTest, CleanupCalledOnException) {
    EXPECT_CALL(api, getClassDevs(_, _, _, _))
            .WillOnce(Return(INVALID_HANDLE_VALUE));
    // destroyDeviceInfoList must NOT be called — handle was never valid
    EXPECT_CALL(api, destroyDeviceInfoList(_)).Times(0);

    EXPECT_THROW(getComPorts(api), std::runtime_error);
}

/// Cleanup is always called on the happy path (no leak)
TEST_F(GetComPortsTest, CleanupAlwaysCalledOnSuccess) {
    EXPECT_CALL(api, getClassDevs(_, _, _, _)).WillOnce(Return(K_FAKE_HANDLE));
    EXPECT_CALL(api, enumDeviceInfo(K_FAKE_HANDLE, 0, _)).WillOnce(Return(FALSE));
    // strict: exactly one destroy call
    EXPECT_CALL(api, destroyDeviceInfoList(K_FAKE_HANDLE))
            .Times(1)
            .WillOnce(Return(TRUE));

    getComPorts(api); // should not throw
}

static BYTE g_fake_sid_storage = 0;
static PSID g_fake_sid = &g_fake_sid_storage;

void IsUserAdminTest::SetUp() {
    ON_CALL(api, checkTokenMembership(_, _, _))
            .WillByDefault(Return(FALSE));
    ON_CALL(api, freeSid(_))
            .WillByDefault(Return(nullptr));
}

void IsUserAdminTest::expectAllocSucceeds() {
    EXPECT_CALL(api, allocateAndInitializeSid(_, _, _, _, _, _, _, _, _, _, _))
            .WillOnce([](PSID_IDENTIFIER_AUTHORITY, BYTE, DWORD, DWORD, DWORD,
                         DWORD, DWORD, DWORD, DWORD, DWORD, PSID *out_sid) -> BOOL {
                *out_sid = g_fake_sid;
                return TRUE;
            });
}

// Convenience: make checkTokenMembership set *IsMember and return TRUE.
void IsUserAdminTest::expectCheckToken(BOOL memberValue, BOOL returnValue) {
    EXPECT_CALL(api, checkTokenMembership(nullptr, g_fake_sid, _))
            .WillOnce(DoAll(SetArgPointee<2>(memberValue), Return(returnValue)));
}

void IsUserAdminTest::expectFreeSid() {
    EXPECT_CALL(api, freeSid(g_fake_sid))
            .WillOnce(Return(nullptr)); // mock eats it — real freeSid never called
}


/// Happy path – user IS an administrator
TEST_F(IsUserAdminTest, ReturnsTrue_WhenUserIsAdmin) {
    expectAllocSucceeds();
    expectCheckToken(/*memberValue=*/TRUE);
    expectFreeSid();

    EXPECT_TRUE(isUserAdmin(api));
}

/// Happy path – user is NOT an administrator
TEST_F(IsUserAdminTest, ReturnsFalse_WhenUserIsNotAdmin) {
    expectAllocSucceeds();
    expectCheckToken(/*memberValue=*/FALSE);
    expectFreeSid();

    EXPECT_FALSE(isUserAdmin(api));
}

/// allocateAndInitializeSid fails → must return false, never call checkTokenMembership
TEST_F(IsUserAdminTest, ReturnsFalse_WhenAllocSidFails) {
    EXPECT_CALL(api, allocateAndInitializeSid(_, _, _, _, _, _, _, _, _, _, _))
            .WillOnce(Return(FALSE));
    EXPECT_CALL(api, checkTokenMembership(_, _, _)).Times(0);
    EXPECT_CALL(api, freeSid(_)).Times(0);

    EXPECT_FALSE(isUserAdmin(api));
}

/// checkTokenMembership fails (returns FALSE) → must treat as non-admin
TEST_F(IsUserAdminTest, ReturnsFalse_WhenCheckTokenMembershipFails) {
    expectAllocSucceeds();
    // returnValue = FALSE simulates the API call itself failing
    expectCheckToken(/*memberValue=*/TRUE, /*returnValue=*/FALSE);
    expectFreeSid();

    EXPECT_FALSE(isUserAdmin(api));
}

/// freeSid is always called when allocateAndInitializeSid succeeds (no leak)
TEST_F(IsUserAdminTest, FreeSidAlwaysCalled_AfterSuccessfulAlloc) {
    expectAllocSucceeds();
    expectCheckToken(FALSE);
    // Strict expectation: freeSid called exactly once with the allocated SID
    EXPECT_CALL(api, freeSid(g_fake_sid)).Times(1).WillOnce(Return(nullptr));

    isUserAdmin(api);
}

/// The return type is exactly bool (no BOOL/int leakage)
TEST_F(IsUserAdminTest, ReturnType_IsStrictlyBool) {
    expectAllocSucceeds();
    expectCheckToken(TRUE);
    expectFreeSid();

    auto result = isUserAdmin(api); // NOLINT
    EXPECT_TRUE((std::is_same_v<decltype(result), bool>));
}
