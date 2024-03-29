#include <MqttSnClient.hpp>
#include <Network.hpp>
#include <Timer.hpp>
#include <Terminal.hpp>
#include <Loop.hpp>
#include <Debug.hpp>
#include <StringOperators.hpp>


constexpr int defaultQos = 0;


AwaitableCoroutine subscribe(MqttSnClient &client) {
	MqttSnClient::Result result;

	// subscribe topics
	uint16_t foo;
	int8_t qos = defaultQos;
	co_await client.subscribeTopic(result, foo, qos, "foo");
	Terminal::out << "subscribed to topic 'foo' " << dec(foo) << " qos " << dec(qos) << "\n";
	while (client.isConnected()) {
		uint16_t msgId;
		uint16_t topicId;
		mqttsn::Flags flags;
		uint8_t data[32];
		int length = array::count(data);
		co_await client.receive(result, msgId, topicId, flags, length, data);
		if (result == MqttSnClient::Result::OK) {
			int8_t qos = mqttsn::getQos(flags);
			
			// acknowledge if quality of service is 1 or 2
			//if (qos > 0)
				co_await client.ackReceive(msgId, topicId, topicId == foo);
			
			String str(length, data);
			Terminal::out << (str);
		}
	}
}

Coroutine publish() {
	uint16_t localPort = 1337;
	Network::Endpoint gatewayEndpoint = {Network::Address::fromString("::1"), 10000};
	
	MqttSnClient client(localPort);
	MqttSnClient::Result result;

	// connect to gateway
	co_await client.connect(result, gatewayEndpoint, "MyClient");
	
	auto s = subscribe(client);

	// register topics
	uint16_t foo;
	co_await client.registerTopic(result, foo, "foo");
	Terminal::out << "registered topic 'foo' " << dec(foo) << "\n";
	uint16_t bar;
	co_await client.registerTopic(result, bar, "bar");
	Terminal::out << "registered topic 'bar' " << dec(bar) << "\n";

	while (client.isConnected()) {
		co_await Timer::sleep(1s);

		// publish on topics
		co_await client.publish(result, foo, mqttsn::makeQos(defaultQos), "a");
		//co_await client.publish(result, bar, mqttsn::makeQos(defaultQos), "b");
	}
	
	// wait for subsrcibe coroutine
	co_await s;

	Terminal::out << "exit\n";
}

int main(void) {
	Loop::init();
	Timer::init();
	Network::init();
	Output::init();
		
	publish();

	Loop::run();	
}
