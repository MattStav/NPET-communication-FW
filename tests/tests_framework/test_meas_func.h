#ifndef TEST_XOR_CHECKSUM_H
#define TEST_XOR_CHECKSUM_H

#include <array>
#include <cstdint>
#include <string>

#include "meas_func.h"

struct XorChecksumParams {
    std::array<uint8_t, 13> input;
    uint8_t expected;
};

struct GetMeasurementCmdParams {
    int channel;
    int num;
    std::string expected;
};

struct ProcessMeasurementInvalidHeaderParams {
    std::array<uint8_t, 13> input;
};

// Packet format:
//   byte[0]  = 1    (header)
//   byte[1]  = 11   (header)
//   byte[2]  = meas_num
//   byte[3..5]  = high word (integer seconds contribution)
//   byte[6..8]  = middle word  (sub-second contribution)
//   byte[9..11] = low word  (fractional nanosecond contribution)
//   byte[12]    = XOR of bytes[0..11]
//
// Setting byte[9]=0xFF, byte[10]=0xFF, byte[11]=0x3F makes the fractional
// term equal exactly 0 (cancels the built-in -2^22+1 bias in the formula).
// With byte[3..8]=0 this gives measured_value = 0.
static std::array<uint8_t, 13> make_zero_packet(const uint8_t meas_num = 0) {
    std::array<uint8_t, 13> arr = {1, 11, meas_num, 0, 0, 0, 0, 0, 0, 0xFF, 0xFF, 0x3F, 0};
    arr[12] = xor_checksum(arr);
    return arr;
}

static std::array<uint8_t, 13> set_header(const uint8_t b0, const uint8_t b1) {
    std::array<uint8_t, 13> arr = {b0, b1, 0, 0, 0, 0, 0, 0, 0, 0xFF, 0xFF, 0x3F, 0};
    arr[12] = xor_checksum(arr);
    return arr;
}

struct MeasNumParams {
    uint8_t meas_num;
};

struct TimeConstantParams {
    measurement time_const;
    int         expected_intp;
    double      expected_fracp;
};

struct Float128ToStringParams {
    __float128  input;
    std::string expected;
};


#endif //TEST_XOR_CHECKSUM_H
