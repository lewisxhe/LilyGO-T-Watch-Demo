#ifndef BLUETOOTH_H
#define BLUETOOTH_H

typedef enum {
    BLE_DEVICE_CONNECT,
    BLE_DEVICE_DISCONNECT,
    BLE_DEVICE_NO_SUPPORT
}Ble_Connect_t;


void setupBuletooth();
void bluetooth_start();
Ble_Connect_t bluetooth_connect(const char *devname);
bool bluetooth_isConnect();
void bluetooth_stop();
int bluetooth_get_connect_type();
void buletooth_notify_en(bool en);

#endif /*BLUETOOTH_H */