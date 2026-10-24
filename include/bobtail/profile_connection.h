/* SPDX-License-Identifier: MIT */
#pragma once
#include <stdbool.h>
#include <zephyr/bluetooth/bluetooth.h>

bool bobtail_profile_advertising_allowed(void);
int bobtail_profile_refresh_advertising(void);
int bobtail_profile_begin_pairing(void);
