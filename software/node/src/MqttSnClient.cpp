#include "MqttSnClient.hpp"
#include <Timer.hpp>
#include <appConfig.hpp>


// number of retries when a send fails
constexpr int MAX_RETRY = 2;


MqttSnClient::MqttSnClient(uint16_t localPort) {
	Network::open(NETWORK_MQTT, localPort);
}

MqttSnClient::~MqttSnClient() {
	Network::close(NETWORK_MQTT);
}

/*
MqttSnClient::Result MqttSnClient::searchGateway() {
	// allocate message
	Message m = allocateSendMessage(3, mqttsn::SEARCHGW);
	if (m.info == nullptr)
		return Result::BUSY;

	// serialize message
	m.data[2] = 1; // radius
	
	this->state = State::SEARCHING_GATEWAY;

	// before sending, wait for a jitter time to prevent flooding of gateway when multiple clients are switched on.
	// Therefore the random value must be different for each client (either hardware or client id dependent)
	//todo: use random time
	setSystemTimeout1(getSystemTime() + 100ms);
		
	return Result::OK;
}
*/

AwaitableCoroutine MqttSnClient::connect(Result &result, Network::Endpoint const &gatewayEndpoint, String name,
	bool cleanSession, bool willFlag)
{
	if (!canConnect()) {
		result = Result::INVALID_STATE;
		co_return;
	}
	this->gatewayEndpoint = gatewayEndpoint;

	for (int retry = 0; retry <= MAX_RETRY; ++retry) {
		// send connect message
		{
			auto flags = (cleanSession ? mqttsn::Flags::CLEAN_SESSION : mqttsn::Flags::NONE)
				| (willFlag ? mqttsn::Flags::WILL : mqttsn::Flags::NONE);
		
			PacketWriter w(this->tempMessage);
			w.e8<mqttsn::MessageType>(mqttsn::MessageType::CONNECT);
			w.e8(flags); // flags
			w.u8(0x01); // protocol name/version
			w.u16B(KEEP_ALIVE_TIME.toSeconds());
			w.string(name);
			co_await Network::send(NETWORK_MQTT, gatewayEndpoint, w.finish());
		}

		// wait for a reply from the gateway
		{
			Network::Endpoint source;
			int length = MAX_MESSAGE_LENGTH;
			int s = co_await select(Network::receive(NETWORK_MQTT, source, length, this->tempMessage),
				Timer::sleep(RETRANSMISSION_TIME));
			if (s == 1) {
				// check if the message is from the gateway
				// todo

				PacketReader r(this->tempMessage);
				
				// check if message is complete
				if (r.end - this->tempMessage > length) {
					result = Result::PROTOCOL_ERROR;
					co_return;
				}
				
				// check if we received a CONNACK
				auto messageType = r.e8<mqttsn::MessageType>();
				if (messageType != mqttsn::MessageType::CONNACK) {
					result = Result::PROTOCOL_ERROR;
					co_return;
				}

				// check if connection was accepted
				auto returnCode = r.e8<mqttsn::ReturnCode>();
				if (returnCode != mqttsn::ReturnCode::ACCEPTED) {
					result = Result::REJECTED;
					co_return;
				}
								
				// now we are connected
				this->state = State::CONNECTED;
					
				// start coroutines
				this->pingCoroutine = ping();
				this->receiveCoroutine = receive();
				
				result = Result::OK;
				co_return;
			}
		}
	}
	result = Result::TIMEOUT;
}

AwaitableCoroutine MqttSnClient::disconnect() {
	if (!isConnected()) {
		co_return;
	}

	uint8_t message[4];

	for (int retry = 0; retry <= MAX_RETRY; ++retry) {
		// send connect message
		{
			PacketWriter w(message);
			w.e8<mqttsn::MessageType>(mqttsn::MessageType::DISCONNECT);
			co_await Network::send(NETWORK_MQTT, this->gatewayEndpoint, w.finish());
		}

		// wait for disconnect reply from gateway
		{
			int length = array::count(message);
			int s = co_await select(this->ackWaitlist.wait(mqttsn::MessageType::DISCONNECT, uint16_t(0), length, message),
				Timer::sleep(RETRANSMISSION_TIME));
			if (s == 1)
				break;
		}
	}
	
	// assume that we are disconnected when the gateway does not answer
	this->pingCoroutine.cancel();
	this->receiveCoroutine.cancel();
	disconnectInternal();
}

AwaitableCoroutine MqttSnClient::registerTopic(Result &result, uint16_t &topicId, String topicName) {
	if (!isConnected()) {
		result = Result::INVALID_STATE;
		co_return;
	}

	uint8_t message[MAX_MESSAGE_LENGTH];

	// generate message id
	uint16_t msgId = getNextMsgId();

	for (int retry = 0; retry <= MAX_RETRY; ++retry) {
		// send register message
		{
			PacketWriter w(message);
			w.e8<mqttsn::MessageType>(mqttsn::MessageType::REGISTER);
			w.u16B(0); // topic id not known yet
			w.u16B(msgId);
			w.string(topicName);
			co_await Network::send(NETWORK_MQTT, this->gatewayEndpoint, w.finish());
		}

		// wait for acknowledge from gateway
		{
			int length = array::count(message);
			int s = co_await select(this->ackWaitlist.wait(mqttsn::MessageType::REGACK, msgId, length, message),
				Timer::sleep(RETRANSMISSION_TIME));
			
			// check if still connected
			if (!isConnected()) {
				// make sure we are resumed from the event loop, not from ping()
				co_await Timer::sleep(100ms);
				result = Result::INVALID_STATE;
				co_return;
			}

			// check if we received a message
			if (s == 1) {
				// get topic id and return code
				MessageReader r(length, message);
				topicId = r.u16B();
				r.skip(2); // msgId
				auto returnCode = r.e8<mqttsn::ReturnCode>();
				if (!r.isValid())
					result = Result::PROTOCOL_ERROR;
				else if (returnCode == mqttsn::ReturnCode::ACCEPTED)
					result = Result::OK;
				else
					result = Result::REJECTED;
				co_return;
			}
		}
	}
	result = Result::TIMEOUT;
}

AwaitableCoroutine MqttSnClient::publish(Result &result, uint16_t topicId, mqttsn::Flags flags,
	int length, uint8_t const *data)
{
	if (!isConnected()) {
		result = Result::INVALID_STATE;
		co_return;
	}

	uint8_t message[MAX_MESSAGE_LENGTH];

	int8_t qos = mqttsn::getQos(flags);

	// generate a message id
	uint16_t msgId = qos <= 0 ? 0 : getNextMsgId();

	for (int retry = 0; retry <= MAX_RETRY; ++retry) {
		// send publish message
		{
			PacketWriter w(message);
			w.e8(mqttsn::MessageType::PUBLISH);
			w.e8(flags);
			w.u16B(topicId);
			w.u16B(msgId);
			w.data8(length, data);
			co_await Network::send(NETWORK_MQTT, this->gatewayEndpoint, w.finish());
		}
		
		if (qos <= 0) {
			result = Result::OK;
			break;
		}
		
		// wait for acknowledge from gateway
		{
			int length = array::count(message);
			int s = co_await select(this->ackWaitlist.wait(mqttsn::MessageType::PUBACK, msgId, length, message),
				Timer::sleep(RETRANSMISSION_TIME));

			// check if still connected
			if (!isConnected()) {
				// make sure we are resumed from the event loop, not from ping()
				co_await Timer::sleep(100ms);
				result = Result::INVALID_STATE;
				break;
			}

			// check if we received a message
			if (s == 1) {
				MessageReader r(length, message);
				
				// get topic id and return code
				topicId = r.u16B();
				r.skip(2); // msgId
				auto returnCode = r.e8<mqttsn::ReturnCode>();
				if (!r.isValid()) {
					result = Result::PROTOCOL_ERROR;
					// try again
				} else if (returnCode == mqttsn::ReturnCode::ACCEPTED) {
					result = Result::OK;
					break;
				} else {
					result = Result::REJECTED;
					// try again
				}
			} else {
				result = Result::TIMEOUT;
			}
		}

		// set duplicate flag and try again
		flags |= mqttsn::Flags::DUP;
	}
}

AwaitableCoroutine MqttSnClient::subscribeTopic(Result &result, uint16_t &topicId, int8_t &qos, String topicFilter) {
	if (!isConnected()) {
		result = Result::INVALID_STATE;
		co_return;
	}

	uint8_t message[MAX_MESSAGE_LENGTH];

	// generate message id
	uint16_t msgId = getNextMsgId();

	for (int retry = 0; retry <= MAX_RETRY; ++retry) {
		// send subscribe message
		{
			PacketWriter w(message);
			w.e8(mqttsn::MessageType::SUBSCRIBE);
			auto flags = mqttsn::Flags::TOPIC_TYPE_NORMAL | mqttsn::makeQos(qos);
			w.e8(flags);
			w.u16B(msgId);
			w.string(topicFilter);
			co_await Network::send(NETWORK_MQTT, this->gatewayEndpoint, w.finish());
		}

		// wait for acknowledge from gateway
		{
			int length = array::count(message);
			int s = co_await select(this->ackWaitlist.wait(mqttsn::MessageType::SUBACK, msgId, length, message),
				Timer::sleep(RETRANSMISSION_TIME));

			// check if still connected
			if (!isConnected()) {
				// make sure we are resumed from the event loop, not from ping()
				co_await Timer::sleep(100ms);
				result = Result::INVALID_STATE;
				co_return;
			}

			// check if we received a message
			if (s == 1) {
				MessageReader r(length, message);
					
				// get flags, topic id and return code
				auto flags = r.e8<mqttsn::Flags>();
				qos = mqttsn::getQos(flags);
				topicId = r.u16B();
				r.skip(2); // msgId
				auto returnCode = r.e8<mqttsn::ReturnCode>();
				if (!r.isValid())
					result = Result::PROTOCOL_ERROR;
				else if (returnCode == mqttsn::ReturnCode::ACCEPTED)
					result = Result::OK;
				else
					result = Result::REJECTED;
				co_return;
			}
		}
	}
	result = Result::TIMEOUT;
}

AwaitableCoroutine MqttSnClient::unsubscribeTopic(Result &result, String topicFilter) {
	if (!isConnected()) {
		result = Result::INVALID_STATE;
		co_return;
	}

	uint8_t message[MAX_MESSAGE_LENGTH];

	// generate message id
	uint16_t msgId = getNextMsgId();

	for (int retry = 0; retry <= MAX_RETRY; ++retry) {
		// send unsubscribe message
		{
			PacketWriter w(message);
			w.e8(mqttsn::MessageType::UNSUBSCRIBE);
			w.e8(mqttsn::Flags::TOPIC_TYPE_NORMAL);
			w.u16B(msgId);
			w.string(topicFilter);
			co_await Network::send(NETWORK_MQTT, this->gatewayEndpoint, w.finish());
		}

		// wait for acknowledge from gateway
		{
			int length = array::count(message);
			int s = co_await select(this->ackWaitlist.wait(mqttsn::MessageType::UNSUBACK, msgId, length, message),
				Timer::sleep(RETRANSMISSION_TIME));

			// check if still connected
			if (!isConnected()) {
				// make sure we are resumed from the event loop, not from ping()
				co_await Timer::sleep(100ms);
				result = Result::INVALID_STATE;
				co_return;
			}

			// check if we received a message
			if (s == 1) {
				result = Result::OK;
				co_return;
			}
		}
	}
	result = Result::TIMEOUT;
}

AwaitableCoroutine MqttSnClient::receive(Result &result, uint16_t &msgId, uint16_t &topicId, mqttsn::Flags &flags,
	int &length, uint8_t *data)
{
	if (!isConnected()) {
		result = Result::INVALID_STATE;
		co_return;
	}

	uint8_t message[MAX_MESSAGE_LENGTH];

	int length2 = array::count(message);
	co_await this->receiveWaitlist.wait(length2, message);

	// check if still connected
	if (!isConnected()) {
		// make sure we are resumed from the event loop, not from ping()
		co_await Timer::sleep(100ms);
		result = Result::INVALID_STATE;
		co_return;
	}

	// check if message is complete
	if (length2 > MAX_MESSAGE_LENGTH) {
		result = Result::PROTOCOL_ERROR;
		co_return;
	}

	// read message
	MessageReader r(length2, message);
	flags = r.e8<mqttsn::Flags>();
	topicId = r.u16B();
	msgId = r.u16B();
	int l = r.getRemaining();
	if (l > length) {
		// error: message too long for given data buffer
		result = Result::INVALID_PARAMETER;
		length = 0;
	} else {
		length = l;
		array::copy(l, data, r.current);
		result = Result::OK;
	}
		
	// user has to call ackReceive to acknowledge the message
}

Awaitable<Network::SendParameters> MqttSnClient::ackReceive(uint16_t msgId, uint16_t topicId, bool ok) {
	// message id is only set when qos was 1 or 2
	if (msgId == 0)
		return {};
		
	uint8_t message[8];

	PacketWriter w(message);
	w.e8(mqttsn::MessageType::PUBACK);
	w.u16B(topicId);
	w.u16B(msgId);
	w.e8(ok ? mqttsn::ReturnCode::ACCEPTED : mqttsn::ReturnCode::REJECTED_INVALID_TOPIC_ID);
	return Network::send(NETWORK_MQTT, this->gatewayEndpoint, w.finish());
}

AwaitableCoroutine MqttSnClient::ping() {
	uint8_t message[4];

	while (true) {
		co_await Timer::sleep(KEEP_ALIVE_TIME);

		for (int retry = 0; ; ++retry) {
			// send idle ping
			{
				PacketWriter w(message);
				w.e8<mqttsn::MessageType>(mqttsn::MessageType::PINGREQ);
				co_await Network::send(NETWORK_MQTT, this->gatewayEndpoint, w.finish());
			}
			
			// wait for ping response
			{
				int length = array::count(message);
				int s = co_await select(this->ackWaitlist.wait(mqttsn::MessageType::PINGRESP, uint16_t(0), length, message),
					Timer::sleep(RETRANSMISSION_TIME));
				if (s == 1)
					break;
			}
			
			// after max retry, assume we are disconnected
			if (retry == MAX_RETRY) {
				// disconnect
				this->receiveCoroutine.cancel();
				disconnectInternal();
				co_return;
			}
		}
	}
}

AwaitableCoroutine MqttSnClient::receive() {
	uint8_t *message = this->tempMessage;

	while (true) {
		// receive a message from the gateway
		Network::Endpoint source;
		int length = MAX_MESSAGE_LENGTH;
		co_await Network::receive(NETWORK_MQTT, source, length, message);

		// check if the message is from the gateway
		// todo

		PacketReader r(message);
		
		// check if message is complete
		if (r.end - message > length)
			continue;
		
		// get message type
		auto msgType = r.e8<mqttsn::MessageType>();

		// check if we read past the end of the message
		if (!r.isValid()) {
			continue;
		}
				
		// get message without length and type
		int l = r.end - r.current;
		uint8_t *m = r.current;

		if (msgType == mqttsn::MessageType::PUBLISH) {
			// the gateway published to a topic
			this->receiveWaitlist.resumeFirst([l, m](ReceiveParameters &p) {
				p.length = l;
				int length = min(p.length, l);
				array::copy(length, p.message, m);
				return true;
			});
		} else {
			// the gateway acknowledges one of REGISTER, PUBLISH, SUBSCRIBE or UNSUBSCRIBE
			uint16_t msgId;
			switch (msgType) {
			case mqttsn::MessageType::REGACK:
			case mqttsn::MessageType::PUBACK:
				r.skip(2);
				msgId = r.u16B();
				break;
			case mqttsn::MessageType::SUBACK:
				r.skip(3);
				msgId = r.u16B();
				break;
			case mqttsn::MessageType::UNSUBACK:
				msgId = r.u16B();
				break;
			case mqttsn::MessageType::PINGRESP:
				msgId = 0;
				break;
			case mqttsn::MessageType::DISCONNECT:
				// disconnect
				this->pingCoroutine.cancel();
				disconnectInternal();
				co_return;
			default:
				// unsupported message type
				continue;
			}
			
			// check if we read past the end of the message
			if (!r.isValid()) {
				continue;
			}
			
			// resume the coroutine that waits for msgId
			this->ackWaitlist.resumeOne([msgType, msgId, l, m](AckParameters &p) {
				if (p.msgType == msgType && p.msgId == msgId) {
					p.length = l;
					int length = min(p.length, l);
					array::copy(length, p.message, m);
					return true;
				}
				return false;
			});
		}
	}
}

void MqttSnClient::disconnectInternal() {
	this->state = State::DISCONNECTED;
	
	// resume all waiting coroutines
	this->receiveWaitlist.resumeAll([](ReceiveParameters &p) {
		p.length = 0;
		return true;
	});
	this->ackWaitlist.resumeAll([](AckParameters &p) {
		p.length = 0;
		return true;
	});
}
