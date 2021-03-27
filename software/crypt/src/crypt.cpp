#include "crypt.hpp"
#include "DataBuffer.hpp"
#include <util.hpp>


constexpr int L = 2; // size of message length field


static void crypt(uint8_t *p, uint8_t *u, uint32_t deviceId, uint32_t counter,
	uint8_t const *c, int payloadLength, uint8_t const *t, int micLength, AesKey const &aesKey)
{
	// C.4.1 Decryption Transformation
	uint8_t const flags = L - 1;

	// AES in counter mode, see https://de.wikipedia.org/wiki/Counter_Mode
	DataBuffer<16> Ai;
	Ai.setInt8(0, flags);
	Ai.setLittleEndianInt32(1, deviceId);
	Ai.setLittleEndianInt32(5, deviceId);
	Ai.setLittleEndianInt32(9, counter);
	Ai.setInt8(13, 0x05);

	DataBuffer<16> Si;
	int i = 1;
	int length = payloadLength;
	while (length > 0) {
		// set CTR-counter for Ai
		Ai.setBigEndianInt16(14, i++);

		// Si = E(Key, Ai)
		tc_aes_encrypt(Si.data, Ai.data, &aesKey);

		// Pi = E(Key, Ai) ^ Ci
		int l = min(length, 16);
		length -= l;
		for (int j = 0; j < l; ++j) {
			*p = Si.data[j] ^ *c;
			++c;
			++p;
		}
	}
	
	// set CTR-counter for A0
	Ai.setBigEndianInt16(14, 0);

	// S0 = E(Key, A0)
	tc_aes_encrypt(Si.data, Ai.data, &aesKey);
	
	// U = S0 ^ T
	for (int j = 0; j < micLength; ++j) {
		*u = Si.data[j] ^ *t;
		++t;
		++u;
	}
}

bool decrypt(uint8_t *result, uint32_t deviceId, uint32_t counter, uint8_t const *header, int headerLength,
	uint8_t const *message, int payloadLength, int micLength, AesKey const &aesKey)
{
	int M = 4; // size of authentication field (is 4 also for security level 1)
	uint8_t const flags = 0x40 | ((M - 2) / 2 << 3) | (L - 1);

	uint8_t T[4];
	crypt(result, T, deviceId, counter, message, payloadLength, message + payloadLength, micLength, aesKey);
	
	// C.4.2 Authentication Checking Transformation
	// C.3.2 Authentication Transformation
	{
		// AES in block chaining mode, see https://de.wikipedia.org/wiki/Cipher_Block_Chaining_Mode
		DataBuffer<16> Xi;

		// X0 ^ B0 (X0 is zero)
		Xi.setInt8(0, flags);
		Xi.setLittleEndianInt32(1, deviceId);
		Xi.setLittleEndianInt32(5, deviceId);
		Xi.setLittleEndianInt32(9, counter);
		Xi.setInt8(13, 0x05);
		Xi.setBigEndianInt16(14, payloadLength);

		// X1 = E(key, X0 ^ B0)
		tc_aes_encrypt(Xi.data, Xi.data, &aesKey);
		
		// X1 ^ B1
		Xi.xorBigEndianInt16(0, headerLength);
		Xi.xorData(2, header, headerLength);

		// X2 = E(key, X1 ^ B1)
		tc_aes_encrypt(Xi.data, Xi.data, &aesKey);

		uint8_t *b = result;
		int length = payloadLength;
		while (length > 0) {
			// Xi ^ Bi
			int l = min(length, 16);
			length -= l;
			for (int j = 0; j < l; ++j) {
				Xi.data[j] ^= *b;
				++b;
			}

			// Xi+1 = E(key, Xi ^ Bi)
			tc_aes_encrypt(Xi.data, Xi.data, &aesKey);
		}
	
		// compare authentication tag T
		uint8_t ok = 0;
		for (int j = 0; j < micLength; ++j) {
			ok |= T[j] ^ Xi.data[j];
		}
		return ok == 0;
	}
}

void encrypt(uint8_t *result, uint32_t deviceId, uint32_t counter, uint8_t const *header, int headerLength,
	uint8_t const *message, int payloadLength, int micLength, AesKey const &aesKey)
{
	int L = 2; // size of message length field
	int M = 4; // size of authentication field (is 4 also for security level 1)
	uint8_t const flags = 0x40 | ((M - 2) / 2 << 3) | (L - 1);

	
	// C.3.2 Authentication Transformation
	// AES in block chaining mode, see https://de.wikipedia.org/wiki/Cipher_Block_Chaining_Mode
	DataBuffer<16> Xi;

	// X0 ^ B0 (X0 is zero)
	Xi.setInt8(0, flags);
	Xi.setLittleEndianInt32(1, deviceId);
	Xi.setLittleEndianInt32(5, deviceId);
	Xi.setLittleEndianInt32(9, counter);
	Xi.setInt8(13, 0x05);
	Xi.setBigEndianInt16(14, payloadLength);

	// X1 = E(key, X0 ^ B0)
	tc_aes_encrypt(Xi.data, Xi.data, &aesKey);
	
	// X1 ^ B1
	Xi.xorBigEndianInt16(0, headerLength);
	Xi.xorData(2, header, headerLength);

	// X2 = E(key, X1 ^ B1)
	tc_aes_encrypt(Xi.data, Xi.data, &aesKey);

	uint8_t const *b = message;
	int length = payloadLength;
	while (length > 0) {
		// Xi ^ Bi
		int l = min(length, 16);
		length -= l;
		for (int j = 0; j < l; ++j) {
			Xi.data[j] ^= *b;
			++b;
		}

		// Xi+1 = E(key, Xi ^ Bi)
		tc_aes_encrypt(Xi.data, Xi.data, &aesKey);
	}

	// authentication tag T is in Xi
/*		// create authentication tag T
		for (int j = 0; j < micLength; ++j) {
			m[j] = Xi.data[j];
		}
	}
*/
	crypt(result, result + payloadLength, deviceId, counter, message, payloadLength, Xi.data, micLength, aesKey);
}
