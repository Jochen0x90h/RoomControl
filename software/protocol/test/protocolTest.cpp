#include "bus.hpp"
#include "crc.hpp"
#include "crypt.hpp"
#include "Nonce.hpp"
#include "hash.hpp"
#include <util.hpp>
#include <gtest/gtest.h>



TEST(protocolTest, DataBuffer) {
	DataBuffer<16> b;
	
	auto a1 = b.array<8>(8);
	b.setU8(8, 50);
	EXPECT_EQ(a1[0], 50);
	
	b.setData(0, b);
}

TEST(protocolTest, crc16) {
	// calc crc with CRC-16/CCITT-FALSE
	uint16_t crc = crc16(12, "Hello World!");

	// expected result from https://crccalc.com/
	EXPECT_EQ(crc, 0x882a);
}



static uint8_t const key[] = {0xC0, 0xC1, 0xC2, 0xC3, 0xC4, 0xC5, 0xC6, 0xC7, 0xC8, 0xC9, 0xCa, 0xCb, 0xCc, 0xCd, 0xCe, 0xCf};

constexpr uint32_t deviceId = 0x87654321;
constexpr uint32_t counter = 0x00000002;

// H.2.2 SecurityLevel=0b01
// H.3.2 SecurityLevel=0b01
TEST(protocolTest, security01) {
	
	// header = a (everything goes into the header, no payload, is not encrypted)
	static uint8_t const header1[] = {0x02, 0xFF, 0xFF, 0xFF, 0xFF, 0x8C, 0x28, 0x21, 0x43, 0x65, 0x87, 0x20};
	static uint8_t const header2[] = {0x02, 0xFF, 0xFF, 0xFF, 0xFF, 0x8C, 0x08, 0x21, 0x43, 0x65, 0x87, 0x20};

	// message (only contains the MIC)
	static uint8_t const message1[] = {0x61, 0x02};
	static uint8_t const message2[] = {0xB7, 0x55};

	AesKey aesKey;
	setKey(aesKey, key);

	Nonce nonce(deviceId, counter);
	bool result1 = decrypt(nullptr, header1, array::count(header1), message1, 0, 2, nonce, aesKey);
	bool result2 = decrypt(nullptr, header2, array::count(header2), message2, 0, 2, nonce, aesKey);
	EXPECT_TRUE(result1);
	EXPECT_TRUE(result2);
}

// H.3.3 SecurityLevel=0b10
TEST(protocolTest, security10) {
	
	// header = a (everything goes into the header, no payload, is not encrypted)
	static uint8_t const header[] = {0x8C, 0x30, 0x21, 0x43, 0x65, 0x87, 0x02, 0x00, 0x00, 0x00, 0x20};
	
	// message (only contains the MIC)
	static uint8_t const message[] = {0xAD, 0x69, 0xA9, 0x78};

	AesKey aesKey;
	setKey(aesKey, key);

	Nonce nonce(deviceId, counter);
	bool result = decrypt(nullptr, header, array::count(header), message, 0, 4, nonce, aesKey);
	EXPECT_TRUE(result);
}

// H.3.4 SecurityLevel=0b11
TEST(protocolTest, security11) {
	// header = a (is not encrypted)
	static uint8_t const header[] = {0x8C, 0x38, 0x21, 0x43, 0x65, 0x87, 0x02, 0x00, 0x00, 0x00};
	
	// message (1 byte payload and 4 byte MIC)
	static uint8_t const message[] = {0x83, 0x5F, 0x1A, 0x30, 0x34};

	AesKey aesKey;
	setKey(aesKey, key);

	Nonce nonce(deviceId, counter);

	uint8_t decrypted[1];
	bool result = decrypt(decrypted, header, array::count(header), message, 1, 4, nonce, aesKey);
	EXPECT_TRUE(result);
	EXPECT_EQ(decrypted[0], 0x20);

	uint8_t encrypted[5];
	encrypt(encrypted, header, array::count(header), decrypted, 1, 4, nonce, aesKey);
	EXPECT_EQ(encrypted[0], 0x83);
	EXPECT_EQ(encrypted[1], 0x5F);
	EXPECT_EQ(encrypted[2], 0x1A);
	EXPECT_EQ(encrypted[3], 0x30);
	EXPECT_EQ(encrypted[4], 0x34);
}

// C.5.1
TEST(protocolTest, hash1) {
	static uint8_t const input[] = {0xC0};
	static uint8_t const expectedOutput[] = {0xAE, 0x3A, 0x10, 0x2A, 0x28, 0xD4, 0x3E, 0xE0, 0xD4, 0xA0, 0x9E, 0x22, 0x78, 0x8B, 0x20, 0x6C};

	DataBuffer<16> output;
	
	hash(output, input, array::count(input));
	
	for (int i = 0; i < 16; ++i) {
		EXPECT_EQ(output[i], expectedOutput[i]);
	}
}

// C.5.2
TEST(protocolTest, hash2) {
	static uint8_t const input[] = {0xC0, 0xC1, 0xC2, 0xC3, 0xC4, 0xC5, 0xC6, 0xC7, 0xC8, 0xC9, 0xCA, 0xCB, 0xCC, 0xCD, 0xCE, 0xCF};
	static uint8_t const expectedOutput[] = {0xA7, 0x97, 0x7E, 0x88, 0xBC, 0x0B, 0x61, 0xE8, 0x21, 0x08, 0x27, 0x10, 0x9A, 0x22, 0x8F, 0x2D};

	DataBuffer<16> output;
	
	hash(output, input, array::count(input));
	
	for (int i = 0; i < 16; ++i) {
		EXPECT_EQ(output[i], expectedOutput[i]);
	}
}

// C.5.3
TEST(protocolTest, hash3) {
	uint8_t input[8191];
	static uint8_t const expectedOutput[] = {0x24, 0xEC, 0x2F, 0xE7, 0x5B, 0xBF, 0xFC, 0xB3, 0x47, 0x89, 0xBC, 0x06, 0x10, 0xE7, 0xF1, 0x65};

	for (int i = 0; i < array::count(input); ++i)
		input[i] = i;
	DataBuffer<16> output;
	
	hash(output, input, array::count(input));
	
	for (int i = 0; i < 16; ++i) {
		EXPECT_EQ(output[i], expectedOutput[i]);
	}
}

// C.6.1
TEST(protocolTest, keyHash1) {
	static uint8_t const key[] = {0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4A, 0x4B, 0x4C, 0x4D, 0x4E, 0x4F};
	static uint8_t const expectedOutput[] = {0x45, 0x12, 0x80, 0x7B, 0xF9, 0x4C, 0xB3, 0x40, 0x0F, 0x0E, 0x2C, 0x25, 0xFB, 0x76, 0xE9, 0x99};

	DataBuffer<16> output;
	
	keyHash(output, key, 0xC0);

	for (int i = 0; i < 16; ++i) {
		EXPECT_EQ(output[i], expectedOutput[i]);
	}
}


// generators
// ----------

// print tinycrypt aes key
void printKey(char const *name, AesKey const &key) {
	std::cout << "AesKey const " << name << " = {{";
	bool first = true;
	for (auto w : key.words) {
		if (!first)
			std::cout << ", ";
		first = false;
		std::cout << "0x" << std::hex << std::setfill('0') << std::setw(8) << w;
	}
	std::cout << "}};\n";
}

// generate bus key for use with tinycrypt
void generateBusKey() {
	AesKey aesKey;
	setKey(aesKey, bus::defaultKey);
	printKey("defaultAesKey", aesKey);
}

// generate zbee keys for use with tinycrypt
void generateZa09Keys() {
	AesKey aesKey;

	// link key
	setKey(aesKey, zb::za09LinkKey);
	printKey("za09LinkAesKey", aesKey);

	// transport key
	DataBuffer<16> hashedKey;
	keyHash(hashedKey, zb::za09LinkKey, 0);
	setKey(aesKey, hashedKey);
	printKey("za09KeyTransportAesKey", aesKey);

	// key load key
	keyHash(hashedKey, zb::za09LinkKey, 2);
	setKey(aesKey, hashedKey);
	printKey("za09KeyLoadAesKey", aesKey);
}

// generate lookup table for crc16
void generateCrc16Table(uint16_t polynomial) {
	uint16_t  crc_table[256];
	for (int b = 0; b <= 255; ++b) {
		uint16_t v;
		int i;
		for (v = b << 8, i = 8; --i >= 0;) {
			if ((v & 0x8000) != 0x0000)
				v = ( v << 1 ) ^ polynomial;
			else
				v = v << 1;
		}
		crc_table[b] = v;
	}

	for (int j = 0; j < 16; ++j) {
		for (int i = 0; i < 16; ++i) {
			std::cout << "0x" << std::hex << std::setfill('0') << std::setw(4) << crc_table[j * 16 + i] << ", ";
		}
		std::cout << std::endl;
	}
}

int main(int argc, char **argv) {
	//generateBusKey();
	//generateZa09Keys();
	//generateCrc16Table(0x1021);

	testing::InitGoogleTest(&argc, argv);
	int success = RUN_ALL_TESTS();	
	return success;
}
