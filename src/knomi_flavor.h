#ifndef KNOMI_FLAVOR_H
#define KNOMI_FLAVOR_H
// Build flavour, picked in platformio.ini (one source, two firmwares):
//   OctoPrint (default)  the OctoPrint-KNOMI plugin drives the KNOMI over Bluetooth
//   Moonraker            stock behaviour: the KNOMI joins your WiFi and talks to Moonraker (Klipper)
// Code for one flavour only is marked with `#if KNOMI_OCTOPRINT` (or `if (KNOMI_OCTOPRINT)`, which the
// compiler drops just the same in the other build).
#if defined(KNOMI_FLAVOR_MOONRAKER)
#define KNOMI_OCTOPRINT 0
#define KNOMI_FLAVOR "moonraker"
#define KNOMI_FLAVOR_NAME "Moonraker (Klipper)"
#else
#define KNOMI_OCTOPRINT 1
#define KNOMI_FLAVOR "octoprint"
#define KNOMI_FLAVOR_NAME "OctoPrint"
#endif

#ifdef KNOMIV1
#define KNOMI_BOARD "knomiv1"
#else
#define KNOMI_BOARD "knomiv2"
#endif
// this build's file in the GitHub releases (the updater and the plugin install it)
#define KNOMI_ASSET KNOMI_BOARD "-" KNOMI_FLAVOR "-firmware.bin"
// written into the firmware so an update can tell which flavour a .bin is (see knomi_flavor_of)
#define KNOMI_FLAVOR_TAG_PREFIX "KNOMI-FLAVOR:"
#define KNOMI_FLAVOR_TAG_OFFSET 288   // in the .bin (knomi_flavor.cpp)
#ifdef __cplusplus
#include <stddef.h>
#include <stdint.h>
const char * knomi_flavor_of(const uint8_t * image_head, size_t n);   // "" if the image has no tag (older firmware)
const char * knomi_flavor_self(void);                                  // this firmware's (keeps the tag in the build)
#endif
#endif
