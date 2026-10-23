/* SPDX-License-Identifier: MIT */
#include <errno.h>
#include <string.h>
#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zmk/ble.h>
#include <bobtail/pairing_diagnostics.h>

/* Callbacks only observe host connections. Bond removal requires the repair key. */
static struct k_spinlock status_lock;
static struct bobtail_pairing_status statuses[ZMK_BLE_PROFILE_COUNT];
static bt_addr_le_t observed_peers[ZMK_BLE_PROFILE_COUNT];
static bool peer_valid[ZMK_BLE_PROFILE_COUNT];
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
    observed_peers[profile] = *bt_conn_get_dst(conn);
    peer_valid[profile] = !error;
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

struct bond_search {
    bt_addr_le_t peer;
    bool valid;
    bool found;
    unsigned int count;
};

static void inspect_bond(const struct bt_bond_info *info, void *user_data) {
    struct bond_search *search = user_data;
    search->count++;
    if (search->valid && bt_addr_le_cmp(&info->addr, &search->peer) == 0) {
        search->found = true;
    }
}

void bobtail_pairing_snapshot(struct bobtail_pairing_status *status) {
    int profile = zmk_ble_active_profile_index();
    struct bond_search search = {0};
    k_spinlock_key_t key = k_spin_lock(&status_lock);
    if (profile >= 0 && profile < ZMK_BLE_PROFILE_COUNT) {
        *status = statuses[profile];
        search.peer = observed_peers[profile];
        search.valid = peer_valid[profile];
    } else {
        memset(status, 0, sizeof(*status));
    }
    k_spin_unlock(&status_lock, key);
    bt_foreach_bond(BT_ID_DEFAULT, inspect_bond, &search);
    status->bonds = search.count;
    status->peer_bond = search.found ? 1U : 0U;
    int peer_profile = search.valid ? zmk_ble_profile_index(&search.peer) : -1;
    status->peer_profile = peer_profile >= 0 ? (unsigned int)peer_profile + 1U : 0U;
}

int bobtail_pairing_clear_unmapped_failed_bond(int profile) {
    if (profile < 0 || profile >= ZMK_BLE_PROFILE_COUNT ||
        zmk_ble_active_profile_index() != profile || !zmk_ble_profile_is_open(profile)) {
        return -EBUSY;
    }
    struct bond_search search = {0};
    k_spinlock_key_t key = k_spin_lock(&status_lock);
    const struct bobtail_pairing_status *status = &statuses[profile];
    bool failed = status->security_error == BT_SECURITY_ERR_AUTH_REQUIREMENT ||
                  status->pairing_error == BT_SECURITY_ERR_AUTH_REQUIREMENT;
    if (peer_valid[profile] && failed && (status->stage == 6 || status->stage == 7)) {
        search.peer = observed_peers[profile];
        search.valid = true;
    }
    k_spin_unlock(&status_lock, key);
    if (!search.valid) {
        return 0;
    }
    /* Do not invalidate a host still assigned to any of the other profile slots. */
    if (zmk_ble_profile_index(&search.peer) >= 0) {
        return -EBUSY;
    }
    bt_foreach_bond(BT_ID_DEFAULT, inspect_bond, &search);
    if (!search.found) {
        return 0;
    }
    struct bt_conn *connection = bt_conn_lookup_addr_le(BT_ID_DEFAULT, &search.peer);
    if (connection) {
        bt_conn_unref(connection);
        return -EBUSY;
    }
    if (zmk_ble_active_profile_index() != profile || !zmk_ble_profile_is_open(profile) ||
        zmk_ble_profile_index(&search.peer) >= 0) {
        return -EAGAIN;
    }
    int result = bt_unpair(BT_ID_DEFAULT, &search.peer);
    if (result < 0) {
        return result;
    }
    search.found = false;
    bt_foreach_bond(BT_ID_DEFAULT, inspect_bond, &search);
    return search.found ? -EIO : 0;
}
