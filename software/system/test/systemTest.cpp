#include "SystemTime.hpp"
#include <gtest/gtest.h>



TEST(systemTest, SystemTime) {
	// reference time
	SystemTime r = {0x00010010};
	SystemTime16 r16;
	r16 = r;
	
	SystemTime t1 = {0x00011000};
	SystemTime t2 = {0x00020000};
	SystemTime t3 = {0x00020010};
	SystemTime e0 = r16.expand(r);
	SystemTime e1 = r16.expand(t1);
	SystemTime e2 = r16.expand(t2);
	SystemTime e3 = r16.expand(t3);

	EXPECT_EQ(e0, r);
	EXPECT_EQ(e1, r);
	EXPECT_EQ(e2, r);
	EXPECT_EQ(e3, SystemTime{0x00020010});
}


int main(int argc, char **argv) {
	testing::InitGoogleTest(&argc, argv);
	int success = RUN_ALL_TESTS();	
	return success;
}
