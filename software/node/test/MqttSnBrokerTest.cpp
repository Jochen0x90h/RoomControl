#include <MqttSnBroker.hpp>
#include <Network.hpp>
#include <Terminal.hpp>
#include <Loop.hpp>
#include <Debug.hpp>
#include <StringOperators.hpp>
#ifdef PLATFORM_POSIX
#include <string>
#endif


StringBuffer<32> name;
uint16_t localPort = 1337;
uint16_t gatewayPort = 10000;
StringBuffer<32> inTopic;
StringBuffer<32> outTopic;

struct Test {
	MqttSnBroker broker;

	Test(uint16_t localPort) : broker(localPort) {}

	// connect to gateway and keep the connection alive
	Coroutine connect(uint16_t gatewayPort, String name) {
		Network::Endpoint gatewayEndpoint = {Network::Address::fromString("::1"), gatewayPort};

		while (true) {
			// connect to gateway
			while (!this->broker.isGatewayConnected()) {
				// connect to gateway
				Terminal::out << name << " connect to gateway\n";
				co_await this->broker.connect(gatewayEndpoint, name);
			}
			
			// now we are connected and need to keep the connection alive
			Terminal::out << name << " keep connection alive\n";
			co_await this->broker.keepAlive();
			
			// connection lost, try to connect again
		}
	}

	// simple "function" that subscribes to a topic, processes the data and publishes it on another topic
	Coroutine function() {
/*		PublishInfo::Barrier barrier;
		Subscriber subscriber;
		subscriber.convertOptions.commands = 0 | (1 << 3) | -1 << 6;
		subscriber.destination.type = MessageType::BINARY_IN;
		subscriber.destination.plug.id = 0;
		subscriber.barrier = &barrier;

		Terminal::out << name << " subscribe to '" << inTopic << "'\n";
		Terminal::out << name << " publish on '" << outTopic << "'\n";
		this->broker.subscribe(inTopic, MessageType::BINARY_OUT, subscriber);
		auto publishInfo = this->broker.getPublishInfo(outTopic, MessageType::BINARY_IN);

		while (true) {
			MessageInfo info;
			Message message;
			co_await barrier.wait(info, &message);

			if (message.command < 2)
				message.command = message.command ^ 1;

			// publish
			publishInfo.barrier->resumeFirst([&info, &message] (PublishInfo::Parameters &p) {
				p.info = info;

				// convert to destination message type and resume coroutine if conversion was successful
				auto &dst = *reinterpret_cast<Message *>(p.message);
				dst.command = message.command;
				return true;
			});
		}*/
		co_return;
	}
};


#ifdef PLATFORM_POSIX
int main(int argc, char const **argv) {
	name = "Client";
	if (argc >= 3) {
		localPort = std::stoi(argv[1]);
		gatewayPort = std::stoi(argv[2]);
		inTopic = dec(localPort) + '/';
		outTopic = dec(localPort) + '/';
	}
	name += dec(localPort);
#else
int main(void) {
	name = "Client";
#endif
	inTopic += "in";
	outTopic += "out";

	loop::init();
	Network::init();
	Output::init();

	Test test(localPort);
	test.connect(gatewayPort, name);
	test.function();

	loop::run();
}
