/* SPDX-License-Identifier: MIT */
#include <stdio.h>
#include <string.h>
#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <zephyr/settings/settings.h>
#include <zephyr/sys/atomic.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/logging/log.h>
#include <zmk/ble.h>
#include <zmk/event_manager.h>
#include <zmk/events/ble_active_profile_changed.h>

LOG_MODULE_REGISTER(bobtail_profile_name, CONFIG_ZMK_LOG_LEVEL);

BUILD_ASSERT(CONFIG_APPLICATION_INIT_PRIORITY > CONFIG_ZMK_BLE_INIT_PRIORITY,
             "Register the name handler after ZMK's BLE settings handler");
BUILD_ASSERT(CONFIG_BT_DEVICE_NAME_MAX >= sizeof("BobTail255") - 1,
             "Bluetooth name buffer must include the profile suffix");

static atomic_t settings_ready;
static int applied_profile = -1;

static void update_profile_name(struct k_work *work);
K_WORK_DEFINE(profile_name_work, update_profile_name);

static void queue_profile_name(void) {
    int result = k_work_submit(&profile_name_work);
    if (result < 0) {
        LOG_ERR("Could not queue Bluetooth name update: %d", result);
    }
}

static void update_profile_name(struct k_work *work) {
    ARG_UNUSED(work);
    if (!atomic_get(&settings_ready)) {
        return;
    }
    int profile = zmk_ble_active_profile_index();
    if (profile < 0 || profile >= ZMK_BLE_PROFILE_COUNT) {
        LOG_ERR("Invalid Bluetooth profile: %d", profile);
        return;
    }
    char name[CONFIG_BT_DEVICE_NAME_MAX + 1];
    int length = snprintf(name, sizeof(name), "BobTail%d", profile);
    if (length < 0 || (size_t)length >= sizeof(name)) {
        return;
    }
    if (applied_profile == profile && strcmp(bt_get_name(), name) == 0) {
        return;
    }
    /* This fork's API updates GAP and restarts its own name-bearing advertising.
     * It does not change the BLE identity, host bonds, or the split connection.
     */
    int result = zmk_ble_set_device_name(name);
    if (result < 0) {
        LOG_ERR("Bluetooth profile name update failed: %d", result);
        return;
    }
    applied_profile = profile;
    /* A profile switch can occur while this work item is running. */
    if (zmk_ble_active_profile_index() != profile) {
        queue_profile_name();
    }
}

static int profile_name_listener(const zmk_event_t *event) {
    ARG_UNUSED(event);
    if (atomic_get(&settings_ready)) {
        queue_profile_name();
    }
    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(bobtail_profile_name, profile_name_listener);
ZMK_SUBSCRIPTION(bobtail_profile_name, zmk_ble_active_profile_changed);

static int profile_name_settings_commit(void) {
    /* Dynamic handlers commit in registration order in the pinned Zephyr 3.5.
     * ZMK has already restored the selected slot and started BLE advertising.
     * Use work so settings persistence in bt_set_name runs outside this callback.
     */
    atomic_set(&settings_ready, 1);
    queue_profile_name();
    return 0;
}

static struct settings_handler profile_name_settings = {
    .name = "bobtail_profile_name",
    .h_commit = profile_name_settings_commit,
};

static int profile_name_init(void) {
    return settings_register(&profile_name_settings);
}

SYS_INIT(profile_name_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
