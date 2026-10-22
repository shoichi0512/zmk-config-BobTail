/* SPDX-License-Identifier: MIT */

#include <errno.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/clock_control/nrf_clock_control.h>
#include <zephyr/sys/onoff.h>
#include <zephyr/sys/notify.h>
#include <zephyr/sys/atomic.h>
#include <zephyr/logging/log.h>
#include <bobtail/clock_diagnostic.h>

LOG_MODULE_REGISTER(bobtail_hfxo_hold, CONFIG_ZMK_LOG_LEVEL);

/* This client owns only its own reference, independently of USB and the radio. */
static struct onoff_client hold_client;
static struct onoff_manager *hold_manager;
static atomic_t requested = ATOMIC_INIT(0);
static atomic_t hold_state = ATOMIC_INIT(BOBTAIL_HFXO_OFF);
K_MUTEX_DEFINE(clock_request_lock);

static void hold_ready(struct onoff_manager *manager, struct onoff_client *client,
                       uint32_t state, int result) {
    ARG_UNUSED(manager);
    ARG_UNUSED(client);
    ARG_UNUSED(state);
    if (!atomic_get(&requested)) {
        return;
    }
    if (result < 0) {
        atomic_set(&requested, 0);
        atomic_set(&hold_state, BOBTAIL_HFXO_ERROR);
        LOG_ERR("HFXO hold failed: %d", result);
    } else {
        atomic_set(&hold_state, BOBTAIL_HFXO_ACTIVE);
        LOG_INF("Battery diagnostic HFXO hold active");
    }
}

unsigned int bobtail_hfxo_status(void) {
    return (unsigned int)atomic_get(&hold_state);
}

/* Serialize only our own client. The clock manager handles USB/radio references. */
int bobtail_hfxo_toggle(void) {
    if (!hold_manager) {
        atomic_set(&hold_state, BOBTAIL_HFXO_ERROR);
        return -ENODEV;
    }
    if (k_mutex_lock(&clock_request_lock, K_NO_WAIT) != 0) {
        return -EBUSY;
    }

    int result;
    if (atomic_get(&requested)) {
        atomic_set(&requested, 0);
        result = onoff_cancel_or_release(hold_manager, &hold_client);
        if (result >= 0) {
            atomic_set(&hold_state, BOBTAIL_HFXO_OFF);
        } else {
            atomic_set(&requested, 1);
            atomic_set(&hold_state, BOBTAIL_HFXO_ERROR);
        }
    } else {
        atomic_set(&requested, 1);
        atomic_set(&hold_state, BOBTAIL_HFXO_PENDING);
        sys_notify_init_callback(&hold_client.notify, hold_ready);
        result = onoff_request(hold_manager, &hold_client);
        if (result < 0) {
            atomic_set(&requested, 0);
            atomic_set(&hold_state, BOBTAIL_HFXO_ERROR);
        }
    }
    k_mutex_unlock(&clock_request_lock);
    return result < 0 ? result : 0;
}

static int hold_hfxo_init(void) {
    if (!device_is_ready(DEVICE_DT_GET(DT_NODELABEL(clock)))) {
        atomic_set(&hold_state, BOBTAIL_HFXO_ERROR);
        return -ENODEV;
    }
    hold_manager = z_nrf_clock_control_get_onoff(CLOCK_CONTROL_NRF_SUBSYS_HF);
    if (!hold_manager) {
        atomic_set(&hold_state, BOBTAIL_HFXO_ERROR);
        return -ENODEV;
    }

    if (IS_ENABLED(CONFIG_BOBTAIL_KEEP_HFXO_ON)) {
        return bobtail_hfxo_toggle();
    }
    /* Default: no extra clock request. On-demand requests are asynchronous. */
    return 0;
}

SYS_INIT(hold_hfxo_init, POST_KERNEL, CONFIG_APPLICATION_INIT_PRIORITY);
