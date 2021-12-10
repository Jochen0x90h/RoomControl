#include "MqttSnClient.hpp"
#include <timer.hpp>
#include <appConfig.hpp>


// number of retries when a send fails
constexpr int MAX_RETRY = 2;


MqttSnClient::MqttSnClient(Configuration &configuration)
	: configuration(configuration)
{
}

MqttSnClient::~MqttSnClient() {
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

AwaitableCoroutine MqttSnClient::connect(Result &result, bool cleanSession, bool willFlag) {
	if (!canConnect()) {
		result = Result::INVALID_STATE;
		co_return;
	}

	for (int retry = 0; retry <= MAX_RETRY; ++retry) {
		// send connect message
		{
			MessageWriter w(this->tempMessage);
			w.enum8<mqttsn::MessageType>(mqttsn::MessageType::CONNECT);
			w.uint8(0); // flags
			w.uint8(0x01); // protocol name/version
			w.uint16(KEEP_ALIVE_INTERVAL.toSeconds());
			w.data(this->configuration->getName());
			co_await network::send(NETWORK_MQTTSN, this->configuration->networkGateway, w.finish());
		}

		// wait for a reply from the gateway
		{
			network::Endpoint source;
			int length = MAX_MESSAGE_LENGTH;
			int s = co_await select(network::receive(NETWORK_MQTTSN, source, length, this->tempMessage),
				timer::sleep(RETRANSMISSION_INTERVAL));
			if (s == 1) {
				// check if the message is from the gateway
				// todo

				MessageReader r(this->tempMessage);
				
				// check if message is complete
				if (r.end - this->tempMessage > length) {
					result = Result::PROTOCOL_ERROR;
					co_return;
				}
				
				// check if we received a CONNACK
				auto messageType = r.enum8<mqttsn::MessageType>();
				if (messageType != mqttsn::MessageType::CONNACK) {
					result = Result::PROTOCOL_ERROR;
					co_return;
				}

				// check if connection was accepted
				auto returnCode = r.enum8<mqttsn::ReturnCode>();
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
			MessageWriter w(message);
			w.enum8<mqttsn::MessageType>(mqttsn::MessageType::DISCONNECT);
			co_await network::send(NETWORK_MQTTSN, this->configuration->networkGateway, w.finish());
		}

		// wait for disconnect reply from gateway
		{
			int length = array::length(message);
			int s = co_await select(this->ackWaitlist.wait(mqttsn::MessageType::DISCONNECT, uint16_t(0), length, message),
				timer::sleep(RETRANSMISSION_INTERVAL));
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
			MessageWriter w(message);
			w.enum8<mqttsn::MessageType>(mqttsn::MessageType::REGISTER);
			w.uint16(0); // topic id not known yet
			w.uint16(msgId);
			w.data(topicName);
			co_await network::send(NETWORK_MQTTSN, this->configuration->networkGateway, w.finish());
		}

		// wait for acknowledge from gateway
		{
			int length = array::length(message);
			int s = co_await select(this->ackWaitlist.wait(mqttsn::MessageType::REGACK, msgId, length, message),
				timer::sleep(RETRANSMISSION_INTERVAL));
			
			// check if still connected
			if (!isConnected()) {
				// make sure we are resumed from the event loop, not from ping()
				co_await timer::sleep(100ms);
				result = Result::INVALID_STATE;
				co_return;
			}

			// check if we received a message
			if (s == 1) {
				// get topic id and return code
				MessageReader r(length, message);
				topicId = r.uint16();
				r.skip(2); // msgId
				auto returnCode = r.enum8<mqttsn::ReturnCode>();
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

	// generate message id
	uint16_t msgId = qos > 0 ? getNextMsgId() : 0;

	for (int retry = 0; retry <= MAX_RETRY; ++retry) {
		// send publish message
		{
			MessageWriter w(message);
			w.enum8(mqttsn::MessageType::PUBLISH);
			w.enum8(flags);
			w.uint16(topicId);
			w.uint16(msgId);
			w.data({length, data});
			co_await network::send(NETWORK_MQTTSN, this->configuration->networkGateway, w.finish());
		}
		
		if (qos == 0) {
			result = Result::OK;
			co_return;
		}
		
		// wait for acknowledge from gateway
		{
			int length = array::length(message);
			int s = co_await select(this->ackWaitlist.wait(mqttsn::MessageType::PUBACK, msgId, length, message),
				timer::sleep(RETRANSMISSION_INTERVAL));

			// check if still connected
			if (!isConnected()) {
				// make sure we are resumed from the event loop, not from ping()
				co_await timer::sleep(100ms);
				result = Result::INVALID_STATE;
				co_return;
			}

			// check if we received a message
			if (s == 1) {
				MessageReader r(length, message);
				
				// get topic id and return code
				topicId = r.uint16();
				r.skip(2); // msgId
				auto returnCode = r.enum8<mqttsn::ReturnCode>();
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
			MessageWriter w(message);
			w.enum8(mqttsn::MessageType::SUBSCRIBE);
			auto flags = mqttsn::Flags::TOPIC_TYPE_NORMAL | mqttsn::makeQos(qos);
			w.enum8(flags);
			w.uint16(msgId);
			w.data(topicFilter);
			co_await network::send(NETWORK_MQTTSN, this->configuration->networkGateway, w.finish());
		}

		// wait for acknowledge from gateway
		{
			int length = array::length(message);
			int s = co_await select(this->ackWaitlist.wait(mqttsn::MessageType::SUBACK, msgId, length, message),
				timer::sleep(RETRANSMISSION_INTERVAL));

			// check if still connected
			if (!isConnected()) {
				// make sure we are resumed from the event loop, not from ping()
				co_await timer::sleep(100ms);
				result = Result::INVALID_STATE;
				co_return;
			}

			// check if we received a message
			if (s == 1) {
				MessageReader r(length, message);
					
				// get flags, topic id and return code
				auto flags = r.enum8<mqttsn::Flags>();
				qos = mqttsn::getQos(flags);
				topicId = r.uint16();
				r.skip(2); // msgId
				auto returnCode = r.enum8<mqttsn::ReturnCode>();
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
			MessageWriter w(message);
			w.enum8(mqttsn::MessageType::UNSUBSCRIBE);
			w.enum8(mqttsn::Flags::TOPIC_TYPE_NORMAL);
			w.uint16(msgId);
			w.data(topicFilter);
			co_await network::send(NETWORK_MQTTSN, this->configuration->networkGateway, w.finish());
		}

		// wait for acknowledge from gateway
		{
			int length = array::length(message);
			int s = co_await select(this->ackWaitlist.wait(mqttsn::MessageType::UNSUBACK, msgId, length, message),
				timer::sleep(RETRANSMISSION_INTERVAL));

			// check if still connected
			if (!isConnected()) {
				// make sure we are resumed from the event loop, not from ping()
				co_await timer::sleep(100ms);
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

	int length2 = array::length(message);
	co_await this->receiveWaitlist.wait(length2, message);

	// check if still connected
	if (!isConnected()) {
		// make sure we are resumed from the event loop, not from ping()
		co_await timer::sleep(100ms);
		result = Result::INVALID_STATE;
		co_return;
	}

	// check if message is complete
	if (length2 > MAX_MESSAGE_LENGTH) {
		result = Result::PROTOCOL_ERROR;
		co_return;
	}

	MessageReader r(length2, message);

	flags = r.enum8<mqttsn::Flags>();
	topicId = r.uint16();
	msgId = r.uint16();
	
	int l = r.getRemaining();
	if (l > length) {
		// error: message too long
		result = Result::INVALID_PARAMETER;
		length = 0;
	} else {
		length = l;
		array::copy(l, data, r.current);
		result = Result::OK;
	}
}

Awaitable<network::SendParameters> MqttSnClient::ackReceive(uint16_t msgId, uint16_t topicId, bool ok) {
	if (msgId == 0)
		return {};
		
	uint8_t message[8];

	MessageWriter w(message);
	w.enum8(mqttsn::MessageType::PUBACK);
	w.uint16(topicId);
	w.uint16(msgId);
	w.enum8(ok ? mqttsn::ReturnCode::ACCEPTED : mqttsn::ReturnCode::REJECTED_INVALID_TOPIC_ID);
	return network::send(NETWORK_MQTTSN, this->configuration->networkGateway, w.finish());
}

AwaitableCoroutine MqttSnClient::ping() {
	uint8_t message[4];

	while (true) {
		co_await timer::sleep(KEEP_ALIVE_INTERVAL);

		for (int retry = 0; ; ++retry) {
			// send idle ping
			{
				MessageWriter w(message);
				w.enum8<mqttsn::MessageType>(mqttsn::MessageType::PINGREQ);
				co_await network::send(NETWORK_MQTTSN, this->configuration->networkGateway, w.finish());
			}
			
			// wait for ping response
			{
				int length = array::length(message);
				int s = co_await select(this->ackWaitlist.wait(mqttsn::MessageType::PINGRESP, uint16_t(0), length, message),
					timer::sleep(RETRANSMISSION_INTERVAL));
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
		network::Endpoint source;
		int length = MAX_MESSAGE_LENGTH;
		co_await network::receive(NETWORK_MQTTSN, source, length, message);

		// check if the message is from the gateway
		// todo

		MessageReader r(message);
		
		// check if message is complete
		if (r.end - message > length) {
			continue;
		}
		
		auto msgType = r.enum8<mqttsn::MessageType>();

		// get message without length and type
		int l = r.end - r.current;
		uint8_t *m = r.current;

		// check if we read past the end of the message
		if (!r.isValid()) {
			continue;
		}
				
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
				msgId = r.uint16();
				break;
			case mqttsn::MessageType::SUBACK:
				r.skip(3);
				msgId = r.uint16();
				break;
			case mqttsn::MessageType::UNSUBACK:
				msgId = r.uint16();
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
