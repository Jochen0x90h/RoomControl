#pragma once


/**
 * Check if a type is a subclass of a given class, e.g. IsSubclass<Foo, Bar>::Result
 * @tparam T type to check if it derives from base class B
 * @tparam B base class
 */
template <typename T, class B>
class IsSubclass {
	class No {};
	class Yes {No no[3];};

	static Yes test(B *);
	static No test(...);
public:
	enum {RESULT = sizeof(test(static_cast<T*>(0))) == sizeof(Yes)};
};

// specialization for T being a reference which is not derived from B
template <typename T, class B>
class IsSubclass<T &, B> {
public:
	enum {RESULT = 0};
};
