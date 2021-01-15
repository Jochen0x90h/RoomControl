#include "MqttSnBroker.hpp"

#include <iostream>
#include <fstream>
#include <vector>
#include <thread>
#include <iterator>


class MySystemTimer : public SystemTimer {
public:

	void onSystemTimeout1(SystemTime time) override {
		std::cout << "timeout 1" << std::endl;
	}

	void onSystemTimeout2(SystemTime time) override {
		std::cout << "timeout 2" << std::endl;
	}

	void onSystemTimeout3(SystemTime time) override {
		std::cout << "timeout 3" << std::endl;
	}
};

class MyClient : public MqttSnClient {
public:

// Network
// -------

	void onUpConnected() override {
		connect("myClient");
	}

	void onDownReceived(uint16_t clientId, uint8_t const *data, int length) override {
	
	}
	
	void onDownSent() override {
	
	}


// MqttSnClient
// ------------

	void onConnected() override{
		std::cout << "connected to gateway" << std::endl;
	}

	void onDisconnected() override{

	}

	void onSleep() override {

	}

	void onWakeup() override {

	}

	void onRegistered(uint16_t msgId, String topicName, uint16_t topicId) override {

	}

	void onSubscribed(uint16_t msgId, String topicName, uint16_t topicId, int8_t qos) override {

	}

	mqttsn::ReturnCode onPublish(uint16_t topicId, uint8_t const *data, int length, int8_t qos, bool retain) override {
		return mqttsn::ReturnCode::ACCEPTED;
	}

	void onError(int error, mqttsn::MessageType messageType) override {

	}


// SystemTimer
// -----------

	void onSystemTimeout2(SystemTime time) override {}
	void onSystemTimeout3(SystemTime time) override {}
};

class MyBroker : public MqttSnBroker {
public:

// Network
// -------
	
	void onUpConnected() override {
		connect("myClient");
	}

	
// MqttSnClient
// ------------

	void onConnected() override{
		std::cout << "connected to gateway" << std::endl;

		registerTopic(this->foo, "foo");
		registerTopic(this->bar, "bar");
		setSystemTimeout3(getSystemTime() + 1s);
	
		unregisterTopic(this->foo);
		unregisterTopic(this->bar);
	
		subscribeTopic(this->foo, "foo", 1);
		subscribeTopic(this->bar, "bar", 1);
	}
	
	void onDisconnected() override{
	
	}
	
	void onSleep() override {
	
	}
	
	void onWakeup() override {
	
	}

	void onError(int error, mqttsn::MessageType messageType) override {
	
	}


// MqttSnBroker
// ------------
	
	void onPublished(uint16_t topicId, uint8_t const *data, int length, int8_t qos, bool retain) override {
		std::string s((char const*)data, length);
		std::cout << "published " << topicId << " " << s << std::endl;
	}


// SystemTimer
// -----------
	
	void onSystemTimeout3(SystemTime time) override {
		std::cout << "publish" << std::endl;

		publish(this->foo, "foo", 1);
		publish(this->bar, "bar", 1);

		setSystemTimeout3(getSystemTime() + 1s);		
	}


	// topics
	uint16_t foo = 0;
	uint16_t bar = 0;
};


void testSystemTimer() {

	MySystemTimer timer;
	timer.setSystemTimeout1(timer.getSystemTime() + 1s);
	timer.setSystemTimeout2(timer.getSystemTime() + 2s);
	timer.setSystemTimeout3(timer.getSystemTime() + 3s);

	// run event loop
	global::context.run();
}

void testClient() {

	MyClient client;

	// run event loop
	global::context.run();
}

/**
 * start mqtt broker (e.g. mosquitto)
 * clone mqtt-sn gateway from https://github.com/eclipse/paho.mqtt-sn.embedded-c
 * configure OS, SENSORNET, INCLUDE and LIB in makefile and compile MQTTSNGateway (make && make install)
 * configure mqtt-sn gateway: GatewayUDP6Port=47193, GatewayUDP6If=<local> (e.g. lo0 or lo)
 * start gateway (is next to paho.mqtt-sn.embedded-c folder)
 * start testBroker
 * publish on command line: mqtt pub --topic foo --message testMessage
 * subscribe on command line: mqtt sub --topic foo --topic bar
 */
void testBroker() {

	MyBroker broker;
	
	// run event loop
	global::context.run();
}

int main(int argc, const char **argv) {
	boost::system::error_code ec;
	asio::ip::address localhost = asio::ip::address::from_string("::1", ec);
	global::local = asio::ip::udp::endpoint(asio::ip::udp::v6(), 1337);
	global::upLink = asio::ip::udp::endpoint(localhost, 47193);

	//testSystemTimer();
	//testClient();
	testBroker();
	return 0;
}
