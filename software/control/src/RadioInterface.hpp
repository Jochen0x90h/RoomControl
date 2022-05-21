#pragma once

#include "Interface.hpp"
#include <Configuration.hpp>
#include <State.hpp>
#include <Storage.hpp>
#include <Radio.hpp>
#include <crypt.hpp>
#include <MessageReader.hpp>
#include <MessageWriter.hpp>
#include <Coroutine.hpp>
#include <appConfig.hpp>


/**
 * Devices connected by radio using IEEE 802.15.4 standard
 */
class RadioInterface : public Interface {
public:
	// maximum number of devices that can be commissioned
	static constexpr int MAX_DEVICE_COUNT = 64;

	// number of coroutines for receiving and publishing
	static constexpr int PUBLISH_COUNT = 4;
	static constexpr int RECEIVE_COUNT = 4;


	/**
	 * Constructor
	 * @param configuration global configuration
	 * @param stateManager persistent state manager for counters
	 */
	RadioInterface(PersistentStateManager &stateManager);

	~RadioInterface() override;

	// start the interface
	Coroutine start(uint16_t panId, DataBuffer<16> const &key, AesKey const &aesKey);

	void setCommissioning(bool enabled) override;

	int getDeviceCount() override;
	Device &getDeviceByIndex(int index) override;
	Device *getDeviceById(uint8_t id) override;

private:

	// counters
	uint8_t macCounter = 0;
	uint8_t nwkCounter = 0;
	uint8_t apsCounter = 0;
	uint8_t zdpCounter = 0;
	uint8_t zclCounter = 0;
	PersistentState<uint32_t> securityCounter;
	uint8_t routeCounter = 0;

	// configuration
	uint16_t panId;
	DataBuffer<16> const *key;
	AesKey const *aesKey;

	// self-powered devices
	class GpDevice;

	struct GpDeviceFlash {
		static constexpr int MAX_ENDPOINT_COUNT = 32;

		// 32 bit green power id
		uint32_t deviceId;

		// device key
		AesKey aesKey;

		// interface id
		uint8_t interfaceId;

		// device type from commissioning message
		uint8_t deviceType;

		// number of endpoints of the device
		uint8_t endpointCount;

		// endpoint types
		MessageType endpoints[MAX_ENDPOINT_COUNT];

		// note: endpoints must be the last member

		/**
		 * Returns the size in bytes needed to store the device configuration in flash
		 * @return size in bytes
		 */
		int size() const;

		/**
		 * Allocates a state object
		 * @return new state object
		 */
		GpDevice *allocate() const;
	};

	class GpDevice : public Device, public Storage::Element<GpDeviceFlash> {
	public:
		explicit GpDevice(GpDeviceFlash const &flash) : Storage::Element<GpDeviceFlash>(flash) {}
		~GpDevice() override;

		uint8_t getId() const override;
		String getName() const override;
		void setName(String name) override;
		Array<MessageType const> getEndpoints() const override;
		void addPublisher(uint8_t endpointIndex, Publisher &publisher) override;
		void addSubscriber(uint8_t endpointIndex, Subscriber &subscriber) override;

		// back pointer to interface
		RadioInterface *interface;

		// last security counter value of device
		// todo: make persistent
		uint32_t securityCounter = 0;

		uint8_t state = 0;

		// subscriptions
		SubscriberList subscribers;
	};


	// zbee devices
	class ZbDevice;

	struct ZbDeviceFlash {
		static constexpr int MAX_ENDPOINT_COUNT = 32;

		// 64 bit ieee 802.15.4 long device address, also used as device id
		uint64_t longAddress;

		// short device address
		uint16_t shortAddress;

		// interface id
		uint8_t interfaceId;

		// send flags for this device (wait for data request or not)
		Radio::SendFlags sendFlags;

		// number of endpoints of the device
		uint8_t endpointCount;

		// endpoint types followed by pairs of endpoint info index and zbee endpoint index
		uint8_t endpoints[MAX_ENDPOINT_COUNT * 3];

		// note: endpoints must be the last member

		/**
		 * Returns the size in bytes needed to store the device configuration in flash
		 * @return size in bytes
		 */
		int size() const;

		/**
		 * Allocates a state object
		 * @return new state object
		 */
		ZbDevice *allocate() const;

		// pairs of endpoint info index and zbee endpoint index
		uint8_t const *getEndpointIndices() const {return this->endpoints + this->endpointCount;}
	};

	class ZbDevice : public Storage::Element<ZbDeviceFlash>, public Device {
	public:
		explicit ZbDevice(ZbDeviceFlash const &flash) : Storage::Element<ZbDeviceFlash>(flash) , sendFlags(flash.sendFlags) {}
		~ZbDevice() override;

		uint8_t getId() const override;
		String getName() const override;
		void setName(String name) override;
		Array<MessageType const> getEndpoints() const override;
		void addPublisher(uint8_t endpointIndex, Publisher &publisher) override;
		void addSubscriber(uint8_t endpointIndex, Subscriber &subscriber) override;

		// back pointer to interface
		RadioInterface *interface = nullptr;

		// send flags for next hop in route (wait for data request or not)
		Radio::SendFlags sendFlags;

		// last security counter value of device
		// todo: make persistent
		uint32_t securityCounter = 0;

		//std::vector<uint16_t> route;

		// short address of router for this device (0xffff means that the route is unknown)
		uint16_t routerAddress = 0xffff;

		// cost of the route to the device
		uint8_t cost;

		static constexpr int RESPONSE_LENGTH = 64;


		struct ZdpResponse {
			zb::ZdpCommand command;
			uint8_t zdpCounter;
			uint16_t& length;
			uint8_t *response;
		};

		// a coroutine (handleAssociationRequest()) waits on this barrier until a zdp response arrives
		Waitlist<ZdpResponse> zdpResponseBarrier;


		struct ZclResponse {
			uint8_t dstEndpoint;
			zb::ZclCluster cluster;
			zb::ZclProfile profile;
			uint8_t srcEndpoint;
			uint8_t zclCounter;
			zb::ZclCommand command;
			uint16_t& length;
			uint8_t *response;
		};

		// a coroutine (handleAssociationRequest()) waits on this barrier until a zcl response arrives
		Waitlist<ZclResponse> zclResponseBarrier;

		// list of subscribers
		SubscriberList subscribers;

		// list of publishers
		PublisherList publishers;

		// barrier for waiting until a route is available
		Barrier<> routeBarrier;
	};



	// find zbee device by short address
	ZbDevice *findZbDevice(uint16_t address);

	void allocateInterfaceId();
	void allocateNextAddress();

	// next interface id and short address "on stock" to be fast when a device sends an association request
	uint8_t nextInterfaceId;
	uint16_t nextShortAddress;

public:

	// list of commissioned devices
	Storage::Array<GpDevice> gpDevices;
	Storage::Array<ZbDevice> zbDevices;


	// data reader for radio packets
	class PacketReader : public DecryptReader {
	public:
		/**
		 * Construct on radio packet where the length (including 2 byte crc) is in the first byte
		 */
		PacketReader(uint8_t *packet) : DecryptReader(packet[0] - 2, packet + 1) {}

		/**
		 * Construct on data
		 */
		PacketReader(int length, uint8_t *data) : DecryptReader(length, data) {}

		void restoreSecurityLevel(zb::SecurityControl securityLevel) {
			*this->current |= uint8_t(securityLevel);
		}
	};

	struct WriterState {
		uint8_t const *header;
		uint8_t *message;
		uint8_t *securityControl;
		uint32_t securityCounter;
	};

	// data writer for radio packets with additional fields for building security headers
	class PacketWriter : public EncryptWriter {
	public:
		/**
		 * Construct on radio packet where the length (including 2 byte crc) is in the first byte
		 */
		template <int N>
		PacketWriter(uint8_t (&packet)[N]) : EncryptWriter(packet + 1), begin(packet)
#ifdef EMU
			, end(packet + N)
#endif
		{}

		void security(zb::SecurityControl securityControl, uint32_t securityCounter) {
			this->securityControl = this->current;
			u8(uint8_t(securityControl));
			u32L(this->securityCounter = securityCounter);
		}

		zb::SecurityControl getSecurityControl() {return zb::SecurityControl(*this->securityControl);}
		uint32_t getSecurityCounter() {return this->securityCounter;}

		void clearSecurityLevel() {
			*this->securityControl &= ~uint8_t(zb::SecurityControl::LEVEL_MASK);
		}

		bool hasSecurity() {
			return this->securityControl != nullptr;
		}

		WriterState push() {
			auto securityControl = this->securityControl;
			this->securityControl = nullptr;
			return {this->header, this->message, securityControl, this->securityCounter};
		}

		void pop(WriterState const &s) {
			this->header = s.header;
			this->message = s.message;
			this->securityControl = s.securityControl;
			this->securityCounter = s.securityCounter;
		}

		/**
		 * Set send flags and length of packet
		 */
		void finish(Radio::SendFlags sendFlags) {
#ifdef EMU
			assert(this->current < this->end - 1);
#endif
			*this->current = uint8_t(sendFlags);
			this->begin[0] = this->current - (this->begin + 1) + 2; // + 2 for crc added by hardware
		}

	protected:
		uint8_t *begin;
#ifdef EMU
		uint8_t *end;
#endif
		uint8_t *securityControl = nullptr;
		uint32_t securityCounter;
	};

private:
	// write helpers
	void writeIeeeBroadcastData(PacketWriter &w);
	void writeIeeeData(PacketWriter &w, ZbDevice &device);

	void writeNwkBroadcastCommand(PacketWriter &w, zb::NwkCommand command);
	void writeNwk(PacketWriter &w, zb::NwkFrameControl nwkFrameControl, uint8_t radius, ZbDevice &device);
	void writeNwkCommand(PacketWriter &w, ZbDevice &device, zb::NwkCommand command) {
		zb::NwkFrameControl nwkFrameControl = zb::NwkFrameControl::TYPE_COMMAND
			| zb::NwkFrameControl::VERSION_2
			| zb::NwkFrameControl::DISCOVER_ROUTE_SUPPRESS
			| zb::NwkFrameControl::SECURITY
			| zb::NwkFrameControl::DESTINATION
			| zb::NwkFrameControl::EXTENDED_SOURCE;
		writeNwk(w, nwkFrameControl, 1, device);
		w.e8(command);
	}
	void writeNwkData(PacketWriter &w, ZbDevice &device) {
		zb::NwkFrameControl nwkFrameControl = zb::NwkFrameControl::TYPE_DATA
			| zb::NwkFrameControl::VERSION_2
			| zb::NwkFrameControl::DISCOVER_ROUTE_ENABLE
			| zb::NwkFrameControl::SECURITY;
		writeNwk(w, nwkFrameControl, 30, device);
	}
	void writeNwkDataNoSecurity(PacketWriter &w, ZbDevice &device) {
		zb::NwkFrameControl nwkFrameControl = zb::NwkFrameControl::TYPE_DATA
			| zb::NwkFrameControl::VERSION_2
			| zb::NwkFrameControl::DISCOVER_ROUTE_ENABLE;
		writeNwk(w, nwkFrameControl, 30, device);
	}
	void writeNwkSecurity(PacketWriter &w);

	void writeApsCommand(PacketWriter &w, zb::SecurityControl keyType, zb::ApsCommand command);
	void writeApsAck(PacketWriter &w, uint8_t apsCounter);

	uint8_t writeApsDataZdp(PacketWriter &w, zb::ZdpCommand request);
	void writeApsDataZdp(PacketWriter &w, zb::ZdpCommand response, uint8_t zdpCounter);
	void writeApsAckZdp(PacketWriter &w, uint8_t apsCounter, zb::ZdpCommand command);

	void writeApsDataZcl(PacketWriter &w, uint8_t destinationEndpoint, zb::ZclCluster clusterId, zb::ZclProfile profile,
		uint8_t sourceEndpoint);
	void writeApsAckZcl(PacketWriter &w, uint8_t destinationEndpoint, zb::ZclCluster clusterId,
		zb::ZclProfile profile, uint8_t sourceEndpoint);
	void writeFooter(PacketWriter &w, Radio::SendFlags sendFlags);


	// coroutine that sends link status and many-to-one route request in a regular interval
	Coroutine broadcast();

	// coroutine that sends a beacon on request
	Coroutine sendBeacon();

	// beacon coroutine waits on this barrier until a beacon request arrives
	Barrier<> beaconBarrier;


	// receive coroutine, calls handle... methods
	Coroutine receive();

	void handleGp(uint8_t const *mac, PacketReader &r);
	void handleGpCommission(uint32_t deviceId, PacketReader& r);

	//void handleAps(PacketReader &r, ZbDevice &device, uint8_t const *extendedSource);
	//void handleZdp(PacketReader &r, ZbDevice &device);
	void handleZcl(PacketReader &r, ZbDevice &device, uint8_t destinationEndpoint);

	// coroutine for handling association requests from new devices
	AwaitableCoroutine handleAssociationRequest(uint64_t sourceAddress, Radio::SendFlags sendFlags);

	Coroutine publish();


	// true for commissioning mode, joining of new devices is allowed
	bool commissioning = false;

	AwaitableCoroutine associationCoroutine;
	ZbDevice *tempDevice;


	Event publishEvent;
};
