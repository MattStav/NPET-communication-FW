#ifndef FW_VERSION_H
#define FW_VERSION_H
#include <string>
#include <quadmath.h>

///
/// Represents the NPET firmware version and encapsulates version-specific behaviour,
/// such as the measurement multiplier and a human-readable description.
class FWVersion {
public:
    // Recognized NPET firmware versions
    static constexpr int ORIGINAL = 1;
    static constexpr int AD_REVISION = 2;
    static constexpr int VIRTUAL = 3;

    FWVersion() = default;
    explicit FWVersion(int VERSION);

    // Get the raw numerical firmware version
    [[nodiscard]] int getValue() const;

    // Get the measurement multiplier associated with this firmware version
    [[nodiscard]] __float128 getMultiplier() const;

    // Get a human-readable description of this firmware version
    [[nodiscard]] std::string_view getDescription() const;

    bool operator==(const FWVersion &other) const;

    bool operator!=(const FWVersion &other) const;

private:
    int version_{0};
}; // end of FWVersion class

#endif //FW_VERSION_H
