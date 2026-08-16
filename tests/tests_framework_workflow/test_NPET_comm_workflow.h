#ifndef TEST_FRAMEWORK_WORKFLOW_H
#define TEST_FRAMEWORK_WORKFLOW_H

#include <string>

#include "test_workflow_fixture.h"

// "name" must contain only letters, digits, and underscores - no spaces, dots, or signs.
struct FrequencyParams {
    std::string name;
    int frequency;
};

struct PulseCountParams {
    std::string name;
    int num_of_pulses;
};

struct TimeConstantParams {
    std::string name;
    int intp;
    __float128 fracp;
};

#endif //TEST_FRAMEWORK_WORKFLOW_H
