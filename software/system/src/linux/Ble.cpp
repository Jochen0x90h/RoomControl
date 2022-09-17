#include "../Ble.hpp"
#include "../posix/Loop.hpp"
#include "Timer.hpp"
#include "bt.hpp"
#include <boardConfig.hpp>
#include <cerrno>
#include <poll.h>
#include <bluetooth/bluetooth.h>
#include <bluetooth/l2cap.h>
#include <unistd.h>
#include <fcntl.h>


namespace Ble {

constexpr int attCid = 4;

char const *results[] = {"E8:85:47:17:BF:5A"};

// emulates a scanner
class Scanner : public Loop::Timeout {
public:
	void activate() override {
		auto time = this->time;
		this->time += 1s;

		// resume first coroutine
		this->waitlist.resumeFirst([this, time](ScanParameters &p) {
			bdaddr_t bdaddr;
			str2ba(Ble::results[this->index], &bdaddr);
			array::copy(p.result->address.u8, bdaddr.b);
			p.result->address.type = Address::Type::Random;

			this->index = (this->index + 1) % array::count(Ble::results);
			return true;
		});

		// remove from event loop if nobody listens any more
		if (this->waitlist.isEmpty()) {
			remove();
		}
	}

	// waiting coroutines
	Waitlist<ScanParameters> waitlist;

	// emulated result index
	int index = 0;
};

Scanner scanner;

class Context : public Loop::FileDescriptor {
public:

	Waitlist<ReceiveParameters> receiveWaitlist;
	Waitlist<SendParameters> sendWaitlist;


	void activate(uint16_t events) override {
		if (events & POLLIN) {
			this->receiveWaitlist.resumeFirst([this](ReceiveParameters &p) {
				// receive
				auto receivedCount = read(this->fd, p.data, *p.length);
				if (receivedCount >= 0) {
					*p.length = receivedCount;
					return true;
				}
				return false;
			});
			if (this->receiveWaitlist.isEmpty())
				this->events &= ~POLLIN;
		}
		if (events & POLLOUT) {
			this->sendWaitlist.resumeFirst([this](SendParameters &p) {
				// send
				auto sentCount = write(this->fd, p.data, p.length);
				return sentCount >= 0;
			});
			if (this->sendWaitlist.isEmpty())
				this->events &= ~POLLOUT;
		}
	}
};

bool inited = false;
Context contexts[BLUETOOTH_CONTEXT_COUNT];

void init() {
	Ble::inited = true;
}

Awaitable<ScanParameters> scan(ScanResult &result) {
	// add to event loop if necessary
	if (!Ble::scanner.isInList()) {
		Ble::scanner.time = Timer::now();
		Loop::timeouts.add(Ble::scanner);
	}

	// add to wait list
	return {Ble::scanner.waitlist, &result};
}

bool open(int index, Address const &address) {
	assert(Ble::inited);
	assert(uint(index) < NETWORK_CONTEXT_COUNT);
	auto &context = Ble::contexts[index];

	// create socket
	assert(context.fd == -1);
	int fd = socket(PF_BLUETOOTH, SOCK_SEQPACKET, BTPROTO_L2CAP);

	// bind
	sockaddr_l2 srcaddr = {
		.l2_family = AF_BLUETOOTH,
		.l2_cid = htobs(attCid)};
	if (bind(fd, (sockaddr *)&srcaddr, sizeof(srcaddr)) < 0) {
		::close(fd);
		return false;
	}

	// set security level
	struct bt_security btsec = {.level = BT_SECURITY_LOW};
	if (setsockopt(fd, SOL_BLUETOOTH, BT_SECURITY, &btsec, sizeof(btsec)) != 0) {
		::close(fd);
		return false;
	}

	// destination address
	sockaddr_l2 dstaddr = {
		.l2_family = AF_BLUETOOTH,
		.l2_cid = htobs(attCid),
		.l2_bdaddr_type = uint8_t(address.type == Address::Type::Public ? BDADDR_LE_PUBLIC : BDADDR_LE_RANDOM),
	};
	array::copy(dstaddr.l2_bdaddr.b, address.u8);

	// connect
	if (connect(fd, (sockaddr *)&dstaddr, sizeof(dstaddr)) < 0) {
		::close(fd);
		return false;
	}

	fcntl(context.fd, F_SETFL, O_NONBLOCK);

	// set file descriptor
	context.fd = fd;
	context.events = 0;
	return true;
}

void close(int index) {
	assert(uint(index) < BLUETOOTH_CONTEXT_COUNT);
	auto &context = Ble::contexts[index];
	assert(context.fd != -1);

	::close(context.fd);
	context.fd = -1;
	context.events = 0;

	// resume waiting coroutines
	context.receiveWaitlist.resumeAll([](ReceiveParameters &p) {
		*p.length = 0;
		return true;
	});
	context.sendWaitlist.resumeAll([](SendParameters &p) {
		return true;
	});

	// don't remove() yet as the iterator in the event loop may point to this context
}

Awaitable<ReceiveParameters> receive(int index, int &length, uint8_t *data) {
	assert(uint(index) < BLUETOOTH_CONTEXT_COUNT);
	auto &context = Ble::contexts[index];
	assert(context.fd != -1);

	context.events |= POLLIN;

	// add to event loop if necessary
	if (!context.isInList())
		Loop::fileDescriptors.add(context);

	// add to wait list
	return {context.receiveWaitlist, &length, data};
}

Awaitable<SendParameters> send(int index, int length, uint8_t const *data) {
	assert(uint(index) < BLUETOOTH_CONTEXT_COUNT);
	auto &context = Ble::contexts[index];
	assert(context.fd != -1);

	context.events |= POLLOUT;

	// add to event loop if necessary
	if (!context.isInList())
		Loop::fileDescriptors.add(context);

	// add to wait list
	return {context.sendWaitlist, length, data};
}

} // namespace Ble
