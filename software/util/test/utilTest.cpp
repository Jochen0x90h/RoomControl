#include <Array.hpp>
#include <ArrayList.hpp>
#include <convert.hpp>
#include <Coroutine.hpp>
#include <DataQueue.hpp>
#include <enum.hpp>
#include <IsSubclass.hpp>
#include <LinkedList.hpp>
#include <Queue.hpp>
#include <StringBuffer.hpp>
#include <StringHash.hpp>
#include <StringSet.hpp>
#include <StringOperators.hpp>
#include <TopicBuffer.hpp>
#include <Cie1931.hpp>
#include <gtest/gtest.h>
#include <random>
#include <netinet/in.h> // htonl


// test utility functions and classes

constexpr String strings[] = {"a", "bar", "bar2", "foo", "foo2", "foobar", "foobar2", "z"};

// array helper functions
TEST(utilTest, array) {
	int a1[] = {10, 20, 30};

	// before first value
	EXPECT_EQ(array::binaryLowerBound(a1, [](int a) {return a < 0;}), 0);

	// between
	EXPECT_EQ(array::binaryLowerBound(a1, [](int a) {return a < 19;}), 1);
	EXPECT_EQ(array::binaryLowerBound(a1, [](int a) {return a < 20;}), 1);
	EXPECT_EQ(array::binaryLowerBound(a1, [](int a) {return a < 21;}), 2);

	// after last value
	EXPECT_EQ(array::binaryLowerBound(a1, [](int a) {return a < 40;}), 2);
}


// array with fixed size
TEST(utilTest, ArrayN) {
	int a1[] = {10, 11, 12};
	int const a2[] = {20, 21, 22};
	
	// construct arrays from c-arrays
	Array<int, 3> b1(a1);
	Array<int const, 3> b2(a2);
	Array<char const, 4> str("foo");
		
	EXPECT_EQ(b1[1], a1[1]);
	EXPECT_EQ(b2[1], a2[1]);
	EXPECT_EQ(str[1], 'o');

	EXPECT_FALSE(b1.isEmpty());
	EXPECT_FALSE(b2.isEmpty());
	EXPECT_EQ(b1.count(), 3);
	EXPECT_EQ(b2.count(), 3);
	
	// construct const array from non-const array
	Array<int const, 3> b3(b1);
	EXPECT_EQ(b3.count(), 3);

	// comparison
	EXPECT_TRUE(b1 != b2);
	EXPECT_TRUE(b1 == b3);

	// assign a value
	b1[1] = 10;
	EXPECT_EQ(b1[1], 10);
	//b2[1] = 10; // should not compile
	
	// assign from other buffer
	b3 = b1;
	EXPECT_EQ(b3[1], b1[1]);
	b3 = b2;
	EXPECT_EQ(b3[1], b2[1]);

	// fill with value and check count
	b1.fill(50);
	int count = 0;
	for (int i : b1) {
		++count;
		EXPECT_EQ(i, 50);
	}
	EXPECT_EQ(count, 3);
}


// array with variable size
TEST(utilTest, Array) {
	int a1[] = {10, 11, 12};
	int const a2[] = {20, 21, 22};
	
	// construct arrays from c-arrays
	Array<int> b1(a1);
	Array<int const> b2(a2);
	Array<char const> str("foo");
	
	EXPECT_EQ(b1[1], a1[1]);
	EXPECT_EQ(b2[1], a2[1]);
	EXPECT_EQ(str[1], 'o');

	EXPECT_FALSE(b1.isEmpty());
	EXPECT_FALSE(b2.isEmpty());
	EXPECT_EQ(b1.count(), 3);
	EXPECT_EQ(b2.count(), 3);

	// construct const array from non-const array
	Array<int const> b3(b1);

	// assign a value
	b1[1] = 10;
	EXPECT_EQ(b1[1], 10);
	//b2[1] = 10; // should not compile

	// assign from other buffer
	b3 = b1;
	EXPECT_EQ(b3[1], b1[1]);
	b3 = b2;
	EXPECT_EQ(b3[1], b2[1]);

	// fill with value and check count
	b1.fill(50);
	int count = 0;
	for (int i : b1) {
		++count;
		EXPECT_EQ(i, 50);
	}
	EXPECT_EQ(count, 3);

	// assign
	b1.assign(a2);
	EXPECT_EQ(b1[2], 22);
}

// std::initializer_list
struct InitStruct {
	std::initializer_list<int> i;
	std::initializer_list<float> f;
};
TEST(utilTest, initializer_list) {
	InitStruct const arrayStruct = {
		{1, 2, 3},
		{1.0f, 2.0f, 3.0f}
	};

	EXPECT_EQ(*(arrayStruct.i.begin() + 1), 2);
	EXPECT_EQ(*(arrayStruct.f.begin() + 1), 2.0f);
}

TEST(utilTest, ArrayList) {
	std::mt19937 gen(1337); // standard mersenne_twister_engine seeded with 1337
	std::uniform_int_distribution<> distrib(0, array::count(strings) - 1);
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
				if (index >= list.count() || !(list[index] == s)) {
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
			for (int i = 0; i < list.count() - 1; ++i) {
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


// flags enum
// ----------

enum class Flags : uint8_t {
	FOO = 1,
	BAR = 2
};
FLAGS_ENUM(Flags);
std::ostream &operator <<(std::ostream &s, Flags flags) {
	switch (flags) {
	case Flags::FOO:
		s << "FOO";
		break;
	case Flags::BAR:
		s << "BAR";
		break;
	default:
		s << "<unknown>";
	}
	return s;
}

TEST(utilTest, flagsEnum) {
	Flags a = Flags::FOO;
	a |= Flags::BAR;
	EXPECT_EQ(a, Flags::FOO | Flags::BAR);

	a &= ~Flags::FOO;
	EXPECT_EQ(a, Flags::BAR);
}


// IsSubclass
// ----------

struct BaseClass {};
struct Subclass : public BaseClass {};

TEST(utilTest, IsSubclass) {
	EXPECT_EQ((IsSubclass<Subclass, BaseClass>::RESULT), 1);
	EXPECT_EQ((IsSubclass<Subclass &, BaseClass>::RESULT), 0);
	EXPECT_EQ((IsSubclass<Subclass *, BaseClass>::RESULT), 0);
	EXPECT_EQ((IsSubclass<BaseClass, Subclass>::RESULT), 0);
}


// LinkedListNode
// --------------

struct ElementBaseClass {
	int i;
};

struct MyListElement : public ElementBaseClass, public LinkedListNode {
	int foo;

	MyListElement(int foo) : foo(foo) {}
	MyListElement(LinkedList<MyListElement> &list, int foo) : LinkedListNode(list), foo(foo) {}
};

using MyList = LinkedList<MyListElement>;

TEST(utilTest, LinkedListNode) {
	// create list
	MyList list;
	EXPECT_TRUE(list.isEmpty());
	EXPECT_EQ(list.count(), 0);

	// create an element
	MyListElement element(10);
	EXPECT_FALSE(element.isInList());

	// add element to list
	list.add(element);
	EXPECT_FALSE(list.isEmpty());
	EXPECT_EQ(list.count(), 1);
	EXPECT_TRUE(element.isInList());

	// create another element and add to list in constructor
	MyListElement element2(list, 50);
	EXPECT_FALSE(list.isEmpty());
	EXPECT_EQ(list.count(), 2);
	EXPECT_TRUE(element.isInList());

	// iterate over elements
	int count = 0;
	for (auto &e : list) {
		EXPECT_EQ(e.foo, count == 0 ? 10 : 50);
		++count;
	}
	EXPECT_EQ(count, 2);

	// remove elements
	element.remove();
	EXPECT_EQ(list.count(), 1);
	EXPECT_FALSE(element.isInList());
	EXPECT_TRUE(element2.isInList());

	element2.remove();
	EXPECT_EQ(list.count(), 0);
	EXPECT_FALSE(element2.isInList());

	// call remove second time
	element.remove();
	element2.remove();
}


// Queue
// -----

TEST(utilTest, Queue) {
	using Q = Queue<int, 4>;
	Q queue;
	
	// check if initially empty
	EXPECT_TRUE(queue.isEmpty());
	
	// add and remove one element
	queue.addBack(5);
	EXPECT_EQ(queue.getBack(), 5);
	queue.removeFront();
	EXPECT_TRUE(queue.isEmpty());

	// add elements until queue is full
	int i = 0;
	int &head = queue.getNextBack();
	while (!queue.isFull()) {
		queue.addBack(1000 + i);
		++i;
	}
	EXPECT_EQ(i, 4);
	EXPECT_EQ(queue.count(), 4);
	EXPECT_EQ(head, 1000);

	// remove one element
	queue.removeFront();
	EXPECT_FALSE(queue.isEmpty());
	EXPECT_FALSE(queue.isFull());
	EXPECT_EQ(queue.count(), 3);
	EXPECT_EQ(queue.getFront(), 1000 + 1);
	
	// clear and "resurrect" first element (is used in radio driver)
	queue.clear();
	queue.addBack();
	EXPECT_EQ(queue.count(), 1);
	EXPECT_EQ(queue.getFront(), 1000 + 1);
}


// Stream
// ------

struct NoStream {
	float f;
	operator float() const {return this->f;}
};

class TestStream : public Stream {
public:
	Stream &operator <<(char ch) override {
		std::cout << "char " << ch << std::endl;
		return *this;
	}
	Stream &operator <<(String const &str) override {
		std::string s(str.data, str.count());
		std::cout << "char " << s << std::endl;
		return *this;
	}
	Stream &operator <<(Command command) override {
		std::cout << "char " << int(command) << std::endl;
		return *this;
	}
};

TEST(utilTest, Stream) {
	NoStream n{1.0f};
	TestStream s;
	auto a = 1.0f < n + 1.0f;
}


// String
// ------

TEST(utilTest, String) {
	// constructor from c-string
	{
		String foo("foo");
		EXPECT_EQ(foo, "foo");
		EXPECT_EQ(foo.length, 3);

		char const *cstr = "bar";
		EXPECT_EQ(length(cstr), 3);
		String bar(cstr);
		EXPECT_EQ(bar, "bar");
		EXPECT_EQ(bar.length, 3);
		
		// from r-value
		String bar2(reinterpret_cast<char const *>(cstr));
		EXPECT_EQ(bar2, "bar");
		EXPECT_EQ(bar2.length, 3);
	}
	
	// constructor from c-array
	{
		char ar[] = {'f', 'o', 'o'};
		char dummy = 'x';
		EXPECT_EQ(length(ar), 3);
		String foo(ar);
		EXPECT_EQ(foo, "foo");
		EXPECT_EQ(foo.length, 3);
	}

	// less operator
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
	b += flt(0.0f);
	b += " ";
	b += flt(0.0f, 0, 2);
	b += " ";
	b += flt(0.001234567f, 9);
	b += " ";
	b += flt(0.5f) + ' ';
	b += flt(0.5f, 0, 2) + " ";
	b += flt(1.0f, 1);
	b += ' ' + flt(1.0f, -1);
	b += " " + flt(-5.9f);
	b += " " + flt(100.9999f) + " " + flt(2000000000);
	EXPECT_EQ(b.string(), "123456 -099 5 abcdef12 0 0 0.001234567 0.5 .5 1 1.0 -5.9 101 2000000000");

	StringBuffer<5> c = flt(0.000000001f, 9);
	EXPECT_EQ(c.string(), "0.000");

	StringBuffer<32> d = 'x' + hex(0xff) + String("foo") + dec(50) + '-';
	EXPECT_EQ(d.string(), "x000000fffoo50-");
}

TEST(utilTest, StringSet) {
	std::mt19937 gen(1337); // standard mersenne_twister_engine seeded with 1337
	std::uniform_int_distribution<> distrib(0, array::count(strings) - 1);

	// number of elements to add to set
	for (int count = 1; count < 7; ++count) {
		// do several testing rounds
		for (int round = 0; round < count * count; ++round) {
			StringSet<20, 1024> set;
            std::set<String> stdSet;
			EXPECT_TRUE(set.isEmpty());

			// add elements to set
			for (int i = 0; i < count; ++i) {
				String s = strings[distrib(gen)];
				set.add(s);
                stdSet.insert(s);
			}
			
			// print list
			/*
			std::cout << std::endl;
			for (String s : set) {
				std::cout << s << std::endl;
			}
			*/
			
            // check if both sets have the same number of values
            EXPECT_EQ(set.count(), stdSet.size());
            
            // check if both sets contain the same number of elements
            auto it1 = set.begin();
            auto it2 = stdSet.begin();
            for (; it1 != set.end(); ++it1, ++it2) {
                EXPECT_EQ(*it1, *it2);
            }
            
			// check if the set contains unique sorted elements
			for (int i = 0; i < set.count() - 1; ++i) {
				EXPECT_TRUE(set[i] < set[i + 1]);
			}
		}
	}
}

TEST(utilTest, StringHash) {
	std::mt19937 gen(1337); // standard mersenne_twister_engine seeded with 1337
	std::uniform_int_distribution<> distrib(0, array::count(strings) - 1);

	// number of elements to add to set
	for (int count = 1; count < 7; ++count) {
		// do several testing rounds
		for (int round = 0; round < count * count; ++round) {
			StringHash<128, 100, 1024, int> hash;
			std::map<String, std::pair<int, int>> stdMap;
			EXPECT_TRUE(hash.isEmpty());

			// add elements to set
			for (int i = 0; i < count; ++i) {
				int index = distrib(gen);
				String s = strings[index];
				int location = hash.getOrPut(s, [index]() {return index;});
				stdMap[s] = std::make_pair(location, index);
			}
	
			// compare contents to std::map
			for (int i = 0; i < array::count(strings); ++i) {
				String s = strings[i];
				auto it = hash.find(s);
				auto it2 = stdMap.find(s);
				if (it2 == stdMap.end()) {
					EXPECT_EQ(-1, hash.locate(s));
					EXPECT_EQ(hash.end(), it);
				} else {
					EXPECT_EQ(it2->second.first, hash.locate(s));
					EXPECT_EQ(it2->second.second, it->value);
				}
			}
			EXPECT_EQ(stdMap.size(), hash.count());

			// iterate over all entries in the hash
			int count = 0;
			for (auto const [key, value] : hash) {
				// key is a const value
				static_assert(!std::is_reference<decltype(key)>::value);
				static_assert(std::is_const<decltype(key)>::value);

				// value is a non-const reference
				static_assert(std::is_reference<decltype(value)>::value);
				static_assert(!std::is_const<decltype(value)>::value);
				
				EXPECT_EQ(stdMap[key].second, value);
				++count;
			}
			EXPECT_EQ(hash.count(), count);
		}
	}
	
}


// Topic
// -----

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


// UInt
// ----
TEST(utilTest, UInt) {
	static_assert(sizeof(UInt<256>::Type) == 1);
	static_assert(sizeof(UInt<257>::Type) == 2);
	static_assert(sizeof(UInt<65536>::Type) == 2);
	static_assert(sizeof(UInt<65537>::Type) == 4);
}


// return structures
// -----------------

struct Return {
	int i;

	Return() {
		std::cout << "Default" << std::endl;
	}

	Return(int i) : i(i) {
		std::cout << "Constructor" << std::endl;
	}

	Return(const Return &) = delete;

	Return(Return &&foo) : i(foo.i) {
		std::cout << "Move Constructor" << std::endl;
	}

	Return &operator =(Return &&foo) {
		std::cout << "Move Assignment" << std::endl;
		this->i = foo.i;
		return *this;
	}
};

Return return1() {
	return {50};
}

Return return2() {
	return return1();
}

Return return3() {
	Return foo = return2();
	return foo;
}

TEST(utilTest, Return) {
	{
		std::cout << "Variant 1" << std::endl;
		Return r = return2();
	}
	{
		std::cout << "Variant 2" << std::endl;
		Return r = return3();
	}
	{
		std::cout << "Variant 3" << std::endl;
		Return r[2];
		r[0] = std::move(return3());
	}
}


// Coroutine
// ---------

// helper object for coroutine tests
struct Object {
	Object(char const *name) : name(name) {
		std::cout << "enter " << name << std::endl;
	}
	~Object() {
		std::cout << "exit " << name << std::endl;
	}
	char const *name;
};

// wait lists
Waitlist<> waitlist1;
Waitlist<> waitlist2;

// wait functions
Awaitable<> wait1() {
	return {waitlist1};
}
Awaitable<> wait2() {
	return {waitlist2};
}

int selectResult = 0;

Coroutine coroutine() {
	// an object whose destructor gets called when the coroutine ends or gets cancelled
	Object o("coroutine()");

	std::cout << "!wait1" << std::endl;
	co_await wait1();
	std::cout << "!wait2" << std::endl;
	co_await wait2();
}

AwaitableCoroutine inner() {
	Object o("inner()");

	std::cout << "!inner wait1" << std::endl;
	co_await wait1();
	std::cout << "!inner wait2" << std::endl;
	co_await wait2();
}

Coroutine outer() {
	Object o("outer()");

	co_await inner();

	std::cout << "!outer select" << std::endl;
	int result = co_await select(wait1(), wait2());
	selectResult = result;
	switch (result) {
	case 1:
		std::cout << "selected 1" << std::endl;
		break;
	case 2:
		std::cout << "selected 2" << std::endl;
		break;
	}
}

TEST(utilTest, Coroutine) {
	// start coroutine
	coroutine();

	// resume coroutine
	std::cout << "!resume wait1" << std::endl;
	EXPECT_FALSE(waitlist1.isEmpty());
	waitlist1.resumeAll();
	EXPECT_TRUE(waitlist1.isEmpty());
	std::cout << "!resume wait2" << std::endl;
	EXPECT_FALSE(waitlist2.isEmpty());
	waitlist2.resumeAll();
	EXPECT_TRUE(waitlist2.isEmpty());
}

TEST(utilTest, DestroyCoroutine) {
	// start coroutine
	Coroutine c = coroutine();

	// destroy coroutine
	c.destroy();
}

TEST(utilTest, NestedCoroutine) {
	// start outer coroutine
	outer();

	// resume inner coroutine
	std::cout << "!resume inner wait1" << std::endl;
	waitlist1.resumeAll();
	std::cout << "!resume inner wait2" << std::endl;
	waitlist2.resumeAll();

	std::cout << "!resume outer wait2" << std::endl;
	EXPECT_FALSE(waitlist2.isEmpty());
	waitlist2.resumeAll();
	EXPECT_TRUE(waitlist2.isEmpty());
	EXPECT_EQ(selectResult, 2);
}

TEST(utilTest, DestroyNestedCoroutine) {
	// start outer coroutine (enters inner() and waits on wait1())
	Coroutine c = outer();
	//Coroutine c = coroutine2();

	// destroy outer coroutine
	std::cout << "!destroy outer" << std::endl;
	c.destroy();
}

TEST(utilTest, AwaitAwaitableCoroutine) {
	AwaitableCoroutine c = inner();

	// check that the coroutine is alive
	EXPECT_TRUE(c.isAlive());
	EXPECT_FALSE(c.hasFinished());
	EXPECT_FALSE(c.await_ready());

	// move assign to c2
	AwaitableCoroutine c2;
	c2 = std::move(c);

	// check that the c now reports ready
	EXPECT_TRUE(c.await_ready());

	// check that the coroutine is still running
	EXPECT_FALSE(c2.await_ready());

	// resume the coroutine
	waitlist1.resumeAll();
	waitlist2.resumeAll();

	// check that the coroutine has stopped
	EXPECT_TRUE(c2.await_ready());

	// should have no effect
	c.cancel();


	// start again and assign to finished c2
	c2 = inner();

	// start again and assign to c2 when it is still alive
	c2 = inner();

	// cancel the coroutine
	c2.cancel();

	// check that the coroutine has stopped
	EXPECT_TRUE(c2.await_ready());
}


// Coroutine with parameters
// -------------------------

class Parameters1 : public WaitlistNode {
public:
	// default constructor
	explicit Parameters1(int value) : value(value) {}

	void append(WaitlistNode &list) {
		std::cout << "Parameters::append" << std::endl;
		list.add(*this);
	}

	void cancel() {
		std::cout << "Parameters::cancel" << std::endl;
		remove();
	}

	// handle of waiting coroutine
	std::coroutine_handle<> handle = nullptr;

	int value;
};

Waitlist<Parameters1> waitListValue;

Awaitable<Parameters1> delay2(int value) {
	return {waitListValue, value};
}

Coroutine waitForDelay1() {
	Object o("waitForDelay1()");
	co_await delay2(5);
	std::cout << "resumed delay(5)" << std::endl;
}

Coroutine waitForDelay2() {
	Object o("waitForDelay2()");
	switch (co_await select(wait1(), delay2(10))) {
	case 1:
		std::cout << "resumed wait1" << std::endl;
		break;
	case 2:
		std::cout << "resumed delay(10)" << std::endl;
		break;
	}
}

TEST(utilTest, CoroutineValue2) {
	waitForDelay1();
	waitForDelay2();

	waitListValue.resumeAll([] (Parameters1 const &p) {return p.value == 5;});
	waitListValue.resumeAll([] (Parameters1 const &p) {return p.value == 10;});
}


// Awaitable move constructor
// --------------------------

Coroutine move() {
	Awaitable<> a = wait1();
	Awaitable<> b(std::move(a));

	EXPECT_TRUE(a.hasFinished());
	co_await a;

	std::cout << "await b" << std::endl;
	co_await b;
	std::cout << "resumed from b" << std::endl;

	EXPECT_TRUE(a.hasFinished());
	EXPECT_TRUE(b.hasFinished());
}

TEST(utilTest, AwaitableMove) {
	move();

	waitlist1.resumeAll();
	EXPECT_TRUE(waitlist1.isEmpty());
}


// Barrier
// -------

Barrier barrier;

Coroutine waitForBarrier() {
	std::cout << "wait on barrier" << std::endl;
	co_await barrier.wait();
	std::cout << "passed barrier" << std::endl;
}

TEST(utilTest, Barrier) {
	waitForBarrier();
	std::cout << "resume barrier" << std::endl;
	barrier.resumeAll();
}

struct BarrierParameters {
	int i;
	float f;
};
Barrier<BarrierParameters> barrierWithParameters;

Coroutine waitForBarrierWithParameters() {
	std::cout << "wait on barrier" << std::endl;
	co_await barrierWithParameters.wait(1, 2.0f);
	std::cout << "passed barrier" << std::endl;
}

TEST(utilTest, BarrierWithParameters) {
	waitForBarrierWithParameters();
	std::cout << "resume barrier" << std::endl;
	barrierWithParameters.resumeAll([](BarrierParameters &p) {
		EXPECT_EQ(p.i, 1);
		return true;
	});
}


struct Resumer {
	Resumer(Barrier<> &barrier) : barrier(barrier) {}
	~Resumer() {
		std::cout << "Resumer::~Resumer()" << std::endl;
		this->barrier.resumeFirst();
	}
	Barrier<> &barrier;
};

Barrier<> resumeAfterReturnBarrier;
Waitlist<int> resumeAfterReturnList;

Coroutine resumedAfterReturn() {
	co_await resumeAfterReturnBarrier.wait();
	resumeAfterReturnList.resumeFirst([](int i) {
		std::cout << "resumed after return " << i << std::endl;
		return true;
	});
}

Awaitable<int> resumeAfterReturn(int i) {
	Resumer r(resumeAfterReturnBarrier);
	return {resumeAfterReturnList, i};
	// destructor of Resumer resumes the barrier here
}

TEST(utilTest, ResumeBarrierAfterReturn) {
	resumedAfterReturn();
	auto awaitable = resumeAfterReturn(50);
	EXPECT_TRUE(awaitable.hasFinished());
}


// Semaphore
// ---------

Semaphore semaphore(3);

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
	auto c1 = worker1();
	auto c2 = worker2();
	for (int i = 0; i < 10; ++i) {
		semaphore.post();
	}

	// destroy coroutines so that no coroutine waits on the semaphore when it gets destroyed
	c1.destroy();
	c2.destroy();
}

TEST(utilTest, color) {
	auto xy1 = hueToCie(0.0f, 0.0f);
	auto xy2 = hueToCie(0.0f, 1.0f);
	auto xy3 = hueToCie(359.0f, 1.0f);
}

TEST(utilTest, endian) {
	EXPECT_EQ(u32B(0x44332211), htonl(0x44332211));
}

int main(int argc, char **argv) {
	testing::InitGoogleTest(&argc, argv);
	int success = RUN_ALL_TESTS();	
	return success;
}
