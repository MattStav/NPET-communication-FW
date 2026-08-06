#include "test_vm_main.h"

#include <gtest/gtest.h>
#include <boost/asio.hpp>
#include <stdexcept>


class InvalidFrequencyTest : public testing::TestWithParam<InvalidFrequencyParams> {
};

// launchVm rejects a channel-1 frequency above 2500Hz before ever touching the serial port,
// so this must hold regardless of com_port and without any COM port actually existing.
TEST_P(InvalidFrequencyTest, FrequencyAboveLimitThrowsBeforeOpeningPort) {
    const VmConfig CONFIG{.com_port = 1, .ch1_frequency = GetParam().ch1_frequency};
    EXPECT_THROW((void) launchVm(CONFIG), std::invalid_argument);
}

INSTANTIATE_TEST_SUITE_P(
    AboveFrequencyLimit,
    InvalidFrequencyTest,
    testing::Values(
        InvalidFrequencyParams{2501},
        InvalidFrequencyParams{5000},
        InvalidFrequencyParams{100000}
    )
);

// A frequency at or below the 2500Hz limit must pass validation, so launchVm proceeds to open
// the (here, non-existent) COM port; the resulting failure comes from Boost.Asio rather than
// the std::invalid_argument thrown by the validation check itself, proving the check was cleared.
TEST(VmConfigValidation, FrequencyAtLimitPassesValidationAndAttemptsToOpenPort) {
    const VmConfig CONFIG{.com_port = 250, .ch1_frequency = 2500};
    EXPECT_THROW((void) launchVm(CONFIG), boost::system::system_error);
}
