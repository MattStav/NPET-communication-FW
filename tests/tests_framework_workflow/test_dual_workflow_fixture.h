#ifndef TEST_DUAL_WORKFLOW_FIXTURE_H
#define TEST_DUAL_WORKFLOW_FIXTURE_H

#include <gtest/gtest.h>
#include <memory>
#include <thread>

#include "NPET_dual.h"
#include "test_workflow_fixture.h"
#include "virtual_machine.h"

///
/// Two independent virtual null-modem pairs, provisioned ahead of time. The start leg
/// reuses VM_COM_PORT/CLIENT_COM_PORT (see test_workflow_fixture.h) - safe because each
/// gtest_discover_tests-discovered test runs in its own process, so only one fixture's
/// SetUpTestSuite() (and thus one open of COM8/COM9) is ever active at a time.
inline constexpr int START_VM_COM_PORT = VM_COM_PORT;
inline constexpr int START_CLIENT_COM_PORT = CLIENT_COM_PORT;
inline constexpr int STOP_VM_COM_PORT = 10;
inline constexpr int STOP_CLIENT_COM_PORT = 11;

///
/// Brings up one VirtualMachine per NPETDual leg (start/stop) once per test suite, mirroring
/// FrameworkWorkflowFixture but doubled up for NPETDual. Unlike FrameworkWorkflowFixture, the
/// client side (NPETDual::start_/stop_) is not static: this fixture derives from NPETDual
/// directly, so every test gets its own fresh, unopened pair of NPETComm members (per
/// NPETDualFixture's pattern - see test_NPET_dual.h) which SetUp() opens onto the already-running
/// VMs' client-side ports, and ~NPETComm() closes again as the fixture is torn down.
class DualFrameworkWorkflowFixture : public ::testing::Test, public NPETDual {
protected:
    using NPETDual::one_;
    using NPETDual::two_;
    using NPETDual::start;
    using NPETDual::stop;
    using NPETDual::exportConstants;
    using NPETDual::clearConstants;

    static std::unique_ptr<VirtualMachine> start_vm;
    static std::unique_ptr<VirtualMachine> stop_vm;
    static std::unique_ptr<std::jthread> start_vm_thread;
    static std::unique_ptr<std::jthread> stop_vm_thread;

    static void SetUpTestSuite();

    static void TearDownTestSuite();

    void SetUp() override;

    void TearDown() override;
};

#endif //TEST_DUAL_WORKFLOW_FIXTURE_H
