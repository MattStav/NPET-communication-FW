#include "fw_version.h"

#include <stdexcept>
#include <spdlog/spdlog.h>

#include "meas_func.h"

FWVersion::FWVersion(const int VERSION) : version_(VERSION) {
} // end of FWVersion constructor


///
/// Get the raw numerical firmware version.
/// @return Numerical firmware version
int FWVersion::getValue() const {
    return version_;
} // end of FWVersion::getValue function


///
/// Get the measurement multiplier associated with this firmware version.
/// @return Measurement multiplier as a 128-bit floating point number
__float128 FWVersion::getMultiplier() const {
    __float128 mult{};
    if (version_ == ORIGINAL) {
        mult = 0.00000002;
    } else if (version_ == AD_REVISION || version_ == VIRTUAL) {
        mult = 0.00000001;
    } else {
        throw std::invalid_argument("Unknown FW version");
    }
    SPDLOG_DEBUG("Measurement multiplier for FW version {}: {}", version_, float128ToString(mult));
    return mult;
} // end of FWVersion::getMultiplier function


///
/// Get a human-readable description of this firmware version.
/// @return Description of the firmware version
std::string_view FWVersion::getDescription() const {
    if (version_ == ORIGINAL) {
        return "Original";
    }
    if (version_ == AD_REVISION) {
        return "Revision for NPET with AD component";
    }
    if (version_ == VIRTUAL) {
        return "Virtual NPET";
    }
    throw std::invalid_argument("Unknown FW version");
} // end of FWVersion::getDescription function


bool FWVersion::operator==(const FWVersion &other) const {
    return version_ == other.version_;
} // end of FWVersion::operator== function


bool FWVersion::operator!=(const FWVersion &other) const {
    return !(*this == other); // NOLINT(*-redundant-parentheses)
} // end of FWVersion::operator!= function
