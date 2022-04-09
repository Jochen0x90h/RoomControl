#include "SwapChain.hpp"


Bitmap<DISPLAY_WIDTH, DISPLAY_HEIGHT> *SwapChain::get() {
	Bitmap<DISPLAY_WIDTH, DISPLAY_HEIGHT> *r;
	
	// check if there is a bitmap waiting to be rendered
	if (this->showList != null) {
		// we "cancel" the bitmap waiting to be rendered and return it
		r = this->showList;
		this->showList = nullptr;
	} else {
		// assert that free list is not empty
		assert(this->freeHead > this->freeTail);
		r = this->freeList[this->freeTail];
		
		if (++this->freeTail == 2) {
			this->freeTail = 0;
			this->freeHead -= 2;
		}
	}
	
	// clear bitmap
	r->clear();
	return r;
}

void SwapChain::put(Bitmap<DISPLAY_WIDTH, DISPLAY_HEIGHT> *bitmap) {
	assert(bitmap != nullptr);
	assert(this->freeHead < this->freeTail + 2);
	this->freeList[this->freeHead & 1] = bitmap;
	++this->freeHead;
}

void SwapChain::show(Bitmap<DISPLAY_WIDTH, DISPLAY_HEIGHT> *bitmap) {
	assert(bitmap != nullptr);
	assert(this->showList == nullptr);
	this->showList = bitmap;
	this->barrier.resumeFirst();
}

Coroutine SwapChain::transfer() {
	// initialize and enable display
	co_await this->display.init();
	co_await this->display.enable();

	while (true) {
		while (this->showList != nullptr) {
			auto bitmap = this->showList;
			this->showList = nullptr;
			
			// transfer bitmap to display (a new bitmap can be added to the render list while waiting)
			co_await this->display.set(*bitmap);
			
			// but bitmap back to free list
			put(bitmap);
		}

		co_await this->barrier.wait();		
	}
}
