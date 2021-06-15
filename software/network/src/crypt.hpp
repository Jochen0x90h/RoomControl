#pragma once

#include "tinycrypt/aes.h"
#include "DataBuffer.hpp"
#include "zb.hpp"


using AesKey = tc_aes_key_sched_struct;

inline void setKey(AesKey &aesKey, DataBufferConstRef<16> key) {
	tc_aes128_set_encrypt_key(&aesKey, key.data);
}

inline void encrypt(DataBufferRef<16> out, DataBufferConstRef<16> in, AesKey const &aesKey) {
	tc_aes_encrypt(out.data, in.data, &aesKey);
}

/**
 * Encrypt and authenticate messages using a MIC (message integrity code).
 * Pass plaintext followed by space for MIC in message, the message gets encrypted in-place
 *
 * @param result encrypted message and MIC, length is payloadLength + micLength
 * @param nonce nonce
 * @param header header to authenticate
 * @param headerLength length of header
 * @param message plaintext message to encrypt and authenticate, length is payloadLength
 * @param payloadLength length of payload in message/result
 * @param micLength length of the MIC in result after payload
*/
void encrypt(uint8_t *result, DataBuffer<13> const &nonce, uint8_t const *header, int headerLength,
	uint8_t const *message, int payloadLength, int micLength, AesKey const &aesKey);


/**
 * Decrypt and authenticate messages using a MIC (message integrity code).
 * Pass ciphertext followed by MIC in message and check result
 *
 * @param result decrypted message, length is payloadLength
 * @param nonce nonce
 * @param header header that only gets authenticated
 * @param headerLength length of header
 * @param message encrypted message and MIC to decrypt and authenticate, length is payloadLength + micLength
 * @param payloadLength length of payload in message/result
 * @param micLength length of the MIC in message after payload
 * @return true if ok
 */
bool decrypt(uint8_t *result, DataBuffer<13> const &nonce, uint8_t const *header, int headerLength,
	uint8_t const *message, int payloadLength, int micLength, AesKey const &aesKey);
