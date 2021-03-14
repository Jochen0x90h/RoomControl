#include "crypt.hpp"
#include "util.hpp"
#include <gtest/gtest.h>


static uint8_t const key[] = {0xC0, 0xC1, 0xC2, 0xC3, 0xC4, 0xC5, 0xC6, 0xC7, 0xC8, 0xC9, 0xCa, 0xCb, 0xCc, 0xCd, 0xCe, 0xCf};

constexpr uint32_t deviceId = 0x87654321;
constexpr uint32_t counter = 0x00000002;


// H.2.2 SecurityLevel=0b01
// H.3.2 SecurityLevel=0b01
TEST(cryptTest, security01) {
	
	// header = a (everything goes into the header, no payload, is not encrypted)
	static uint8_t const header1[] = {0x02, 0xFF, 0xFF, 0xFF, 0xFF, 0x8C, 0x28, 0x21, 0x43, 0x65, 0x87, 0x20};
	static uint8_t const header2[] = {0x02, 0xFF, 0xFF, 0xFF, 0xFF, 0x8C, 0x08, 0x21, 0x43, 0x65, 0x87, 0x20};

	// message (only contains the MIC)
	uint8_t message1[] = {0x61, 0x02};
	uint8_t message2[] = {0xB7, 0x55};

	AesKey aesKey;
	setKey(aesKey, key);

	bool result1 = crypt(deviceId, counter, header1, array::size(header1), message1, 0, 2, aesKey);
	bool result2 = crypt(deviceId, counter, header2, array::size(header2), message2, 0, 2, aesKey);
	EXPECT_TRUE(result1);
	EXPECT_TRUE(result2);
}

// H.3.3 SecurityLevel=0b10
TEST(cryptTest, security10) {
	
	// header = a (everything goes into the header, no payload, is not encrypted)
	static uint8_t const header[] = {0x8C, 0x30, 0x21, 0x43, 0x65, 0x87, 0x02, 0x00, 0x00, 0x00, 0x20};
	
	// message (only contains the MIC)
	uint8_t message[] = {0xAD, 0x69, 0xA9, 0x78};

	AesKey aesKey;
	setKey(aesKey, key);

	bool result = crypt(deviceId, counter, header, array::size(header), message, 0, 4, aesKey);
	EXPECT_TRUE(result);
}

// H.3.4 SecurityLevel=0b11
TEST(cryptTest, security11) {
	// header = a (is not encrypted)
	static uint8_t const header[] = {0x8C, 0x38, 0x21, 0x43, 0x65, 0x87, 0x02, 0x00, 0x00, 0x00};
	
	// message (payload and MIC)
	uint8_t message[] = {0x83, 0x5F, 0x1A, 0x30, 0x34};

	AesKey aesKey;
	setKey(aesKey, key);

	bool result = crypt(deviceId, counter, header, array::size(header), message, 1, 4, aesKey);
	EXPECT_TRUE(result);
}
