#pragma once


template <typename T>
struct LinkedListNode {
	T *next;
	T *prev;
	
	/**
	 * Construct an empty list or an element that is "not in list"
	 */
	LinkedListNode() {
		this->next = this->prev = reinterpret_cast<T *>(this);
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
	 * Return true if the node is an empty list head or if an element is "not in list"
	 */
	bool isEmpty() const {
		return this->next == this;
	}

	/**
	 * Add a node before this node (add at end of list if this is the list head)
	 */
	void add(T &node) {
		node.next = reinterpret_cast<T *>(this);
		node.prev = this->prev;
		this->prev->next = &node;
		this->prev = &node;
	}

	/**
	 * Remove this element from the list
	 */
	void remove() noexcept {
		this->next->prev = this->prev;
		this->prev->next = this->next;

		// set to "not in list"
		this->next = this->prev = reinterpret_cast<T *>(this);
	}
	
	
	struct Iterator {
		T *node;
		T &operator *() {return *this->node;}
		Iterator &operator ++() {this->node = this->node->next; return *this;}
		bool operator ==(Iterator it) const {return this->node == it.node;}
		bool operator !=(Iterator it) const {return this->node != it.node;}
	};
	
	Iterator begin() {return {this->next};}
	Iterator end() {return {reinterpret_cast<T *>(this)};}
};
