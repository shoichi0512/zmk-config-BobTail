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
};

void bobtail_pairing_snapshot(struct bobtail_pairing_status *status);
