/* SPDX-License-Identifier: MIT */
#define DT_DRV_COMPAT bobtail_behavior_scroll_speed

#include <errno.h>
#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/settings/settings.h>
#include <zephyr/sys/atomic.h>
#include <drivers/behavior.h>
#include <zmk/behavior.h>
#include <bobtail/scroll_speed.h>

#if IS_ENABLED(CONFIG_PMW3610)

static atomic_t scroll_level = ATOMIC_INIT(BOBTAIL_SCROLL_DEFAULT_LEVEL);

int bobtail_scroll_tick_get(void) {
    return bobtail_scroll_level_tick((uint8_t)atomic_get(&scroll_level));
}

#if IS_ENABLED(CONFIG_SETTINGS)
static void save_scroll_level(struct k_work *work) {
    uint8_t level = (uint8_t)atomic_get(&scroll_level);
    settings_save_one("bobtail_scroll/level", &level, sizeof(level));
}

/* Save after the last adjustment, not on every press. */
K_WORK_DELAYABLE_DEFINE(save_scroll_work, save_scroll_level);

static int load_scroll_level(const char *name, size_t len, settings_read_cb read_cb, void *cb_arg) {
    if (strcmp(name, "level") != 0) {
        return -ENOENT;
    }
    if (len != sizeof(uint8_t)) {
        return -EINVAL;
    }
    uint8_t level;
    int rc = read_cb(cb_arg, &level, sizeof(level));
    if (rc < 0) {
        return rc;
    }
    if (rc != sizeof(level) || level >= BOBTAIL_SCROLL_LEVEL_COUNT) {
        return -EINVAL;
    }
    atomic_set(&scroll_level, level);
    return 0;
}

SETTINGS_STATIC_HANDLER_DEFINE(bobtail_scroll, "bobtail_scroll", NULL, load_scroll_level, NULL, NULL);
#endif

#endif /* CONFIG_PMW3610 */

#if DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT)

static int change_scroll_speed(struct zmk_behavior_binding *binding,
                               struct zmk_behavior_binding_event event) {
#if IS_ENABLED(CONFIG_PMW3610)
    int old_level, new_level;
    if (binding->param1 > 1) {
        return -EINVAL;
    }
    do {
        old_level = atomic_get(&scroll_level);
        new_level = CLAMP(old_level + (binding->param1 ? 1 : -1),
                          0, BOBTAIL_SCROLL_LEVEL_COUNT - 1);
    } while (!atomic_cas(&scroll_level, old_level, new_level));

#if IS_ENABLED(CONFIG_SETTINGS)
    if (new_level != old_level) {
        k_work_reschedule(&save_scroll_work, K_MSEC(1500));
    }
#endif
    return ZMK_BEHAVIOR_OPAQUE;
#else
    /* Split behavior dispatch runs on the right central, which owns the sensor. */
    return -ENOTSUP;
#endif
}

static int release_scroll_speed(struct zmk_behavior_binding *binding,
                                struct zmk_behavior_binding_event event) {
    return ZMK_BEHAVIOR_OPAQUE;
}

static const struct behavior_driver_api scroll_speed_api = {
    .binding_pressed = change_scroll_speed,
    .binding_released = release_scroll_speed,
};

BEHAVIOR_DT_INST_DEFINE(0, NULL, NULL, NULL, NULL, POST_KERNEL,
                        CONFIG_KERNEL_INIT_PRIORITY_DEFAULT, &scroll_speed_api);

#endif
