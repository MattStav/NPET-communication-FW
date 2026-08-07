#include "NPET_dual.h"


bool NPETDual::isStartResponsive(const bool END_STREAM) {
    return start_.isResponsive(END_STREAM);
}

bool NPETDual::isStopResponsive(const bool END_STREAM) {
    return stop_.isResponsive(END_STREAM);
}

void NPETDual::purgeStartPort() {
    start_.getPort().cancel();
    PurgeComm(start_.getPort().native_handle(), PURGE_RXCLEAR | PURGE_RXABORT | PURGE_TXCLEAR | PURGE_TXABORT);
}

void NPETDual::purgeStopPort() {
    stop_.getPort().cancel();
    PurgeComm(stop_.getPort().native_handle(), PURGE_RXCLEAR | PURGE_RXABORT | PURGE_TXCLEAR | PURGE_TXABORT);
}
