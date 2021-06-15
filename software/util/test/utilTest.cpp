#include "ArrayList.hpp"
#include "Coroutine.hpp"
#include "enum.hpp"
#include "DataQueue.hpp"
#include "Queue.hpp"
#include "StringBuffer.hpp"
#include "StringSet.hpp"
#include "TopicBuffer.hpp"
#include <gtest/gtest.h>
#include <random>


// test utility functions and classes

constexpr String strings[] = {"a", "bar", "bar2", "foo", "foo2", "foobar", "foobar2", "z"};


TEST(utilTest, ArrayList) {
	std::mt19937 gen(1337); // standard mersenne_twister_engine seeded with 1337
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

TEST(utilTest, DataQueue) {
	using Q = DataQueue<int, 32, 256>;
	Q queue;

	EXPECT_TRUE(queue.hasSpace(255));
	EXPECT_FALSE(queue.hasSpace(256));
	
	auto e1 = queue.add(0, 128);
	EXPECT_TRUE(queue.hasSpace(127));
	EXPECT_FALSE(queue.hasSpace(128));
	EXPECT_EQ(e1.data, queue.data());
	queue.remove();

	auto e2 = queue.add(0, 128);
	EXPECT_TRUE(queue.hasSpace(127));
	EXPECT_FALSE(queue.hasSpace(128));
	EXPECT_EQ(e2.data, queue.data() + 128);
	queue.remove();

	
	// add some entries
	for (int i = 0; i < 5; ++i) {
		auto e1 = queue.add(i, i);
		std::fill(e1.data, e1.data + e1.length, uint8_t(i));
	}

	// add and remove many entries
	for (int i = 5; i < 105; ++i) {
		auto e1 = queue.add(i, i & 15);
		std::fill(e1.data, e1.data + e1.length, uint8_t(i));

		auto e2 = queue.get();
		EXPECT_EQ(e2.info, i - 5);
		EXPECT_EQ(e2.length, (i - 5) & 15);
		for (int i = 0; i < e2.length; ++i) {
			EXPECT_EQ(e2.data[i], e2.info);
		}
		
		queue.remove();
	}

	// remove last entries
	for (int i = 0; i < 5; ++i) {
		queue.remove();
	}
	
	// now queue must be empty again
	EXPECT_TRUE(queue.isEmpty());
}

enum class Foo : uint8_t {
	FOO = 1,
	BAR = 2
};
FLAGS_ENUM(Foo);

TEST(utilTest, flagsEnum) {
	Foo a = Foo::FOO;
	a |= Foo::BAR;
	EXPECT_EQ(a, Foo::FOO | Foo::BAR);

	uint8_t b = 0;
	b |= Foo::FOO;
	EXPECT_EQ(Foo::FOO, b);
}

TEST(utilTest, Queue) {
	using Q = Queue<int, 4>;
	Q queue;
	
	// check if initially empty
	EXPECT_TRUE(queue.isEmpty());
	
	// add and remove one element
	queue.add() = 5;
	EXPECT_EQ(queue.get(), 5);
	queue.remove();
	EXPECT_TRUE(queue.isEmpty());

	// add elements until queue is full
	int i = 0;
	int &head = queue.getHead();
	while (!queue.isFull()) {
		queue.add() = 1000 + i;
		++i;
	}
	EXPECT_EQ(i, 4);
	EXPECT_EQ(queue.size(), 4);
	EXPECT_EQ(head, 1000);

	// remove one element
	queue.remove();
	EXPECT_FALSE(queue.isEmpty());
	EXPECT_FALSE(queue.isFull());
	EXPECT_EQ(queue.size(), 3);
	EXPECT_EQ(queue.get(), 1000 + 1);
	
	// clear and "resurrect" first element (is used in radio driver)
	queue.clear();
	queue.increment();
	EXPECT_EQ(queue.size(), 1);
	EXPECT_EQ(queue.get(), 1000 + 1);
}

TEST(utilTest, String) {
	for (int j = 0; j < std::size(strings); ++j) {
		for (int i = 0; i < std::size(strings); ++i) {
			EXPECT_EQ(i < j, strings[i] < strings[j]);
		}
	}
}

TEST(utilTest, StringBuffer) {
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


TEST(utilTest, TopicBuffer) {
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

TEST(utilTest, StringSet) {
	// test StringSet
	std::mt19937 gen(1337); // standard mersenne_twister_engine seeded with 1337
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

TEST(utilTest, UInt) {
	static_assert(sizeof(UInt<256>::Type) == 1);
	static_assert(sizeof(UInt<257>::Type) == 2);
	static_assert(sizeof(UInt<65536>::Type) == 2);
	static_assert(sizeof(UInt<65537>::Type) == 4);
}

// test coroutine
// --------------

CoList<void> coList;

Awaitable<void> wait() {
	return coList;
}

CoList<void> coList2;

Awaitable<void> wait2() {
	return coList2;
}

Awaitable<void> inner() {
	std::cout << "inner wait 1" << std::endl;
	co_await wait();
	std::cout << "inner wait 2" << std::endl;
	co_await wait();
}

Coroutine outer() {
	co_await inner();

	std::cout << "outer select" << std::endl;
	switch (co_await select(wait(), wait2())) {
	case 1:
		std::cout << "selected 1" << std::endl;
		break;
	case 2:
		std::cout << "selected 2" << std::endl;
		break;
	}
}

TEST(utilTest, Coroutine) {
	outer();
	
	std::cout << "inner resume 1" << std::endl;
	coList.resumeAll();
	std::cout << "inner resume 2" << std::endl;
	coList.resumeAll();
	
	std::cout << "outer resume 2" << std::endl;
	coList2.resumeAll();
}


// test coroutine with extra value
// -------------------------------

CoList<int> coListValue;

Awaitable<int> delay(int value) {
	return {coListValue, value};
}

Coroutine waitForDelay1() {
	co_await delay(5);
	std::cout << "resumed delay(5)" << std::endl;
}

Coroutine waitForDelay2() {
	switch (co_await select(wait(), delay(10))) {
	case 1:
		std::cout << "resumed wait" << std::endl;
		break;
	case 2:
		std::cout << "resumed delay(10)" << std::endl;
		break;
	}
}

TEST(utilTest, CoroutineValue) {
	waitForDelay1();
	waitForDelay2();
	
	coListValue.resumeAll([] (int value) {return value == 5;});
	coListValue.resumeAll([] (int value) {return value == 10;});
}


// test wait on CoList<>
// ---------------------

CoList<> waitList;

Coroutine waitForList() {
	std::cout << "wait for list" << std::endl;
	co_await waitList.wait();
	std::cout << "list resumed" << std::endl;
}

TEST(utilTest, CoListWait) {
	waitForList();
	std::cout << "resume list" << std::endl;
	waitList.resumeAll();
}


// test Semaphore
// --------------

Semaphore semaphore(0);

Coroutine worker1() {
	while (true) {
		co_await semaphore.wait();
		std::cout << "worker 1" << std::endl;
	}
}

Coroutine worker2() {
	while (true) {
		co_await semaphore.wait();
		std::cout << "worker 2" << std::endl;
	}
}

TEST(utilTest, Semaphore) {
	worker1();
	worker2();
	for (int i = 0; i < 10; ++i) {
		semaphore.post();
	}
}



int main(int argc, char **argv) {
	testing::InitGoogleTest(&argc, argv);
	int success = RUN_ALL_TESTS();	
	return success;
}
