#ifdef _WIN32
#define NOMINMAX
#include <WinSock2.h>
#include <WS2tcpip.h>
#undef interface
#undef INTERFACE
#undef IN
#undef OUT
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/udp.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
inline void closesocket(int s) {close(s);}
#endif
#include "../Network.hpp"
#include "Loop.hpp"
#include <boardConfig.hpp>
#include <cerrno>


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


class Context : public Loop::SocketHandler {
public:
	void activate(uint16_t events) override {
		if (events & POLLIN) {
			this->receiveWaitlist.resumeFirst([this](ReceiveParameters &p) {
				// receive
				struct sockaddr_in6 source = {};
				socklen_t length = sizeof(source);
				auto receivedCount = recvfrom(this->socket, (char*)p.data, *p.length, 0, (struct sockaddr*)&source, &length);

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
				auto sentCount = sendto(this->socket, (const char *)p.data, p.length, 0, (struct sockaddr*)&destination, sizeof(destination));
				return sentCount >= 0;
			});
			if (this->sendWaitlist.isEmpty())
				this->events &= ~POLLOUT;
		}
	}

	// waiting coroutines
	Waitlist<ReceiveParameters> receiveWaitlist;
	Waitlist<SendParameters> sendWaitlist;
};

bool inited = false;
Context contexts[NETWORK_CONTEXT_COUNT];

void init() {
	Network::inited = true;
}

bool open(int index, uint16_t port) {
	assert(Network::inited);
	assert(uint(index) < NETWORK_CONTEXT_COUNT);
	auto &context = Network::contexts[index];

	// assert that socket is not open already
	assert(context.socket == -1);

	// create socket
	auto s = socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP);
#ifdef _WIN32
	u_long blocking = 0;
    ioctlsocket(s, FIONBIO, &blocking);
#else
	fcntl(s, F_SETFL, O_NONBLOCK);
#endif

	// set reuse address and port
	int reuse = 1;
	setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (const char *)&reuse, sizeof(reuse));
	//setsockopt(s, SOL_SOCKET, SO_REUSEPORT, &reuse, sizeof(reuse));
	int broadcast = 1;
	setsockopt(s, SOL_SOCKET, SO_BROADCAST, (const char *)&broadcast, sizeof(broadcast));

	// bind to local port
	struct sockaddr_in6 address = {.sin6_family = AF_INET6, .sin6_port= htons(port)};
	if (bind(s, (struct sockaddr*)&address, sizeof(address)) < 0) {
		int e = errno;
		closesocket(s);
		return false;
	}

	// set file descriptor
	context.socket = s;
	context.events = 0;
	return true;
}

bool join(int index, Address const &multicastGroup) {
	assert(uint(index) < NETWORK_CONTEXT_COUNT);
	auto &context = Network::contexts[index];
	assert(context.socket != -1);

	// join multicast group
	struct ipv6_mreq group;
	array::copy(16, group.ipv6mr_multiaddr.s6_addr, multicastGroup.u8);
	group.ipv6mr_interface = 0;
	int r = setsockopt(context.socket, IPPROTO_IPV6, IPV6_JOIN_GROUP, (const char *)&group, sizeof(group));
	if (r < 0) {
		int e = errno;
		return false;
	}
	return true;
}

void close(int index) {
	assert(uint(index) < NETWORK_CONTEXT_COUNT);
	auto &context = Network::contexts[index];
	assert(context.socket != -1);

	closesocket(context.socket);
	context.socket = -1;
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

Awaitable<ReceiveParameters> receive(int index, Endpoint& source, int &length, void *data) {
	assert(uint(index) < NETWORK_CONTEXT_COUNT);
	auto &context = Network::contexts[index];
	assert(context.socket != -1);

	context.events |= POLLIN;

	// add to event loop if necessary
	if (!context.isInList())
		Loop::socketHandlers.add(context);

	// add to wait list
	return {context.receiveWaitlist, &source, &length, data};
}

Awaitable<SendParameters> send(int index, Endpoint const &destination, int length, void const *data) {
	assert(uint(index) < NETWORK_CONTEXT_COUNT);
	auto &context = Network::contexts[index];
	assert(context.socket != -1);

	context.events |= POLLOUT;

	// add to event loop if necessary
	if (!context.isInList())
		Loop::socketHandlers.add(context);

	// add to wait list
	return {context.sendWaitlist, &destination, length, data};
}

} // namespace Network
