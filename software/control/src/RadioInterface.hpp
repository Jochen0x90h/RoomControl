#pragma once

#include "Interface.hpp"
#include <Configuration.hpp>
#include <State.hpp>
#include <Storage.hpp>
#include <radio.hpp>
#include <crypt.hpp>
#include <DataReader.hpp>
#include <DataWriter.hpp>
#include <Coroutine.hpp>
#include <appConfig.hpp>
#include <vector>
#include <list>


/**
 * Devices connected by radio using IEEE 802.15.4 standard
 */
class RadioInterface : public Interface {
public:
	static constexpr int MAX_DEVICE_COUNT = 64;

	/**
	 * Constructor
	 * @param configuration global configuration
	 * @param stateManager persistent state manager for counters
	 */
	RadioInterface(Configuration &configuration, PersistentStateManager &stateManager);

	~RadioInterface() override;

	void setCommissioning(bool enabled) override;

	int getDeviceCount() override;

	DeviceId getDeviceId(int index) override;

	Array<EndpointType const> getEndpoints(DeviceId deviceId) override;

	void addPublisher(DeviceId deviceId, uint8_t endpointIndex, Publisher &publisher) override;

	void addSubscriber(DeviceId deviceId, uint8_t endpointIndex, Subscriber &subscriber) override;

private:

	// initialize the interface
	Coroutine init(PersistentStateManager &stateManager);

	// reference to global configuration (array contains only one element)
	Configuration &configuration;

	
	// self-powered devices
	struct GpDevice;
	
	struct GpDeviceFlash {
		static constexpr int MAX_ENDPOINT_COUNT = 32;
		
		// device id, either 64 bit ieee 802.15.4 long device address or 32 bit green power id
		DeviceId deviceId;
		
		// device type from commissioning message
		uint8_t deviceType;
		
		// device key
		AesKey aesKey;
			
		// number of endpoints of the device
		uint8_t endpointCount;

		// endpoint types
		EndpointType endpoints[MAX_ENDPOINT_COUNT];

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

	class GpDevice : public Storage::Element<GpDeviceFlash> {
	public:
	
		GpDevice(GpDeviceFlash const &flash) : Storage::Element<GpDeviceFlash>(flash) {}

	
		// last security counter value of device
		// todo: make persistent
		uint32_t securityCounter = 0;
		
		uint8_t state = 0;

		// subscriptions
		SubscriberList subscribers;
	};


	// zbee devices
	struct ZbDevice;
	
	struct ZbDeviceFlash {
		static constexpr int MAX_ENDPOINT_COUNT = 32;
		
		// device id, 64 bit ieee 802.15.4 long device address
		DeviceId deviceId;

		// short device address
		uint16_t shortAddress;

		// send flags for this device (wait for data request or not)
		radio::SendFlags sendFlags;

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

	class ZbDevice : public Storage::Element<ZbDeviceFlash> {
	public:
	
		ZbDevice(ZbDeviceFlash const &flash)
			: Storage::Element<ZbDeviceFlash>(flash)
			, sendFlags(flash.sendFlags)
			, defaultResponseFlags{}
		{}

		// send flags for next hop in route (wait for data request or not)
		radio::SendFlags sendFlags;

	
		// last security counter value of device
		// todo: make persistent
		uint32_t securityCounter = 0;
		
		//std::vector<uint16_t> route;
		
		// first hop on route towards the device (0xffff means that the route is unknown)
		uint16_t firstHop = 0xffff;


		static constexpr int RESPONSE_LENGTH = 64;

				
		struct ZdpResponse {
			zb::ZdpCommand command;
			uint8_t zdpCounter;
			uint16_t& length;
			uint8_t *response;
		};
		
		// the query coroutine waits on this barrier until a zdp response arrives
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
		
		// pending default responses for each endpoint
		struct DefaultResponse {
			uint8_t zclCounter;
			uint8_t command;
		};
		uint32_t defaultResponseFlags[(ZbDeviceFlash::MAX_ENDPOINT_COUNT + 31) / 32];
		DefaultResponse defaultResponses[ZbDeviceFlash::MAX_ENDPOINT_COUNT];

		// pending rejoin
		bool rejoinPending = false;
		
		// routing
		Barrier<> routeBarrier;
	};
	
	
	
	// find zbee device by short address
	ZbDevice *findZbDevice(uint16_t address);

	void allocateNextAddress();

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

		/**
		 * Set send flags and length of packet
		 */
		void finish(radio::SendFlags sendFlags) {
#ifdef EMU
			assert(this->current < this->end - 1);
#endif
			*this->current = uint8_t(sendFlags);
			this->begin[0] = this->current - (this->begin + 1) + 2; // for crc
		}

		uint8_t *begin;
#ifdef EMU
		uint8_t *end;
#endif
		uint8_t *securityControlPtr;
	};

private:
	// write helpers
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
		w.enum8(command);
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
	void writeApsCommand(PacketWriter &w, zb::ApsFrameControl apsFrameControl);
	void writeApsData(PacketWriter &w, uint8_t destinationEndpoint, zb::ZclCluster clusterId, zb::ZclProfile profile,
		uint8_t sourceEndpoint);
	uint8_t writeZdpData(PacketWriter &w, zb::ZdpCommand command);
	void writeFooter(PacketWriter &w, radio::SendFlags sendFlags);

	
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

	void handleNwk(PacketReader &r, ZbDevice &device);
	void handleNwkCommand(PacketReader &r, ZbDevice &device);
	void handleAps(PacketReader &r, ZbDevice &device, uint8_t const *extendedSource);
	void handleZdp(PacketReader &r, ZbDevice &device);
	void handleZcl(PacketReader &r, ZbDevice &device, uint8_t destinationEndpoint);

	// coroutine for handling association requests from new devices
	AwaitableCoroutine handleAssociationRequest(uint64_t sourceAddress, radio::SendFlags sendFlags);


	Coroutine publish();


	// true for commissioning mode, joining of new devices is allowed
	bool commissioning = false;

	AwaitableCoroutine associationCoroutine;
	ZbDevice *tempDevice;

	
	Event publishEvent;


	// counters
	uint8_t macCounter = 0;
	uint8_t nwkCounter = 0;
	uint8_t apsCounter = 0;
	uint8_t zdpCounter = 0;
	uint8_t zclCounter = 0;
	PersistentState<uint32_t> securityCounter;
	uint8_t routeCounter = 0;
};
