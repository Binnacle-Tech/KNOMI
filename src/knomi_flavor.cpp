#include <string.h>
#include "knomi_flavor.h"

// This firmware's flavour, at a fixed place in the image: right after the app description, in the section
// ESP-IDF keeps for that (image header 24 + segment header 8 + app description 256 = byte 288 of the .bin).
// An upload is checked against it, so the OctoPrint build can't be swapped for the Moonraker one by mistake
// (or the other way round): the KNOMI would come back unable to talk to the printer.
extern "C" const char knomi_flavor_tag[32] __attribute__((section(".rodata_custom_desc"))) =
    KNOMI_FLAVOR_TAG_PREFIX KNOMI_FLAVOR;

const char * knomi_flavor_of(const uint8_t * image_head, size_t n) {
    static char out[24];
    const size_t at = KNOMI_FLAVOR_TAG_OFFSET, pre = sizeof(KNOMI_FLAVOR_TAG_PREFIX) - 1;
    if (n < at + 32 || memcmp(image_head + at, KNOMI_FLAVOR_TAG_PREFIX, pre) != 0) return "";   // older firmware: no tag
    size_t i = 0;
    for (; i < sizeof(out) - 1 && at + pre + i < at + 32; i++) {
        char c = (char)image_head[at + pre + i];
        if (!c) break;
        out[i] = c;
    }
    out[i] = 0;
    return out;
}

const char * knomi_flavor_self(void) { return knomi_flavor_tag + sizeof(KNOMI_FLAVOR_TAG_PREFIX) - 1; }
