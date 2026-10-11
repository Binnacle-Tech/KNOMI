#ifndef KNOMI_LINK_H
#define KNOMI_LINK_H
// The KNOMI network: the Pi hosts it, the KNOMI joins (OctoPrint build; knomi_link.cpp)
#include <Arduino.h>
#include <IPAddress.h>
#define KNOMI_LINK_PI_IP   IPAddress(192, 168, 77, 1)   // the Pi, hosting the network
#define KNOMI_LINK_IP      IPAddress(192, 168, 77, 2)   // the KNOMI
#define KNOMI_RESCUE_IP    IPAddress(192, 168, 78, 1)   // the KNOMI's own network, only when it can't reach the Pi
#define KNOMI_LINK_CHANNEL 6

void knomi_link_load(void);           // name and password (made up on first start)
bool knomi_link_start(void);          // join the Pi's network
bool knomi_link_joined(void);         // on it now
int knomi_link_rssi(void);            // dBm while joined
void knomi_link_set_rescue(bool on);  // host a network of our own too (can't reach the Pi)
bool knomi_link_rescue(void);
const char * knomi_link_rescue_ssid(void);
void knomi_link_new_password(void);   // after forgetting paired devices (rejoins with it)
bool knomi_link_take_restart(void);   // WiFi task: rejoin now
const char * knomi_link_ssid(void);
int knomi_link_clients(void);         // devices on our own network (rescue only)
void knomi_link_password(char * out, size_t n);   // only for the QR code on the KNOMI's own screen
String knomi_link_secret_json(void);  // {"ssid","psk","host":"pi","ip","pi","ch"}: only for the encrypted Bluetooth characteristic
#endif
