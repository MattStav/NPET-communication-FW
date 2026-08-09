#ifndef NTP_SYNC_H
#define NTP_SYNC_H

///
/// Ensure that the system time is accurate by triggering an NTP sync.
/// This function only works on Windows systems.
/// @return True if the system time was successfully synchronized, false otherwise.
bool ensureAccurateSystemTime();

#endif //NTP_SYNC_H
