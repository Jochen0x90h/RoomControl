#include "SpiMasterImpl.hpp"
#include <Terminal.hpp>
#include <StringOperators.hpp>



Awaitable<SpiMaster::Parameters> SpiMasterImpl::transfer(int writeCount, void const *writeData, int readCount, void *readData) {
	if (!isInList()) {
		this->time = Loop::now() + 100ms; // emulate 100ms transfer time
		Loop::timeHandlers.add(*this);
	}
	return {this->waitlist, nullptr, writeCount, writeData, readCount, readData};
}

void SpiMasterImpl::transferBlocking(int writeCount, void const *writeData, int readCount, void *readData) {
	Terminal::out << str(this->name.c_str()) << ' ';
	if (writeCount < 0)
		Terminal::out << "command ";
	Terminal::out << dec(writeCount & 0x7fffffff) << ' ' << dec(readCount) << '\n';
}

void SpiMasterImpl::activate() {
	this->remove();

	// resume all coroutines
	this->waitlist.resumeAll([this](Parameters &p) {
		transferBlocking(p.writeCount, p.writeData, p.readCount, p.readData);
		return true;
	});
}
