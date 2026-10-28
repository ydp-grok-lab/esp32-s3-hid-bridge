#pragma once

// Only meaningful in [env:wokwi] (-DWOKWI_SELFTEST). Starts a FreeRTOS task that
// waits for the network, then calls the device's own HTTP API over loopback and
// prints "[selftest] ..." lines for `wokwi-cli --expect-text / --fail-text`.
#ifdef WOKWI_SELFTEST
void wokwiSelftestBegin();
#endif
