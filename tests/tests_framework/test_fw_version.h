#ifndef TEST_FW_VERSION_H
#define TEST_FW_VERSION_H

#include <string>
#include <quadmath.h>

struct KnownVersionParams {
    int version;
    __float128 expected_multiplier;
    std::string expected_description;
};

struct EqualityParams {
    int a;
    int b;
    bool expected_equal;
};

#endif //TEST_FW_VERSION_H
