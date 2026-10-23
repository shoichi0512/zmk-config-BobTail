/* SPDX-License-Identifier: MIT */
#pragma once

struct bobtail_pairing_status {
    unsigned int attempts;
    unsigned int stage;
    unsigned int connect_error;
    unsigned int security_error;
    unsigned int pairing_error;
    unsigned int disconnect_reason;
    unsigned int bonds;
    unsigned int peer_bond;
    unsigned int peer_profile;
};

void bobtail_pairing_snapshot(struct bobtail_pairing_status *status);
/* Called only by an explicit repair key on an open profile. */
int bobtail_pairing_clear_unmapped_failed_bond(int profile);
