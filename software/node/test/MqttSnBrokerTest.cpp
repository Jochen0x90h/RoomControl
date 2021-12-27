#include <MqttSnBroker.hpp>
#include <network.hpp>
#include <timer.hpp>
#include <sys.hpp>
#include <loop.hpp>
#include <debug.hpp>


struct Test {
	MqttSnBroker broker;
	
	Test() : broker(1337) {}
	

	Coroutine connect() {
		network::Endpoint gatewayEndpoint = network::Endpoint::fromString("::1", 10000);

		while (true) {
			while (!this->broker.isGatewayConnected()) {
				// connect to gateway
				co_await this->broker.connect(gatewayEndpoint, "MyBroker");
			}
			co_await this->broker.registerSubscriber("f/in");
			co_await this->broker.registerPublisher("f/out");

			co_await this->broker.ping();
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

		this->broker.addSubscriber("f/in", subscriber);
		this->broker.addPublisher("f/out", publisher);
		
		while (true) {
			uint8_t subscriptionIndex;
			co_await barrier.wait(subscriptionIndex, &message);
			
			if (message.onOff < 2)
				message.onOff = message.onOff ^ 1;
			
			publisher.publish();
		}
	}
};

int main(void) {
	loop::init();
	timer::init();
	network::init();
	out::init();
	
	Test test;
	test.connect();
	test.function();

	loop::run();	
}
