/* SPDX-License-Identifier: MIT */

#include <errno.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/clock_control/nrf_clock_control.h>
#include <zephyr/sys/onoff.h>
#include <zephyr/sys/notify.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(bobtail_hfxo_hold, CONFIG_ZMK_LOG_LEVEL);

/* A separate, permanent reference: USB disconnect must not release this client. */
static struct onoff_client hold_client;

static void hold_ready(struct onoff_manager *manager, struct onoff_client *client,
                       uint32_t state, int result) {
    ARG_UNUSED(manager);
    ARG_UNUSED(client);
    ARG_UNUSED(state);
    if (result < 0) {
        LOG_ERR("HFXO hold failed: %d", result);
    } else {
        LOG_INF("Battery diagnostic HFXO hold active");
    }
}

static int hold_hfxo_init(void) {
    if (!device_is_ready(DEVICE_DT_GET(DT_NODELABEL(clock)))) {
        return -ENODEV;
    }
    struct onoff_manager *manager =
        z_nrf_clock_control_get_onoff(CLOCK_CONTROL_NRF_SUBSYS_HF);
    if (!manager) {
        return -ENODEV;
    }

    sys_notify_init_callback(&hold_client.notify, hold_ready);
    int result = onoff_request(manager, &hold_client);
    if (result < 0) {
        LOG_ERR("HFXO request failed: %d", result);
        return result;
    }
    /* Asynchronous: do not spin or block application/Bluetooth startup. */
    return 0;
}

SYS_INIT(hold_hfxo_init, POST_KERNEL, CONFIG_APPLICATION_INIT_PRIORITY);
