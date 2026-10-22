/* SPDX-License-Identifier: MIT */
#define DT_DRV_COMPAT bobtail_behavior_device_diagnostics

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/atomic.h>
#include <drivers/behavior.h>
#include <zmk/behavior.h>

#if DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT)

#if IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)

#include <dt-bindings/zmk/keys.h>
#include <zmk/events/keycode_state_changed.h>

#if IS_ENABLED(CONFIG_ZMK_BLE)
#include <zephyr/bluetooth/conn.h>
#endif

#if IS_ENABLED(CONFIG_HWINFO)
#include <zephyr/drivers/hwinfo.h>
#endif

#if IS_ENABLED(CONFIG_BOBTAIL_CLOCK_DIAGNOSTIC)
#include <bobtail/clock_diagnostic.h>
#endif

enum diagnostic_phase {
    DIAGNOSTIC_START,
    DIAGNOSTIC_CAPTURE,
    DIAGNOSTIC_EMIT,
};

static atomic_t diagnostic_busy;
static enum diagnostic_phase phase;
static uint32_t command;
static uint32_t boot_reset_cause;
static char diagnostic_text[128];
static size_t text_length;
static size_t text_position;
static uint32_t pressed_key;

struct diagnostic_connection_state {
    uint32_t host_connections;
    uint32_t host_disconnections;
    uint32_t host_last_reason;
    uint32_t host_last_disconnect_seconds;
    uint32_t split_disconnections;
};

static struct diagnostic_connection_state connection_state;
static struct k_spinlock connection_lock;

#if IS_ENABLED(CONFIG_ZMK_BLE)
static void diagnostic_connected(struct bt_conn *conn, uint8_t error) {
    struct bt_conn_info info;
    if (error != 0 || bt_conn_get_info(conn, &info) != 0 ||
        info.type != BT_CONN_TYPE_LE || info.role != BT_CONN_ROLE_PERIPHERAL) {
        return;
    }
    /* PC links use the peripheral role; the right half is central to the left. */
    k_spinlock_key_t key = k_spin_lock(&connection_lock);
    connection_state.host_connections++;
    k_spin_unlock(&connection_lock, key);
}

static void diagnostic_disconnected(struct bt_conn *conn, uint8_t reason) {
    struct bt_conn_info info;
    if (bt_conn_get_info(conn, &info) != 0 || info.type != BT_CONN_TYPE_LE) {
        return;
    }
    uint32_t seconds = (uint32_t)(k_uptime_get() / 1000);
    k_spinlock_key_t key = k_spin_lock(&connection_lock);
    if (info.role == BT_CONN_ROLE_PERIPHERAL) {
        connection_state.host_disconnections++;
        connection_state.host_last_reason = reason;
        connection_state.host_last_disconnect_seconds = seconds;
    } else if (info.role == BT_CONN_ROLE_CENTRAL) {
        connection_state.split_disconnections++;
    }
    k_spin_unlock(&connection_lock, key);
}

/* Observe existing events only: no polling, new GATT requests, or flash writes. */
BT_CONN_CB_DEFINE(bobtail_diagnostics) = {
    .connected = diagnostic_connected,
    .disconnected = diagnostic_disconnected,
};
#endif

static struct diagnostic_connection_state connection_snapshot(void) {
    k_spinlock_key_t key = k_spin_lock(&connection_lock);
    struct diagnostic_connection_state snapshot = connection_state;
    k_spin_unlock(&connection_lock, key);
    return snapshot;
}

static void diagnostic_work_handler(struct k_work *work);
K_WORK_DELAYABLE_DEFINE(diagnostic_work, diagnostic_work_handler);

static int capture_boot_reset_cause(void) {
#if IS_ENABLED(CONFIG_HWINFO)
    if (hwinfo_get_reset_cause(&boot_reset_cause) < 0) {
        boot_reset_cause = UINT32_MAX;
    }
#else
    boot_reset_cause = UINT32_MAX;
#endif
    /* Preserve reset flags for other diagnostics and the bootloader. */
    return 0;
}

SYS_INIT(capture_boot_reset_cause, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);

static unsigned int clock_status(void) {
#if IS_ENABLED(CONFIG_BOBTAIL_CLOCK_DIAGNOSTIC)
    return bobtail_hfxo_status();
#else
    return 0;
#endif
}

static uint32_t diagnostic_keycode(char c) {
    if (c >= '1' && c <= '9') {
        return N1 + (uint32_t)(c - '1');
    }
    switch (c) {
    case '0': return N0;
    case 'u': return U;
    case 'r': return R;
    case 'h': return H;
    case 'c': return C;
    case 'd': return D;
    case 'e': return E;
    case 't': return T;
    case 's': return S;
    case ' ': return SPACE;
    case '\n': return ENTER;
    default: return 0;
    }
}

static void schedule_diagnostic(uint32_t delay_ms) {
    if (k_work_reschedule(&diagnostic_work, K_MSEC(delay_ms)) < 0) {
        /* A scheduled press must never be left without its matching release. */
        if (pressed_key != 0) {
            raise_zmk_keycode_state_changed_from_encoded(pressed_key, false, k_uptime_get());
            pressed_key = 0;
        }
        atomic_clear(&diagnostic_busy);
    }
}

static void diagnostic_work_handler(struct k_work *work) {
    ARG_UNUSED(work);

    if (phase == DIAGNOSTIC_START) {
        phase = DIAGNOSTIC_CAPTURE;
#if IS_ENABLED(CONFIG_BOBTAIL_CLOCK_DIAGNOSTIC)
        if (command == 1) {
            bobtail_hfxo_toggle();
            /* Let the asynchronous clock request finish without blocking input. */
            schedule_diagnostic(100);
            return;
        }
#endif
    }

    if (phase == DIAGNOSTIC_CAPTURE) {
        struct diagnostic_connection_state snapshot = connection_snapshot();
        int length = snprintf(diagnostic_text, sizeof(diagnostic_text),
                              "u %u r %u h %u c %u d %u e %u t %u s %u\n",
                              (unsigned int)(uint32_t)(k_uptime_get() / 1000),
                              (unsigned int)boot_reset_cause, clock_status(),
                              (unsigned int)snapshot.host_connections,
                              (unsigned int)snapshot.host_disconnections,
                              (unsigned int)snapshot.host_last_reason,
                              (unsigned int)snapshot.host_last_disconnect_seconds,
                              (unsigned int)snapshot.split_disconnections);
        if (length < 0 || (size_t)length >= sizeof(diagnostic_text)) {
            atomic_clear(&diagnostic_busy);
            return;
        }
        text_length = (size_t)length;
        text_position = 0;
        phase = DIAGNOSTIC_EMIT;
    }

    if (pressed_key != 0) {
        /* Release even if sending the preceding press returned an error. */
        raise_zmk_keycode_state_changed_from_encoded(pressed_key, false, k_uptime_get());
        pressed_key = 0;
        text_position++;
    } else if (text_position < text_length) {
        pressed_key = diagnostic_keycode(diagnostic_text[text_position]);
        if (pressed_key != 0) {
            raise_zmk_keycode_state_changed_from_encoded(pressed_key, true, k_uptime_get());
        } else {
            text_position++;
        }
    }

    if (text_position < text_length || pressed_key != 0) {
        schedule_diagnostic(10);
    } else {
        /* No timer or periodic task remains after this explicit request. */
        atomic_clear(&diagnostic_busy);
    }
}

#endif /* CONFIG_ZMK_SPLIT_ROLE_CENTRAL */

static int diagnostic_pressed(struct zmk_behavior_binding *binding,
                               struct zmk_behavior_binding_event event) {
    ARG_UNUSED(event);
#if IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
    if (binding->param1 > 1) {
        return -EINVAL;
    }
    if (!atomic_cas(&diagnostic_busy, 0, 1)) {
        return ZMK_BEHAVIOR_OPAQUE;
    }
    command = binding->param1;
    phase = DIAGNOSTIC_START;
    pressed_key = 0;
    schedule_diagnostic(250);
    return ZMK_BEHAVIOR_OPAQUE;
#else
    ARG_UNUSED(binding);
    return -ENOTSUP;
#endif
}

static int diagnostic_released(struct zmk_behavior_binding *binding,
                                struct zmk_behavior_binding_event event) {
    ARG_UNUSED(binding);
    ARG_UNUSED(event);
    return ZMK_BEHAVIOR_OPAQUE;
}

static const struct behavior_driver_api diagnostic_api = {
    .binding_pressed = diagnostic_pressed,
    .binding_released = diagnostic_released,
};

BEHAVIOR_DT_INST_DEFINE(0, NULL, NULL, NULL, NULL, POST_KERNEL,
                        CONFIG_KERNEL_INIT_PRIORITY_DEFAULT, &diagnostic_api);

#endif /* DT_HAS_COMPAT_STATUS_OKAY */
