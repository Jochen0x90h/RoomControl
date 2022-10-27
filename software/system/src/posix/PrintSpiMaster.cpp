#include "PrintSpiMaster.hpp"
#include <Terminal.hpp>
#include <StringOperators.hpp>



Awaitable<SpiMaster::Parameters> PrintSpiMaster::transfer(int writeCount, void const *writeData, int readCount, void *readData) {
	this->remove();
	this->time = Loop::now() + 100ms; // emulate 100ms transfer time
	Loop::timeouts.add(*this);
	return {this->waitlist, nullptr, writeCount, writeData, readCount, readData};
}

void PrintSpiMaster::transferBlocking(int writeCount, void const *writeData, int readCount, void *readData) {
	Terminal::out << str(this->name.c_str()) << ' ';
	if (writeCount < 0)
		Terminal::out << "command ";
	Terminal::out << dec(writeCount & 0x7fffffff) << ' ' << dec(readCount) << '\n';
}

void PrintSpiMaster::activate() {
	this->remove();

	// resume all coroutines
	this->waitlist.resumeAll([this](Parameters &p) {
		transferBlocking(p.writeCount, p.writeData, p.readCount, p.readData);
		return true;
	});
}
