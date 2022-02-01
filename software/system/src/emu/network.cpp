#include "../network.hpp"
#include "loop.hpp"
//#include "sys.hpp"
#include <boost/asio.hpp>


namespace asio = boost::asio;


namespace network {

Endpoint Endpoint::fromString(String s, uint16_t defaultPort) {
	boost::system::error_code ec;
	asio::ip::address_v6 address = asio::ip::make_address(std::string(s.data, s.length), ec).to_v6();
	
	Endpoint endpoint;
	array::copy(16, endpoint.address.u8, address.to_bytes().data());
	endpoint.port = defaultPort;
	
	return endpoint;
}


struct Context {
	asio::ip::udp::socket socket;

	Waitlist<ReceiveParameters> receiveWaitlist;
	Waitlist<SendParameters> sendWaitlist;

	Context() : socket(loop::context, asio::ip::udp::v6()) {
		this->socket.non_blocking(true);
	}
};

Context contexts[NETWORK_CONTEXT_COUNT];


// event loop handler chain
loop::Handler nextHandler;
void handle(Gui &gui) {
	for (int index = 0; index < NETWORK_CONTEXT_COUNT; ++index) {
		auto &context = network::contexts[index];
	
		context.receiveWaitlist.resumeFirst([&context](ReceiveParameters &p) {
			// receive
			asio::ip::udp::endpoint source;
			boost::system::error_code ec;
			size_t receivedCount = context.socket.receive_from(boost::asio::buffer(p.data, *p.length), source, 0, ec);
			
			if (receivedCount > 0) {
				*p.length = receivedCount;
				
				// convert source
				asio::ip::address_v6 address = source.address().to_v6();
				array::copy(16, p.source->address.u8, address.to_bytes().data());
				p.source->port = source.port();
				return true;
			}
			return false;
		});

		context.sendWaitlist.resumeFirst([&context](SendParameters &p) {
			// build destination endpoint
			std::array<unsigned char, 16> bytes;
			std::copy(std::begin(p.destination->address.u8), std::end(p.destination->address.u8), std::begin(bytes));
			asio::ip::address_v6 address(bytes);
			asio::ip::udp::endpoint destination(address, p.destination->port);
			
			// send
			boost::system::error_code ec;
			context.socket.send_to(boost::asio::buffer(p.data, p.length), destination, 0, ec);
			return true;
		});
	}
	
	// call next handler in chain
	network::nextHandler(gui);
}

void init() {
	// add to event loop handler chain
	network::nextHandler = loop::addHandler(handle);
}

void setLocalPort(int index, uint16_t port) {
	assert(uint(index) < NETWORK_CONTEXT_COUNT);

	auto &context = network::contexts[index];
	context.socket.bind(asio::ip::udp::endpoint(asio::ip::udp::v6(), port));
}

Awaitable<ReceiveParameters> receive(int index, Endpoint& source, int &length, void *data) {
	assert(uint(index) < NETWORK_CONTEXT_COUNT);

	auto &context = network::contexts[index];
	return {context.receiveWaitlist, &source, &length, data};
}

Awaitable<SendParameters> send(int index, Endpoint const &destination, int length, void const *data) {
	assert(uint(index) < NETWORK_CONTEXT_COUNT);

	auto &context = network::contexts[index];
	return {context.sendWaitlist, &destination, length, data};
}

} // namespace network
