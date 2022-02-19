#include <MqttSnBroker.hpp>
#include <Network.hpp>
#include <Timer.hpp>
#include <Terminal.hpp>
#include <Loop.hpp>
#include <Debug.hpp>
#include <StringOperators.hpp>
#ifdef EMU
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
		Network::Endpoint gatewayEndpoint = Network::Endpoint::fromString("::1", gatewayPort);

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
		Message message;

		Subscriber::Barrier barrier;
		Subscriber subscriber;
		subscriber.subscriptionIndex = 0;
		subscriber.messageType = MessageType::ON_OFF;
		subscriber.barrier = &barrier;

		Publisher publisher;
		publisher.messageType = MessageType::ON_OFF;
		publisher.message = &message;

		Terminal::out << name << " subscribe to '" << inTopic << "'\n";
		Terminal::out << name << " publish on '" << outTopic << "'\n";
		this->broker.addSubscriber(inTopic, subscriber);
		this->broker.addPublisher(outTopic, publisher);

		while (true) {
			uint8_t subscriptionIndex;
			co_await barrier.wait(subscriptionIndex, &message);

			if (message.onOff < 2)
				message.onOff = message.onOff ^ 1;

			publisher.publish();
		}
	}
};


#ifdef EMU
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

	Loop::init();
	Timer::init();
	Network::init();
	Output::init();

	Test test(localPort);
	test.connect(gatewayPort, name);
	test.function();

	Loop::run();
}
