#include "MqttSnBroker.hpp"

// min for qos (quality of service)
constexpr int8_t min(int8_t x, int8_t y) {return x < y ? x : y;}


MqttSnBroker::MqttSnBroker() {
	// init clients
	for (int i = 0; i < MAX_CLIENT_COUNT; ++i) {
		ClientInfo &client = this->clients[i];
		client.clientId = 0;
		client.index = i;
	}

	// init topics
	for (int i = 0; i < MAX_TOPIC_COUNT; ++i) {
		TopicInfo &topic = this->topics[i];
		topic.hash = 0;
	}

	memset(this->sendBuffer, 0, SEND_BUFFER_SIZE);
}


MqttSnBroker::Result MqttSnBroker::registerTopic(uint16_t &topicId, String topicName) {
	if (!isValid(topicName)) {
		assert(false);
		return Result::INVALID_PARAMETER;
	}
	
	// get or add topic by name
	uint16_t newTopicId = getTopicId(topicName);
	if (newTopicId == 0) {
		assert(false);
		return Result::OUT_OF_MEMORY;
	}
	
	// assert that both topic ids are the same
	assert(topicId == 0 || newTopicId == topicId);

	Result result = Result::OK;
	TopicInfo &topic = getTopic(newTopicId);
		
	// check if we need to register the topic at the gateway
	if (MqttSnClient::isConnected() && topic.gatewayTopicId == 0) {
		MessageResult r = MqttSnClient::registerTopic(topicName);
		topic.gatewayTopicId = r.msgId; // store message id in topic id until client receives REGACK
		topic.gatewayQos = -1; // mark as waiting for topic id from gateway
		result = r.result;
	}
	topicId = newTopicId;
std::cout << "registerTopic " << topicName << " " << topicId << std::endl;
	return result;
}

MqttSnBroker::Result MqttSnBroker::publish(uint16_t topicId, uint8_t const *data, int length, int8_t qos, bool retain) {
	if (isClientBusy() || isBrokerBusy())
		return Result::BUSY;

	TopicInfo *topic = findTopic(topicId);
	if (topic == nullptr || length > MAX_MESSAGE_LENGTH - 7)
		return Result::INVALID_PARAMETER;

	// publish to all clients and local client
	sendPublish(topicId, topic, data, length, qos, retain);

	// publish to gateway
	if (topic->gatewayTopicId != 0) {
		MqttSnClient::publish(topic->gatewayTopicId, data, length, qos, retain, topic->gatewayQos < 0);//waitForTopicId);
	}

	return Result::OK;
}
/*
MqttSnBroker::TopicResult MqttSnBroker::subscribeTopic(String topicFilter, int8_t qos) {
	if (!isValid(topicFilter))
		return {0, Result::INVALID_PARAMETER};

	// get or add topic by name
	uint16_t topicId = getTopicId(topicFilter);
	if (topicId == 0)
		return {0, Result::OUT_OF_MEMORY};

	Result result = Result::OK;
	TopicInfo &topic = getTopic(topicId);
	
	// check if we need to subscribe topic at the gateway
	if (MqttSnClient::isConnected() && qos > topic.getMaxQos())
		result = MqttSnClient::subscribeTopic(topicFilter, qos).result;

	//todo
	// check if there is a retained message for this topic
		
	// set quality of service of subscription (overwrites previous value)
	topic.setQos(LOCAL_CLIENT_INDEX, qos);
	
	// increment subscription counter
	++topic.subscribeCount;
std::cout << "subscribeTopic " << topicFilter << " " << topicId << " (" << int(topic.subscribeCount) << ")" << std::endl;
	return {topicId, result};
}
*/
MqttSnBroker::Result MqttSnBroker::subscribeTopic(uint16_t &topicId, String topicFilter, int8_t qos) {
	if (!isValid(topicFilter)) {
		assert(false);
		return Result::INVALID_PARAMETER;
	}

	// get or add topic by name
	uint16_t newTopicId = getTopicId(topicFilter);
	if (newTopicId == 0) {
		assert(false);
		return Result::OUT_OF_MEMORY;
	}

	Result result = Result::OK;
	TopicInfo &topic = getTopic(newTopicId);
	
	// check if we need to subscribe topic at the gateway
	if (MqttSnClient::isConnected() && qos > topic.getMaxQos())
		result = MqttSnClient::subscribeTopic(topicFilter, qos).result;

	//todo
	// check if there is a retained message for this topic
		
	// set quality of service of subscription (overwrites previous value)
	topic.setQos(LOCAL_CLIENT_INDEX, qos);
	
	// increment subscription counter
	if (topicId == 0)
		++topic.subscribeCount;
	else
		assert(topicId == newTopicId);
	topicId = newTopicId;
std::cout << "subscribeTopic " << topicFilter << " " << topicId << " (" << int(topic.subscribeCount) << ")" << std::endl;
	return result;
}

MqttSnBroker::Result MqttSnBroker::unsubscribeTopic(uint16_t &topicId, String topicFilter) {
	if (!isValid(topicFilter)) {
		assert(false);
		return Result::INVALID_PARAMETER;
	}

	// get topic by name
	uint16_t newTopicId = getTopicId(topicFilter, false);
	if (newTopicId == 0) {
		assert(false);
		return Result::INVALID_PARAMETER;
	}

	// assert that both topic ids are the same
	assert(topicId == newTopicId);
	
	TopicInfo &topic = getTopic(newTopicId);
	
	// check subscription counter
	if (--topic.subscribeCount == 0) {
		// remove subscription of local client from flags
		int8_t qos = topic.clearQos(LOCAL_CLIENT_INDEX);
	
		// check if maximum qos was reduced
		int8_t maxQos = topic.getMaxQos();
		if (maxQos < qos) {
			if (maxQos == -1) {
				// all subscriptions removed: unsubscribe at the gateway
				MqttSnClient::unsubscribeTopic(topicFilter);
				topic.gatewayTopicId = 0;
				topic.gatewayQos = 0;
			} else {
				// subscribe at the gateway with reduced qos
				MqttSnClient::subscribeTopic(topicFilter, qos);
			}
		}
	}
std::cout << "unsubscribeTopic " << topicFilter << " " << topicId << " (" << int(topic.subscribeCount) << ")" << std::endl;
	topicId = 0;
	return Result::OK;
}


// MqttSnClient user callbacks
// ---------------------------

void MqttSnBroker::onRegistered(uint16_t msgId, String topicName, uint16_t gatewayTopicId) {
	// topic was registered at gateway
	uint16_t topicId = getTopicId(topicName);
	std::cout << topicName << " -> " << topicId << " " << gatewayTopicId << std::endl;
	if (topicId != 0) {
		TopicInfo &topic = getTopic(topicId);
		topic.gatewayTopicId = gatewayTopicId;
		if (topic.gatewayQos < 0)
			topic.gatewayQos = 0;
		//topic.waitForTopicId = false;
	}
}

void MqttSnBroker::onSubscribed(uint16_t msgId, String topicName, uint16_t gatewayTopicId, int8_t qos) {
	// topic was subscribed at gateway
	uint16_t topicId = getTopicId(topicName);
	std::cout << topicName << " -> " << topicId << " " << gatewayTopicId << std::endl;
	if (topicId != 0) {
		TopicInfo &topic = getTopic(topicId);
		topic.gatewayTopicId = gatewayTopicId;
		topic.gatewayQos = qos;
	}
}

mqttsn::ReturnCode MqttSnBroker::onPublish(uint16_t gatewayTopicId, uint8_t const *data, int length, int8_t qos,
	bool retain)
{
	// check if we are able to forward to our clients
	if (isBrokerBusy())
		return mqttsn::ReturnCode::REJECTED_CONGESTED;

	uint16_t topicId = findTopicId(gatewayTopicId);
	TopicInfo *topic = findTopic(topicId);
	if (topic == nullptr)
		return mqttsn::ReturnCode::REJECTED_INVALID_TOPIC_ID;

	// publish to all clients and local client
	sendPublish(topicId, topic, data, length, qos, retain);
	
	return mqttsn::ReturnCode::ACCEPTED;
}

// transport
// ---------

void MqttSnBroker::onDownReceived(uint16_t clientId, uint8_t const *data, int length) {
	// get message type
	mqttsn::MessageType messageType = mqttsn::MessageType(data[0]);

	switch (messageType) {
	case mqttsn::CONNECT:
		// a new client connects
		if (length < 6) {
			onError(ERROR_MESSAGE, messageType);
		} else {
			// create reply message
			Message m = addSendMessage(2, {clientId});
			if (m.data != nullptr) {
				// serialize message
				m.data[0] = mqttsn::CONNACK;
				
				// add new client
				ClientInfo *client = findClient(0);
				if (client != nullptr) {
					uint8_t flags = data[1];
					uint8_t protocolId = data[2];
					uint16_t duration = mqttsn::getUShort(data + 3);
					String clientName(data + 5, length - 5);
					client->name = clientName;

					m.data[1] = uint8_t(mqttsn::ReturnCode::ACCEPTED);
				} else {
					// error: too many clients
					m.data[1] = uint8_t(mqttsn::ReturnCode::REJECTED_CONGESTED);
				}

				// send message
				if (!isDownSendBusy())
					sendCurrentMessage();
			} else {
				// error: send message queue full
			}
		}
		break;
	case mqttsn::DISCONNECT:
		// a client disconnects or wants to go asleep
		{
			// create reply message
			Message m = addSendMessage(1, {clientId});
			if (m.data != nullptr) {
				// serialize message
				m.data[0] = mqttsn::DISCONNECT;

				// disconnect client
				ClientInfo *client = findClient(clientId);
				if (client != nullptr) {
					// todo: handle duration
					uint16_t duration = 0;
					if (length >= 3)
						duration = mqttsn::getUShort(data + 1);

					client->disconnect();
					
					// clear subscriptions of this client from all topics
					for (int i = 0; i < this->topicCount; ++i) {
						this->topics[i].clearQos(client->index);
					}
				}
				
				// send reply message
				if (!isDownSendBusy())
					sendCurrentMessage();
			} else {
				// error: send message queue full
			}
		}
		break;
	case mqttsn::REGISTER:
		// client wants to register a topic so that it can publish to it
		if (length < 6) {
			onError(ERROR_MESSAGE, messageType);
		} else {
			// get client
			ClientInfo *client = findClient(clientId);
			if (client != nullptr) {
				// deserialize message
				uint16_t msgId = mqttsn::getUShort(data + 3);
				String topicName(data + 5, length - 5);

				// create reply message
				Message m = addSendMessage(6, {clientId});
				if (m.data != nullptr) {
					// serialize message
					m.data[0] = mqttsn::REGACK;
					mqttsn::setUShort(m.data + 3, msgId);

					// get or add topic by name
					uint16_t topicId = getTopicId(topicName);
					if (topicId != 0) {
						mqttsn::setUShort(m.data + 1, topicId);
						TopicInfo &topic = getTopic(topicId);
						
						// check if we need to register the topic at gateway
						Result result = Result::OK;
						if (topic.gatewayTopicId == 0)
							result = MqttSnClient::registerTopic(topicName).result;
						
						if (result == Result::OK) {
							// serialize reply message
							m.data[5] = uint8_t(mqttsn::ReturnCode::ACCEPTED);
						} else {
							// error: could not register at gateway
							m.data[5] = uint8_t(mqttsn::ReturnCode::REJECTED_CONGESTED);
						}
					} else {
						// error: could not allocate a topic id
						mqttsn::setUShort(m.data + 1, 0);
						m.data[5] = uint8_t(mqttsn::ReturnCode::REJECTED_CONGESTED);
					}

					// send reply message
					if (!isDownSendBusy())
						sendCurrentMessage();
				} else {
					// error: send message queue full
				}
			} else {
				// error: client is not connected, send DISCONNECT
				sendDisconnect(clientId);
			}
		}
		break;
	case mqttsn::PUBLISH:
		// client wants to publish a message on a topic
		if (length < 6) {
			onError(ERROR_MESSAGE, messageType);
		} else {
			// get client
			ClientInfo *client = findClient(clientId);
			if (client != nullptr) {
				// deserialize message
				uint8_t flags = data[1];
				bool dup = mqttsn::getDup(flags);
				int8_t qos = mqttsn::getQos(flags);
				bool retain = mqttsn::getRetain(flags);
				mqttsn::TopicType topicType = mqttsn::getTopicType(flags);
				uint16_t topicId = mqttsn::getUShort(data + 2);
				uint16_t msgId = mqttsn::getUShort(data + 4);
				uint8_t const *payload = data + 6;
				int payloadLength = length - 6;

				// create reply message
				Message m = addSendMessage(6, {clientId});
				if (m.data != nullptr) {
					// serialize message
					m.data[0] = mqttsn::PUBACK;
					mqttsn::setUShort(m.data + 1, topicId);
					mqttsn::setUShort(m.data + 3, msgId);

					if (topicType == mqttsn::TopicType::NORMAL) {
						// find topic by id
						TopicInfo *topic = findTopic(topicId);
						if (topic != nullptr) {
							
							if (!isClientBusy() && !isBrokerBusy()) {
								// publish to clients except sender and local client
								sendPublish(topicId, topic, data, length, qos, retain, clientId);

								// publish to gateway
								if (topic->gatewayTopicId != 0) {
									MqttSnClient::publish(topic->gatewayTopicId, data, length,
										qos, retain, topic->gatewayQos < 0);//waitForTopicId);
								}

								// serialize reply message
								m.data[5] = uint8_t(mqttsn::ReturnCode::ACCEPTED);
							} else {
								// error: uplink or downlink congested
								m.data[5] = uint8_t(mqttsn::ReturnCode::REJECTED_CONGESTED);
							}
						} else {
							// error: topic not found
							m.data[5] = uint8_t(mqttsn::ReturnCode::REJECTED_INVALID_TOPIC_ID);
						}
					} else {
						// error: topic type not supported
						m.data[5] = uint8_t(mqttsn::ReturnCode::NOT_SUPPORTED);
					}

					// send reply message
					if (!isDownSendBusy())
						sendCurrentMessage();
				} else {
					// error: send message queue full
				}
			} else {
				// error: client is not connected, send DISCONNECT
				sendDisconnect(clientId);
			}
		}
		break;
	case mqttsn::PUBACK:
		// sent by the client in response to a PUBLISH message (if qos was greater zero)
		if (length < 6) {
			onError(ERROR_MESSAGE, messageType);
		} else {
			// get client
			ClientInfo *client = findClient(clientId);
			if (client != nullptr) {

				// deserialize message
				uint16_t topicId = mqttsn::getUShort(data + 1);
				uint16_t msgId = mqttsn::getUShort(data + 3);
				mqttsn::ReturnCode returnCode = mqttsn::ReturnCode(data[5]);
				
				// mark this client as done and remove PUBLISH message from queue if all are done
				removeSentMessage(msgId, client->index);

				switch (returnCode) {
				case mqttsn::ReturnCode::ACCEPTED:
					break;
				case mqttsn::ReturnCode::REJECTED_CONGESTED:
					onError(ERROR_CONGESTED, messageType);
					break;
				default:
					;
				}
			} else {
				// error: client is not connected, send DISCONNECT
				sendDisconnect(clientId);
			}
		}
		break;
	case mqttsn::SUBSCRIBE:
		// client wants to subscribe to a topic or topic wildcard
		if (length < 5) {
			onError(ERROR_MESSAGE, messageType);
		} else {
			// get client
			ClientInfo *client = findClient(clientId);
			if (client != nullptr) {
				// deserialize message
				uint8_t flags = data[1];
				bool dup = mqttsn::getDup(flags);
				int8_t qos = mqttsn::getQos(flags);
				mqttsn::TopicType topicType = mqttsn::getTopicType(flags);
				uint16_t msgId = mqttsn::getUShort(data + 2);

				// create reply message
				Message m = addSendMessage(7, {clientId});
				if (m.data != nullptr) {
					// serialize message
					m.data[0] = mqttsn::SUBACK;
					m.data[1] = mqttsn::makeQos(qos); // granted qos
					mqttsn::setUShort(m.data + 4, msgId);

					if (topicType == mqttsn::TopicType::NORMAL) {
						String topicName(data + 4, length - 4);

						// get or add topic by name
						uint16_t topicId = getTopicId(topicName);
						if (topicId != 0) {
							TopicInfo &topic = getTopic(topicId);
							
							// check if we need to subscribe topic at the gateway
							Result result = Result::OK;
							if (qos > topic.getMaxQos())
								result = MqttSnClient::subscribeTopic(topicName, qos).result;
							
							if (result == Result::OK) {
								//todo
								// check if there is a retained message for this topic
								
								// add subscription of client to flags
								topic.setQos(client->index, qos);
								
								// serialize reply message
								mqttsn::setUShort(m.data + 2, topicId);
								m.data[6] = uint8_t(mqttsn::ReturnCode::ACCEPTED);
							} else {
								// error: could not subscribe at the gateway
								mqttsn::setUShort(m.data + 2, topicId);
								m.data[6] = uint8_t(mqttsn::ReturnCode::REJECTED_CONGESTED);
							}
						} else {
							// error: could not allocate a topic id
							mqttsn::setUShort(m.data + 2, 0);
							m.data[6] = uint8_t(mqttsn::ReturnCode::REJECTED_CONGESTED);
						}
					} else {
						// error: topic type not supported
						mqttsn::setUShort(m.data + 2, 0);
						m.data[6] = uint8_t(mqttsn::ReturnCode::NOT_SUPPORTED);
					}
					
					// send reply message
					if (!isDownSendBusy())
						sendCurrentMessage();
				} else {
					// error: send message queue full
				}
			} else {
				// error: client is not connected, send DISCONNECT
				sendDisconnect(clientId);
			}
		}
		break;
	case mqttsn::UNSUBSCRIBE:
		// client wants to unsubscribe from a topic or topic wildcard
		if (length < 5) {
			onError(ERROR_MESSAGE, messageType);
		} else {
			// get client
			ClientInfo *client = findClient(clientId);
			if (client != nullptr) {
				// deserialize message
				uint8_t flags = data[1];
				mqttsn::TopicType topicType = mqttsn::getTopicType(flags);
				uint16_t msgId = mqttsn::getUShort(data + 2);

				// create reply message
				Message m = addSendMessage(3, {clientId});
				if (m.data != nullptr) {
					// serialize message
					m.data[0] = mqttsn::UNSUBACK;
					mqttsn::setUShort(m.data + 1, msgId);

					if (topicType == mqttsn::TopicType::NORMAL) {
						String topicName(data + 4, length - 4);

						//todo: check if topicFilter is a topic name or wildcard

						// get topic by name
						uint16_t topicId = getTopicId(topicName, false);
						if (topicId != 0) {
							TopicInfo &topic = getTopic(topicId);

							// remove subscription of client from flags
							int8_t qos = topic.clearQos(client->index);
						
							// check if maximum qos was reduced
							int8_t maxQos = topic.getMaxQos();
							if (maxQos < qos) {
								if (maxQos == -1) {
									// all subscriptions removed: unsubscribe at the gateway
									MqttSnClient::unsubscribeTopic(topicName);
									topic.gatewayTopicId = 0;
									topic.gatewayQos = 0;
								} else {
									// subscribe at the gateway with reduced qos
									MqttSnClient::subscribeTopic(topicName, qos);
								}
							}
						}
					} else {
						// error: topic type not supported
					}

					// send reply message
					if (!isDownSendBusy())
						sendCurrentMessage();
				} else {
					// error: send message queue full
				}
			} else {
				// error: client is not connected
				sendDisconnect(clientId);
			}
		}
		break;
	case mqttsn::PINGREQ:
		// client sends a ping to keep connection alive
		{
			// get client
			ClientInfo *client = findClient(clientId);
			if (client != nullptr) {
				//todo: reset connection timeout
			
				// create reply message
				Message m = addSendMessage(1, {clientId});
				if (m.data != nullptr) {
					// serialize message
					m.data[0] = mqttsn::PINGRESP;

					// send reply message
					if (!isDownSendBusy())
						sendCurrentMessage();
				} else {
					// error: send message queue full
				}
			} else {
				// error: client is not connected
				sendDisconnect(clientId);
			}
		}
		break;
	default:
		// error: unsupported message type
		onError(ERROR_BROKER_UNSUPPORTED, messageType);
	}
}

void MqttSnBroker::onDownSent() {
	if (this->resendPending) {
		// resend a message that was not acknowledged
		this->resendPending = false;
		resend();
	} else {
		// check if there are more messages to send in the queue
		if (this->sendMessagesHead != this->sendMessagesCurrent)
			sendCurrentMessage();
	}
}

void MqttSnBroker::onSystemTimeout2(SystemTime time) {
	if (this->sendMessagesTail < this->sendMessagesCurrent) {
		// resend a message that was not acknowledged or mark as pending
		if (!(this->resendPending = isDownSendBusy()))
			resend();
	}
}

bool MqttSnBroker::TopicInfo::isRemoteSubscribed() {
	for (int i = 0; i < (MAX_CLIENT_COUNT + 15) / 16; ++i) {
		uint32_t clientQos = this->clientQosArray[i];
		if (i == MAX_CLIENT_COUNT / 16)
			clientQos |= 0xffffffff << (MAX_CLIENT_COUNT & 15) * 2;

		if (clientQos != 0xffffffff)
			return true;
	}
	return false;
}

int8_t MqttSnBroker::TopicInfo::getMaxQos() {
	uint32_t a = 0xffffffff;
	uint32_t b = 0xffffffff;
	for (int i = 0; i < (MAX_CLIENT_COUNT + 15) / 16; ++i) {
		uint32_t clientQos = this->clientQosArray[i];
		if (i == MAX_CLIENT_COUNT / 16)
			clientQos |= 0xffffffff << (MAX_CLIENT_COUNT & 15) * 2;
		//if ((i + 1) * 16 > MAX_CLIENT_COUNT)
		//	clientQos |= 0xffffffff << (MAX_CLIENT_COUNT - i * 16) * 2;
		
		uint32_t c = clientQos;
		uint32_t d = clientQos >> 1;
		
		uint32_t x = (a & c) | ((a | c) & ~b & ~d);
		uint32_t y = (b & d) | (~a & b) | (~c & d);
		
		a = x;
		b = y;
	}
	
	for (int shift = 16; shift >= 2; shift >>= 1) {
		uint32_t c = a >> shift;
		uint32_t d = b >> shift;
		
		uint32_t x = (a & c) | ((a | c) & ~b & ~d);
		uint32_t y = (b & d) | (~a & b) | (~c & d);
		
		a = x;
		b = y;
	}
	int8_t qos = ((((a & 1) | ((b & 1) << 1)) + 1) & 3) - 1;
	return qos;
}

void MqttSnBroker::TopicInfo::setQos(int clientIndex, int8_t qos) {
	uint32_t &clientQos = this->clientQosArray[clientIndex >> 4];
	int shift = (clientIndex << 1) & 31;
	clientQos = (clientQos & ~(3 << shift)) | ((qos & 3) << shift);
}

int8_t MqttSnBroker::TopicInfo::clearQos(int clientIndex) {
	uint32_t &clientQos = this->clientQosArray[clientIndex >> 4];
	int shift = (clientIndex << 1) & 31;
	int8_t qos = (((clientQos >> shift) + 1) & 3) - 1;
	clientQos |= 3 << shift;
	return qos;
}

MqttSnBroker::ClientInfo *MqttSnBroker::findClient(uint16_t clientId) {
	// client at index 0 is local client
	for (int i = 1; i < MAX_CLIENT_COUNT; ++i) {
		ClientInfo &client = this->clients[i];
		if (client.clientId == clientId)
			return &client;
	}
	return nullptr;
}

uint16_t MqttSnBroker::getTopicId(String name, bool add) {
	// calc djb2 hash of name
	// http://www.cse.yorku.ca/~oz/hash.html
	uint32_t hash = 5381;
	for (char c : name) {
		hash = (hash << 5) + hash + uint8_t(c); // hash * 33 + c
    }
	
	// search for topic
	int empty = -1;
	for (int i = 0; i < this->topicCount; ++i) {
		TopicInfo *topic = &this->topics[i];
		if (topic->hash == hash)
			return i + 1;
		if (topic->hash == 0)
			empty = i;
	}
	if (!add)
		return 0;
			
	// add a new topic if no empty topic was found
	if (empty == -1) {
		if (this->topicCount >= MAX_TOPIC_COUNT)
			return 0; // OUT_OF_MEMORY error
		empty = this->topicCount++;
	}
	
	// initialize topic
	TopicInfo *topic = &this->topics[empty];
	topic->hash = hash;
	topic->retainedOffset = this->retainedSize;
	topic->retainedAllocated = 0;
	topic->retainedLength = 0;
	for (uint32_t &clientQos : topic->clientQosArray)
		clientQos = 0xffffffff;
	topic->gatewayTopicId = 0;
	topic->gatewayQos = 0;
	topic->subscribeCount = 0;
	
	return empty + 1;
}

MqttSnBroker::TopicInfo *MqttSnBroker::findTopic(uint16_t topicId) {
	if (topicId < 1 || topicId > this->topicCount)
		return nullptr;
	TopicInfo *topic = &this->topics[topicId - 1];
	return topic->hash != 0 ? topic : nullptr;
}

uint16_t MqttSnBroker::findTopicId(uint16_t gatewayTopicId) {
	for (int i = 0; i < this->topicCount; ++i) {
		TopicInfo &topic = this->topics[i];
		if (topic.hash != 0 && topic.gatewayTopicId == gatewayTopicId)
			return i + 1;
	}
	return 0;
}

uint16_t MqttSnBroker::insertRetained(int offset, int length) {
	// make space in the memory for retained messages
	for (int i = offset; i < this->retainedSize; ++i) {
		this->retained[i + length] = this->retained[i];
	}
	this->retainedSize += length;
	
	// adjust offsets of all retained messages starting at or behind offset
	for (int i = 0; i < this->topicCount; ++i) {
		TopicInfo &topic = this->topics[i];
		if (topic.retainedOffset >= offset)
			topic.retainedOffset += length;
	}
	
	return uint16_t(offset);
}

uint16_t MqttSnBroker::eraseRetained(int offset, int length) {
	// erase region in the memory for retained messages
	for (int i = this->retainedSize; i >= offset; --i) {
		this->retained[i + length] = this->retained[i];
	}

	// adjust offsets of all retained messages starting at or behind offset
	for (int i = 0; i < this->topicCount; ++i) {
		TopicInfo &topic = this->topics[i];
		if (topic.retainedOffset >= offset)
			topic.retainedOffset -= length;
	}

	return uint16_t(this->retainedSize);
}

MqttSnBroker::Message MqttSnBroker::addSendMessage(int length, ClientSet clientSet, uint16_t msgId) {
	if (this->busy)
		return {};

	// garbage collect old messages
	if (this->sendMessagesHead + 1 + 1 > MAX_SEND_COUNT
		|| this->sendBufferHead + length + MAX_MESSAGE_LENGTH > SEND_BUFFER_SIZE )
	{
		int j = 0;
		uint16_t offset = 0;
		for (int i = this->sendMessagesTail; i < this->sendMessagesHead; ++i) {
			MessageInfo &info = this->sendMessages[i];
			if (info.msgId != 0 || i >= this->sendMessagesCurrent) {
				uint8_t *data = this->sendBuffer + info.offset;
				int length = info.length;
				
				MessageInfo &info2 = this->sendMessages[j++];
				info2 = info;
				info2.offset = offset;
				memcpy(this->sendBuffer + offset, data, length);
				offset += length;
			}

		}
		this->sendMessagesTail = 0;
		this->sendMessagesCurrent = j - (this->sendMessagesHead - this->sendMessagesCurrent);
		this->sendMessagesHead = j;
		this->sendBufferHead = offset;
		this->sendBufferFill = offset;
	}

	// allocate message info
	MessageInfo &info = this->sendMessages[this->sendMessagesHead++];
		
	// allocate data
	info.offset = this->sendBufferHead;
	info.length = length;
	this->sendBufferHead += length;
	this->sendBufferFill += length;

	// set busy flag if no new message will fit
	this->busy = this->sendMessagesHead >= MAX_SEND_COUNT || this->sendBufferFill + MAX_MESSAGE_LENGTH > SEND_BUFFER_SIZE;

	// set client set
	info.clientSet = clientSet;

	// set message id
	info.msgId = msgId;


	// return allocated message (space for "naked" mqtt-sn message without length)
	return {this->sendBuffer + info.offset/*, length*/};
}

void MqttSnBroker::sendCurrentMessage() {
	int current = this->sendMessagesCurrent;

	// get message
	MessageInfo &info = this->sendMessages[current];
	uint8_t *data = this->sendBuffer + info.offset;
	int length = info.length;
	
	// get flags of PUBLISH message
	uint8_t flags = data[1];

	// get client id
	uint16_t clientId;
	int clientIndex;
	if (data[0] != mqttsn::PUBLISH) {
		// send message to just one client
		clientId = info.clientSet.clientId;
		clientIndex = MAX_CLIENT_COUNT;
	} else {
		// send message to all clients whose flag is set
		clientIndex = this->sendMessagesClientIndex;
		
		// skip empty flags (guaranteed that one flag is set)
		while (((info.clientSet.flags[clientIndex >> 5] >> (clientIndex & 31)) & 1) == 0) {
			++clientIndex;
		}
		
		// get client id
		clientId = this->clients[clientIndex].clientId;
		
		// get topic
		uint16_t topicId = mqttsn::getUShort(data + 2);
		TopicInfo &topic = getTopic(topicId);
		
		// set actual qos to message
		int8_t clientQos = topic.getQos(clientIndex);
		int8_t qos = mqttsn::getQos(flags);
		data[1] = (flags & ~mqttsn::makeQos(3)) | mqttsn::makeQos(min(qos, clientQos));
		
		// go to next set flag
		while (clientIndex < MAX_CLIENT_COUNT) {
			++clientIndex;
			if (((info.clientSet.flags[clientIndex >> 5] >> (clientIndex & 31)) & 1) != 0)
				break;
		}
	}
		
	// send to client
	downSend(clientId, data, length);

	// restore flags in PUBLISH message
	data[1] = flags;

	// set sent time
	SystemTime now = getSystemTime();
	info.sentTime = now;

	if (current == this->sendMessagesTail) {
		if (info.msgId == 0) {
			// remove message from tail because it's not needed any more such as PINGREQ
			if (clientIndex == MAX_CLIENT_COUNT)
				this->sendMessagesTail = current + 1;

			// set timer for idle ping to keep the connection alive
			setSystemTimeout2(now + KEEP_ALIVE_INTERVAL);
		} else {
			// start retransmission timeout
			if (this->sendMessagesClientIndex == 0)
				setSystemTimeout2(now + RETRANSMISSION_INTERVAL);
		}
	}

	// make next message current
	if (clientIndex == MAX_CLIENT_COUNT) {
		this->sendMessagesCurrent = current + 1;
		clientIndex = 0;
	}
	
	this->sendMessagesClientIndex = clientIndex;
}

void MqttSnBroker::resend() {
	// get message
	MessageInfo &info = this->sendMessages[this->sendMessagesTail];
	uint8_t *data = this->sendBuffer + info.offset;
	int length = info.length;

	// get flags of PUBLISH message
	uint8_t flags = data[1];

	// get client id
	uint16_t clientId;
	int clientIndex;
	if (data[0] != mqttsn::PUBLISH) {
		// send message to just one client
		clientId = info.clientSet.clientId;
		clientIndex = MAX_CLIENT_COUNT;
	} else {
		// send message to all clients whose flag is set
		clientIndex = this->sendMessagesClientIndex;
		
		// skip empty flags (guaranteed that one flag is set)
		while (((info.clientSet.flags[clientIndex >> 5] >> (clientIndex & 31)) & 1) == 0) {
			++clientIndex;
		}
		
		// get client id
		clientId = this->clients[clientIndex].clientId;
		
		// get topic
		uint16_t topicId = mqttsn::getUShort(data + 2);
		TopicInfo &topic = getTopic(topicId);
		
		// set actual qos to message
		int8_t clientQos = topic.getQos(clientIndex);
		int8_t qos = mqttsn::getQos(flags);
		data[1] = (flags & ~mqttsn::makeQos(3)) | mqttsn::makeQos(min(qos, clientQos));
	}

	// send to client
	downSend(clientId, data, length);

	// restore flags in PUBLISH message
	data[1] = flags;

	// start timeout, either for retransmission or idle ping
	setSystemTimeout2(getSystemTime() + RETRANSMISSION_INTERVAL);
}

void MqttSnBroker::removeSentMessage(uint16_t msgId, uint16_t clientIndex) {
	int tail = this->sendMessagesTail;
	
	// mark message with msgId as obsolete
	for (int i = tail; i < this->sendMessagesCurrent; ++i) {
		MessageInfo &info = this->sendMessages[i];
		
		if (info.msgId == msgId) {
			uint32_t allClients = 0;

			if (clientIndex < MAX_CLIENT_COUNT) {
				// clear client flag
				info.clientSet.flags[clientIndex >> 5] &= ~(1 << (clientIndex & 31));
				
				// get all client flags
				for (uint32_t clients : info.clientSet.flags)
					allClients |= clients;
			}
			
			// remove message if all clients have acknowledged
			if (allClients == 0) {
				info.msgId = 0;
				int length = this->sendBuffer[info.offset];
				this->sendBufferFill -= length;
				this->busy = this->sendBufferFill + MAX_MESSAGE_LENGTH > SEND_BUFFER_SIZE;
			}
			break;
		}
	}
	
	// remove acknowledged messages and messages that don't need acknowledge
	for (int i = tail; i < this->sendMessagesCurrent; ++i) {
		MessageInfo &info = this->sendMessages[i];
	
		if (info.msgId == 0)
			++tail;
		else
			break;
	}
	this->sendMessagesTail = tail;

	SystemTime systemTime = getSystemTime();
	if (tail < this->sendMessagesCurrent) {
		// get next message
		MessageInfo &info = this->sendMessages[tail];
		
		// check if we have to resend it immediately
		if (systemTime >= info.sentTime + RETRANSMISSION_INTERVAL) {
			// resend a message that was not acknowledged or mark as pending
			if (!(this->resendPending = isDownSendBusy()))
				resend();
		} else {
			// wait until next message has to be resent
			setSystemTimeout1(info.sentTime + RETRANSMISSION_INTERVAL);
		}
	} else {
		// set timer for idle ping to keep the connection alive
		setSystemTimeout1(systemTime + KEEP_ALIVE_INTERVAL);
	}
}

void MqttSnBroker::sendPublish(uint16_t topicId, TopicInfo *topic, uint8_t const *data, int length, int8_t qos,
	bool retain, uint16_t excludeClientId)
{
	// handle retained message
	if (retain) {
		if (length > 0) {
			// store retained message
			if (length > topic->retainedAllocated) {
				// enlarge space for retained message
				topic->retainedOffset = insertRetained(topic->retainedOffset,
					length - topic->retainedAllocated);
				topic->retainedAllocated = length;
			}
			memcpy(&this->retained[topic->retainedOffset], data, length);
			topic->retainedLength = length;
		} else {
			// delete retained message
			topic->retainedOffset = eraseRetained(topic->retainedOffset,
				topic->retainedAllocated);
			topic->retainedAllocated = 0;
			topic->retainedLength = 0;
		}
	}
	
	// check if at least one remote client has subscribed the topic
	if (topic->isRemoteSubscribed()) {
		// publish to all clients that have subscribed to the topic
		ClientSet clientSet = {.flags = {}};
		for (ClientInfo &client : this->clients) {
			uint16_t clientId = client.clientId;
			if (clientId != 0 && clientId != excludeClientId) {
				int clientQos = topic->getQos(client.index);
				if (clientQos >= 0) {
					clientSet.flags[client.index >> 5] |= 1 << (client.index & 31);
				}
			}
		}
		
		// generate and set message id
		uint16_t msgId = getNextMsgId();

		// allocate message
		Message m = addSendMessage(6 + length, clientSet, msgId);
		if (m.data != nullptr) {
			// serialize message
			m.data[0] = mqttsn::PUBLISH;
			m.data[1] = mqttsn::makeQos(qos) | mqttsn::makeRetain(retain) | mqttsn::makeTopicType(mqttsn::TopicType::NORMAL);
			mqttsn::setUShort(m.data + 2, topicId);
			mqttsn::setUShort(m.data + 4, msgId);
			memcpy(m.data + 6, data, length);
			
			// send message
			if (!isDownSendBusy())
				sendCurrentMessage();
		} else {
			// error: message queue full
		}
	}

	// publish to local client if it has subscribed to the topic
	int8_t localQos = topic->getQos(LOCAL_CLIENT_INDEX);
	if (localQos >= 0)
		onPublished(topicId, data, length, min(qos, localQos), retain);
}

void MqttSnBroker::sendDisconnect(uint16_t clientId) {
	// create reply message
	Message m = addSendMessage(1, {clientId});
	if (m.data != nullptr) {
		// serialize message
		m.data[0] = mqttsn::DISCONNECT;
		
		// send message
		if (!isDownSendBusy())
			sendCurrentMessage();
	} else {
		// error: send message queue full
	}
}
