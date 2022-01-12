#include "MqttSnBroker.hpp"

#define DEBUG_PRINT

#ifdef DEBUG_PRINT
#include <sys.hpp>
#endif


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

	// start coroutines
	for (int i = 0; i < PUBLISH_COUNT; ++i)
		publish();
	for (int i = 0; i < RECEIVE_COUNT; ++i)
		receive();
	for (int i = 0; i < FORWARD_COUNT; ++i)
		forward();
}

AwaitableCoroutine MqttSnBroker::connect(network::Endpoint const &gatewayEndpoint, String name,
	bool cleanSession, bool willFlag)
{
	uint8_t message[6 + MAX_CLIENT_ID_LENGTH];
	ConnectionInfo &gateway = this->connections[0];

	// reset granted qos and topic id for connection to gateway
	for (auto [topicName, topic] : this->topics) {
		topic.qosArray.set(0, 3);
		topic.gatewayTopicId = 0;
	}

	// temporarily set connection to gateway as connected so that CONNACK passes in receive()
	gateway.endpoint = gatewayEndpoint;
	gateway.name = name;
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
			co_await network::send(NETWORK_MQTT, gateway.endpoint, w.finish());
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

					// set connected flag
					this->connectedFlags.set(0, 1);

					co_return;
				}
			}
		}
	}

	// mark connection to gateway as not connected again
	this->connectedFlags.set(0, 0);
}

AwaitableCoroutine MqttSnBroker::keepAlive() {
	uint8_t message[MAX_MESSAGE_LENGTH];
	ConnectionInfo &gateway = this->connections[0];

	// set event so that all existing topics get registered/subscribed immediately
	this->keepAliveEvent.set();

	// ping gateway as long as we are connected
	while (isGatewayConnected()) {
		int s = co_await select(timer::sleep(KEEP_ALIVE_TIME), this->keepAliveEvent.wait());

		if (s == 1) {
			if (isGatewayConnected()) {
				int retry;
				for (retry = 0; retry <= MAX_RETRY; ++retry) {
					// send idle ping
					{
						MessageWriter w(message);
						w.enum8<mqttsn::MessageType>(mqttsn::MessageType::PINGREQ);
						co_await network::send(NETWORK_MQTT, gateway.endpoint, w.finish());
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

				// if maximum number of retries is exceeded, we assume to be disconnected from the gateway
				if (retry > MAX_RETRY) {
					this->connectedFlags.set(0, 0);
					break;
				}
			}
		} else {
			// register/subscribe topics at gateway
			this->keepAliveEvent.clear();
			for (auto [topicName, topic] : this->topics) {
				bool clientSubscribed = topic.isClientSubscribed();

				// determine action for this topic
				bool reg = !topic.isRegisteredAtGateway();
				bool sub = clientSubscribed && !topic.isSubscribedAtGateway();
				bool unsub = !clientSubscribed && topic.isSubscribedAtGateway();
				if (isGatewayConnected() && (reg || sub || unsub)) {
					// generate message id
					uint16_t msgId = getNextMsgId();

					int retry;
					if (sub) {
						// subscribe
						for (retry = 0; retry <= MAX_RETRY; ++retry) {
							// send subscribe message
							{
								MessageWriter w(message);
								w.enum8(mqttsn::MessageType::SUBSCRIBE);
								auto flags = mqttsn::Flags::TOPIC_TYPE_NORMAL | mqttsn::makeQos(QOS);
								w.enum8(flags);
								w.uint16(msgId);
								w.string(topicName);
								co_await network::send(NETWORK_MQTT, gateway.endpoint, w.finish());
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

										// set quality of service level granted by the gateway
										topic.qosArray.set(0, qos);
										break;
									}
								}
							}
						}
					} else if (unsub) {
						// unsubscribe
						for (retry = 0; retry <= MAX_RETRY; ++retry) {
							// send subscribe message
							{
								MessageWriter w(message);
								w.enum8(mqttsn::MessageType::UNSUBSCRIBE);
								auto flags = mqttsn::Flags::TOPIC_TYPE_NORMAL;
								w.enum8(flags);
								w.uint16(msgId);
								w.string(topicName);
								co_await network::send(NETWORK_MQTT, gateway.endpoint, w.finish());
							}

							// wait for acknowledge from gateway
							{
								int length = array::count(message);
								int s = co_await select(this->ackWaitlist.wait(uint8_t(0), mqttsn::MessageType::UNSUBACK, msgId, length,
									message), timer::sleep(RETRANSMISSION_TIME));

								// check if still connected
								if (!isGatewayConnected())
									break;

								// check if we received a message
								if (s == 1) {
									MessageReader r(length, message);
									r.skip(2); // msgId

									// check if successful
									if (r.isValid()) {
										// reset quality of service level granted by the gateway
										topic.qosArray.set(0, 3);
										break;
									}
								}
							}
						}
					} else {
						// register
						for (retry = 0; retry <= MAX_RETRY; ++retry) {
							// send register message
							{
								MessageWriter w(message);
								w.enum8<mqttsn::MessageType>(mqttsn::MessageType::REGISTER);
								w.uint16(0); // topic id not known yet
								w.uint16(msgId);
								w.string(topicName);
								co_await network::send(NETWORK_MQTT, gateway.endpoint, w.finish());
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

					// if maximum number of retries is exceeded, we assume to be disconnected from the gateway
					if (retry > MAX_RETRY) {
						this->connectedFlags.set(0, 0);
						break;
					}
				}
			}
		}
	}
}

void MqttSnBroker::addPublisher(String topicName, Publisher &publisher) {
	int topicIndex = obtainTopicIndex(topicName);
	if (topicIndex == -1) {
		// error: topic list is full
		return;
	}

	// remove the publisher in case this was called a 2nd time e.g. after reconnect to gateway
	publisher.remove();
	publisher.index = topicIndex;
	publisher.event = &this->publishEvent;
	this->publishers.add(publisher);

	TopicInfo &topic = this->topics[topicIndex];

	// wake up keepAlive() unless already registered at gateway
	if (topic.gatewayTopicId == 0)
		this->keepAliveEvent.set();
}

void MqttSnBroker::addSubscriber(String topicName, Subscriber &subscriber) {
	int topicIndex = obtainTopicIndex(topicName);
	if (topicIndex == -1) {
		// error: topic list is full
		return;
	}

	// remove the subscriber in case this was called a 2nd time e.g. after reconnect to gateway
	subscriber.remove();
	subscriber.index = topicIndex;
	this->subscribers.add(subscriber);

	TopicInfo &topic = this->topics[topicIndex];
	topic.subscribed = true;

	// wake up keepAlive() unless already subscribed at gateway
	if (topic.qosArray.get(0) == 3)
		this->keepAliveEvent.set();
}

int MqttSnBroker::obtainTopicIndex(String name) {
	if (name.isEmpty())
		return -1;
	return this->topics.obtain(name, [](int) {return TopicInfo{BitField<MAX_CONNECTION_COUNT, 2>().set(), 0, false, 0, 0};});
}

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
		auto &thisName = this->connections[0].name;

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

				// get quality of service (3: client is not subscribed)
				int qos = connectionIndex == 0 ? QOS : topic.qosArray.get(connectionIndex);
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
							auto c = w.current;

							// message data
							if (!writeMessage(w, publisher.messageType, publisher.message))
								break;

#ifdef DEBUG_PRINT
							sys::out.write(thisName.string() + " publishes " + dec(w.current - c) + " bytes to ");
							if (connectionIndex == 0)
								sys::out.write("gateway");
							else
								sys::out.write(connection.name.string());
							sys::out.write(" on topic " + this->topics.get(publisher.index)->key + " msgid " + dec(msgId) + '\n');
#endif

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
	}
}

Coroutine MqttSnBroker::receive() {
	uint8_t message[MAX_MESSAGE_LENGTH];

	while (true) {
		auto &thisName = this->connections[0].name;

		// receive a message from the gateway or a client
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
			if (this->isConnected(i) && this->connections[i].endpoint == source) {
				connectionIndex = i;
				break;
			}
		}

		if (msgType == mqttsn::MessageType::CONNECT) {
			// a client wants to (re)connect
			auto flags = r.enum8<mqttsn::Flags>();
			auto protocolId = r.uint8();
			auto keepAliveDuration = r.uint16();
			auto clientId = r.string();
#ifdef DEBUG_PRINT
			sys::out.write(clientId + " connects to " + thisName.string() + '\n');
#endif
			if (connectionIndex == -1) {
				// try to find an unused connection
				connectionIndex = (~this->connectedFlags).set(0, 0).findFirstNonzero();
			}

			auto returnCode = mqttsn::ReturnCode::ACCEPTED;
			if (connectionIndex == -1) {
				// error: list of connections is full
				returnCode = mqttsn::ReturnCode::REJECTED_CONGESTED;
			} else if (connectionIndex == 0) {
				// error: the gateway can't connect
				returnCode = mqttsn::ReturnCode::NOT_SUPPORTED;
			} else {
				// initialize the connection
				this->connections[connectionIndex].endpoint = source;
				this->connections[connectionIndex].name = clientId;

				// set connected flag
				this->connectedFlags.set(connectionIndex, 1);

				// reset qos entry of this connection for all topics
				// todo: to support persistent sessions, don't do this when a client reconnects without clean session flag
				for (auto [topicName, topic] : this->topics) {
					topic.qosArray.set(connectionIndex, 3);
				}
			}

			// reply with CONNACK
			MessageWriter w(message);
			w.enum8(mqttsn::MessageType::CONNACK);
			w.enum8(returnCode);
			co_await network::send(NETWORK_MQTT, source, w.finish());
		} else if (connectionIndex == -1) {
			// unknown client: reply with DISCONNECT
			MessageWriter w(message);
			w.enum8<mqttsn::MessageType>(mqttsn::MessageType::DISCONNECT);
			co_await network::send(NETWORK_MQTT, source, w.finish());
		} else if (msgType == mqttsn::MessageType::PINGREQ) {
			// ping request: reply with PINGRESP
			MessageWriter w(message);
			w.enum8(mqttsn::MessageType::PINGRESP);
			co_await network::send(NETWORK_MQTT, source, w.finish());
		} else if (msgType == mqttsn::MessageType::REGISTER) {
			// the gateway or a client want to register a topic
			auto topicId = r.uint16();
			auto msgId = r.uint16();
			auto topicName = r.string();
#ifdef DEBUG_PRINT
			sys::out.write(this->connections[connectionIndex].name.string() + " registers " + topicName + " at " + thisName.string() + '\n');
#endif
			// register the topic
			auto returnCode = mqttsn::ReturnCode::ACCEPTED;
			int topicIndex = obtainTopicIndex(topicName);
			if (topicIndex == -1) {
				// error: out of topic ids
				returnCode = mqttsn::ReturnCode::REJECTED_CONGESTED;
			} else {
				TopicInfo &topic = this->topics[topicIndex];
				if (connectionIndex == 0) {
					// gateway: set gateway topic id
					topic.gatewayTopicId = topicId;
				} else {
					// client: return our topic id to client
					topicId = topicIndex + 1;

					// wake up keepAlive() to register the topic at gateway unless already registered
					if (!topic.isRegisteredAtGateway())
						this->keepAliveEvent.set();
				}
			}

			// reply with REGACK
			{
				MessageWriter w(message);
				w.enum8(mqttsn::MessageType::REGACK);
				w.uint16(topicId);
				w.uint16(msgId);
				w.enum8(returnCode);
				co_await network::send(NETWORK_MQTT, source, w.finish());
			}
		} else if (msgType == mqttsn::MessageType::SUBSCRIBE) {
			// a client wants to subscribe to a topic
			auto flags = r.enum8<mqttsn::Flags>();
			auto qos = min(mqttsn::getQos(flags), QOS);
			//auto topicType = flags & mqttsn::Flags::TOPIC_TYPE_MASK;
			auto msgId = r.uint16();
			auto topicName = r.string();
#ifdef DEBUG_PRINT
			sys::out.write(this->connections[connectionIndex].name.string() + " subscribes " + topicName + " at " + thisName.string() + '\n');
#endif
			// register the topic
			int topicIndex = -1;
			auto returnCode = mqttsn::ReturnCode::ACCEPTED;
			if (connectionIndex == 0) {
				// gateway: the gateway can't subscribe to topics
				returnCode = mqttsn::ReturnCode::NOT_SUPPORTED;
			} else {
				topicIndex = obtainTopicIndex(topicName);
				if (topicIndex == -1) {
					// error: out of topic ids
					returnCode = mqttsn::ReturnCode::REJECTED_CONGESTED;
				} else {
					// client: set qos and return our topic id to client
					TopicInfo &topic = this->topics[topicIndex];
					topic.qosArray.set(connectionIndex, qos);

					// wake up keepAlive() to subscribe at gateway unless already subscribed
					if (!topic.isSubscribedAtGateway())
						this->keepAliveEvent.set();
				}
			}
			uint16_t topicId = topicIndex + 1;

			// reply with SUBACK
			{
				MessageWriter w(message);
				w.enum8(mqttsn::MessageType::SUBACK);
				w.enum8(mqttsn::makeQos(qos));
				w.uint16(topicId);
				w.uint16(msgId);
				w.enum8(returnCode);
				co_await network::send(NETWORK_MQTT, source, w.finish());
			}
		} else if (msgType == mqttsn::MessageType::UNSUBSCRIBE) {
			// a client wants to unsubscribe to a topic
			auto flags = r.enum8<mqttsn::Flags>();
			auto qos = min(mqttsn::getQos(flags), QOS);
			//auto topicType = flags & mqttsn::Flags::TOPIC_TYPE_MASK;
			auto msgId = r.uint16();
			auto topicName = r.string();
#ifdef DEBUG_PRINT
			sys::out.write(this->connections[connectionIndex].name.string() + " subscribes " + topicName + " from " + thisName.string() + '\n');
#endif
			// register the topic
			if (connectionIndex == 0) {
				// gateway: the gateway can't subscribe to topics
			} else {
				int topicIndex = topics.locate(topicName);
				if (topicIndex == -1) {
					// error: nopic name not found
				} else {
					// client: reset qos
					TopicInfo &topic = this->topics[topicIndex];
					topic.qosArray.set(connectionIndex, 3);

					// wake up keepAlive() to unsubscribe from gateway when no client is subscribed any more
					if (!topic.isClientSubscribed() && topic.isSubscribedAtGateway())
						this->keepAliveEvent.set();
				}
			}

			// reply with UNSUBACK
			{
				MessageWriter w(message);
				w.enum8(mqttsn::MessageType::UNSUBACK);
				w.uint16(msgId);
				co_await network::send(NETWORK_MQTT, source, w.finish());
			}
		} else if (msgType == mqttsn::MessageType::PUBLISH) {
			// the gateway or a client published to a topic
			uint8_t message2[MAX_MESSAGE_LENGTH];
			ConnectionInfo &connection = this->connections[connectionIndex];

			// read message
			auto flags = r.enum8<mqttsn::Flags>();
			auto topicType = flags & mqttsn::Flags::TOPIC_TYPE_MASK;
			auto qos = mqttsn::getQos(flags);
			uint16_t topicId = r.uint16();
			uint16_t msgId = r.uint16();
			int pubLength = r.getRemaining();
			uint8_t *pubData = r.current;

			// get topic index
			int topicIndex;
			switch (topicType) {
			case mqttsn::Flags::TOPIC_TYPE_NORMAL:
				{
					if (connectionIndex == 0) {
						// connection to gateway
						// todo: optimize linear search
						for (topicIndex = 0; topicIndex < MAX_TOPIC_COUNT; ++topicIndex) {
							if (this->topics[topicIndex].gatewayTopicId == topicId) {
								break;
							}
						}
					} else {
						// connection to a client
						topicIndex = topicId - 1;
					}
				}
				break;
			case mqttsn::Flags::TOPIC_TYPE_SHORT:
				{
					char topicName[2] = {char(topicId >> 8), char(topicId)};
					topicIndex = this->topics.locate(topicName);
				}
				break;
			default:
				topicIndex = -1;
			}

			// check if topic is ok
			bool topicOk = this->topics.isValid(topicIndex);

			// acknowledge or report error
			if (qos >= 1 || !topicOk) {
				uint8_t message2[12];
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
			TopicInfo &topic = this->topics[topicIndex];

#ifdef DEBUG_PRINT
			sys::out.write(thisName.string() + " receives " + dec(pubLength) + " bytes from ");
			if (connectionIndex == 0)
				sys::out.write("gateway");
			else
				sys::out.write(connection.name.string());
			sys::out.write(" on topic " + this->topics.get(topicIndex)->key + " msgid " + dec(msgId) + '\n');
#endif

			// check if message is duplicate
			if (qos > 0) {
				if (topic.lastConnectionIndex == connectionIndex && topic.lastMsgId == msgId)
					continue;
				topic.lastConnectionIndex = connectionIndex;
				topic.lastMsgId = msgId;
			}

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

			// forward to other connections
			this->forwardWaitlist.resumeFirst([connectionIndex, topicIndex, pubLength, pubData](ForwardParameters &p) {
				p.sourceConnectionIndex = connectionIndex;
				p.topicIndex = topicIndex;
				p.length = min(p.length, pubLength);
				array::copy(p.length, p.message, pubData);
				return true;
			});

		} else {
			// handle acknowledge and disconnect messages

			// get message without length and type
			int l = r.end - r.current;
			uint8_t *m = r.current;

			uint16_t msgId;
			String typeString;
			switch (msgType) {
			case mqttsn::MessageType::CONNACK:
				typeString = "CONNACK";
				msgId = 0;
				break;
			case mqttsn::MessageType::PINGRESP:
				typeString = "PINGRESP";
				msgId = 0;
				break;
			case mqttsn::MessageType::REGACK:
				typeString = "REGACK";
				r.skip(2);
				msgId = r.uint16();
				break;
			case mqttsn::MessageType::PUBACK:
				typeString = "PUBACK";
				r.skip(2);
				msgId = r.uint16();
				break;
			case mqttsn::MessageType::SUBACK:
				typeString = "SUBACK";
				r.skip(3);
				msgId = r.uint16();
				break;
			case mqttsn::MessageType::UNSUBACK:
				typeString = "UNSUBACK";
				msgId = r.uint16();
				break;
			case mqttsn::MessageType::DISCONNECT:
				typeString = "DISCONNECT";

				// clear connected flag
				this->connectedFlags.set(connectionIndex, 0);

				// reset qos entry of this connection for all topics
				// todo: to support persistent sessions, this should be done after some timeout
				for (auto [topicName, topic] : this->topics) {
					topic.qosArray.set(connectionIndex, 3);
				}

				this->keepAliveEvent.set();
				continue;
			default:
				// unsupported message type
				continue;
			}

#ifdef DEBUG_PRINT
			sys::out.write(thisName.string() + " received " + typeString + " from ");
			if (connectionIndex == 0)
				sys::out.write("gateway");
			else
				sys::out.write(this->connections[connectionIndex].name.string());
			sys::out.write(" msgid " + dec(msgId) + '\n');
#endif

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

Coroutine MqttSnBroker::forward() {
	uint8_t message[MAX_MESSAGE_LENGTH];
	while (true) {
		auto &thisName = this->connections[0].name;

		// wait for data to publish (place it at the right position)
		uint16_t sourceConnectionIndex;
		uint16_t topicIndex;
		int length;
		{
			MessageWriter w(message);
			w.skip(6);
			length = message + MAX_MESSAGE_LENGTH - w.current;
			co_await this->forwardWaitlist.wait(sourceConnectionIndex, topicIndex, length, w.current);
		}
		TopicInfo &topic = this->topics[topicIndex];

		// publish to other connections
		for (int connectionIndex = 0; connectionIndex < MAX_CONNECTION_COUNT; ++connectionIndex) {
			ConnectionInfo &connection = this->connections[connectionIndex];

			// get quality of service and topic id
			int qos;
			uint16_t topicId;
			if (connectionIndex == 0) {
				// publish to gateway (set qos to 3 if topic is not registered at gateway)
				qos = topic.gatewayTopicId == 0 ? 3 : QOS;
				topicId = topic.gatewayTopicId;
			} else {
				// publish to client (qos is 3 if client is not subscribed on the topic)
				qos = topic.qosArray.get(connectionIndex);
				topicId = topicIndex + 1;
			}

			// don't publish on connection over which we received the message
			if (connectionIndex != sourceConnectionIndex && isConnected(connectionIndex) && qos != 3) {
				// generate message id
				uint16_t msgId = qos <= 0 ? 0 : getNextMsgId();

#ifdef DEBUG_PRINT
				sys::out.write(thisName.string() + " forwards " + dec(length) + " bytes to ");
				if (connectionIndex == 0)
					sys::out.write("gateway");
				else
					sys::out.write(connection.name.string());
				sys::out.write(" on topic " + this->topics.get(topicIndex)->key + " msgid " + dec(msgId) + '\n');
#endif

				// message flags
				auto flags = mqttsn::makeQos(qos);

				// send to gateway or client
				for (int retry = 0; retry <= MAX_RETRY; ++retry) {
					// send publish message
					{
						MessageWriter w(message);
						w.enum8(mqttsn::MessageType::PUBLISH);
						w.enum8(flags);
						w.uint16(topicId);
						w.uint16(msgId);
						w.skip(length); // data is already in the message
						co_await network::send(NETWORK_MQTT, connection.endpoint, w.finish());
					}

					if (qos <= 0)
						break;

					// wait for acknowledge from gateway or client
					{
						uint8_t message2[8];
						int length2 = array::count(message);
						int s = co_await select(this->ackWaitlist.wait(uint8_t(connectionIndex),
							mqttsn::MessageType::PUBACK, msgId, length2, message2),
							timer::sleep(RETRANSMISSION_TIME));

						// check if still connected
						if (!isConnected(connectionIndex))
							break;

						// check if we received a message
						if (s == 1) {
							MessageReader r(length2, message2);

							// get topic id and return code
							auto topicId2 = r.uint16();
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
	}
}

