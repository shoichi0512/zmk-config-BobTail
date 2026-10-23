/* SPDX-License-Identifier: MIT */
#include <string.h>
#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zmk/ble.h>
#include <bobtail/pairing_diagnostics.h>

/* Observe host connections only. Never change authentication or erase a bond. */
static struct k_spinlock status_lock;
static struct bobtail_pairing_status statuses[ZMK_BLE_PROFILE_COUNT];
/* Zero means unassigned; otherwise this is the slot at connection establishment + 1. */
static uint8_t connection_slots[CONFIG_BT_MAX_CONN];

static int host_connection_index(struct bt_conn *conn) {
    struct bt_conn_info info;
    if (bt_conn_get_info(conn, &info) < 0 || info.type != BT_CONN_TYPE_LE ||
        info.role != BT_CONN_ROLE_PERIPHERAL) {
        return -1;
    }
    uint8_t index = bt_conn_index(conn);
    return index < CONFIG_BT_MAX_CONN ? (int)index : -1;
}

static void host_connected(struct bt_conn *conn, uint8_t error) {
    int index = host_connection_index(conn);
    if (index < 0) {
        return;
    }
    int profile = zmk_ble_profile_index(bt_conn_get_dst(conn));
    if (profile < 0) {
        profile = zmk_ble_active_profile_index();
    }
    if (profile < 0 || profile >= ZMK_BLE_PROFILE_COUNT) {
        return;
    }
    k_spinlock_key_t key = k_spin_lock(&status_lock);
    struct bobtail_pairing_status *status = &statuses[profile];
    unsigned int attempts = status->attempts + 1;
    memset(status, 0, sizeof(*status));
    status->attempts = attempts;
    status->stage = error ? 1 : 2;
    status->connect_error = error;
    connection_slots[index] = error ? 0 : (uint8_t)(profile + 1);
    k_spin_unlock(&status_lock, key);
}

enum result_kind { SECURITY_RESULT, PAIRING_SUCCESS, PAIRING_FAILURE, DISCONNECT_RESULT };

static void record_result(struct bt_conn *conn, enum result_kind kind, unsigned int result) {
    int index = host_connection_index(conn);
    if (index < 0) {
        return;
    }
    k_spinlock_key_t key = k_spin_lock(&status_lock);
    int profile = (int)connection_slots[index] - 1;
    if (profile >= 0 && profile < ZMK_BLE_PROFILE_COUNT) {
        struct bobtail_pairing_status *status = &statuses[profile];
        switch (kind) {
        case SECURITY_RESULT:
            status->security_error = result;
            status->stage = result ? 4 : 3;
            break;
        case PAIRING_SUCCESS:
            status->stage = 5;
            status->pairing_error = 0;
            break;
        case PAIRING_FAILURE:
            status->stage = 6;
            status->pairing_error = result;
            break;
        case DISCONNECT_RESULT:
            status->stage = 7;
            status->disconnect_reason = result;
            connection_slots[index] = 0;
            break;
        }
    }
    k_spin_unlock(&status_lock, key);
}

static void host_security_changed(struct bt_conn *conn, bt_security_t level,
                                  enum bt_security_err error) {
    ARG_UNUSED(level);
    record_result(conn, SECURITY_RESULT, error);
}

static void host_pairing_complete(struct bt_conn *conn, bool bonded) {
    ARG_UNUSED(bonded);
    record_result(conn, PAIRING_SUCCESS, 0);
}

static void host_pairing_failed(struct bt_conn *conn, enum bt_security_err error) {
    record_result(conn, PAIRING_FAILURE, error);
}

static void host_disconnected(struct bt_conn *conn, uint8_t reason) {
    record_result(conn, DISCONNECT_RESULT, reason);
}

BT_CONN_CB_DEFINE(bobtail_pairing_connections) = {
    .connected = host_connected,
    .security_changed = host_security_changed,
    .disconnected = host_disconnected,
};

static struct bt_conn_auth_info_cb pairing_callbacks = {
    .pairing_complete = host_pairing_complete,
    .pairing_failed = host_pairing_failed,
};

static int pairing_diagnostics_init(void) {
    return bt_conn_auth_info_cb_register(&pairing_callbacks);
}

SYS_INIT(pairing_diagnostics_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);

static void count_bond(const struct bt_bond_info *info, void *user_data) {
    ARG_UNUSED(info);
    (*(unsigned int *)user_data)++;
}

void bobtail_pairing_snapshot(struct bobtail_pairing_status *status) {
    int profile = zmk_ble_active_profile_index();
    k_spinlock_key_t key = k_spin_lock(&status_lock);
    if (profile >= 0 && profile < ZMK_BLE_PROFILE_COUNT) {
        *status = statuses[profile];
    } else {
        memset(status, 0, sizeof(*status));
    }
    k_spin_unlock(&status_lock, key);
    status->bonds = 0;
    bt_foreach_bond(BT_ID_DEFAULT, count_bond, &status->bonds);
}
