#include <MqttSnClient.hpp>
#include <Configuration.hpp>
#include <network.hpp>
#include <timer.hpp>
#include <sys.hpp>
#include <loop.hpp>
#include <debug.hpp>


constexpr int defaultQos = 0;


AwaitableCoroutine subscribe(MqttSnClient &client) {
	MqttSnClient::Result result;

	// subscribe topics
	uint16_t foo;
	int8_t qos = defaultQos;
	co_await client.subscribeTopic(result, foo, qos, "foo");
	sys::out.write(String("subscribed foo ") + dec(foo) + " qos " + dec(qos) + "\n");
	while (client.isConnected()) {
		uint16_t msgId;
		uint16_t topicId;
		mqttsn::Flags flags;
		uint8_t data[32];
		int length = array::length(data);
		co_await client.receive(result, msgId, topicId, flags, length, data);
		if (result == MqttSnClient::Result::OK) {
			int8_t qos = mqttsn::getQos(flags);
			
			// acknowledge if quality of service is 1 or 2
			//if (qos > 0)
				co_await client.ackReceive(msgId, topicId, topicId == foo);
			
			String str(length, data);
			sys::out.write(str);
		}
	}
}

Coroutine publish() {
	ConfigurationFlash flash;
	assign(flash.name, "myClient");
	flash.networkLocalPort = 1337;
	flash.networkGateway = network::Endpoint::fromString("::1", 10000);
	Configuration config(flash);
	
	MqttSnClient client(config);
	MqttSnClient::Result result;

	// connect to gateway
	co_await client.connect(result);
	
	auto s = subscribe(client);

	// register topics
	uint16_t foo;
	co_await client.registerTopic(result, foo, "foo");
	sys::out.write(String("registered foo ") + dec(foo) + "\n");
	uint16_t bar;
	co_await client.registerTopic(result, bar, "bar");
	sys::out.write(String("registered bar ") + dec(bar) + "\n");

	while (client.isConnected()) {
		co_await timer::sleep(1s);

		// publish on topics
		co_await client.publish(result, foo, mqttsn::makeQos(defaultQos), "a");
		//co_await client.publish(result, bar, mqttsn::makeQos(defaultQos), "b");
	}
	
	// wait for subsrcibe coroutine
	co_await s;

	sys::out.write(String("exit\n"));
}

int main(void) {
	loop::init();
	timer::init();
	network::init();
	out::init();
		
	publish();

	loop::run();	
}
