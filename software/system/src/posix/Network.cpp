#include "../Network.hpp"
#include "Loop.hpp"
#include <poll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/udp.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>


constexpr int NETWORK_CONTEXT_COUNT = 16;


namespace Network {

Address Address::fromString(String s) {
	char buffer[64];
	array::copy(s.count(), buffer, s.data);
	buffer[s.count()] = 0;

	in6_addr a;
	inet_pton(AF_INET6, buffer, &(a));

	Address address;
	array::copy(16, address.u8, a.s6_addr);
	return address;
}


class Context : public Loop::Handler {
public:

	Waitlist<ReceiveParameters> receiveWaitlist;
	Waitlist<SendParameters> sendWaitlist;


	void activate(uint16_t events) override {
		if (events & POLLIN) {
			this->receiveWaitlist.resumeFirst([this](ReceiveParameters &p) {
				// receive
				struct sockaddr_in6 source = {};
				socklen_t length = sizeof(source);
				auto receivedCount = recvfrom(this->fd, p.data, *p.length, 0, (struct sockaddr*)&source, &length);

				if (receivedCount >= 0) {
					*p.length = receivedCount;

					// convert source
					array::copy(16, p.source->address.u8, source.sin6_addr.s6_addr);
					p.source->port = ntohs(source.sin6_port);
					return true;
				}
				return false;
			});
			if (this->receiveWaitlist.isEmpty())
				this->events &= ~POLLIN;
		}
		if (events & POLLOUT) {
			this->sendWaitlist.resumeFirst([this](SendParameters &p) {
				// build destination endpoint
				struct sockaddr_in6 destination = {};
				destination.sin6_family = AF_INET6;
				array::copy(16, destination.sin6_addr.s6_addr, p.destination->address.u8);
				destination.sin6_port = htons(p.destination->port);

				// send
				auto sentCount = sendto(this->fd, p.data, p.length, 0, (struct sockaddr*)&destination, sizeof(destination));
				return sentCount >= 0;
			});
			if (this->sendWaitlist.isEmpty())
				this->events &= ~POLLOUT;
		}
	}
};

bool inited = false;
Context contexts[NETWORK_CONTEXT_COUNT];

void init() {
	Network::inited = true;
	for (auto &context : contexts)
		context.fd = -1;
}

bool open(int index, uint16_t port) {
	assert(Network::inited);
	assert(uint(index) < NETWORK_CONTEXT_COUNT);
	auto &context = Network::contexts[index];

	// create socket
	assert(context.fd == -1);
	int fd = socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP);
	fcntl(context.fd, F_SETFL, O_NONBLOCK);

	// set reuse address and port
	int reuse = 1;
	//setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
	setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, &reuse, sizeof(reuse));

	// bind to local port
	struct sockaddr_in6 address = {.sin6_family = AF_INET6, .sin6_port= htons(port)};
	int r = bind(fd, (struct sockaddr*)&address, sizeof(address));
    if (r < 0) {
		int e = errno;
		::close(fd);
		return false;
	}

	// set file descriptor
	context.fd = fd;
	context.events = 0;
	return true;
}

bool join(int index, Address const &multicastGroup) {
	assert(Network::inited);
	assert(uint(index) < NETWORK_CONTEXT_COUNT);
	auto &context = Network::contexts[index];

	// join multicast group
	struct ipv6_mreq group;
	array::copy(16, group.ipv6mr_multiaddr.s6_addr, multicastGroup.u8);
	group.ipv6mr_interface = 0;
	int r = setsockopt(context.fd, IPPROTO_IPV6, IPV6_JOIN_GROUP, &group, sizeof(group));
	if (r < 0) {
		int e = errno;
		return false;
	}
	return true;
}

void close(int index) {
	assert(uint(index) < NETWORK_CONTEXT_COUNT);
	auto &context = Network::contexts[index];

	assert(context.fd != -1);
	::close(context.fd);
	context.fd = -1;
	context.events = 0;
}

Awaitable<ReceiveParameters> receive(int index, Endpoint& source, int &length, void *data) {
	assert(Network::inited);
	assert(uint(index) < NETWORK_CONTEXT_COUNT);

	auto &context = Network::contexts[index];
	context.events |= POLLIN;
	if (context.isEmpty())
		Loop::addHandler(context);
	return {context.receiveWaitlist, &source, &length, data};
}

Awaitable<SendParameters> send(int index, Endpoint const &destination, int length, void const *data) {
	assert(Network::inited);
	assert(uint(index) < NETWORK_CONTEXT_COUNT);

	auto &context = Network::contexts[index];
	context.events |= POLLOUT;
	if (context.isEmpty())
		Loop::addHandler(context);
	return {context.sendWaitlist, &destination, length, data};
}

} // namespace Network
