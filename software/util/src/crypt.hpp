#pragma once

#include "tinycrypt/aes.h"


using AesKey = tc_aes_key_sched_struct;

inline void setKey(AesKey &aesKey, uint8_t const *key) {tc_aes128_set_encrypt_key(&aesKey, key);}


/**
 * Encrypt, decrypt and authenticate messages using a MIC (message integrity code)
 *
 * Encrypt: Pass plaintext followed by MIC set to zero in message, the message gets encrypted in-place
 * Decrypt: Pass ciphertext followed by MIC in message and check result
 *
 * @param deviceId device id
 * @param counter security counter
 * @param header header that only gets authenticated
 * @param headerLength length of header
 * @param message plaintext+zeros for encryption, ciphertext+MIC for decryption. Gets transformed from one to the other
 * @param payloadLength length of payload in message
 * @param micLength length of the MIC in message after payload
 */
bool crypt(uint32_t deviceId, uint32_t counter, uint8_t const *header, int headerLength,
	uint8_t *message, int payloadLength, int micLength, AesKey &aesKey);
