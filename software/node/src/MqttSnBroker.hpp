#pragma once

#include "MqttSnClient.hpp"
#include "Message.hpp"
#include "Publisher.hpp"
#include "Subscriber.hpp"
#include <BitField.hpp>
#include <LinkedListNode.hpp>
#include <StringBuffer.hpp>


/**
 * MQTT-SN broker that clients can connect to and which connects to a gateway using MqttSnClient
 */
class MqttSnBroker {
public:
	// Maximum length of Client ID according to the protocol spec in bytes
	static constexpr int MAX_CLIENT_ID_LENGTH = 23;

	// Maximum length of a message
	static constexpr int MAX_MESSAGE_LENGTH = 64;

	// reconnect time (after this time a new connection attempt to the gateway is made)
	static constexpr SystemDuration RECONNECT_TIME = 60s;

	// retransmission time (after this time a message is resent if no acknowledge was received)
	static constexpr SystemDuration RETRANSMISSION_TIME = 1s;

	// the MQTT broker disconnects us if we don't send anything in one and a half times the keep alive time
	static constexpr SystemDuration KEEP_ALIVE_TIME = 60s;

	// maximum number of connections (first for gateway, others for clients)
	static constexpr int MAX_CONNECTION_COUNT = 32;
	
	// maximum number of topics that can be handled
	static constexpr int MAX_TOPIC_COUNT = 1024;


	MqttSnBroker(uint16_t localPort);

	/**
	 * Connect to the gateway
	 * @param gatewayEndpoint udp endpoint of gateway
	 * @param name name of this client
	 * @param cleanSession start with a clean session, i.e. clear all previous subscriptions and will topic/message
	 * @param willFlag if set, the gateway will request will topic and message
	 */
	[[nodiscard]] AwaitableCoroutine connect(network::Endpoint const &gatewayEndpoint, String name,
		bool cleanSession = true, bool willFlag = false);

	bool isGatewayConnected() {
		return this->connectedFlags.get(0) != 0;
	}

	/**
	 * Ping gateway to reset keep alive timer
	 */
	[[nodiscard]] AwaitableCoroutine ping();
	

	/**
	 * Add a publisher to the device. Gets inserted into a linked list
	 * @param deviceId device id
	 * @param publisher publisher to insert
	 */
	void addPublisher(String topicName, Publisher &publisher);
	[[nodiscard]] AwaitableCoroutine registerPublisher(String topicName);

	/**
	 * Add a subscriber to the device. Gets inserted into a linked list
	 * @param deviceId device id
	 * @param publisher subscriber to insert
	 */
	void addSubscriber(String topicName, Subscriber &subscriber);
	[[nodiscard]] AwaitableCoroutine registerSubscriber(String topicName);


	using MessageReader = MqttSnClient::MessageReader;

	// data writer for mqtt-sn messages
	class MessageWriter : public DataWriter {
	public:
		/**
		 * Construct on radio packet where the length (including 2 byte crc) is in the first byte
		 */
		template <int N>
		MessageWriter(uint8_t (&message)[N]) : DataWriter(message + 1), begin(message)
#ifdef EMU
			, end(message + N)
#endif
		{}

		void uint16(uint16_t value) {
			this->current[0] = value >> 8;
			this->current[1] = value;
			this->current += 2;
		}

		template <typename T>
		void enum16(T value) {
			static_assert(std::is_same<typename std::underlying_type<T>::type, uint16_t>::value);
			uint16(uint16_t(value));
		}

		void floatString(float value, int digitCount, int decimalCount) {
			this->current += toString(value, reinterpret_cast<char *>(this->current), 10, digitCount, decimalCount);
		}

		/**
		 * Set send flags and length of packet
		 */
		Array<uint8_t const> finish() {
#ifdef EMU
			assert(this->current < this->end);
#endif
			int length = this->current - this->begin;
			this->begin[0] = length;
			return {length, this->begin};
		}

		uint8_t *begin;
#ifdef EMU
		uint8_t *end;
#endif
	};

protected:
	
	bool isConnected(int connectionIndex) {
		return this->connectedFlags.get(connectionIndex) != 0;
	}
	
	// initialize a connection to gateway or client
	void initConnection(int connectionIndex);

	/**
	 * Get or add topic by name and return its index
	 * @param name topic name (path without wildcards)
	 * @param add add new topic if true
	 * @return -1 if no new topic could be found or added
	 */
	int getTopicIndex(String name, bool add = true);

	// get a message id for publish messages of qos 1 or 2 to detect resent messages and associate acknowledge
	uint16_t getNextMsgId() {
		int i = this->nextMsgId + 1;
		return this->nextMsgId = i + (i >> 16);
	}
	
	// publish messages to gateway and clients
	Coroutine publish();
	
	// receive and distribute messages
	Coroutine receive();


	struct ConnectionInfo {
		// endpoint (address and port) of client or gateway
		network::Endpoint endpoint;
				
		// name of client or this broker
		StringBuffer<MAX_CLIENT_ID_LENGTH> name;
		
		// check if connection is active
		//bool isConnected() {return this->endpoint.port != 0;}
	};
	
	struct TopicInfo {
		// hash of topic string or zero for unused entry
		uint32_t hash;
		
		// retained message
		//uint16_t retainedOffset;
		//uint8_t retainedAllocated;
		//uint8_t retainedLength;

		// for each connection two bits that indicate subscription qos (0-2: qos, 3: not subscribed)
		BitField<MAX_CONNECTION_COUNT, 2> qosArray;

		// topic id at gateway (broker at other side of up-link)
		uint16_t gatewayTopicId;
	};
	

	// message id generator
	uint16_t nextMsgId = 0;
	
	// connections to gateway and clients
	ConnectionInfo connections[MAX_CONNECTION_COUNT];
	BitField<MAX_CONNECTION_COUNT, 1> connectedFlags;
	
	// topics
	int topicCount = 0;
	TopicInfo topics[MAX_TOPIC_COUNT];

	
	// subscribers
	SubscriberList subscribers;

	// publishers
	Event publishEvent;
	PublisherList publishers;
	Publisher *currentPublisher = nullptr;
	BitField<MAX_CONNECTION_COUNT, 1> dirtyFlags;

	
	struct AckParameters {
		uint8_t connectionIndex;
		mqttsn::MessageType msgType;
		uint16_t msgId;
		int &length;
		uint8_t *message;
	};
	Waitlist<AckParameters> ackWaitlist;

	struct ReceiveParameters {
		int &length;
		uint8_t *message;
	};
	Waitlist<ReceiveParameters> receiveWaitlist;
};
