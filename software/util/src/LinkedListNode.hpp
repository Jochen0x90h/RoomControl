#pragma once


/**
 * Linked list node that list elements inherit from
 * @tparam T list element that inherits LinkedListNode, e.g class Element : public LinkedListNode<Element>
 */
template <typename T>
struct LinkedListNode {
	T *next;
	T *prev;
	
	/**
	 * Construct an empty list or an element that is "not in list"
	 */
	LinkedListNode() noexcept {
		this->next = this->prev = static_cast<T *>(this);
	}

	/**
	 * Construct a new list element and add to list
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
	 * Return true if the node is an empty list head
	 */
	bool isEmpty() const {
		return this->next == this;
	}

	/**
	 * Return true if the node part of a list
	 */
	bool isInList() const {
		return this->next != this;
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
	 * Add one or multiple nodes at the end of the list (assuming this node is the list head)
	 * @param node node to add, can be part of a "ring" of nodes
	 */
	void add(T &node) {
		auto p = node.prev;
		node.prev->next = static_cast<T *>(this);
		node.prev = this->prev;
		this->prev->next = &node;
		this->prev = p;
/*		node.next = static_cast<T *>(this);
		node.prev = this->prev;
		this->prev->next = &node;
		this->prev = &node;*/
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
		this->next = this->prev = static_cast<T *>(this);
	}
	
	
	struct Iterator {
		T *node;
		T &operator *() {return *this->node;}
		T *operator ->() {return this->node;}
		Iterator &operator ++() {this->node = this->node->next; return *this;}
		bool operator ==(Iterator it) const {return this->node == it.node;}
		bool operator !=(Iterator it) const {return this->node != it.node;}
	};
	
	Iterator begin() {return {this->next};}
	Iterator end() {return {static_cast<T *>(this)};}
};
