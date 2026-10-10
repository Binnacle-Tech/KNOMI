// The KNOMI's own WiFi network for the Pi (OctoPrint build). The KNOMI never joins your WiFi: Bluetooth is the
// control link, and this network carries the heavy things (pages, firmware, animations, later the camera).
// Name and password are made up on first start and kept; the plugin reads them over the paired, encrypted
// Bluetooth link and the Pi joins on a spare WiFi interface. Forgetting paired devices makes a new password,
// so a Pi that's no longer paired can't stay on either.
#include "knomi_flavor.h"
#if KNOMI_OCTOPRINT
#include <Arduino.h>
#include <WiFi.h>
#include <Preferences.h>
#include <esp_random.h>
#include "knomi_link.h"

static char link_ssid[33], link_psk[33];
static portMUX_TYPE link_mux = portMUX_INITIALIZER_UNLOCKED;

static void make_psk(char * out, size_t n) {
    // 20 characters from an alphabet without look-alikes: ~115 bits, and typeable if it ever has to be
    static const char abc[] = "abcdefghijkmnpqrstuvwxyzACDEFGHJKLMNPQRTUVWXY3479";
    size_t len = 20 < n - 1 ? 20 : n - 1;
    for (size_t i = 0; i < len; i++) out[i] = abc[esp_random() % (sizeof(abc) - 1)];
    out[len] = 0;
}

void knomi_link_load(void) {
    Preferences p;
    p.begin("knomilink", false);
    String ssid = p.getString("ssid", ""), psk = p.getString("psk", "");
    if (ssid.isEmpty()) {
        uint8_t mac[6];
        WiFi.macAddress(mac);
        char b[16];
        snprintf(b, sizeof(b), "KNOMI-%02X%02X%02X", mac[3], mac[4], mac[5]);
        ssid = b;
        p.putString("ssid", ssid);
    }
    if (psk.length() < 8) {
        char buf[33];
        make_psk(buf, sizeof(buf));
        psk = buf;
        p.putString("psk", psk);
        Serial.println("link: made a new network password");
    }
    p.end();
    portENTER_CRITICAL(&link_mux);
    strlcpy(link_ssid, ssid.c_str(), sizeof(link_ssid));
    strlcpy(link_psk, psk.c_str(), sizeof(link_psk));
    portEXIT_CRITICAL(&link_mux);
}

static volatile bool restart_wanted = false;
void knomi_link_new_password(void) {
    Preferences p;
    p.begin("knomilink", false);
    p.remove("psk");
    p.end();
    knomi_link_load();
    restart_wanted = true;   // the WiFi task restarts the network with it (anyone on the old one drops off)
}
bool knomi_link_take_restart(void) { bool r = restart_wanted; restart_wanted = false; return r; }

const char * knomi_link_ssid(void) { return link_ssid; }

bool knomi_link_start(void) {
    WiFi.mode(WIFI_AP);
    WiFi.softAPConfig(KNOMI_LINK_IP, KNOMI_LINK_IP, IPAddress(255, 255, 255, 0));
    char ssid[33], psk[33];
    portENTER_CRITICAL(&link_mux);
    memcpy(ssid, link_ssid, sizeof(ssid)); memcpy(psk, link_psk, sizeof(psk));
    portEXIT_CRITICAL(&link_mux);
    // WPA2, fixed channel, room for the Pi and one more (a phone, to look at the pages directly)
    bool ok = WiFi.softAP(ssid, psk, KNOMI_LINK_CHANNEL, 0, 2);
    memset(psk, 0, sizeof(psk));
    Serial.printf("link: %s %s on channel %d, %s\r\n", ok ? "network" : "COULDN'T START network", ssid,
                  KNOMI_LINK_CHANNEL, WiFi.softAPIP().toString().c_str());
    return ok;
}

int knomi_link_clients(void) { return WiFi.softAPgetStationNum(); }

void knomi_link_password(char * out, size_t n) {   // for the join-me QR code on the KNOMI's own screen
    portENTER_CRITICAL(&link_mux);
    strlcpy(out, link_psk, n);
    portEXIT_CRITICAL(&link_mux);
}

// for the plugin, over the encrypted Bluetooth link only (the password is never logged or shown)
String knomi_link_secret_json(void) {
    char ssid[33], psk[33];
    portENTER_CRITICAL(&link_mux);
    memcpy(ssid, link_ssid, sizeof(ssid)); memcpy(psk, link_psk, sizeof(psk));
    portEXIT_CRITICAL(&link_mux);
    String s = String("{\"ssid\":\"") + ssid + "\",\"psk\":\"" + psk + "\",\"ip\":\"" +
               IPAddress(KNOMI_LINK_IP).toString() + "\"}";
    memset(psk, 0, sizeof(psk));
    return s;
}
#endif
