/* SPDX-License-Identifier: MIT */
#include <errno.h>
#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <zephyr/settings/settings.h>
#include <zephyr/sys/atomic.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/logging/log.h>
#include <zmk/ble.h>
#include <zmk/event_manager.h>
#include <zmk/events/ble_active_profile_changed.h>

LOG_MODULE_REGISTER(bobtail_profile_connection, CONFIG_ZMK_LOG_LEVEL);

BUILD_ASSERT(CONFIG_APPLICATION_INIT_PRIORITY > CONFIG_ZMK_BLE_INIT_PRIORITY,
             "Restore profiles before applying the host connection policy");

static atomic_t profiles_ready;
static void enforce_profile_connections(struct k_work *work);
K_WORK_DEFINE(profile_connection_work, enforce_profile_connections);

static void queue_connection_policy(void) {
    if (!atomic_get(&profiles_ready)) {
        return;
    }
    int result = k_work_submit(&profile_connection_work);
    if (result < 0) {
        LOG_ERR("Could not queue profile connection update: %d", result);
    }
}

static bool keep_host(const bt_addr_le_t *peer) {
    int active = zmk_ble_active_profile_index();
    if (active < 0 || active >= ZMK_BLE_PROFILE_COUNT) {
        /* Do not disconnect anything while the profile state is invalid. */
        return true;
    }
    if (bt_addr_le_cmp(peer, zmk_ble_profile_address(active)) == 0) {
        /* This also preserves the host if duplicate slots contain its address. */
        return true;
    }
    /* An empty selected slot must still accept a new, unassigned host for pairing. */
    return zmk_ble_profile_is_open(active) && zmk_ble_profile_index(peer) < 0;
}

static void disconnect_inactive_host(struct bt_conn *conn, void *user_data) {
    ARG_UNUSED(user_data);
    struct bt_conn_info info;
    if (bt_conn_get_info(conn, &info) < 0 || info.type != BT_CONN_TYPE_LE ||
        info.role != BT_CONN_ROLE_PERIPHERAL || info.state != BT_CONN_STATE_CONNECTED) {
        return;
    }
    bt_addr_le_t peer = *bt_conn_get_dst(conn);
    /* Use the current selection rather than a slot captured when this work was
     * queued, so coalesced rapid switches follow the latest selection.
     */
    if (keep_host(&peer)) {
        return;
    }
    int result = bt_conn_disconnect(conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
    if (result < 0 && result != -ENOTCONN && result != -EALREADY) {
        LOG_WRN("Inactive host disconnect failed: %d", result);
    }
}

static void enforce_profile_connections(struct k_work *work) {
    ARG_UNUSED(work);
    if (atomic_get(&profiles_ready)) {
        /* Zephyr holds a connection reference for each callback. */
        bt_conn_foreach(BT_CONN_TYPE_LE, disconnect_inactive_host, NULL);
    }
}

static int profile_connection_listener(const zmk_event_t *event) {
    ARG_UNUSED(event);
    queue_connection_policy();
    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(bobtail_profile_connection, profile_connection_listener);
ZMK_SUBSCRIPTION(bobtail_profile_connection, zmk_ble_active_profile_changed);

static void host_connected(struct bt_conn *conn, uint8_t error) {
    struct bt_conn_info info;
    if (!error && bt_conn_get_info(conn, &info) == 0 && info.type == BT_CONN_TYPE_LE &&
        info.role == BT_CONN_ROLE_PERIPHERAL) {
        /* Also close an inactive host that automatically reconnects later. */
        queue_connection_policy();
    }
}

BT_CONN_CB_DEFINE(bobtail_profile_connection) = {
    .connected = host_connected,
};

static int connection_policy_settings_commit(void) {
    atomic_set(&profiles_ready, 1);
    queue_connection_policy();
    return 0;
}

static struct settings_handler connection_policy_settings = {
    .name = "bobtail_profile_connection",
    .h_commit = connection_policy_settings_commit,
};

static int profile_connection_init(void) {
    return settings_register(&connection_policy_settings);
}

SYS_INIT(profile_connection_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
