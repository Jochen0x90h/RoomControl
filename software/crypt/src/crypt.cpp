#include "crypt.hpp"
#include "DataBuffer.hpp"
#include <util.hpp>


constexpr int L = 2; // size of message length field

static void crypt(uint8_t *p, uint8_t *u, DataBuffer<13> const &nonce,
	uint8_t const *c, int payloadLength, uint8_t const *t, int micLength, AesKey const &aesKey)
{
	// C.4.1 Decryption Transformation
	uint8_t const flags = L - 1;

	// AES in counter mode, see https://de.wikipedia.org/wiki/Counter_Mode
	DataBuffer<16> Ai;
	Ai.setInt8(0, flags);
	Ai.setData(1, nonce);

	DataBuffer<16> Si;
	int i = 1;
	int length = payloadLength;

	while (length > 0) {
		// set CTR-counter for Ai (starts with A1)
		Ai.setBigEndianInt16(14, i++);

		// Si = E(Key, Ai)
		encrypt(Si, Ai, aesKey);

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
	encrypt(Si, Ai, aesKey);
	
	// U = S0 ^ T
	for (int j = 0; j < micLength; ++j) {
		*u = Si.data[j] ^ *t;
		++t;
		++u;
	}
}

void encrypt(uint8_t *result, DataBuffer<13> const &nonce, uint8_t const *header, int headerLength,
	uint8_t const *message, int payloadLength, int micLength, AesKey const &aesKey)
{
	int L = 2; // size of message length field
	int M = 4; // size of authentication field (is 4 also for GP security level 1 where only 2 byte MIC is transferred)
	uint8_t const flags = 0x40 | ((M - 2) / 2 << 3) | (L - 1);

	
	// C.3.2 Authentication Transformation
	// AES in block chaining mode, see https://de.wikipedia.org/wiki/Cipher_Block_Chaining_Mode
	DataBuffer<16> Xi;

	// X0 ^ B0 (X0 is zero)
	Xi.setInt8(0, flags);
	Xi.setData(1, nonce);
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
		encrypt(Xi, Xi, aesKey);
	}

	// authentication tag T is in Xi (message integrity code)

	crypt(result, result + payloadLength, nonce, message, payloadLength, Xi.data, micLength, aesKey);
}

bool decrypt(uint8_t *result, DataBuffer<13> const &nonce, uint8_t const *header, int headerLength,
	uint8_t const *message, int payloadLength, int micLength, AesKey const &aesKey)
{
	int M = 4; // size of authentication field (is 4 also for security level 1)
	uint8_t const flags = 0x40 | ((M - 2) / 2 << 3) | (L - 1);

	uint8_t T[4];
	crypt(result, T, nonce, message, payloadLength, message + payloadLength, micLength, aesKey);
	
	// C.4.2 Authentication Checking Transformation
	// C.3.2 Authentication Transformation
	{
		// AES in block chaining mode, see https://de.wikipedia.org/wiki/Cipher_Block_Chaining_Mode
		DataBuffer<16> Xi;

		// X0 ^ B0 (X0 is zero)
		Xi.setInt8(0, flags);
		Xi.setData(1, nonce);
		Xi.setBigEndianInt16(14, payloadLength);

		// X1 = E(key, X0 ^ B0)
		tc_aes_encrypt(Xi.data, Xi.data, &aesKey);
		
		// X1 ^ B1
		Xi.xorBigEndianInt16(0, headerLength);
		Xi.xorData(2, header, headerLength);
		header += 14;
		headerLength -= 14;
		
		// X2 = E(key, X1 ^ B1)
		encrypt(Xi, Xi, aesKey);

		// repeat for remainder of header
		while (headerLength > 0) {
			// Xi ^ Bi
			Xi.xorData(0, header, headerLength);
			header += 16;
			headerLength -= 16;

			// Xi+1 = E(key, Xi ^ Bi)
			encrypt(Xi, Xi, aesKey);
		}

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
			encrypt(Xi, Xi, aesKey);
		}
	
		// compare authentication tag T
		uint8_t ok = 0;
		for (int j = 0; j < micLength; ++j) {
			ok |= T[j] ^ Xi.data[j];
		}
		return ok == 0;
	}
}
