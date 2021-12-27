#include "MqttSnBroker.hpp"
#include <sys.hpp>


// default quality of service
constexpr int8_t QOS = 1;

// number of retries when a send fails
constexpr int MAX_RETRY = 2;

// min for qos (quality of service)
constexpr int8_t min(int8_t x, int8_t y) {return x < y ? x : y;}



MqttSnBroker::MqttSnBroker(uint16_t localPort)
{
	network::setLocalPort(NETWORK_MQTT, localPort);
	
	// init connections
	for (ConnectionInfo &connection : this->connections) {
		connection.endpoint.port = 0;
	}
	this->connectedFlags.clear();

	// init topics
	for (TopicInfo &topic : this->topics) {
		topic.hash = 0;
		topic.gatewayTopicId = 0;
	}
	
	// start receiving coroutine
	receive();
	publish();
}

AwaitableCoroutine MqttSnBroker::connect(network::Endpoint const &gatewayEndpoint, String name,
	bool cleanSession, bool willFlag)
{
	uint8_t message[6 + MAX_CLIENT_ID_LENGTH];
	
	// temporarily set connection to gateway as connected so that CONNACK passes in receive()
	this->connections[0].endpoint = gatewayEndpoint;
	this->connectedFlags.set(0, 1);
	
	for (int retry = 0; retry <= MAX_RETRY; ++retry) {
		// send connect message
		{
			auto flags = (cleanSession ? mqttsn::Flags::CLEAN_SESSION : mqttsn::Flags::NONE)
				| (willFlag ? mqttsn::Flags::WILL : mqttsn::Flags::NONE);
		
			MessageWriter w(message);
			w.enum8<mqttsn::MessageType>(mqttsn::MessageType::CONNECT);
			w.enum8(flags); // flags
			w.uint8(0x01); // protocol name/version
			w.uint16(KEEP_ALIVE_TIME.toSeconds());
			w.string(name);
//sys::out.write("send1...\n");
			co_await network::send(NETWORK_MQTT, this->connections[0].endpoint/*gatewayEndpoint*/, w.finish());
		}

		// wait for a reply from the gateway
		{
			int length = array::count(message);
			int s = co_await select(this->ackWaitlist.wait(uint8_t(0), mqttsn::MessageType::CONNACK,
				uint16_t(0), length, message), timer::sleep(RECONNECT_TIME));

			// check if we received a message
			if (s == 1) {
				// get topic id and return code
				MessageReader r(length, message);
				auto returnCode = r.enum8<mqttsn::ReturnCode>();

				// check if connect request was accepted
				if (returnCode == mqttsn::ReturnCode::ACCEPTED) {
					// set connected flag and reset qos for each topic
					// todo: don't clear everything if cleanSession is false
					initConnection(0);
					co_return;
				}
			}
		}
	}
	
	// mark connection to gateway as not connected again
	this->connectedFlags.set(0, 0);
}

AwaitableCoroutine MqttSnBroker::ping() {
	uint8_t message[4];
	
	// ping gateway as long as we are connected
	while (isGatewayConnected()) {
		co_await timer::sleep(KEEP_ALIVE_TIME);

		int retry;
		for (retry = 0; retry < MAX_RETRY; ++retry) {
			// send idle ping
			{
				MessageWriter w(message);
				w.enum8<mqttsn::MessageType>(mqttsn::MessageType::PINGREQ);
				co_await network::send(NETWORK_MQTT, this->connections[0].endpoint, w.finish());
			}
			
			// wait for ping response
			{
				int length = array::count(message);
				int s = co_await select(this->ackWaitlist.wait(uint8_t(0), mqttsn::MessageType::PINGRESP,
					uint16_t(0), length, message), timer::sleep(RETRANSMISSION_TIME));
				if (s == 1)
					break;
			}
		}

		// after max retry, assume we are disconnected
		if (retry == MAX_RETRY) {
			this->connectedFlags.set(0, 0);
			break;
		}
	}
}

void MqttSnBroker::addPublisher(String topicName, Publisher &publisher) {
	int topicIndex = getTopicIndex(topicName);
	if (topicIndex == -1) {
		// error: topic list is full
		return;
	}
	publisher.index = topicIndex;
/*
	TopicInfo &topic = this->topics[topicIndex];

	// register topic at gateway
	if (topic.gatewayTopicId == 0 && isGatewayConnected()) {
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
				w.string(topicName);
				co_await network::send(NETWORK_MQTT, this->connections[0].endpoint, w.finish());
			}

			// wait for acknowledge from gateway
			{
				int length = array::count(message);
				int s = co_await select(this->ackWaitlist.wait(uint8_t(0), mqttsn::MessageType::REGACK, msgId, length,
					message), timer::sleep(RETRANSMISSION_TIME));
				
				// check if still connected
				if (!isGatewayConnected())
					break;

				// check if we received a message
				if (s == 1) {
					// get topic id and return code
					MessageReader r(length, message);
					auto topicId = r.uint16();
					r.skip(2); // msgId
					auto returnCode = r.enum8<mqttsn::ReturnCode>();
					
					// check if successful
					if (r.isValid() && returnCode == mqttsn::ReturnCode::ACCEPTED) {
						topic.gatewayTopicId = topicId;
						break;
					}
				}
			}
		}
	}
*/
	// remove the publisher in case this was called a 2nd time e.g. after reconnect to gateway
	publisher.remove();
	publisher.event = &this->publishEvent;
	this->publishers.add(publisher);
}

AwaitableCoroutine MqttSnBroker::registerPublisher(String topicName) {
	int topicIndex = getTopicIndex(topicName);
	if (topicIndex == -1) {
		// error: topic list is full
		co_return;
	}
	
	TopicInfo &topic = this->topics[topicIndex];

	// register topic at gateway
	if (topic.gatewayTopicId == 0 && isGatewayConnected()) {
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
				w.string(topicName);
//sys::out.write("send1...\n");
				co_await network::send(NETWORK_MQTT, this->connections[0].endpoint, w.finish());
//sys::out.write(hex(size_t(&x.element)) + ' ' + hex(size_t(x.element.next)) + '\n');
			}

			// wait for acknowledge from gateway
			{
				int length = array::count(message);
				int s = co_await select(this->ackWaitlist.wait(uint8_t(0), mqttsn::MessageType::REGACK, msgId, length,
					message), timer::sleep(RETRANSMISSION_TIME));
				
				// check if still connected
				if (!isGatewayConnected())
					break;

				// check if we received a message
				if (s == 1) {
					// get topic id and return code
					MessageReader r(length, message);
					auto topicId = r.uint16();
					r.skip(2); // msgId
					auto returnCode = r.enum8<mqttsn::ReturnCode>();
					
					// check if successful
					if (r.isValid() && returnCode == mqttsn::ReturnCode::ACCEPTED) {
						topic.gatewayTopicId = topicId;
						topic.qosArray.set(0, QOS);
						break;
					}
				}
			}
		}
	}
}

void MqttSnBroker::addSubscriber(String topicName, Subscriber &subscriber) {
	int topicIndex = getTopicIndex(topicName);
	if (topicIndex == -1) {
		// error: topic list is full
		return;
	}
	subscriber.index = topicIndex;
/*
	TopicInfo &topic = this->topics[topicIndex];

	// subscribe to topic on gateway
	if (topic.qosArray.get(0) == 3 && isGatewayConnected()) {
		uint8_t message[MAX_MESSAGE_LENGTH];

		// generate message id
		uint16_t msgId = getNextMsgId();

		for (int retry = 0; retry <= MAX_RETRY; ++retry) {
			// send subscribe message
			{
				MessageWriter w(message);
				w.enum8(mqttsn::MessageType::SUBSCRIBE);
				auto flags = mqttsn::Flags::TOPIC_TYPE_NORMAL | mqttsn::makeQos(QOS);
				w.enum8(flags);
				w.uint16(msgId);
				w.string(topicName);
				co_await network::send(NETWORK_MQTT, this->connections[0].endpoint, w.finish());
			}

			// wait for acknowledge from gateway
			{
				int length = array::count(message);
				int s = co_await select(this->ackWaitlist.wait(uint8_t(0), mqttsn::MessageType::SUBACK, msgId, length,
					message), timer::sleep(RETRANSMISSION_TIME));

				// check if still connected
				if (!isGatewayConnected())
					break;

				// check if we received a message
				if (s == 1) {
					MessageReader r(length, message);
						
					// get flags, topic id and return code
					auto flags = r.enum8<mqttsn::Flags>();
					auto qos = mqttsn::getQos(flags);
					auto topicId = r.uint16();
					r.skip(2); // msgId
					auto returnCode = r.enum8<mqttsn::ReturnCode>();

					// check if successful
					if (r.isValid() && returnCode == mqttsn::ReturnCode::ACCEPTED) {
						topic.gatewayTopicId = topicId;
						topic.qosArray.set(0, qos);
						break;
					}
				}
			}
		}
	}
*/
	// remove the subscriber in case this was called a 2nd time e.g. after reconnect to gateway
	subscriber.remove();
	this->subscribers.add(subscriber);
}

AwaitableCoroutine MqttSnBroker::registerSubscriber(String topicName) {
	int topicIndex = getTopicIndex(topicName);
	if (topicIndex == -1) {
		// error: topic list is full
		co_return;
	}

	TopicInfo &topic = this->topics[topicIndex];

	// subscribe to topic on gateway
	if (topic.qosArray.get(0) == 3 && isGatewayConnected()) {
		uint8_t message[MAX_MESSAGE_LENGTH];

		// generate message id
		uint16_t msgId = getNextMsgId();

		for (int retry = 0; retry <= MAX_RETRY; ++retry) {
			// send subscribe message
			{
				MessageWriter w(message);
				w.enum8(mqttsn::MessageType::SUBSCRIBE);
				auto flags = mqttsn::Flags::TOPIC_TYPE_NORMAL | mqttsn::makeQos(QOS);
				w.enum8(flags);
				w.uint16(msgId);
				w.string(topicName);
				co_await network::send(NETWORK_MQTT, this->connections[0].endpoint, w.finish());
			}

			// wait for acknowledge from gateway
			{
				int length = array::count(message);
				int s = co_await select(this->ackWaitlist.wait(uint8_t(0), mqttsn::MessageType::SUBACK, msgId, length,
					message), timer::sleep(RETRANSMISSION_TIME));

				// check if still connected
				if (!isGatewayConnected())
					break;

				// check if we received a message
				if (s == 1) {
					MessageReader r(length, message);
						
					// get flags, topic id and return code
					auto flags = r.enum8<mqttsn::Flags>();
					auto qos = mqttsn::getQos(flags);
					auto topicId = r.uint16();
					r.skip(2); // msgId
					auto returnCode = r.enum8<mqttsn::ReturnCode>();

					// check if successful
					if (r.isValid() && returnCode == mqttsn::ReturnCode::ACCEPTED) {
						topic.gatewayTopicId = topicId;
						topic.qosArray.set(0, qos);
						break;
					}
				}
			}
		}
	}
}

void MqttSnBroker::initConnection(int connectionIndex) {
	// set connected flag
	this->connectedFlags.set(connectionIndex, 1);

	// reset qos entry of this connection for all topics
	for (int i = 0; i < this->topicCount; ++i) {
		this->topics[i].qosArray.set(connectionIndex, 3);
	}
}

int MqttSnBroker::getTopicIndex(String name, bool add) {
	// calc djb2 hash of name
	// http://www.cse.yorku.ca/~oz/hash.html
	uint32_t hash = 5381;
	for (char c : name) {
		hash = (hash << 5) + hash + uint8_t(c); // hash * 33 + c
    }
	
	// search for topic
	int empty = -1;
	for (int i = this->topicCount - 1; i >= 0; --i) {
		TopicInfo &topic = this->topics[i];
		if (topic.hash == hash)
			return i;
		if (topic.hash == 0)
			empty = i;
	}
	if (!add)
		return -1;
			
	// add a new topic if no empty topic was found
	if (empty == -1) {
		if (this->topicCount >= MAX_TOPIC_COUNT) {
			// error: list of topics is full
			return -1;
		}
		empty = this->topicCount++;
	}
	
	// initialize topic
	TopicInfo &topic = this->topics[empty];
	topic.hash = hash;
	topic.qosArray.set();
	//for (uint32_t &a : topic.qosArray)
	//	a = 0xffffffff;
	topic.gatewayTopicId = 0;
	
	return empty;
}

/*
static int getLowestBitPosition(uint32_t x) {
	int pos = 0;
	if ((x & 0xffff) == 0) {
		x >>= 16;
		pos += 16;
	}
	if ((x & 0xff) == 0) {
		x >>= 8;
		pos += 8;
	}
	if ((x & 0xf) == 0) {
		x >>= 4;
		pos += 4;
	}
	if ((x & 3) == 0) {
		x >>= 2;
		pos += 2;
	}
	if ((x & 1) == 0) {
		x >>= 1;
		pos += 1;
	}
	return pos;
}
*/
static bool writeMessage(MqttSnBroker::MessageWriter &w, MessageType srcType, void const *srcMessage) {
	Message const &src = *reinterpret_cast<Message const *>(srcMessage);
	static char const onOff[] = {'0', '1', '!'};
	static char const trigger[] = {'#', '!'};
	static char const upDown[] = {'#', '+', '-'};

	switch (srcType) {
	case MessageType::ON_OFF:
		w.uint8(onOff[src.onOff]);
		break;
	case MessageType::ON_OFF2:
		// invert on/off (0, 1, 2 -> 1, 0, 2)
		w.uint8(onOff[src.onOff ^ 1 ^ (src.onOff >> 1)]);
		break;
	case MessageType::TRIGGER:
	case MessageType::TRIGGER2:
		w.uint8(trigger[src.trigger]);
		break;
	case MessageType::UP_DOWN:
		w.uint8(upDown[src.upDown]);
		break;
	case MessageType::UP_DOWN2:
		// invert up/down (0, 1, 2 -> 0, 2, 1)
		w.uint8(upDown[(src.upDown << 1) | (src.upDown >> 1)]);
		break;
	case MessageType::LEVEL:
	case MessageType::MOVE_TO_LEVEL:
		{
			// check if relative (increment, decrement)
			if (src.level.getFlag())
				w.uint8('!');
			w.floatString(src.level, 1, 3);

			if (srcType == MessageType::MOVE_TO_LEVEL) {
				w.uint8(' ');
				w.floatString(src.moveToLevel.move, 1, 3);
				if (src.moveToLevel.move.getFlag()) {
					// speed
					w.uint8('/');
				}
				w.uint8('s');
			}
		}
		break;
	default:
		// conversion failed
		return false;
	}
	
	// conversion successful
	return true;
}

struct MessageValue {
	String message;
	int value;
};

static int find(String message, Array<MessageValue const> messageValues) {
	for (MessageValue const &mv : messageValues) {
		if (mv.message == message)
			return mv.value;
	}
	return -1;
}

static bool readMessage(MessageType dstType, void *dstMessage, DataReader r) {
	Message &dst = *reinterpret_cast<Message *>(dstMessage);
	static MessageValue const onOff[] = {
		{"0", 0}, {"1", 1}, {"!", 2},
		{"off", 0}, {"on", 1}, {"toggle", 2}};
	static MessageValue const trigger[] = {
		{"#", 0}, {"!", 1},
		{"inactive", 0}, {"active", 1}};
	static MessageValue const upDown[] = {
		{"#", 0}, {"+", 1}, {"-", 2},
		{"inactive", 0}, {"up", 1}, {"down", 2}};

	switch (dstType) {
	case MessageType::UNKNOWN:
		return false;

	case MessageType::ON_OFF:
		{
			int v = find(r.string(), onOff);
			if (v == -1)
				return false; // conversion failed
			dst.onOff = v;
		}
		break;
	case MessageType::ON_OFF2:
		{
			int v = find(r.string(), onOff);
			if (v == -1)
				return false; // conversion failed
			// invert on/off (0, 1, 2 -> 1, 0, 2)
			dst.onOff = v ^ 1 ^ (v >> 1);
		}
		break;
	
	case MessageType::TRIGGER:
	case MessageType::TRIGGER2:
		{
			int v = find(r.string(), trigger);
			if (v == -1)
				return false; // conversion failed
			dst.trigger = v;
		}
		break;
	
	case MessageType::UP_DOWN:
		{
			int v = find(r.string(), upDown);
			if (v == -1)
				return false; // conversion failed
			dst.upDown = v;
		}
		break;
	case MessageType::UP_DOWN2:
		{
			int v = find(r.string(), upDown);
			if (v == -1)
				return false; // conversion failed
			// invert up/down (0, 1, 2 -> 0, 2, 1)
			dst.upDown = (v << 1) | (v >> 1);
		}
		break;
	
	case MessageType::LEVEL:
	case MessageType::MOVE_TO_LEVEL:
		{
			// check if relative (increment, decrement)
			bool relative = *r.current == '!';
			if (relative)
				r.skip(1);
			String levelString = r.floatString();
			if (levelString.isEmpty())
				return false; // conversion failed
			auto level = parseFloat(levelString);
			if (!level)
				return false; // conversion failed
			dst.level.set(*level, relative);
			
			if (dstType == MessageType::MOVE_TO_LEVEL) {
				r.skipSpace();
				String moveString = r.floatString();
				if (moveString.isEmpty()) {
					dst.moveToLevel.move = 0.0f;
				} else {
					auto move = parseFloat(moveString);
					if (!move)
						return false; // conversion failed
					bool rate = *r.current == '/';
					if (rate)
						r.skip(1);
					if (*r.current != 's')
						return false; // conversion failed
					dst.moveToLevel.move.set(*move, rate);
				}
			}
		}
		break;
		
	default:
		// conversion failed
		return false;
	}
	
	// conversion successful
	return true;
}

Coroutine MqttSnBroker::publish() {
	uint8_t message[MAX_MESSAGE_LENGTH];
	while (true) {
		// wait until something was published
		co_await this->publishEvent.wait();
		
		// check if there is a current publisher
		if (this->currentPublisher != nullptr) {
			int connectionIndex;
			while ((connectionIndex = (this->dirtyFlags & this->connectedFlags).findFirstNonzero()) != -1) {
				Publisher &publisher = *this->currentPublisher;
				
				// clear flag
				this->dirtyFlags.set(connectionIndex, 0);
				
				// get connection info
				ConnectionInfo &connection = this->connections[connectionIndex];

				// get topic info
				TopicInfo &topic = this->topics[publisher.index];

				// get quality of service (3 is not subscribed)
				int qos = topic.qosArray.get(connectionIndex);
				if (qos != 3) {
					// generate message id
					uint16_t msgId = qos <= 0 ? 0 : getNextMsgId();

					// message flags
					auto flags = mqttsn::makeQos(qos);
					
					// topic id
					uint16_t topicId = connectionIndex == 0 ? topic.gatewayTopicId : publisher.index + 1;

					// send to gateway or client
					for (int retry = 0; retry <= MAX_RETRY; ++retry) {
						// send publish message
						{
							MessageWriter w(message);
							w.enum8(mqttsn::MessageType::PUBLISH);
							w.enum8(flags);
							w.uint16(topicId);
							w.uint16(msgId);

							// message data
							if (!writeMessage(w, publisher.messageType, publisher.message))
								break;
							
							co_await network::send(NETWORK_MQTT, connection.endpoint, w.finish());
						}
		
						if (qos <= 0)
							break;
						
						// wait for acknowledge from other end of connection (client or gateway)
						{
							int length = array::count(message);
							int s = co_await select(this->ackWaitlist.wait(uint8_t(connectionIndex),
								mqttsn::MessageType::PUBACK, msgId, length, message),
								timer::sleep(RETRANSMISSION_TIME));

							// check if still connected
							if (!isConnected(connectionIndex))
								break;

							// check if we received a message
							if (s == 1) {
								MessageReader r(length, message);
								
								// get topic id and return code
								topicId = r.uint16();
								r.skip(2); // msgId
								auto returnCode = r.enum8<mqttsn::ReturnCode>();
								
								// check if successful
								if (r.isValid() && returnCode == mqttsn::ReturnCode::ACCEPTED)
									break;
							}
						}
						
						// set duplicate flag and try again
						flags |= mqttsn::Flags::DUP;
					}
				}
			}
			this->currentPublisher = nullptr;
		}
		
		// iterate over publishers
		//bool dirty = false;
		for (auto &publisher : this->publishers) {
			// check if publisher wants to publish
			if (publisher.dirty) {
				publisher.dirty = false;
				
				// forward to subscribers
				for (auto &subscriber : this->subscribers) {
					if (subscriber.index == publisher.index) {
						subscriber.barrier->resumeAll([&subscriber, &publisher] (Subscriber::Parameters &p) {
							p.subscriptionIndex = subscriber.subscriptionIndex;
							
							// convert to target unit and type and resume coroutine if conversion was successful
							return convert(subscriber.messageType, p.message,
								publisher.messageType, publisher.message);
						});
					}
				}
				
				this->currentPublisher = &publisher;
				this->dirtyFlags.set();
				break;
			}
		}
		
		// clear event when no dirty publisher was found
		if (this->currentPublisher == nullptr)
			this->publishEvent.clear();
/*
			
			int connectionIndex;
			while ((connectionIndex = (publisher.dirtyFlags & this->connectedFlags).findFirstNonzero()) != -1) {
				// clear flag
				publisher.dirtyFlags.set(connectionIndex, 0);
				
				// get connection info
				ConnectionInfo &connection = this->connections[connectionIndex];

				// get topic info
				TopicInfo &topic = this->topics[publisher.topicIndex];

				// get quality of service (3 is not subscribed)
				int qos = topic.qosArray.get(connectionIndex);
				if (qos != 3) {
					// generate message id
					uint16_t msgId = qos <= 0 ? 0 : getNextMsgId();

					// message flags
					auto flags = mqttsn::makeQos(qos);
					
					// topic id
					uint16_t topicId = connectionIndex == 0 ? topic.gatewayTopicId : publisher.topicIndex + 1;

					// send to gateway or client
					for (int retry = 0; retry <= MAX_RETRY; ++retry) {
						// send publish message
						{
							MessageWriter w(message);
							w.enum8(mqttsn::MessageType::PUBLISH);
							w.enum8(flags);
							w.uint16(topicId);
							w.uint16(msgId);

							// message data
							if (!writeMessage(w, publisher.messageType, publisher.message))
								break;
							
							co_await network::send(NETWORK_MQTT, connection.endpoint, w.finish());
						}
		
						if (qos <= 0)
							break;
						
						// wait for acknowledge from other end of connection (client or gateway)
						{
							int length = array::count(message);
							int s = co_await select(this->ackWaitlist.wait(uint8_t(connectionIndex),
								mqttsn::MessageType::PUBACK, msgId, length, message),
								timer::sleep(RETRANSMISSION_TIME));

							// check if still connected
							if (!connection.isConnected())
								break;

							// check if we received a message
							if (s == 1) {
								MessageReader r(length, message);
								
								// get topic id and return code
								topicId = r.uint16();
								r.skip(2); // msgId
								auto returnCode = r.enum8<mqttsn::ReturnCode>();
								
								// check if successful
								if (r.isValid() && returnCode == mqttsn::ReturnCode::ACCEPTED)
									break;
							}
						}
						
						// set duplicate flag and try again
						flags |= mqttsn::Flags::DUP;
					}
				}
				
				// forward to subscribers
				if (connectionIndex == 0) {
					for (auto &subscriber : this->subscribers) {
						if (subscriber.topicIndex == publisher.topicIndex) {
							subscriber.barrier->resumeAll([&subscriber, &publisher] (Subscriber::Parameters &p) {
								p.subscriptionIndex = subscriber.subscriptionIndex;
								
								// convert to target unit and type and resume coroutine if conversion was successful
								return convert(subscriber.messageType, p.message,
									publisher.messageType, publisher.message);
							});
						}
					}
				}
				//publisher.dirtyFlags[i] = 0;
			}
		}
		
		// clear event when no dirty publisher was found
		if (!dirty)
			this->publishEvent.clear();
*/
	}
}
/*
Coroutine MqttSnBroker::ping() {
	uint8_t message[6 + MAX_CLIENT_ID_LENGTH];

	while (true) {
		while (true) {
			// send connect message
			{
				MessageWriter w(message);
				w.enum8<mqttsn::MessageType>(mqttsn::MessageType::CONNECT);
				w.uint8(0); // flags
				w.uint8(0x01); // protocol name/version
				w.uint16(KEEP_ALIVE_TIME.toSeconds());
				w.string(this->connections[0].name);
				co_await network::send(NETWORK_MQTT, this->connections[0].endpoint, w.finish());
			}

			// wait for a reply from the gateway
			{
				int length = array::count(message);
				int s = co_await select(this->ackWaitlist.wait(uint8_t(0), mqttsn::MessageType::CONNACK,
					uint16_t(0), length, message), timer::sleep(RECONNECT_TIME));

				// check if we received a message
				if (s == 1) {
					// get topic id and return code
					MessageReader r(length, message);
					auto returnCode = r.enum8<mqttsn::ReturnCode>();

					// check if connect request was accepted
					if (returnCode == mqttsn::ReturnCode::ACCEPTED)
						break;
				}
			}
		}
		
		// set connected flag and reset qos for each topic
		initConnection(0);
		
		// ping gateway as long as we are connected
		while (isGatewayConnected()) {
			co_await timer::sleep(KEEP_ALIVE_TIME);

			int retry;
			for (retry = 0; retry < MAX_RETRY; ++retry) {
				// send idle ping
				{
					MessageWriter w(message);
					w.enum8<mqttsn::MessageType>(mqttsn::MessageType::PINGREQ);
					co_await network::send(NETWORK_MQTT, this->connections[0].endpoint, w.finish());
				}
				
				// wait for ping response
				{
					int length = array::count(message);
					int s = co_await select(this->ackWaitlist.wait(uint8_t(0), mqttsn::MessageType::PINGRESP,
						uint16_t(0), length, message), timer::sleep(RETRANSMISSION_TIME));
					if (s == 1)
						break;
				}
			}

			// after max retry, assume we are disconnected
			if (retry == MAX_RETRY) {
				this->connectedFlags.set(0, 0);
				break;
			}
		}
	}
}
*/
Coroutine MqttSnBroker::receive() {
	uint8_t message[MAX_MESSAGE_LENGTH];

	while (true) {
		// receive a message from the gateway
		network::Endpoint source;
		int length = MAX_MESSAGE_LENGTH;
		co_await network::receive(NETWORK_MQTT, source, length, message);

		MessageReader r(message);
		
		// check if message is complete
		if (r.end - message > length)
			continue;
		
		// get message type
		auto msgType = r.enum8<mqttsn::MessageType>();

		// check if we read past the end of the message
		if (!r.isValid()) {
			continue;
		}
				
		// search connection
		int connectionIndex = -1;
		for (int i = 0; i < MAX_CONNECTION_COUNT; ++i) {
			if (this->isConnected(i))
				connectionIndex = i;
			
			if (this->isConnected(i) && this->connections[i].endpoint == source) {
				connectionIndex = i;
				break;
			}
		}

		if (msgType == mqttsn::MessageType::CONNECT) {
			// a client wants to (re)connect
			if (connectionIndex == -1) {
				// try to find an unused connection
				connectionIndex = (~this->connectedFlags).set(0, 1).findFirstNonzero();
			}

			auto returnCode = mqttsn::ReturnCode::ACCEPTED;
			if (connectionIndex == -1) {
				// error: list of connections is full
				returnCode = mqttsn::ReturnCode::REJECTED_CONGESTED;
			} else {
				// initialize the connection
				this->connections[connectionIndex].endpoint = source;
				initConnection(connectionIndex);
			}
			
			// reply with CONNACK
			MessageWriter w(message);
			w.enum8(mqttsn::MessageType::CONNACK);
			w.enum8(returnCode);
			co_await network::send(NETWORK_MQTT, source, w.finish());
		} else {
			if (connectionIndex == -1) {
				// unknown client: send back a disconnect message
				MessageWriter w(message);
				w.enum8<mqttsn::MessageType>(mqttsn::MessageType::DISCONNECT);
				co_await network::send(NETWORK_MQTT, source, w.finish());
			} else if (msgType == mqttsn::MessageType::REGISTER) {
				// the gateway or a client want to register a topic
				uint16_t topicId = r.uint16();
				uint16_t msgId = r.uint16();
				String topicName = r.string();
				
				// register the topic
				auto returnCode = mqttsn::ReturnCode::ACCEPTED;
				int topicIndex = getTopicIndex(topicName);
				if (topicIndex == -1) {
					// error: out of topic ids
					returnCode = mqttsn::ReturnCode::REJECTED_CONGESTED;
				} else {
					if (connectionIndex == 0) {
						// gateway: set gateway topic id
						this->topics[topicIndex].gatewayTopicId = topicId;
					} else {
						// client: return our topic id to client
						topicId = topicIndex + 1;
					}
				}
				
				// reply with REGACK
				MessageWriter w(message);
				w.enum8(mqttsn::MessageType::REGACK);
				w.uint16(topicId);
				w.uint16(msgId);
				w.enum8(returnCode);
				co_await network::send(NETWORK_MQTT, source, w.finish());
			} else if (msgType == mqttsn::MessageType::SUBSCRIBE) {
				// a client wants to subscribe to a topic
				auto flags = r.enum8<mqttsn::Flags>();
				int8_t qos = min(mqttsn::getQos(flags), QOS);
				uint16_t msgId = r.uint16();
				String topicName = r.string();
				
				// register the topic
				uint16_t topicId = 0;
				auto returnCode = mqttsn::ReturnCode::ACCEPTED;
				if (connectionIndex == 0) {
					// gateway: the gateway can't subscribe to topics
					returnCode = mqttsn::ReturnCode::NOT_SUPPORTED;
				} else {
					int topicIndex = getTopicIndex(topicName);
					if (topicIndex == -1) {
						// error: out of topic ids
						returnCode = mqttsn::ReturnCode::REJECTED_CONGESTED;
					} else {
						// client: set qos and return our topic id to client
						this->topics[topicIndex].qosArray.set(connectionIndex, qos);
						topicId = topicIndex + 1;
					}
				}
				
				// reply with SUBACK
				MessageWriter w(message);
				w.enum8(mqttsn::MessageType::SUBACK);
				w.enum8(mqttsn::makeQos(qos));
				w.uint16(topicId);
				w.uint16(msgId);
				w.enum8(returnCode);
				co_await network::send(NETWORK_MQTT, source, w.finish());
			} else if (msgType == mqttsn::MessageType::PUBLISH) {
				// the gateway or a client published to a topic
				
				ConnectionInfo &connection = this->connections[connectionIndex];

				// read message
				auto flags = r.enum8<mqttsn::Flags>();
				int8_t qos = mqttsn::getQos(flags);
				uint16_t topicId = r.uint16();
				uint16_t msgId = r.uint16();
				int pubLength = r.getRemaining();
				uint8_t *pubData = r.current;
				
				// get topic index
				uint16_t topicIndex;
				if (connectionIndex == 0) {
					// connection to gateway
					// todo: optimize linear search
					for (topicIndex = 0; topicIndex < this->topicCount; ++topicIndex) {
						if (this->topics[topicIndex].gatewayTopicId == topicId) {
							break;
						}
					}
				} else {
					// connection to a client
					topicIndex = topicId - 1;
				}
				TopicInfo &topic = this->topics[topicIndex];
				bool topicOk = topicIndex < MAX_TOPIC_COUNT;
				if (topicOk)
					topicOk = topic.hash != 0;
				
				// acknowledge or report error
				if (qos >= 1 || !topicOk) {
					uint8_t message2[8];

					MessageWriter w(message2);
					w.enum8(mqttsn::MessageType::PUBACK);
					w.uint16(topicId);
					w.uint16(msgId);
					w.enum8(topicOk ? mqttsn::ReturnCode::ACCEPTED : mqttsn::ReturnCode::REJECTED_INVALID_TOPIC_ID);
					co_await network::send(NETWORK_MQTT, connection.endpoint, w.finish());
				}
				
				// check if topic index is valid
				if (!topicOk)
					continue;
				
				// publish to subscribers
				for (auto &subscriber : this->subscribers) {
					// check if this is the right topic
					if (subscriber.index == topicIndex) {
						subscriber.barrier->resumeFirst([&subscriber, &r] (Subscriber::Parameters &p) {
							p.subscriptionIndex = subscriber.subscriptionIndex;

							// read message (note r is passed by value for multiple subscribers)
							return readMessage(subscriber.messageType, p.message, r);
						});
					}
				}
							
				// publish to other connections
				for (int connectionIndex2 = 0; connectionIndex2 < MAX_CONNECTION_COUNT; ++connectionIndex2) {
					ConnectionInfo &connection2 = this->connections[connectionIndex2];

					// get quality of service (3 is not subscribed)
					int qos2 = topic.qosArray.get(connectionIndex2);

					// don't publish on connection over which we received the message
					if (connectionIndex2 != connectionIndex && isConnected(connectionIndex2) && qos2 != 3) {
						// generate message id
						uint16_t msgId2 = qos2 <= 0 ? 0 : getNextMsgId();

						// message flags
						auto flags2 = mqttsn::makeQos(qos2);
						
						// topic id
						uint16_t topicId2 = connectionIndex2 == 0 ? topic.gatewayTopicId : topicIndex + 1;

						// send to gateway or client
						for (int retry = 0; retry <= MAX_RETRY; ++retry) {
							// send publish message
							{
								MessageWriter w(message);
								w.enum8(mqttsn::MessageType::PUBLISH);
								w.enum8(flags2);
								w.uint16(topicId2);
								w.uint16(msgId2);
								w.data(pubLength, pubData);
								co_await network::send(NETWORK_MQTT, connection.endpoint, w.finish());
							}
			
							if (qos2 <= 0)
								break;
							
							// wait for acknowledge from other end of connection (client or gateway)
							{
								int length = array::count(message);
								int s = co_await select(this->ackWaitlist.wait(uint8_t(connectionIndex),
									mqttsn::MessageType::PUBACK, msgId, length, message),
									timer::sleep(RETRANSMISSION_TIME));

								// check if still connected
								if (!isConnected(connectionIndex2))
									break;

								// check if we received a message
								if (s == 1) {
									MessageReader r(length, message);
									
									// get topic id and return code
									topicId = r.uint16();
									r.skip(2); // msgId
									auto returnCode = r.enum8<mqttsn::ReturnCode>();
									
									// check if successful
									if (r.isValid() && topicId == topicId2 && returnCode == mqttsn::ReturnCode::ACCEPTED)
										break;
								}
							}
							
							// set duplicate flag and try again
							flags |= mqttsn::Flags::DUP;
						}
					}
				}
			} else {
				// handle acknowledge and disconnect messages
				
				// get message without length and type
				int l = r.end - r.current;
				uint8_t *m = r.current;
				
				uint16_t msgId;
				switch (msgType) {
				case mqttsn::MessageType::CONNACK:
				case mqttsn::MessageType::PINGRESP:
					msgId = 0;
					break;
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
				case mqttsn::MessageType::DISCONNECT:
					// disconnect
					this->connectedFlags.set(0, 0);
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
				this->ackWaitlist.resumeOne([connectionIndex, msgType, msgId, l, m](AckParameters &p) {
					// check message type and id
					if (p.connectionIndex == connectionIndex && p.msgType == msgType && p.msgId == msgId) {
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
}
