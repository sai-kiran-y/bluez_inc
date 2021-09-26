//
// Created by martijn on 12/9/21.
//

#ifndef TEST_CHARACTERISTIC_H
#define TEST_CHARACTERISTIC_H

#include <gio/gio.h>
#include "service.h"

typedef struct sCharacteristic Characteristic;

typedef enum WriteType {
    WITH_RESPONSE = 0, WITHOUT_RESPONSE = 1
} WriteType;

typedef void (*NotifyingStateChangedCallback)(Characteristic *characteristic);

typedef void (*OnNotifyCallback)(Characteristic *characteristic, GByteArray *byteArray);
typedef void (*OnReadCallback)(Characteristic *characteristic, GByteArray *byteArray, GError *error);
typedef void (*OnWriteCallback)(Characteristic *characteristic, GError *error);

typedef struct sCharacteristic {
    GDBusConnection *connection;
    const char *path;
    const char *uuid;
    const char *service_path;
    const char *service_uuid;
    gboolean notifying;
    GList *flags;

    guint notify_signal;
    NotifyingStateChangedCallback notify_state_callback;
    OnReadCallback on_read_callback;
    OnWriteCallback on_write_callback;
    OnNotifyCallback on_notify_callback;
} Characteristic;

Characteristic *binc_characteristic_create(GDBusConnection *connection, const char *path);

void binc_characteristic_read(Characteristic *characteristic, OnReadCallback callback);
GByteArray *binc_characteristic_read_sync(Characteristic *characteristic, GError **error);

void binc_characteristic_write(Characteristic *characteristic, GByteArray *byteArray, WriteType writeType, OnWriteCallback callback);

void binc_characteristic_register_notifying_state_change_callback(Characteristic *characteristic,
                                                                  NotifyingStateChangedCallback callback);

void binc_characteristic_start_notify(Characteristic *characteristic, OnNotifyCallback callback);

void binc_characteristic_stop_notify(Characteristic *characteristic);

char *binc_characteristic_to_string(Characteristic *characteristic);

#endif //TEST_CHARACTERISTIC_H
