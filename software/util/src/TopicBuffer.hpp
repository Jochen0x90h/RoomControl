#pragma once

#include "String.hpp"
#include "convert.hpp"
#include "util.hpp"


template <typename A, typename B>
struct Div {
	A a;
	B b;
};

inline Div<String, String> operator /(String a, String b) {
	return {a, b};
}

template <typename AA, typename AB>
Div<Div<AA, AB>, String> operator /(Div<AA, AB> a, String b) {
	return {a, b};
}



/**
 * Buffer constructing mqtt topics
 */
class TopicBuffer {
public:
	
	// maximum length of prefix such as "stat" or "cmnd"
	static constexpr int MAX_PREFIX_LENGTH = 4;
	
	// maximum length of topic
	static constexpr int MAX_TOPIC_LENGTH = 32;


	TopicBuffer() : index(0) {
		#ifdef DEBUG
		array::fill(this->data, this->data + MAX_PREFIX_LENGTH, ' ');
		#endif
	}

	template <typename T>
	TopicBuffer(T str) : index(0) {
		#ifdef DEBUG
		array::fill(this->data, this->data + MAX_PREFIX_LENGTH, ' ');
		#endif
		(*this) /= str;
	}

	template <typename T>
	TopicBuffer &operator =(T str) {
		this->index = 0;
		return (*this) /= str;
	}

	TopicBuffer &operator /=(String str);

	template <typename A, typename B>
	TopicBuffer &operator /=(Div<A, B> const &div) {
		(*this) /= div.a;
		return (*this) /= div.b;
	}
	
	/**
	 * Get topic as string without prefix. Stays valid until clear() or removeLast() are called
	 */
	String string() {
		return {max(this->index - 1, 0), this->data + MAX_PREFIX_LENGTH + 1};
	}
	
	/**
	 * Get topic as string with state prefix. Stays valid until clear(), removeLast() or other prefix method are called
	 */
	String state();

	/**
	 * Get topic as string with command prefix. Stays valid until clear(), removeLast() or other prefix method are
	 * called
	 */
	String command();

	/**
	 * Get topic as string with enumeration prefix. Stays valid until clear(), removeLast() or other prefix method are
	 * called
	 */
	String enumeration();

	void clear() {
		this->index = 0;
		#ifdef DEBUG
		this->data[0] = 0;
		#endif
	}

	bool isEmpty() {return this->index == 0;}

	/**
	 * Remove last topic element (e.g. "foo/bar" -> "foo")
	 */
	void removeLast();

	/**
	 * Get number of topic elements
	 * @return number of topic elements
	 */
	int getElementCount();

protected:

	#ifdef DEBUG
	char data[MAX_PREFIX_LENGTH + MAX_TOPIC_LENGTH + 1];
	#else
	char data[MAX_PREFIX_LENGTH + MAX_TOPIC_LENGTH];
	#endif
	int index;
};
