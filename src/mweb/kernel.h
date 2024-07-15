#pragma once

#include "sign.h"

cx_err_t sign_mweb_kernel(
    const blinding_factor_t kernel_blind,
    const blinding_factor_t stealth_blind,
    public_key_t stealth_excess_pubkey,
    signature_t sig);
