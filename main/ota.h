#pragma once

/* Firmware updates over Wi-Fi.
 *
 * The hat joins the Wi-Fi network set in menuconfig, announces itself
 * over mDNS and takes a new image as an HTTP POST to /ota, which it writes
 * to the other app slot and boots. All of it runs on core 1.
 *
 * A freshly updated image has to prove itself: if it does not reach the
 * network and report a working main loop within two minutes, or crashes
 * before then, the bootloader goes back to the previous image. */

/* Starts Wi-Fi and the update server. Returns straight away. With no SSID
 * set, it only accepts the running image. */
void ota_start(void);

/* Tells the update check that the main loop runs. */
void ota_app_ok(void);
