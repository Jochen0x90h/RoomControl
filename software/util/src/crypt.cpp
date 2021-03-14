#include "crypt.hpp"
#include "util.hpp"
#include "DataBuffer.hpp"


bool crypt(uint32_t deviceId, uint32_t counter, uint8_t const *header, int headerLength,
	uint8_t *message, int payloadLength, int micLength, AesKey &aesKey)
{
	//int payloadLength = messageLength - 4;

	int L = 2; // size of message length field
	int M = 4; // size of authentication field (is 4 also for security level 1)
	uint8_t const flags = 0x40 | ((M - 2) / 2 << 3) | (L - 1);

	// C.4.1 Decryption Transformation
	{
		// AES in counter mode, see https://de.wikipedia.org/wiki/Counter_Mode
		DataBuffer<16> Ai;
		Ai.setInt8(0, flags & 7);
		Ai.setLittleEndianInt32(1, deviceId);
		Ai.setLittleEndianInt32(5, deviceId);
		Ai.setLittleEndianInt32(9, counter);
		Ai.setInt8(13, 0x05);

		DataBuffer<16> P;
		int i = 1;
		uint8_t *c = message;
		int length = payloadLength;
		while (length > 0) {
			// set CTR-counter for Ai
			Ai.setBigEndianInt16(14, i++);

			// E(Key, Ai)
			tc_aes_encrypt(P.data, Ai.data, &aesKey);

			// Pi = E(Key, Ai) ^ Ci (do in-place in c)
			int l = min(length, 16);
			length -= l;
			for (int j = 0; j < l; ++j) {
				*c ^= P.data[j];
				++c;
			}
		}
		
		// set CTR-counter for A0
		Ai.setBigEndianInt16(14, 0);

		// S0 = E(Key, A0)
		tc_aes_encrypt(P.data, Ai.data, &aesKey);
		
		// T = S0 ^ U (do in-place in c)
		for (int j = 0; j < micLength; ++j) {
			*c ^= P.data[j];
			++c;
		}
	}
	
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

		uint8_t *m = message;
		int length = payloadLength;
		while (length > 0) {
			// Xi ^ Bi)
			int l = min(length, 16);
			length -= l;
			for (int j = 0; j < l; ++j) {
				Xi.data[j] ^= *m;
				++m;
			}

			// Xi+1 = E(key, Xi ^ Bi)
			tc_aes_encrypt(Xi.data, Xi.data, &aesKey);
		}
	
		// compare authentication tag T
		bool ok = true;
		for (int j = 0; j < micLength; ++j) {
			ok &= (m[j] ^= Xi.data[j]) == 0;
		}
		return ok;
	}
}
