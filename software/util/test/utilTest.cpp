#include "StringBuffer.hpp"
#include "TopicBuffer.hpp"
#include "ArrayList.hpp"
#include "StringSet.hpp"
#include "SystemTime.hpp"
#include <gtest/gtest.h>
#include <random>


// test utility functions and classes

constexpr String strings[] = {"a", "bar", "bar2", "foo", "foo2", "foobar", "foobar2", "z"};

TEST(utilTest, String) {
	for (int j = 0; j < std::size(strings); ++j) {
		for (int i = 0; i < std::size(strings); ++i) {
			EXPECT_EQ(i < j, strings[i] < strings[j]);
		}
	}
}

TEST(testUtil, StringBuffer) {
	int i = *parseInt("-50");
	EXPECT_EQ(i, -50);
	float f = *parseFloat("50.99");
	EXPECT_EQ(f, 50.99f);
	
	StringBuffer<100> b;
	b += dec(123456);
	b += ' ';
	b += dec(-99, 3);
	b += " ";
	b += hex(0x5, 1);
	b += " ";
	b += hex(0xabcdef12);
	b += " ";
	b += flt(-5.9f);
	b += " ";
	b += flt(2000000000);
	b += " ";
	b += flt(100.9999f);
	b += " ";
	b += flt(0.0f);
	b += " ";
	b += flt(0.5f);
	b += " ";
	b += flt(0.0f, 0, 2);
	b += " ";
	b += flt(0.5f, 0, 2);
	b += " ";
	b += flt(0.001234567f, 9);
	EXPECT_EQ(b.string(), "123456 -099 5 abcdef12 -5.9 2000000000 101 0 0.5 0 .5 0.001234567");

	StringBuffer<5> c = flt(0.000000001f, 9);
	EXPECT_EQ(c.string(), "0.000");

	StringBuffer<32> d = 'x' + hex(0xff) + String("foo") + dec(50) + '-';
	EXPECT_EQ(d.string(), "x000000fffoo50-");
}


TEST(testUtil, TopicBuffer) {
	TopicBuffer t = String();
	EXPECT_EQ(t.string(), String());
	
	t = "bar";
	EXPECT_EQ(t.string(), "bar");

	t = String("foo") / "bar";
	EXPECT_EQ(t.string(), "foo/bar");

	t = String("foo") / "bar" / "baz";
	EXPECT_EQ(t.string(), "foo/bar/baz");

	EXPECT_EQ(t.command(), "cmnd/foo/bar/baz");
	EXPECT_EQ(t.state(), "stat/foo/bar/baz");
	EXPECT_EQ(t.enumeration(), "enum/foo/bar/baz");

	t.removeLast();
	EXPECT_EQ(t.string(), "foo/bar");

	t.removeLast();
	t.removeLast();
	EXPECT_EQ(t.string(), String());
}

TEST(testUtil, ArrayList) {
	std::mt19937 gen(1337); //Standard mersenne_twister_engine seeded with 1337
	std::uniform_int_distribution<> distrib(0, array::size(strings) - 1);
	for (int count = 1; count < 7; ++count) {
		for (int round = 0; round < count * count; ++round) {
			ArrayList<String, 20> list;
			
			// add elements to list
			for (int i = 0; i < count; ++i) {
				// pick random string
				String s = strings[distrib(gen)];
				
				// determine where to insert
				int index = list.binaryLowerBound(s);
				
				// check if already exists
				if (index >= list.size() || !(list[index] == s)) {
					// insert into list
					list.insert(index, s);
				}
			}
			
			// print list
			/*
			std::cout << std::endl;
			for (String s : list) {
				std::cout << s << std::endl;
			}*/
			
			// check if the list contains unique sorted elements
			for (int i = 0; i < list.size() - 1; ++i) {
				EXPECT_TRUE(list[i] < list[i + 1]);
			}
		}
	}
}

TEST(testUtil, StringSet) {
	// test StringSet
	std::mt19937 gen(1337); //Standard mersenne_twister_engine seeded with 1337
	std::uniform_int_distribution<> distrib(0, array::size(strings) - 1);
	for (int count = 1; count < 7; ++count) {
		for (int round = 0; round < count * count; ++round) {
			StringSet<20, 1024> set;
			
			// add elements to list
			for (int i = 0; i < count; ++i) {
				String s = strings[distrib(gen)];
				set.add(s);
			}
			
			// print list
			/*
			std::cout << std::endl;
			for (String s : set) {
				std::cout << s << std::endl;
			}
			*/
			
			// check if the set contains unique sorted elements
			for (int i = 0; i < set.size() - 1; ++i) {
				EXPECT_TRUE(set[i] < set[i + 1]);
			}
		}
	}
}

TEST(testUtil, SystemTime) {
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
