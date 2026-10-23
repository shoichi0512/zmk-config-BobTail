/* SPDX-License-Identifier: MIT */
#pragma once
#include <stdbool.h>
#include <zephyr/bluetooth/bluetooth.h>

bool bobtail_profile_advertising_allowed(void);
int bobtail_profile_start_advertising(const struct bt_le_adv_param *params,
                                    const struct bt_data *data, size_t count);
int bobtail_profile_refresh_advertising(void);
int bobtail_profile_begin_pairing(void);
