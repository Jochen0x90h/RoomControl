#include <SystemTime.hpp>
#include <ClockTime.hpp>
#include <gtest/gtest.h>


TEST(systemTest, SystemTime) {
	SystemTime t1 = {0};
	SystemTime t2 = {0x70000000};
	SystemTime t3 = {0x90000000};

	EXPECT_TRUE(t1 < t2);
	EXPECT_TRUE(t2 < t3);
	EXPECT_TRUE(t3 < t1);
}

/*
TEST(systemTest, SystemTime16) {
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
}*/

TEST(systemTest, ClockTime) {
	// time 1 is Tuesday, 10:05:01
	ClockTime time1(1, 10, 5, 1);

	// time 2 is Wednesday, 10:05:01
	ClockTime time2(2, 10, 5, 1);

	// time 2 is Tuesday, 10:05:02
	ClockTime time3(2, 10, 5, 1);

	// alarm on Tuesday, 10:05:01
	AlarmTime alarm(2, 10, 5, 1);

	EXPECT_TRUE(alarm.matches(time1));
	EXPECT_FALSE(alarm.matches(time2));
	EXPECT_FALSE(alarm.matches(time3));
}


int main(int argc, char **argv) {
	testing::InitGoogleTest(&argc, argv);
	int success = RUN_ALL_TESTS();	
	return success;
}
