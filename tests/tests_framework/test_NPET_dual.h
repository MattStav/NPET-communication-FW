#ifndef TEST_NPET_DUAL_H
#define TEST_NPET_DUAL_H

#include <gtest/gtest.h>
#include "NPET_dual.h"

// start_/stop_ are protected on NPETDual; expose them for inspection in tests.
class NPETDualFixture : public ::testing::Test, public NPETDual {
protected:
    using NPETDual::start_;
    using NPETDual::stop_;
};

#endif //TEST_NPET_DUAL_H
