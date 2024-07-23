/*
 *   Copyright (c) 2022 Martijn van Welie
 *
 *   Permission is hereby granted, free of charge, to any person obtaining a copy
 *   of this software and associated documentation files (the "Software"), to deal
 *   in the Software without restriction, including without limitation the rights
 *   to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 *   copies of the Software, and to permit persons to whom the Software is
 *   furnished to do so, subject to the following conditions:
 *
 *   The above copyright notice and this permission notice shall be included in all
 *   copies or substantial portions of the Software.
 *
 *   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 *   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 *   OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 *   SOFTWARE.
 *
 */

#include <glib.h>
#include <stdio.h>
#include <signal.h>
#include "adapter.h"
#include "device.h"
#include "logger.h"
#include "agent.h"
#include "application.h"
#include "advertisement.h"
#include "utility.h"
#include "parser.h"
#include <stdint.h>

#define TAG "Main"

#define VEHICLE_SERVICE_UUID "00001809-0000-1000-8000-00805f9b34fb"
#define CAN_CHAR_UUID "00002a10-0000-1000-8000-00805f9b34fb"
#define GPS_CHAR_UUID "00002a11-0000-1000-8000-00805f9b34fb"
#define GPS_FREQ_CHAR_UUID "00002a12-0000-1000-8000-00805f9b34fb"
#define CAN_FREQ_CHAR_UUID "00002a13-0000-1000-8000-00805f9b34fb"
#define IMU_FREQ_CHAR_UUID "00002a14-0000-1000-8000-00805f9b34fb"
#define UNLOCK_VEHICLE_CHAR_UUID "00002a15-0000-1000-8000-00805f9b34fb"

#define AUTH_SERVICE_UUID "0000a000-0000-1000-8000-00805f9b34fb"
#define PASSWORD_CHAR_UUID "0000a001-0000-1000-8000-00805f9b34fb"
#define IS_AUTHENTICATED_CHAR_UUID "0000a002-0000-1000-8000-00805f9b34fb"

#define CUD_CHAR "00002901-0000-1000-8000-00805f9b34fb"

#define DEFAULT_PASSWORD 0x123456
#define DEFAULT_PASSWORD_LEN 3

#define BLUEZ_ERROR_AUTHORIZATION_FAILED "org.bluez.Error.Failed"

GMainLoop *loop = NULL;
Adapter *default_adapter = NULL;
Advertisement *advertisement = NULL;
Application *app = NULL;
static gboolean is_authenticated = FALSE;

void ble_install_vehicle_service()
{
    log_info(TAG, "Adding Vehicle Service\r\n");

    binc_application_add_service(app, VEHICLE_SERVICE_UUID);

    binc_application_add_characteristic(
            app,
            VEHICLE_SERVICE_UUID,
            CAN_CHAR_UUID,
            GATT_CHR_PROP_NOTIFY);
    
    binc_application_add_characteristic(
            app,
            VEHICLE_SERVICE_UUID,
            GPS_CHAR_UUID,
            GATT_CHR_PROP_NOTIFY);
    
    binc_application_add_characteristic(
            app,
            VEHICLE_SERVICE_UUID,
            GPS_FREQ_CHAR_UUID,
            GATT_CHR_PROP_WRITE);
    
    binc_application_add_characteristic(
            app,
            VEHICLE_SERVICE_UUID,
            CAN_FREQ_CHAR_UUID,
            GATT_CHR_PROP_WRITE);
    
    binc_application_add_characteristic(
            app,
            VEHICLE_SERVICE_UUID,
            IMU_FREQ_CHAR_UUID,
            GATT_CHR_PROP_WRITE);
    
    binc_application_add_characteristic(
            app,
            VEHICLE_SERVICE_UUID,
            UNLOCK_VEHICLE_CHAR_UUID,
            GATT_CHR_PROP_WRITE);

    // binc_application_add_descriptor(
    //         app,
    //         VEHICLE_SERVICE_UUID,
    //         TEMPERATURE_CHAR_UUID,
    //         CUD_CHAR,
    //         GATT_CHR_PROP_READ | GATT_CHR_PROP_WRITE);
}

void ble_install_auth_service()
{
    log_info(TAG, "Adding Auth Service\r\n");

    binc_application_add_service(app, AUTH_SERVICE_UUID);

    binc_application_add_characteristic(
            app,
            AUTH_SERVICE_UUID,
            PASSWORD_CHAR_UUID,
            GATT_CHR_PROP_WRITE);
    
    binc_application_add_characteristic(
            app,
            AUTH_SERVICE_UUID,
            IS_AUTHENTICATED_CHAR_UUID,
            GATT_CHR_PROP_READ);
}

void on_powered_state_changed(Adapter *adapter, gboolean state) {
    log_debug(TAG, "powered '%s' (%s)", state ? "on" : "off", binc_adapter_get_path(adapter));
}

void on_central_state_changed(Adapter *adapter, Device *device) {
    char *deviceToString = binc_device_to_string(device);
    log_debug(TAG, deviceToString);
    g_free(deviceToString);

    log_debug(TAG, "remote central %s is %s", binc_device_get_address(device), binc_device_get_connection_state_name(device));
    ConnectionState state = binc_device_get_connection_state(device);
    if (state == BINC_CONNECTED) {
        binc_adapter_stop_advertising(adapter, advertisement);
    } else if (state == BINC_DISCONNECTED){
        binc_adapter_start_advertising(adapter, advertisement);
    }
}

const char *on_local_char_read(const Application *application, const char *address, const char *service_uuid,
                        const char *char_uuid) {

    log_debug(TAG, "on char read");

    if (g_str_equal(service_uuid, AUTH_SERVICE_UUID) && g_str_equal(char_uuid, IS_AUTHENTICATED_CHAR_UUID)) {
        const char *value = is_authenticated ? "yes" : "no";
        GByteArray *byteArray = g_byte_array_new();
        g_byte_array_append(byteArray, (const guint8 *)value, strlen(value));
        binc_application_set_char_value(application, service_uuid, char_uuid, byteArray);
        return NULL;
    }

    return BLUEZ_ERROR_REJECTED;
}


#define DEFAULT_PASSWORD 0x123456
#define DEFAULT_PASSWORD_LEN 3

const char *on_local_char_write(const Application *application, const char *address, const char *service_uuid,
                                const char *char_uuid, GByteArray *byteArray) {

    log_debug(TAG, "on char write");

    if (g_str_equal(service_uuid, AUTH_SERVICE_UUID) && g_str_equal(char_uuid, PASSWORD_CHAR_UUID)) {
        log_debug(TAG, "Password write received, length: %d", byteArray->len);
        
        if (byteArray->len != DEFAULT_PASSWORD_LEN) {
            log_error(TAG, "Invalid password length: %d (expected %d)", byteArray->len, DEFAULT_PASSWORD_LEN);
            return BLUEZ_ERROR_INVALID_VALUE_LENGTH;
        }

        // Manually construct the received password from the byte array
        uint32_t received_password = 0;
        received_password |= byteArray->data[0];
        received_password <<= 8;
        received_password |= byteArray->data[1];
        received_password <<= 8;
        received_password |= byteArray->data[2];

        log_debug(TAG, "Received password: 0x%06x", received_password);

        if (received_password == DEFAULT_PASSWORD) {
            is_authenticated = TRUE;

            // Write "yes" to IS_AUTHENTICATED_CHAR_UUID
            const uint8_t yes_value[] = {'y', 'e', 's'};
            GByteArray *yesArray = g_byte_array_new();
            g_byte_array_append(yesArray, yes_value, sizeof(yes_value));
            binc_application_set_char_value(application, AUTH_SERVICE_UUID, IS_AUTHENTICATED_CHAR_UUID, yesArray);
            g_byte_array_free(yesArray, TRUE);

            log_info(TAG, "Authentication successful, 'yes' written to IS_AUTHENTICATED_CHAR_UUID");

            // Make VEHICLE_SERVICE_UUID visible
            ble_install_vehicle_service();
        } else {
            log_error(TAG, "Authentication failed, received password: 0x%06x", received_password);
            return BLUEZ_ERROR_AUTHORIZATION_FAILED;
        }
    }

    return NULL;
}


void on_local_char_start_notify(const Application *application, const char *service_uuid, const char *char_uuid) {
    log_debug(TAG, "on start notify");
}

void on_local_char_stop_notify(const Application *application, const char *service_uuid, const char *char_uuid) {
    log_debug(TAG, "on stop notify");
}

gboolean callback(gpointer data) {
    if (app != NULL) {
        binc_adapter_unregister_application(default_adapter, app);
        binc_application_free(app);
        app = NULL;
    }

    if (advertisement != NULL) {
        binc_adapter_stop_advertising(default_adapter, advertisement);
        binc_advertisement_free(advertisement);
    }

    if (default_adapter != NULL) {
        binc_adapter_free(default_adapter);
        default_adapter = NULL;
    }

    g_main_loop_quit((GMainLoop *) data);
    return FALSE;
}

static void cleanup_handler(int signo) {
    if (signo == SIGINT) {
        log_error(TAG, "received SIGINT");
        callback(loop);
    }
}


int main(void) {

    log_set_level(LOG_DEBUG);

    // Get a DBus connection
    GDBusConnection *dbusConnection = g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL, NULL);

    // Setup handler for CTRL+C
    if (signal(SIGINT, cleanup_handler) == SIG_ERR)
        log_error(TAG, "can't catch SIGINT");

    // Setup mainloop
    loop = g_main_loop_new(NULL, FALSE);

    // Get the default adapter
    default_adapter = binc_adapter_get_default(dbusConnection);

    if (default_adapter != NULL) {
        log_debug(TAG, "using adapter '%s'", binc_adapter_get_path(default_adapter));

        // Make sure the adapter is on
        binc_adapter_set_powered_state_cb(default_adapter, &on_powered_state_changed);
        if (!binc_adapter_get_powered_state(default_adapter)) {
            binc_adapter_power_on(default_adapter);
        }

        // Setup remote central connection state callback
        binc_adapter_set_remote_central_cb(default_adapter, &on_central_state_changed);

        // Setup advertisement
        GPtrArray *adv_service_uuids = g_ptr_array_new();
        g_ptr_array_add(adv_service_uuids, VEHICLE_SERVICE_UUID);
        g_ptr_array_add(adv_service_uuids, AUTH_SERVICE_UUID);

        advertisement = binc_advertisement_create();
        binc_advertisement_set_local_name(advertisement, "iWave-BLE");
        binc_advertisement_set_services(advertisement, adv_service_uuids);
        g_ptr_array_free(adv_service_uuids, TRUE);
        binc_adapter_start_advertising(default_adapter, advertisement);

        // Start application
        app = binc_create_application(default_adapter);

        // Install services
        ble_install_auth_service();

        binc_application_set_char_read_cb(app, &on_local_char_read);
        binc_application_set_char_write_cb(app, &on_local_char_write);
        binc_application_set_char_start_notify_cb(app, &on_local_char_start_notify);
        binc_application_set_char_stop_notify_cb(app, &on_local_char_stop_notify);
        binc_adapter_register_application(default_adapter, app);
    } else {
        log_debug("MAIN", "No adapter found");
    }

    // Bail out after some time
    g_timeout_add_seconds(600, callback, loop);

    // Start the mainloop
    g_main_loop_run(loop);

    // Clean up mainloop
    g_main_loop_unref(loop);

    // Disconnect from DBus
    g_dbus_connection_close_sync(dbusConnection, NULL, NULL);
    g_object_unref(dbusConnection);
    return 0;
}
