/* SPDX-License-Identifier: MIT */
#pragma once

enum bobtail_hfxo_state {
    BOBTAIL_HFXO_OFF = 0,
    BOBTAIL_HFXO_PENDING = 1,
    BOBTAIL_HFXO_ACTIVE = 2,
    BOBTAIL_HFXO_ERROR = 3,
};

int bobtail_hfxo_toggle(void);
unsigned int bobtail_hfxo_status(void);
