#pragma once

#include <assert.hpp>
#include <Coroutine.hpp>
#include <appConfig.hpp>
#include <spi.hpp>
#include <defines.hpp>
#include <util.hpp>
#include <LinkedListNode.hpp>

//#define STATE_DEBUG_PRINT
#ifdef STATE_DEBUG_PRINT
#include <iostream>
#endif



/*
struct TaskQueue {
	// list of tasks that wait for processing
	LinkedList list;
	
	// barrier for workers that wait for a new task
	Barrier<> barrier;
};
*/

struct PersistentStateManager;

struct PersistentStateBase : public LinkedListNode<PersistentStateBase> {
	PersistentStateManager *manager;

	uint8_t size __attribute__ ((aligned (4)));
	uint8_t command[3];
	
	// size of PersistentStateBase must be a multiple of 4

	PersistentStateBase(PersistentStateManager *manager, uint8_t size, uint8_t command1, uint8_t command2)
		: manager(manager)
		, size(size)
	{
		this->command[1] = command1;
		this->command[2] = command2;
	}
};

/**
 * Manager for persistent states such as security counters
 */
class PersistentStateManager {
	template <typename T>
	friend class PersistentState;
public:

	PersistentStateManager() : allocationTable{} {
		// start coroutine
		updater();
	}

	/**
	 * Allocate a new persistent state. Only call this after all existing persistent states have been restored
	 * @tparam T type of state to allocate. Maximum size is 4 (int, float)
	 */
	template <typename T>
	int allocate() {
		return allocateInternal(sizeof(T));
	}

protected:

	// coroutine that waits for state changes and makes them persistent
	Coroutine updater();
	
	int allocateInternal(int size);

	AwaitableCoroutine restore(PersistentStateBase &state);

	uint32_t allocationTable[FRAM_SIZE / 8 / 32];

	// queue of state that need to be stored to the persistent memory
	//TaskQueue queue;
	
	// list of tasks that wait for processing
	LinkedListNode<PersistentStateBase> list;
	
	// barrier for workers that wait for a new task
	Barrier<> barrier;
};




/**
 * Persistent state
 * @tparam T type of state. Maximum type size is 4
 */
template <typename T>
class PersistentState : protected PersistentStateBase {
	friend class PersistentStateManager;
public:

	/**
	 * Constructor
	 * @param manager the state manager to use
	 * @param offset offset in non-volatile memory
	 * @param value initial value
	 */
	PersistentState(int offset, T const &value = T())
		: PersistentStateBase(nullptr, sizeof(T), offset >> 8, uint8_t(offset))
		, value(value)
		, counter(0x55)
	{
		//this->command[1] = offset >> 8;
		//this->command[2] = offset;
	}

	/**
	 * Move constructor
	 */
	PersistentState(PersistentState &&state)
		: PersistentStateBase(state.manager, sizeof(T), state.command[1], state.command[2])
		//, command(state.command)
		, value(std::move(state.value))
		, counter(state.counter)
	{
		// check if the other state is dirty
		if (!state.isEmpty()) {
			state.remove();
			update();
		}
	
		// mark other state as invalid (only if assert is active)
		assert((state.manager = nullptr, true));
	}

	/**
	 * Destructor
	 */
	//~PersistentState() {
	//	remove();
	//}

	/**
	 * Restore state from non-volatile memory
	 */
	AwaitableCoroutine restore(PersistentStateManager *manager) {
		assert(this->manager == nullptr);
		this->manager = manager;
		return manager->restore(*this);
	}

	PersistentState &operator =(T const &value) {
		this->value = value;
		update();
		return *this;
	}
	
	operator T() const {
		return this->value;
	}
	
	PersistentState &operator ++() {
		++this->value;
		update();
		return *this;
	}

	T const operator ++(int) {
		T value = this->value++;
		update();
		return value;
	}

	PersistentState &operator --() {
		--this->value;
		update();
		return *this;
	}

	T const operator --(int) {
		T value = this->value--;
		update();
		return value;
	}

protected:
	
	void update() {
		assert(this->manager != nullptr);
		
		// check if not yet in dirty list
		if (this->isEmpty()) {
			// add to dirty list
			this->manager->list.add(*this);
			
			// resume coroutine of manager
			this->manager->barrier.resumeFirst();
		}
	}
/*
	PersistentStateManager *manager;

	uint8_t size;
	uint8_t command[3];
*/
	// these fields are expected to be at the same offset independent of type T
	T value;
	uint8_t counter;
};



//
/*
#include "Message.hpp"


struct PublishQueue {
	TaskQueue *queue;
	uint16_t id;
};
*/

/**
 * Publish state
 * @tparam T type of state. Maximum type size is 4
 */
 /*
template <typename T>
class PublishState : public LinkedListNode {
	friend class PublishStateManager;
public:

	/ **
	 * Constructor
	 * @param value initial value
	 * /
	PublishState(MessageType type, T const &value = T())
		: queue(nullptr)
		, id()
		, type(type)
		//, size(sizeof(T))
		, value(value)
	{
	}

	/ **
	 * Constructor
	 * @param queue the publish queue to use
	 * @param value initial value
	 * /
	PublishState(PublishQueue queue, MessageType type, T const &value = T())
		: queue(queue.queue)
		, id(queue.id)
		, type(type)
		//, size(sizeof(T))
		, value(value)
	{
	}

	/ **
	 * Copy constructor
	 * /
	PublishState(PublishState const &state)
		: queue(state.queue)
		, id(state.id)
		, type(state.type)
		//, size(sizeof(T))
		, value(std::move(state.value))
	{
		// check if the other state is dirty
		if (!state.isEmpty()) {
			update();
		}
	}

	/ **
	 * Destructor
	 * /
	//~PublishState() {
	//	remove();
	//}

	void setQueue(PublishQueue queue) {
		this->queue = queue.queue;
		this->id = queue.id;
	}

	PublishState &operator =(T const &value) {
		this->value = value;
		update();
		return *this;
	}
	
	operator T() const {
		return this->value;
	}
	
	PublishState &operator ++() {
		++this->value;
		update();
		return *this;
	}

	T const operator ++(int) {
		T value = this->value++;
		update();
		return value;
	}

	PublishState &operator --() {
		--this->value;
		update();
		return *this;
	}

	T const operator --(int) {
		T value = this->value--;
		update();
		return value;
	}

protected:
	
	void update() {
		// check if not yet in dirty list
		if (this->queue != nullptr && this->isEmpty()) {
			// add to dirty list
			this->queue->list.add(*this);
			
			// resume coroutine of manager
			this->queue->barrier.resumeFirst();
		} else {
			// mark dirty in case the state is in a separate list waiting for acknowledge
			//this->timer |= 0x80;
		}
	}
		
	TaskQueue *queue;

public:
	// these fields are expected to be at the same offset independent of type T
	uint16_t id;
	MessageType type;
	//uint8_t timer = 0;
	//uint8_t counter = 0;
	//uint8_t size;
	T value __attribute__ ((aligned (4)));
};
*/
