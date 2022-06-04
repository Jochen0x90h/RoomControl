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

Endpoint Endpoint::fromString(String s, uint16_t defaultPort) {
	struct sockaddr_in6 address;
	char buffer[64];
	array::copy(s.count(), buffer, s.data);
	buffer[s.count()] = 0;
	inet_pton(AF_INET6, buffer, &(address.sin6_addr));

	Endpoint endpoint;
	array::copy(16, endpoint.address.u8, address.sin6_addr.s6_addr);
	endpoint.port = defaultPort;
	
	return endpoint;
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

void open(int index, uint16_t port) {
	assert(Network::inited);
	assert(uint(index) < NETWORK_CONTEXT_COUNT);
	auto &context = Network::contexts[index];

	// create socket
	assert(context.fd == -1);
	context.fd = socket(AF_INET6, SOCK_DGRAM, 0);
	fcntl(context.fd, F_SETFL, O_NONBLOCK);

	int reuse = 1;
	setsockopt(context.fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
	//setsockopt(context.fd, )


	context.events = 0;

	// bind to local port
	struct sockaddr_in6 address = {AF_INET6, htons(port)};
	int r = bind(context.fd, (struct sockaddr*)&address, sizeof(address));

	// join multicast group
	struct ipv6_mreq group;
	group.ipv6mr_interface = 0;
	inet_pton(AF_INET6, "ff02::fb", &group.ipv6mr_multiaddr);
	setsockopt(context.fd, IPPROTO_IPV6, IPV6_ADD_MEMBERSHIP, &group, sizeof(group));

	// add to event loop
	Loop::addHandler(context);
}

void close(int index) {
	assert(uint(index) < NETWORK_CONTEXT_COUNT);
	auto &context = Network::contexts[index];

	assert(context.fd != -1);
	::close(context.fd);
	context.fd = -1;

	// remove from event loop
	context.remove();
}

Awaitable<ReceiveParameters> receive(int index, Endpoint& source, int &length, void *data) {
	assert(Network::inited);
	assert(uint(index) < NETWORK_CONTEXT_COUNT);

	auto &context = Network::contexts[index];
	context.events |= POLLIN;
	return {context.receiveWaitlist, &source, &length, data};
}

Awaitable<SendParameters> send(int index, Endpoint const &destination, int length, void const *data) {
	assert(Network::inited);
	assert(uint(index) < NETWORK_CONTEXT_COUNT);

	auto &context = Network::contexts[index];
	context.events |= POLLOUT;
	return {context.sendWaitlist, &destination, length, data};
}

} // namespace Network
