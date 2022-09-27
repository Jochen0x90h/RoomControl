#pragma once


/**
 * Linked list node for list head and elements
 */
class LinkedListNode {
public:
	/**
	 * Construct an empty list or an element that is "not in list"
	 */
	LinkedListNode() noexcept {
		this->next = this->prev = this;
	}

	/**
	 * Construct a new list element and add to given list
	 */
	LinkedListNode(LinkedListNode &list) {
		this->next = &list;
		this->prev = list.prev;
		list.prev->next = this;
		list.prev = this;
	}

	/**
	 * Delete copy constructor
	 */
	LinkedListNode(LinkedListNode const &) = delete;

	/**
	 * Destructor removes the node from the list
	 */
	~LinkedListNode() {
		// remove this element from the list
		this->next->prev = this->prev;
		this->prev->next = this->next;
	}

	/**
	 * Return true if the node part of a list
	 */
	bool isInList() const {
		return this->next != this;
	}

	/**
	 * Add one or more nodes at end of list if this is the list head
	 * @param list list to insert
	 */
/*	void insert(LinkedListNode &list) {
		auto p = list.prev;
		list.prev->next = static_cast<T *>(this);
		list.prev = this->prev;
		this->prev->next = static_cast<T *>(&list);
		this->prev = p;
	}*/

	/**
	 * Remove this element from the list
	 */
	void remove() noexcept {
		this->next->prev = this->prev;
		this->prev->next = this->next;

		// set to "not in list"
		this->next = this;
		this->prev = this;
	}


	LinkedListNode *next;
	LinkedListNode *prev;
};

/**
 * Linked list. The list elements must inherit from LinkedListNode
 * @tparam T list element type that inherits LinkedListNode, e.g class Element : public LinkedListNode {};
 */
template <typename T>
class LinkedList : public LinkedListNode {
public:
	/**
	 * Return true if the node is an empty list head
	 */
	bool isEmpty() const {
		return this->next == this;
	}

	/**
	 * Return the number of entries in the list
	 * @return number of entries
	 */
	int count() const {
		int count = 0;
		auto node = this->next;
		while (node != this) {
			node = node->next;
			++count;
		}
		return count;
	}

	/**
	 * Add one or multiple elements at the end of the list
	 * @param node element to add, can be part of a "ring" of nodes
	 */
	void add(T &node) {
		auto p = node.prev;
		node.prev->next = this;
		node.prev = this->prev;
		this->prev->next = &node;
		this->prev = p;
	}

	/**
	 * Add one list to another, take care to remove the other list from the "ring" of nodes afterwards
	 * @param node list to add
	 */
	void add(LinkedList &node) {
		auto p = node.prev;
		node.prev->next = this;
		node.prev = this->prev;
		this->prev->next = &node;
		this->prev = p;
	}

	/**
	 * Iterator. Do not remove() an element that an iterator points to
	 */
	struct Iterator {
		LinkedListNode *node;
		T &operator *() {return *static_cast<T *>(this->node);}
		T *operator ->() {return static_cast<T *>(this->node);}
		Iterator &operator ++() {this->node = this->node->next; return *this;}
		bool operator ==(Iterator it) const {return this->node == it.node;}
		bool operator !=(Iterator it) const {return this->node != it.node;}
	};

	Iterator begin() {return {this->next};}
	Iterator end() {return {this};}
};
