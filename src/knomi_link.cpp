// The KNOMI network (OctoPrint build): a small WiFi network just between the Pi and the KNOMI. The KNOMI never
// joins your WiFi: Bluetooth is the control link, and this network carries the heavy things (pages, firmware,
// animations, later the camera).
// The KNOMI makes up the name and password on first start and keeps them; the plugin reads them over the paired,
// encrypted Bluetooth link. The Pi hosts the network (OP60+) and the KNOMI joins it: hosting one itself, the ESP32
// has to listen for WiFi all the time and Bluetooth starved (OP58/59: 4 failed connects a second once the Pi was
// on). Only when it can't reach the Pi at all does the KNOMI host one of its own, so a phone can always get in.
// Forgetting paired devices makes a new password, so a Pi that's no longer paired can't stay on either.
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

// join the Pi's network (normal running): fixed addresses, so neither side needs to hand any out
bool knomi_link_start(void) {
    char ssid[33], psk[33];
    portENTER_CRITICAL(&link_mux);
    memcpy(ssid, link_ssid, sizeof(ssid)); memcpy(psk, link_psk, sizeof(psk));
    portEXIT_CRITICAL(&link_mux);
    WiFi.mode(WIFI_STA);
    WiFi.setSleep(true);   // modem sleep: what lets Bluetooth share the radio
    WiFi.config(KNOMI_LINK_IP, KNOMI_LINK_PI_IP, IPAddress(255, 255, 255, 0));
    WiFi.setAutoReconnect(true);
    bool ok = WiFi.begin(ssid, psk) != WL_CONNECT_FAILED;
    memset(psk, 0, sizeof(psk));
    Serial.printf("link: joining %s (hosted by the Pi) as %s\r\n", ssid, IPAddress(KNOMI_LINK_IP).toString().c_str());
    return ok;
}

bool knomi_link_joined(void) { return WiFi.status() == WL_CONNECTED; }
int knomi_link_rssi(void) { return knomi_link_joined() ? WiFi.RSSI() : 0; }

// can't reach the Pi at all: host a network of our own as well (a phone can join it; the QR code on the screen)
static bool rescue = false;
bool knomi_link_rescue(void) { return rescue; }
void knomi_link_set_rescue(bool on) {
    if (on == rescue) return;
    rescue = on;
    if (on) {
        char ssid[40], psk[33];
        portENTER_CRITICAL(&link_mux);
        snprintf(ssid, sizeof(ssid), "%s-setup", link_ssid); memcpy(psk, link_psk, sizeof(psk));
        portEXIT_CRITICAL(&link_mux);
        WiFi.mode(WIFI_AP_STA);   // keeps looking for the Pi's network meanwhile
        WiFi.softAPConfig(KNOMI_RESCUE_IP, KNOMI_RESCUE_IP, IPAddress(255, 255, 255, 0));
        bool ok = WiFi.softAP(ssid, psk, KNOMI_LINK_CHANNEL, 0, 2);
        memset(psk, 0, sizeof(psk));
        Serial.printf("link: can't reach the Pi, %s my own network %s at %s\r\n", ok ? "opened" : "COULDN'T OPEN",
                      ssid, IPAddress(KNOMI_RESCUE_IP).toString().c_str());
    } else {
        WiFi.softAPdisconnect(true);
        WiFi.mode(WIFI_STA);
        Serial.println("link: closed my own network again");
    }
}
const char * knomi_link_rescue_ssid(void) {
    static char b[40];
    portENTER_CRITICAL(&link_mux);
    snprintf(b, sizeof(b), "%s-setup", link_ssid);
    portEXIT_CRITICAL(&link_mux);
    return b;
}

int knomi_link_clients(void) { return rescue ? WiFi.softAPgetStationNum() : 0; }

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
    // host "pi": the Pi hosts the network at "pi" and the KNOMI joins as "ip" (plugin 0.16+; older ones just try to join)
    String s = String("{\"ssid\":\"") + ssid + "\",\"psk\":\"" + psk + "\",\"host\":\"pi\",\"ip\":\"" +
               IPAddress(KNOMI_LINK_IP).toString() + "\",\"pi\":\"" + IPAddress(KNOMI_LINK_PI_IP).toString() + "\",\"ch\":" +
               String(KNOMI_LINK_CHANNEL) + "}";
    memset(psk, 0, sizeof(psk));
    return s;
}
#endif
