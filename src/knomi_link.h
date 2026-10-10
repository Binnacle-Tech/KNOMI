#ifndef KNOMI_LINK_H
#define KNOMI_LINK_H
// The KNOMI's own WiFi network, which only the Pi joins (OctoPrint build; knomi_link.cpp)
#include <Arduino.h>
#include <IPAddress.h>
#define KNOMI_LINK_IP      IPAddress(192, 168, 77, 1)   // the KNOMI; the Pi gets .2 from its DHCP
#define KNOMI_LINK_CHANNEL 6

void knomi_link_load(void);           // name and password (made up on first start)
bool knomi_link_start(void);          // the network itself
void knomi_link_new_password(void);   // after forgetting paired devices (the network restarts with it)
bool knomi_link_take_restart(void);   // WiFi task: restart the network now
const char * knomi_link_ssid(void);
int knomi_link_clients(void);         // devices on the network now
String knomi_link_secret_json(void);  // {"ssid","psk","ip"}: only for the encrypted Bluetooth characteristic
#endif
