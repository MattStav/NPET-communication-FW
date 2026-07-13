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
    EXPECT_NE(USER_FILES.string().find(appdata), std::string::npos);
}

TEST(UserFilesPathTest, AppdataIsNotNull) {
    EXPECT_NE(appdata, nullptr);
}

static const HDEVINFO kFakeHandle = reinterpret_cast<HDEVINFO>(0xDEADBEEF);


// Every test expects exactly one DestroyDeviceInfoList call (RAII cleanup).
void GetComPortsTest::ExpectCleanup() {
    EXPECT_CALL(api, DestroyDeviceInfoList(kFakeHandle))
            .Times(1)
            .WillOnce(Return(TRUE));
}

/// SetupDiGetClassDevs returns INVALID_HANDLE_VALUE → exception thrown
TEST_F(GetComPortsTest, ThrowsWhenHandleIsInvalid) {
    EXPECT_CALL(api, GetClassDevs(_, _, _, _))
            .WillOnce(Return(INVALID_HANDLE_VALUE));

    EXPECT_THROW(get_com_ports(api), std::runtime_error);
}

/// No devices enumerated → empty vector returned
TEST_F(GetComPortsTest, ReturnsEmptyVectorWhenNoDevicesFound) {
    EXPECT_CALL(api, GetClassDevs(_, _, _, _)).WillOnce(Return(kFakeHandle));
    EXPECT_CALL(api, EnumDeviceInfo(kFakeHandle, 0, _)).WillOnce(Return(FALSE));
    ExpectCleanup();

    const auto ports = get_com_ports(api);

    EXPECT_TRUE(ports.empty());
}

/// One device, property fetch succeeds → single entry returned
TEST_F(GetComPortsTest, ReturnsSinglePort) {
    EXPECT_CALL(api, GetClassDevs(_, _, _, _)).WillOnce(Return(kFakeHandle)); {
        InSequence seq;
        EXPECT_CALL(api, EnumDeviceInfo(kFakeHandle, 0, _)).WillOnce(Return(TRUE));
        EXPECT_CALL(api, EnumDeviceInfo(kFakeHandle, 1, _)).WillOnce(Return(FALSE));
    }
    EXPECT_CALL(api, GetDeviceRegistryProperty(kFakeHandle, _, SPDRP_FRIENDLYNAME, _, _, _, _))
            .WillOnce(DoAll(
                [](HDEVINFO, PSP_DEVINFO_DATA, DWORD, PDWORD, PBYTE buf, DWORD, PDWORD) {
                    const char *name = "COM3 (USB Serial)";
                    memcpy(buf, name, strlen(name) + 1);
                },
                Return(TRUE)));
    ExpectCleanup();

    const auto ports = get_com_ports(api);

    ASSERT_EQ(ports.size(), 1u);
    EXPECT_EQ(ports[0], "COM3 (USB Serial)");
}

/// Multiple devices → all entries returned in enumeration order
TEST_F(GetComPortsTest, ReturnsMultiplePorts) {
    EXPECT_CALL(api, GetClassDevs(_, _, _, _)).WillOnce(Return(kFakeHandle)); {
        InSequence seq;
        EXPECT_CALL(api, EnumDeviceInfo(kFakeHandle, 0, _)).WillOnce(Return(TRUE));
        EXPECT_CALL(api, EnumDeviceInfo(kFakeHandle, 1, _)).WillOnce(Return(TRUE));
        EXPECT_CALL(api, EnumDeviceInfo(kFakeHandle, 2, _)).WillOnce(Return(FALSE));
    }
    EXPECT_CALL(api, GetDeviceRegistryProperty(kFakeHandle, _, SPDRP_FRIENDLYNAME, _, _, _, _))
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
    ExpectCleanup();

    const auto ports = get_com_ports(api);

    ASSERT_EQ(ports.size(), 2u);
    EXPECT_EQ(ports[0], "COM1 (Serial Port)");
    EXPECT_EQ(ports[1], "COM7 (Bluetooth)");
}

/// Property fetch fails for a device → that device is silently skipped
TEST_F(GetComPortsTest, SkipsDeviceWhenPropertyFetchFails) {
    EXPECT_CALL(api, GetClassDevs(_, _, _, _)).WillOnce(Return(kFakeHandle)); {
        InSequence seq;
        EXPECT_CALL(api, EnumDeviceInfo(kFakeHandle, 0, _)).WillOnce(Return(TRUE));
        EXPECT_CALL(api, EnumDeviceInfo(kFakeHandle, 1, _)).WillOnce(Return(TRUE));
        EXPECT_CALL(api, EnumDeviceInfo(kFakeHandle, 2, _)).WillOnce(Return(FALSE));
    }
    EXPECT_CALL(api, GetDeviceRegistryProperty(kFakeHandle, _, SPDRP_FRIENDLYNAME, _, _, _, _))
            .WillOnce(Return(FALSE))
            .WillOnce(DoAll(
                [](HDEVINFO, PSP_DEVINFO_DATA, DWORD, PDWORD, PBYTE buf, DWORD, PDWORD) {
                    const char *name = "COM5 (PCI Port)";
                    memcpy(buf, name, strlen(name) + 1);
                },
                Return(TRUE)));
    ExpectCleanup();

    const auto ports = get_com_ports(api);

    ASSERT_EQ(ports.size(), 1u);
    EXPECT_EQ(ports[0], "COM5 (PCI Port)");
}

/// Cleanup is called even when an exception is thrown (RAII guarantee)
TEST_F(GetComPortsTest, CleanupCalledOnException) {
    EXPECT_CALL(api, GetClassDevs(_, _, _, _))
            .WillOnce(Return(INVALID_HANDLE_VALUE));
    // DestroyDeviceInfoList must NOT be called — handle was never valid
    EXPECT_CALL(api, DestroyDeviceInfoList(_)).Times(0);

    EXPECT_THROW(get_com_ports(api), std::runtime_error);
}

/// Cleanup is always called on the happy path (no leak)
TEST_F(GetComPortsTest, CleanupAlwaysCalledOnSuccess) {
    EXPECT_CALL(api, GetClassDevs(_, _, _, _)).WillOnce(Return(kFakeHandle));
    EXPECT_CALL(api, EnumDeviceInfo(kFakeHandle, 0, _)).WillOnce(Return(FALSE));
    // strict: exactly one destroy call
    EXPECT_CALL(api, DestroyDeviceInfoList(kFakeHandle))
            .Times(1)
            .WillOnce(Return(TRUE));

    get_com_ports(api); // should not throw
}

static BYTE g_fake_sid_storage = 0;
static PSID g_fake_sid = &g_fake_sid_storage;

void IsUserAdminTest::SetUp() {
    ON_CALL(api, CheckTokenMembership(_, _, _))
            .WillByDefault(Return(FALSE));
    ON_CALL(api, FreeSid(_))
            .WillByDefault(Return(nullptr));
}

void IsUserAdminTest::ExpectAllocSucceeds() {
    EXPECT_CALL(api, AllocateAndInitializeSid(_, _, _, _, _, _, _, _, _, _, _))
            .WillOnce([](PSID_IDENTIFIER_AUTHORITY, BYTE, DWORD, DWORD, DWORD,
                         DWORD, DWORD, DWORD, DWORD, DWORD, PSID *out_sid) -> BOOL {
                *out_sid = g_fake_sid;
                return TRUE;
            });
}

// Convenience: make CheckTokenMembership set *IsMember and return TRUE.
void IsUserAdminTest::ExpectCheckToken(BOOL memberValue, BOOL returnValue) {
    EXPECT_CALL(api, CheckTokenMembership(nullptr, g_fake_sid, _))
            .WillOnce(DoAll(SetArgPointee<2>(memberValue), Return(returnValue)));
}

void IsUserAdminTest::ExpectFreeSid() {
    EXPECT_CALL(api, FreeSid(g_fake_sid))
            .WillOnce(Return(nullptr)); // mock eats it — real FreeSid never called
}


/// Happy path – user IS an administrator
TEST_F(IsUserAdminTest, ReturnsTrue_WhenUserIsAdmin) {
    ExpectAllocSucceeds();
    ExpectCheckToken(/*memberValue=*/TRUE);
    ExpectFreeSid();

    EXPECT_TRUE(is_user_admin(api));
}

/// Happy path – user is NOT an administrator
TEST_F(IsUserAdminTest, ReturnsFalse_WhenUserIsNotAdmin) {
    ExpectAllocSucceeds();
    ExpectCheckToken(/*memberValue=*/FALSE);
    ExpectFreeSid();

    EXPECT_FALSE(is_user_admin(api));
}

/// AllocateAndInitializeSid fails → must return false, never call CheckTokenMembership
TEST_F(IsUserAdminTest, ReturnsFalse_WhenAllocSidFails) {
    EXPECT_CALL(api, AllocateAndInitializeSid(_, _, _, _, _, _, _, _, _, _, _))
            .WillOnce(Return(FALSE));
    EXPECT_CALL(api, CheckTokenMembership(_, _, _)).Times(0);
    EXPECT_CALL(api, FreeSid(_)).Times(0);

    EXPECT_FALSE(is_user_admin(api));
}

/// CheckTokenMembership fails (returns FALSE) → must treat as non-admin
TEST_F(IsUserAdminTest, ReturnsFalse_WhenCheckTokenMembershipFails) {
    ExpectAllocSucceeds();
    // returnValue = FALSE simulates the API call itself failing
    ExpectCheckToken(/*memberValue=*/TRUE, /*returnValue=*/FALSE);
    ExpectFreeSid();

    EXPECT_FALSE(is_user_admin(api));
}

/// FreeSid is always called when AllocateAndInitializeSid succeeds (no leak)
TEST_F(IsUserAdminTest, FreeSidAlwaysCalled_AfterSuccessfulAlloc) {
    ExpectAllocSucceeds();
    ExpectCheckToken(FALSE);
    // Strict expectation: FreeSid called exactly once with the allocated SID
    EXPECT_CALL(api, FreeSid(g_fake_sid)).Times(1).WillOnce(Return(nullptr));

    is_user_admin(api);
}

/// The return type is exactly bool (no BOOL/int leakage)
TEST_F(IsUserAdminTest, ReturnType_IsStrictlyBool) {
    ExpectAllocSucceeds();
    ExpectCheckToken(TRUE);
    ExpectFreeSid();

    auto result = is_user_admin(api); // NOLINT
    EXPECT_TRUE((std::is_same_v<decltype(result), bool>));
}
