#ifndef TEST_NPET_DUAL_H
#define TEST_NPET_DUAL_H

#include <gtest/gtest.h>
#include <optional>
#include <string>
#include "NPET_dual.h"

// start_/stop_ are protected on NPETDual; expose them for inspection in tests.
class NPETDualFixture : public ::testing::Test, public NPETDual {
protected:
    using NPETDual::start_;
    using NPETDual::stop_;
};

struct DualToStringParams {
    int num_of_meas;
    bool has_monitor;
    std::optional<std::string> save_dir;
    Channel start_channel;
    Channel stop_channel;
    std::string expected;
};

#endif //TEST_NPET_DUAL_H
