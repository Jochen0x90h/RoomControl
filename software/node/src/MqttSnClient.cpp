#include "MqttSnClient.hpp"

/*
static MqttSnClient::Udp6Endpoint const broadcast = {
	//{0xff, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01},
	{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01},
	0,
	MqttSnClient::GATEWAY_PORT
};
*/

MqttSnClient::MqttSnClient() {
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

MqttSnClient::Result MqttSnClient::connect(/*Udp6Endpoint const &gateway, uint8_t gatewayId,*/ String clientName,
	bool cleanSession, bool willFlag)
{
	//if (gatewayId == 0)
	//	return Result::INVALID_PARAMETER;
    if (clientName.empty() || clientName.length > MAX_MESSAGE_LENGTH - 5)
		return Result::INVALID_PARAMETER;
	if (!canConnect())
		return Result::INVALID_STATE;

	//this->gateway = gateway;
	//this->gatewayId = gatewayId;

	// allocate message, always succeeds because message queue is empty, set msgId to 1 so that retries are attempted
	Message m = addSendMessage(5 + clientName.length, 1);

	// serialize message
	m.data[0] = mqttsn::CONNECT;
	m.data[1] = 0; // flags
	m.data[2] = 0x01; // protocol name/version
	mqttsn::setUShort(m.data + 3, KEEP_ALIVE_INTERVAL.toSeconds());
	memcpy(m.data + 5, clientName.data, clientName.length);

	// send message (isSendUpBusy() is guaranteed to be false)
	sendCurrentMessage();

	// update state
	this->state = State::CONNECTING;

	return Result::OK;
}

MqttSnClient::Result MqttSnClient::disconnect() {
    if (!isConnected() && !isAwake())
		return Result::INVALID_STATE;
	
	// allocate message
	Message m = addSendMessage(1);
	if (m.data == nullptr)
		return Result::BUSY;

	// serialize message
	m.data[0] = mqttsn::DISCONNECT;

	// send message
	if (!isUpSendBusy())
		sendCurrentMessage();

	// update state
	if (isConnected())
		this->state = State::DISCONNECTING;
	else
		this->state = State::STOPPED;

	return Result::OK;
}

MqttSnClient::Result MqttSnClient::ping() {
    if (!isConnected())
		return Result::INVALID_STATE;

	// allocate message
	Message m = addSendMessage(1);
	if (m.data == nullptr)
		return Result::BUSY;

	// serialize message
	m.data[0] = mqttsn::PINGREQ;

	// send message
	if (!isUpSendBusy())
		sendCurrentMessage();

	return Result::OK;
}

MqttSnClient::MessageResult MqttSnClient::registerTopic(String topicName) {
    if (!isValid(topicName))
		return {Result::INVALID_PARAMETER, 0};
    if (!isConnected())
		return {Result::INVALID_STATE, 0};
	
	// generate message id
	uint16_t msgId = getNextMsgId();

	// allocate message
	Message m = addSendMessage(5 + topicName.length, msgId);
	if (m.data == nullptr)
		return {Result::BUSY, 0};

	// serialize message
	m.data[0] = mqttsn::REGISTER;
	mqttsn::setUShort(m.data + 1, 0);
	mqttsn::setUShort(m.data + 3, msgId);
	memcpy(m.data + 5, topicName.data, topicName.length);

	// send message
	if (!isUpSendBusy())
		sendCurrentMessage();

	return {Result::OK, msgId};
}

MqttSnClient::Result MqttSnClient::publish(uint16_t topicId, const uint8_t *data, int length, int8_t qos, bool retain,
	bool waitForTopicId)
{
    if (topicId == 0 || length > MAX_MESSAGE_LENGTH - 6)
		return Result::INVALID_PARAMETER;
    if (!isConnected())
		return Result::INVALID_STATE;
	
	// generate message id
	uint16_t msgId = getNextMsgId();

	// allocate message, set msgId only if we expect a PUBACK which is when qos > 0
	Message m = addSendMessage(6 + length, qos > 0 ? msgId : 0, waitForTopicId);
	if (m.data == nullptr)
		return Result::BUSY;

	// serialize message
	m.data[0] = mqttsn::PUBLISH;
	m.data[1] = mqttsn::makeQos(qos) | mqttsn::makeRetain(retain) | mqttsn::makeTopicType(mqttsn::TopicType::NORMAL);
	mqttsn::setUShort(m.data + 2, topicId);
	mqttsn::setUShort(m.data + 4, msgId);
	memcpy(m.data + 6, data, length);

	// send message
	if (!isUpSendBusy())
		sendCurrentMessage();

	return Result::OK;
}

MqttSnClient::MessageResult MqttSnClient::subscribeTopic(String topicFilter, int8_t qos) {
	if (!isValid(topicFilter))
		return {Result::INVALID_PARAMETER, 0};
	if (!isConnected())
		return {Result::INVALID_STATE, 0};
	
	// generate message id
	uint16_t msgId = getNextMsgId();

	// allocate message
	Message m = addSendMessage(4 + topicFilter.length, msgId);
	if (m.data == nullptr)
		return {Result::BUSY, 0};

	// serialize message
	m.data[0] = mqttsn::SUBSCRIBE;
	m.data[1] = mqttsn::makeQos(qos) | mqttsn::makeTopicType(mqttsn::TopicType::NORMAL);
	mqttsn::setUShort(m.data + 2, msgId);
	memcpy(m.data + 4, topicFilter.data, topicFilter.length);

	// send message
	if (!isUpSendBusy())
		sendCurrentMessage();

	return {Result::OK, msgId};
}

MqttSnClient::Result MqttSnClient::unsubscribeTopic(String topicFilter) {
	if (!isValid(topicFilter))
		return Result::INVALID_PARAMETER;
	if (!isConnected())
		return Result::INVALID_STATE;

	// generate message id
	uint16_t msgId = getNextMsgId();

	// allocate message
	Message m = addSendMessage(4 + topicFilter.length, msgId);
	if (m.data == nullptr)
		return Result::BUSY;

	// serialize message
	m.data[0] = mqttsn::UNSUBSCRIBE;
	m.data[1] = mqttsn::makeTopicType(mqttsn::TopicType::NORMAL);
	mqttsn::setUShort(m.data + 2, msgId);
	memcpy(m.data + 4, topicFilter.data, topicFilter.length);

	// send message
	if (!isUpSendBusy())
		sendCurrentMessage();

	return Result::OK;
}


// transport
// ---------

void MqttSnClient::onUpReceived(uint8_t const *data, int length) {
	// check if length and message type are present
			
	// get message type
	mqttsn::MessageType messageType = mqttsn::MessageType(data[0]);

	// handle message depending on state
	switch (this->state) {
	case State::STOPPED:
		break;
/*
	case State::SEARCHING_GATEWAY:
		switch (messageType) {
		case MQTTSN_ADVERTISE:
			// broadcasted periodically by a gateway to advertise its presence
			break;
		case MQTTSN_GWINFO:
			// sent by the gateway in response to our SEARCHGW request
			{
				// remove SEARCHGW message
				removeSentMessage(1);

				uint8_t gatewayId = data[typeOffset + 1];
				onGatewayFound(sender, gatewayId);
			}
			break;
		default:
			// error
			onUnsupportedError(messageType);
		}
		break;
		*/
	case State::CONNECTING:
		switch (messageType) {
		case mqttsn::CONNACK:
			// sent by the gateway in response to our CONNECT request
			if (length < 2) {
				onError(ERROR_MESSAGE, messageType);
			} else {
				// deserialize message
				mqttsn::ReturnCode returnCode = mqttsn::ReturnCode(data[1]);

				// remove CONNECT message
				removeSentMessage(1, mqttsn::CONNECT);

				switch (returnCode)
				{
				case mqttsn::ReturnCode::ACCEPTED:
					// set timer for idle ping to keep the connection alive
					setSystemTimeout1(getSystemTime() + KEEP_ALIVE_INTERVAL);

					// now we are connected to the gateway
					this->state = State::CONNECTED;
					onConnected();
					break;
				case mqttsn::ReturnCode::REJECTED_CONGESTED:
					// the gateway rejected the connection request due to congestion
					onError(ERROR_CONGESTED, messageType);
					break;
				default:
					;
				}
			}
			break;
		default:
			// error
			onError(ERROR_UNSUPPORTED, messageType);
		}
		break;
	default:
		// all other states such as CONNECTED
		switch (messageType) {
		case mqttsn::DISCONNECT:
			switch (this->state) {
			case State::WAITING_FOR_SLEEP:
				// sent by the gateway in response to a sleep request
				stopSystemTimeout1();
				this->state = State::ASLEEP;
				onSleep();
				break;
			default:
				// sent by the gateway in response to a disconnect request
				// or the gateway has dropped our connection for some reason
				stopSystemTimeout1();
				this->state = State::STOPPED;
				onDisconnected();
			}
			break;
		/*case mqttsn::REGISTER:
			// sent by the gateway to inform us about the topic id it has assigned to a topic name
			if (length < 6) {
				onError(ERROR_PACKET, messageType);
			} else {
				// deserialize message
				uint16_t topicId = mqttsn::getUShort(data + 1);
				uint16_t msgId = mqttsn::getUShort(data + 3);
				String topicName(data + 5, length - 5);

				// reply to gateway with regack
				Message m = addSendMessage(6);
				if (m.info != nullptr) {
					// serialize message
					m.data[0] = mqttsn::REGACK;
					mqttsn::setUShort(m.data + 1, topicId);
					mqttsn::setUShort(m.data + 3, msgId);
					m.data[5] = uint8_t(mqttsn::ReturnCode::ACCEPTED);

					// send message
					if (!isSendUpBusy())
						sendCurrentMessage();
				} else {
					// error: send message queue full
				}
				
				// user callback
				onRegister(topicName, topicId);
			}
			break;*/
		case mqttsn::REGACK:
			// sent by the gateway in response to a REGISTER message
			if (length < 6) {
				onError(ERROR_MESSAGE, messageType);
			} else {
				// deserialize message
				uint16_t topicId = mqttsn::getUShort(data + 1);
				uint16_t msgId = mqttsn::getUShort(data + 3);
				mqttsn::ReturnCode returnCode = mqttsn::ReturnCode(data[5]);

				if (returnCode == mqttsn::ReturnCode::ACCEPTED) {
					// set topic id to PUBLISH messages that were already published on this topic
					setTopicId(msgId, topicId);

					// remove REGISTER message, data stays valid until methods are called on this or return to event loop
					Message sm = removeSentMessage(msgId, mqttsn::REGISTER);
					if (sm.data != nullptr) {
						String topicName(sm.data + 5, sm.length - 5);
						onRegistered(msgId, topicName, topicId);
					}
				} else {
					// report error
					onError(ERROR_CONGESTED, messageType);

					// try again
					if (!(this->resendPending = isUpSendBusy()))
						resend();
				}
			}
			break;
		case mqttsn::PUBLISH:
			// sent by the gateway to publish a message on a topic we have subscribed
			if (length < 6) {
				onError(ERROR_MESSAGE, messageType);
			} else {
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

				if (qos <= 0 || !isBusy()) {
					// user callback
					mqttsn::ReturnCode returnCode = mqttsn::ReturnCode::REJECTED_INVALID_TOPIC_ID;
					if (topicType == mqttsn::TopicType::NORMAL)
						returnCode = onPublish(topicId, payload, payloadLength, qos, retain);

					if (qos > 0) {
						// reply to gateway with puback
						Message m = addSendMessage(6);
						
						// serialize message
						m.data[0] = mqttsn::PUBACK;
						mqttsn::setUShort(m.data + 1, topicId);
						mqttsn::setUShort(m.data + 3, msgId);
						m.data[5] = uint8_t(returnCode);

						// send message
						if (!isUpSendBusy())
							sendCurrentMessage();
					}
				} else {
					// error: send message queue full
				}
			}
			break;
		case mqttsn::PUBACK:
			// sent by the gateway in response to a PUBLISH message (if qos was greater zero)
			if (length < 6) {
				onError(ERROR_MESSAGE, messageType);
			} else {
				// deserialize message
				uint16_t topicId = mqttsn::getUShort(data + 1);
				uint16_t msgId = mqttsn::getUShort(data + 3);
				mqttsn::ReturnCode returnCode = mqttsn::ReturnCode(data[5]);
				
				if (returnCode == mqttsn::ReturnCode::ACCEPTED) {
					// remove PUBLISH message from queue
					removeSentMessage(msgId, mqttsn::PUBLISH);
				} else {
					// report error
					onError(ERROR_CONGESTED, messageType);

					// try again
					if (!(this->resendPending = isUpSendBusy()))
						resend();
				}
			}
			break;
		case mqttsn::SUBACK:
			// sent by the gateway in response to a SUBSCRIBE message
			if (length < 7) {
				onError(ERROR_MESSAGE, messageType);
			} else {
				// deserialize message
				uint8_t flags = data[1];
				int8_t qos = mqttsn::getQos(flags);
				uint16_t topicId = mqttsn::getUShort(data + 2);
				uint16_t msgId = mqttsn::getUShort(data + 4);
				mqttsn::ReturnCode returnCode = mqttsn::ReturnCode(data[6]);

				// remove SUBSCRIBE message, data stays valid until methods are called on this or return to event loop
				Message m = removeSentMessage(msgId, mqttsn::SUBSCRIBE);
				if (m.data != nullptr) {
					switch (returnCode) {
					case mqttsn::ReturnCode::ACCEPTED:
						{
							String topicName(m.data + 4, m.length - 4);
							onSubscribed(msgId, topicName, topicId, qos);
						}
						break;
					case mqttsn::ReturnCode::REJECTED_CONGESTED:
						onError(ERROR_CONGESTED, messageType);
						break;
					default:
						;
					}
				}
			}
			break;
		case mqttsn::UNSUBACK:
			// sent by the gateway in response to a UNSUBSCRIBE message
			if (length < 3) {
				onError(ERROR_MESSAGE, messageType);
			} else {
				// deserialize message
				uint16_t msgId = mqttsn::getUShort(data + 1);

				// remove UNSUBSCRIBE message from queue
				removeSentMessage(msgId, mqttsn::UNSUBSCRIBE);
			}
			break;
		case mqttsn::PINGRESP:
			// sent by the gateway in response to a PINGREQ message ("I am alive") and to indicate that it has no more
			// buffered data to a sleeping client
			break;
	/*
		case mqttsn::WILLTOPICREQ:
			break;
		case mqttsn::WILLMSGREQ:
			break;
		case mqttsn::WILLTOPICRESP:
			break;
		case mqttsn::WILLMSGRESP:
			break;
	*/
		default:
			// error: unsupported message type
			onError(ERROR_UNSUPPORTED, messageType);
		}
	}
}

void MqttSnClient::onUpSent() {
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

void MqttSnClient::onSystemTimeout1(SystemTime time) {
	
	switch (this->state) {
	case State::STOPPED:
		break;
	/*case State::SEARCHING_GATEWAY:
		// try again (SEARCHGW message)
		resend();
		break;*/
	case State::CONNECTING:
		// try again (CONNECT message)
		resend();
		break;
	case State::CONNECTED:
		if (this->sendMessagesTail < this->sendMessagesCurrent) {
			// resend a message that was not acknowledged or mark as pending
			if (!(this->resendPending = isUpSendBusy()))
				resend();
		} else {
			// idle ping
			ping();
		}
		break;
	default:
		;
	}
}

// internal
// --------

MqttSnClient::Message MqttSnClient::addSendMessage(int length, uint16_t msgId, bool waitForTopicId) {
	if (this->busy)
		return {};

	// garbage collect old messages
	if (this->sendMessagesHead + 1 + 1 > MAX_SEND_COUNT
		|| this->sendBufferHead + length + MAX_MESSAGE_LENGTH > SEND_BUFFER_SIZE)
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
				memmove(this->sendBuffer + offset, data, length);
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
	
	// set message id
	info.msgId = msgId;
	info.waitForTopicId = waitForTopicId;

	// set busy flag if no new message will fit
	this->busy = this->sendMessagesHead >= MAX_SEND_COUNT || this->sendBufferFill + MAX_MESSAGE_LENGTH > SEND_BUFFER_SIZE;

	
	// return allocated message (space for "naked" mqtt-sn message without length)
	return {this->sendBuffer + info.offset, length};
}

void MqttSnClient::sendCurrentMessage() {
	int current = this->sendMessagesCurrent;

	// get message
	MessageInfo &info = this->sendMessages[current];
	
	// send to gateway
	upSend(this->sendBuffer + info.offset, info.length);

	// set sent time
	SystemTime now = getSystemTime();
	info.sentTime = now;

	if (current == this->sendMessagesTail) {
		if (info.msgId == 0) {
			// remove message from tail because it's not needed any more such as PINGREQ
			this->sendMessagesTail = current + 1;
		
			// set timer for idle ping to keep the connection alive
			setSystemTimeout1(now + KEEP_ALIVE_INTERVAL);
		} else {
			// start retransmission timeout
			setSystemTimeout1(now + RETRANSMISSION_INTERVAL);
		}
	}

	// make next message current
	this->sendMessagesCurrent = current + 1;
}

void MqttSnClient::resend() {
	// todo: disconnect if maximum resend count exceeded

	// get message
	MessageInfo &info = this->sendMessages[this->sendMessagesTail];
	
	// send to gateway
	upSend(this->sendBuffer + info.offset, info.length);

	// set sent time
	SystemTime now = getSystemTime();
	info.sentTime = now;

	// start retransmission timeout
	setSystemTimeout1(now + RETRANSMISSION_INTERVAL);
}

void MqttSnClient::setTopicId(uint16_t msgId, uint16_t topicId) {
	// + 1 because we can skip tail which must be the REGACK message
	for (int i = this->sendMessagesTail + 1; i < this->sendMessagesCurrent; ++i) {
		MessageInfo &info = this->sendMessages[i];
		uint8_t *data = this->sendBuffer + info.offset;
		
		// check if this PUBLISH message waits for the topic id
		// if yes, it has the msgId of the REGISTER message in the topicId field
		if (info.waitForTopicId && mqttsn::getUShort(data + 2) == msgId) {
			info.waitForTopicId = false;
			
			// set topicId in message
			mqttsn::setUShort(data + 2, topicId);
		}
	}
}

MqttSnClient::Message MqttSnClient::removeSentMessage(uint16_t msgId, mqttsn::MessageType type) {
	Message m = {};
	int tail = this->sendMessagesTail;
	
	// mark message with msgId as obsolete
	for (int i = tail; i < this->sendMessagesCurrent; ++i) {
		MessageInfo &info = this->sendMessages[i];
		uint8_t *data = this->sendBuffer + info.offset;
		if (info.msgId == msgId && mqttsn::MessageType(data[0]) == type) {
			// mark this message as done
			info.msgId = 0;
			int length = info.length;
			m.data = data;
			m.length = length;
			this->sendBufferFill -= length;
			this->busy = this->sendBufferFill + MAX_MESSAGE_LENGTH > SEND_BUFFER_SIZE;
			break;
		}
	}
	
	// remove acknowledged messages and messages that don't need acknowledge
	for (int i = tail; i < this->sendMessagesCurrent; ++i) {
		MessageInfo &info = this->sendMessages[i];
	
		if (info.msgId == 0 || info.waitForTopicId)
			++tail;
		else
			break;
	}
	this->sendMessagesTail = tail;
	
	// start timeout for next message or keep alive
	SystemTime now = getSystemTime();
	if (tail < this->sendMessagesCurrent) {
		// get next message
		MessageInfo &info = this->sendMessages[tail];
		
		// check if its retransmission time has been reached
		SystemTime retransmissionTime = info.sentTime + RETRANSMISSION_INTERVAL;
		if (now >= retransmissionTime) {
			// resend a message that was not acknowledged (or mark as pending if busy)
			if (!(this->resendPending = isUpSendBusy()))
				resend();
		} else {
			// wait until it has to be resent
			setSystemTimeout1(retransmissionTime);
		}
	} else {
		// set timer for idle ping to keep the connection alive
		setSystemTimeout1(now + KEEP_ALIVE_INTERVAL);
	}
	
	return m;
}
