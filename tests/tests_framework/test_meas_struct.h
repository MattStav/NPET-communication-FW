#ifndef TEST_MEAS_STRUCT_H
#define TEST_MEAS_STRUCT_H

#include <string>

struct IsEmptyParams {
    int meas_num;
    int intp;
    __float128 fracp;
};

struct ToStringParams {
    int        intp;
    __float128 fracp;
    std::string expected;
};

struct ResolveParams {
    int        intp;
    __float128 fracp;
    int        expected_intp;
    double     expected_fracp;
};

struct RoundParams {
    int        intp;
    __float128 fracp;
    int        expected;
};

struct OperatorPlusParams {
    int        a_intp;
    __float128 a_fracp;
    int        b_intp;
    __float128 b_fracp;
    int        expected_intp;
    double     expected_fracp;
};

#endif //TEST_MEAS_STRUCT_H
