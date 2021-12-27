#pragma once

#include <network.hpp>
#include <timer.hpp>
#include <DataReader.hpp>
#include <DataWriter.hpp>
#include <mqttsn.hpp>
#include <String.hpp>
#include <appConfig.hpp>



/**
 * MQTT-SN client
 * https://www.oasis-open.org/committees/download.php/66091/MQTT-SN_spec_v1.2.pdf
 * Authentication extension (not supported yet): https://modelbasedtesting.co.uk/2020/10/22/mqtt-sn-authentication/
 * Gateway search is not supported, the gateway ip address and port must be configured
 * Default gateway port: 47193
 * Default local port of mqtt client: 47194
 */
class MqttSnClient {
public:

	// Maximum length of Client ID according to the protocol spec in bytes
	static constexpr int MAX_CLIENT_ID_LENGTH = 23;

	// Maximum length of a message
	static constexpr int MAX_MESSAGE_LENGTH = 64;

	// Default retransmission time
	static constexpr SystemDuration RETRANSMISSION_TIME = 1s;

	// The MQTT broker disconnects us if we don't send anything in one and a half times the keep alive time
	static constexpr SystemDuration KEEP_ALIVE_TIME = 60s;

	// Maximum length of will feature buffers. For internal use only
	static constexpr int WILL_TOPIC_MAX_LENGTH = 32;
	static constexpr int WILL_MSG_MAX_LENGTH = 32;


	// request result
	enum class Result {
		// method executed ok
		OK,
		
		// invalid method parameter
		INVALID_PARAMETER,
		
		// method invalid in current client state (e.g. client was disconnected)
		INVALID_STATE,
		
		// no reply from gateway
		TIMEOUT,
		
		// received an unexpected message from gateway
		PROTOCOL_ERROR,
		
		// request was rejected by the gateway (try again later)
		REJECTED,
	};

	// state of this MQTT-SN client
	enum class State {
		DISCONNECTED,
		//SEARCHING_GATEWAY,
		CONNECTED,
		ASLEEP,
		AWAKE,
		WAITING_FOR_SLEEP
	};

	struct Topic {
		const char *topicName;
		uint16_t topicId;
	};
	

	MqttSnClient(uint16_t localPort);

	virtual ~MqttSnClient();

	/**
	 * Get state of the client
	 */
	State getState() {return this->state;}
	bool isDisconnected() {return this->state == State::DISCONNECTED;}
	bool isConnected() {return this->state == State::CONNECTED;}
	bool isAsleep() {return this->state == State::ASLEEP;}
	bool isAwake() {return this->state == State::AWAKE;}
	bool canConnect() {return isDisconnected() || isAsleep() || isAwake();}


	/**
	 * Connect to the gateway
	 * @param result result
	 * @param gatewayEndpoint udp endpoint of gateway
	 * @param name name of this client
	 * @param cleanSession start with a clean session, i.e. clear all previous subscriptions and will topic/message
	 * @param willFlag if set, the gateway will request will topic and message
	 */
	AwaitableCoroutine connect(Result &result, network::Endpoint const &gatewayEndpoint, String name,
		bool cleanSession = true, bool willFlag = false);
	
	/**
	 * Disconnect from gateway. Always succeeds as we assume to be disconnected when the gateway does not reply
	 * @return use co_await on return value
	 */
	AwaitableCoroutine disconnect();

	/**
	 * Register a topic at the gateway
	 * @param result result of the command
	 * @param topicId id assigned to the topic
	 * @param topicName topic name (path without wildcards)
	 * @return use co_await on return value
	 */
	AwaitableCoroutine registerTopic(Result &result, uint16_t &topicId, String topicName);

	/**
	 * Publish a message on a topic
	 * @param result result of the command
	 * @param topicId topic (or message) id obtained using registerTopic() and onRegisterd()
	 * @param data payload data
	 * @param length payload data length
	 * @param qos quality of service level: 0, 1, 2 or -1. Must not be higher than granted qos for the topic
	 * @param retain retain message and deliver to new subscribers
	 * @return use co_await on return value
	 */
	AwaitableCoroutine publish(Result &result, uint16_t topicId, mqttsn::Flags flags, int length, uint8_t const *data);
	AwaitableCoroutine publish(Result &result, uint16_t topicId, mqttsn::Flags flags, String data) {
		return publish(result, topicId, flags, data.length, reinterpret_cast<uint8_t const*>(data.data));
	}

	/**
	 * Subscribe to a topic
	 * @param result out: result of the command
	 * @param topicId out: assigned topic id
	 * @param qos in: requested quality of service level of the subscription, out: granted level
	 * @param topicFilter topic filter (path that may contain wildcards)
	 * @return use co_await on return value
	 */
	AwaitableCoroutine subscribeTopic(Result &result, uint16_t& topicId, int8_t &qos, String topicFilter);

	/**
	 * Unsubscribe from a topic
	 * @param result result of the command
	 * @param flags in: requested quality of service level of the subscription and topic type, out: granted level
	 * @param topicFilter topic filter (path that may contain wildcards)
	 * @return use co_await on return value
	 */
	AwaitableCoroutine unsubscribeTopic(Result &result, String topicFilter);

	/**
	 * Receive messages that are published on topics to which we are subscribed
	 * @param result result of the command
	 * @param msgId message id to be used in ackReceive()
	 * @param topicId topic id
	 * @param flags flags containing qos and topic type
	 * @param length length of received data
	 * @param data received data
	 * @return use co_await on return value
	 */
	AwaitableCoroutine receive(Result &result, uint16_t& msgId, uint16_t& topicId, mqttsn::Flags &flags, int &length, uint8_t *data);
	
	/**
	 * Acknowledge the reception of a message
	 * @param msgId message id to be used in ackReceive()
	 * @param topicId topic id
	 * @param ok pass true to indicate that msgId matches a known topic, false otherwise
	 * @return use co_await on return value
	 */
	Awaitable<network::SendParameters> ackReceive(uint16_t msgId, uint16_t topicId, bool ok);

	/**
	 * Check if a topic is valid
	 * @return ture if topic is valid
	 */
	static bool isValid(String topic) {return uint32_t(topic.length - 1) <= uint32_t(MAX_MESSAGE_LENGTH - 5 - 1);}


	// data reader for radio packets
	class MessageReader : public DataReader {
	public:
		/**
		 * Construct on message where the length is in first one or three bytes
		 */
		MessageReader(uint8_t *message) : DataReader(2, message + 1) {
			if (message[0] == 1) {
				// length is three bytes
				int length = uint16();
				this->end = message + length;
			} else {
				// length is one byte
				int length = message[0];
				this->end = message + length;
			}
		}

		MessageReader(int length, uint8_t *message) : DataReader(length, message) {}

		uint16_t uint16() {
			int16_t value = (this->current[0] << 8) | this->current[1];
			this->current += 2;
			return value;
		}

		template <typename T>
		T enum16() {
			static_assert(std::is_same<typename std::underlying_type<T>::type, uint16_t>::value);
			return T(uint16());
		}
		
		/**
		 * Returns true if the reader is still valid, i.e. did not read past the end
		 * @return true when valid
		 */
		bool isValid() {
			return this->current <= this->end;
		}
		
		int getRemaining() {
			return this->end - this->current;
		}
	};

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
	
	// get an id for publish messages of qos 1 or 2 to detect resent messages and associate acknowledge
	uint16_t getNextMsgId() {
		int i = this->nextMsgId + 1;
		return this->nextMsgId = i + (i >> 16);
	}

	//Configuration &configuration;
	network::Endpoint gatewayEndpoint;

private:

	// ping gateway to reset keep alive timer
	AwaitableCoroutine ping();
	
	// receive and distribute messages
	AwaitableCoroutine receive();
	
	void disconnectInternal();
	
	
	// state
	State state = State::DISCONNECTED;

	// message id generator
	uint16_t nextMsgId = 0;

	uint8_t tempMessage[MAX_MESSAGE_LENGTH];

	AwaitableCoroutine pingCoroutine;
	AwaitableCoroutine receiveCoroutine;

	struct AckParameters {
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
