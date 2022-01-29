#include "hash.hpp"
#include "crypt.hpp"

void hash(Array<uint8_t, 16> output, uint8_t const *input, int input_len) {
	// Clear the first hash block (Hash0)
	output.fill(0);

	// Create the subsequent hash blocks using the formula: Hash[i] = E(Hash[i-1], M[i]) XOR M[i]

	// process complete blocks
	int i = 0;
	DataBuffer<16> cipher_in;
	AesKey aesKey;
	while (i + 16 <= input_len) {
		// copy data into the cipher input
		cipher_in.setData(0, 16, input + i);

		// note that the key input to the cipher is actually the previous hash block, which we are keeping in output
		setKey(aesKey, output);
		encrypt(output, cipher_in, aesKey);

		// xor the input into the hash block
		output ^= cipher_in;
		
		i += 16;
	}
	
	// process remaining bytes
	int j = input_len - i;
	
	// copy data into the cipher input
	cipher_in.setData(0, j, input + i);
	
	// append bit '1' and pad with zero
	cipher_in.setU8(j++, 0x80);
	cipher_in.pad(j);

	// check if 2 byte for input length fit at end of the last block
	if (j >= 14) {
		// no: need extra block
		setKey(aesKey, output);
		encrypt(output, cipher_in, aesKey);
		output ^= cipher_in;
		
		cipher_in.fill(0);
	}

	// set input length in bits at the end of the last block
	cipher_in.setU16B(14, input_len * 8);
	
	setKey(aesKey, output);
	encrypt(output, cipher_in, aesKey);
	output ^= cipher_in;
}

void keyHash(Array<uint8_t, 16> output, Array<uint8_t const, 16> key, uint8_t input) {
    uint8_t const ipad = 0x36;
    uint8_t const opad = 0x5c;

	DataBuffer<17> M1;
	auto key1 = M1.array<16>(0);

	key1.fill(ipad);
	key1 ^= key;
	M1[16] = input;
	
	DataBuffer<32> M2;
	auto key2 = M2.array<16>(0);
	auto hash1 = M2.array<16>(16);

	hash(hash1, M1.data, 17);
	key2.fill(opad);
	key2 ^= key;
	
	hash(output, M2.data, 32);
}
