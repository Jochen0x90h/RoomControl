#include <MqttSnBroker.hpp>
#include <network.hpp>
#include <timer.hpp>
#include <sys.hpp>
#include <loop.hpp>
#include <debug.hpp>


StringBuffer<32> name;
uint16_t localPort = 1337;
uint16_t gatewayPort = 10000;
StringBuffer<32> inTopic;
StringBuffer<32> outTopic;

struct Test {
	MqttSnBroker broker;

	Test(uint16_t localPort) : broker(localPort) {}


	Coroutine connect(uint16_t gatewayPort, String name) {
		network::Endpoint gatewayEndpoint = network::Endpoint::fromString("::1", gatewayPort);

		while (true) {
			while (!this->broker.isGatewayConnected()) {
				// connect to gateway
				sys::out.write(name + " connect\n");
				co_await this->broker.connect(gatewayEndpoint, name);
			}
			sys::out.write(name + " keepAlive\n");
			co_await this->broker.keepAlive();
		}
	}

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

	loop::init();
	timer::init();
	network::init();
	out::init();

	Test test(localPort);
	test.connect(gatewayPort, name);
	test.function();

	loop::run();
}
