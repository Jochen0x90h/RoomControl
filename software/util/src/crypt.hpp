#pragma once

#include "tinycrypt/aes.h"


using AesKey = tc_aes_key_sched_struct;

inline void setKey(AesKey &aesKey, uint8_t const *key) {tc_aes128_set_encrypt_key(&aesKey, key);}


/**
 * Encrypt and authenticate messages using a MIC (message integrity code).
 * Pass plaintext followed by space for MIC in message, the message gets encrypted in-place
 *
 * @param deviceId device id
 * @param counter security counter
 * @param header header that only gets authenticated
 * @param headerLength length of header
 * @param message plaintext+zeros for encryption, ciphertext+MIC for decryption. Gets transformed from one to the other
 * @param payloadLength length of payload in message
 * @param micLength length of the MIC in message after payload
*/
void encrypt(uint32_t deviceId, uint32_t counter, uint8_t const *header, int headerLength,
	uint8_t *message, int payloadLength, int micLength, AesKey const &aesKey);


/**
 * Decrypt and authenticate messages using a MIC (message integrity code).
 * Pass ciphertext followed by MIC in message and check result
 *
 * @param deviceId device id
 * @param counter security counter
 * @param header header that only gets authenticated
 * @param headerLength length of header
 * @param message plaintext+zeros for encryption, ciphertext+MIC for decryption. Gets transformed from one to the other
 * @param payloadLength length of payload in message
 * @param micLength length of the MIC in message after payload
 * @return true if ok
 */
bool decrypt(uint32_t deviceId, uint32_t counter, uint8_t const *header, int headerLength,
	uint8_t *message, int payloadLength, int micLength, AesKey const &aesKey);
