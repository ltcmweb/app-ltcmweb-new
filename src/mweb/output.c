#include "output.h"

#define MWEB_OUTPUT_MESSAGE_STANDARD_FIELDS_FEATURE_BIT 1

cx_err_t mweb_output_create(mweb_output_t *output,
    blinding_factor_t blind, uint64_t v,
    const uint8_t *pA, const uint8_t *pB,
    const secret_key_t sender_key)
{
    public_key_t A, B, sA;
    hash_t n, h;
    secret_key_t s, t;
    uint8_t pt[65];
    blinding_factor_t blind_switch;
    cx_err_t error;

    compress_pubkey(A, pA);
    compress_pubkey(B, pB);

    // We only support standard feature fields for now
    output->message.features = MWEB_OUTPUT_MESSAGE_STANDARD_FIELDS_FEATURE_BIT;

    // Generate 128-bit secret nonce 'n' = Hash128(T_nonce, sender_privkey)
    CX_CHECK(blake3_update("N", 1));
    CX_CHECK(blake3_update(sender_key, sizeof(secret_key_t)));
    CX_CHECK(blake3_final(n));

    // Calculate unique sending key 's' = H(T_send, A, B, v, n)
    CX_CHECK(blake3_update("S", 1));
    CX_CHECK(blake3_update(A, sizeof(A)));
    CX_CHECK(blake3_update(B, sizeof(B)));
    CX_CHECK(blake3_update(&v, sizeof(v)));
    CX_CHECK(blake3_update(n, 16));
    CX_CHECK(blake3_final(s));

    // Derive shared secret 't' = H(T_derive, s*A)
    memcpy(pt, pA, sizeof(pt));
    CX_CHECK(cx_ecfp_scalar_mult_no_throw(CX_CURVE_256K1, pt, s, 32));
    compress_pubkey(sA, pt);
    CX_CHECK(blake3_update("D", 1));
    CX_CHECK(blake3_update(sA, sizeof(sA)));
    CX_CHECK(blake3_final(t));

    // Construct one-time public key for receiver 'Ko' = H(T_outkey, t)*B
    CX_CHECK(blake3_update("O", 1));
    CX_CHECK(blake3_update(t, sizeof(t)));
    CX_CHECK(blake3_final(h));
    memcpy(pt, pB, sizeof(pt));
    CX_CHECK(cx_ecfp_scalar_mult_no_throw(CX_CURVE_256K1, pt, h, 32));
    compress_pubkey(output->receiver_pubkey, pt);

    // Key exchange public key 'Ke' = s*B
    memcpy(pt, pB, sizeof(pt));
    CX_CHECK(cx_ecfp_scalar_mult_no_throw(CX_CURVE_256K1, pt, s, 32));
    compress_pubkey(output->message.key_exchange_pubkey, pt);

    // Calc blinding factor and mask nonce and amount
    CX_CHECK(blake3_update("B", 1));
    CX_CHECK(blake3_update(t, sizeof(t)));
    CX_CHECK(blake3_final(blind));

    CX_CHECK(blake3_update("Y", 1));
    CX_CHECK(blake3_update(t, sizeof(t)));
    CX_CHECK(blake3_final(h));
    output->message.masked_value = v ^ *(uint64_t*)h;

    CX_CHECK(blake3_update("X", 1));
    CX_CHECK(blake3_update(t, sizeof(t)));
    CX_CHECK(blake3_final(h));
    for (int i = 0; i < 16; i++) {
        output->message.masked_nonce[i] = n[i] ^ h[i];
    }

    // Commitment 'C' = r*G + v*H
    CX_CHECK(new_blind_switch(blind_switch, blind, v));
    CX_CHECK(new_commit(output->commit, NULL, blind_switch, v));

    // Calculate the ephemeral send pubkey 'Ks' = ks*G
    CX_CHECK(sk_pub(output->sender_pubkey, sender_key));

    // Derive view tag as first byte of H(T_tag, sA)
    CX_CHECK(blake3_update("T", 1));
    CX_CHECK(blake3_update(sA, sizeof(sA)));
    CX_CHECK(blake3_final(h));
    output->message.view_tag = h[0];
end:
    return error;
}