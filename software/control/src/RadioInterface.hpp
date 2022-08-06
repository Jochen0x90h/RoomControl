#pragma once

#include "Interface.hpp"
#include "SystemTime.hpp"
#include <Configuration.hpp>
#include <State.hpp>
#include <Radio.hpp>
#include <crypt.hpp>
#include <zcl.hpp>
#include <MessageReader.hpp>
#include <MessageWriter.hpp>
#include <Coroutine.hpp>


/**
 * Devices connected by radio using IEEE 802.15.4 standard
 */
class RadioInterface : public Interface {
public:
	// maximum number of devices that can be commissioned (each endpoint counts as one device)
	static constexpr int MAX_DEVICE_COUNT = 128;

	// number of coroutines for receiving and publishing
	static constexpr int RECEIVE_COUNT = 4;
	static constexpr int PUBLISH_COUNT = 4;


	/**
	 * Constructor
	 * @param configuration global configuration
	 * @param stateManager persistent state manager for counters
	 */
	RadioInterface(PersistentStateManager &stateManager);

	~RadioInterface() override;

	/**
	 * Set configuration. Interface starts on first call of this method
	 * @param panId id of pan to use
	 * @param key network key
	 * @param aesKey network key prepared for aes encryption
	 */
	void setConfiguration(uint16_t panId, DataBuffer<16> const &key, AesKey const &aesKey);

	void setCommissioning(bool enabled) override;

	Array<uint8_t const> getDeviceIds() override;
	String getName(uint8_t id) const override;
	void setName(uint8_t id, String name) override;
	Array<MessageType const> getPlugs(uint8_t id) const override;
	void subscribe(uint8_t id, uint8_t plugIndex, Subscriber &subscriber) override;
	PublishInfo getPublishInfo(uint8_t id, uint8_t plugIndex) override;
	void erase(uint8_t id) override;

private:
	static constexpr int MAX_CLUSTER_COUNT = 32;
	static constexpr int MAX_PLUG_COUNT = 64;
	static constexpr int MESSAGE_LENGTH = 80;

	enum class DeviceType : uint8_t {
		PTM215Z = 0x02,
		PTM216Z = 0x07,
		ZBEE = 0xff
	};

	struct DeviceData {
		uint8_t id;

		// device type
		DeviceType deviceType;

		// device name, zero-terminated if shorter than maximum length
		char name[MAX_NAME_LENGTH];

		// number of plugs
		uint8_t plugCount;
	};

	struct GpDeviceData : public DeviceData {
		// 32 bit green power id
		uint32_t deviceId;

		// device key
		AesKey aesKey;

		// plugs
		MessageType plugs[4];

		int size() {return offsetOf(GpDeviceData, plugs[this->plugCount]);}
	};

	class GpDevice /*: public Interface::Device*/ {
	public:
		// takes ownership of the data
		GpDevice(RadioInterface *interface, GpDeviceData *data)
			: /*interface(interface),*/ next(interface->gpDevices), data(data)
		{
			interface->gpDevices = this;
		}

		~GpDevice();

		// back pointer to interface
		//RadioInterface *interface;

		// next device
		GpDevice *next;

		// endpoint data that is stored in flash
		GpDeviceData *data;

		// last state to generate release messages
		uint8_t state = 0;

		// last security counter value of device
		// todo: make persistent
		uint32_t securityCounter = 0;

		// subscriptions
		SubscriberList subscribers;
	};

	// zbee device data that is stored in flash
	struct ZbDeviceData {
		uint8_t id;

		// send flags for this device (wait for data request or not)
		Radio::SendFlags sendFlags;

		// short device address
		uint16_t shortAddress;

		// 64 bit ieee 802.15.4 long device address
		uint64_t longAddress;
	};

	class ZbEndpoint;
	class ZbDevice {
	public:
		ZbDevice(ZbDeviceData const &data)
			: data(data), sendFlags(data.sendFlags)
		{}
		ZbDevice(RadioInterface *interface, ZbDeviceData const &data)
			: next(interface->zbDevices), data(data), sendFlags(data.sendFlags)
		{
			interface->zbDevices = this;
		}
		~ZbDevice();


		// next device
		ZbDevice *next;

		// device data that is stored in flash
		ZbDeviceData data;

		// linked list of endpoints
		ZbEndpoint *endpoints = nullptr;

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

		// barrier for waiting until a route is available
		Barrier<> routeBarrier;
	};

	struct PlugRange {
		uint8_t offset;
		uint8_t count;
	};

	struct ClusterInfo {
		zcl::Cluster cluster;
		PlugRange plugs;
	};

	// zbee endpoint data that is stored in flash
	struct ZbEndpointData : public DeviceData {
		static constexpr int BUFFER_SIZE = 1024;

		// id of device this endpoint belongs to (used while loading from flash)
		uint8_t deviceId;

		// zbee device endpoint with which we communicate
		uint8_t deviceEndpoint;

		// number of server (input) and client (output) clusters
		uint8_t serverClusterCount;
		uint8_t clientClusterCount;

		// data buffer
		uint32_t buffer[BUFFER_SIZE / 4];

		int size() {
			int s1 = (this->plugCount * sizeof(MessageType) + 3) / 4;
			int s2 = ((this->serverClusterCount + this->clientClusterCount) * sizeof(ClusterInfo) + 3) / 4;
			return offsetOf(ZbEndpointData, buffer[s1 + s2]);
		}
	};

	struct ZbEndpointDataBuilder {
		void addPlug(MessageType plug) {this->plugs[plugCount++] = plug;}

		void beginServerCluster(zcl::Cluster cluster) {
			auto &clusterInfo = this->clusterInfos[this->serverClusterIndex];
			clusterInfo.cluster = cluster;
			clusterInfo.plugs = {this->plugCount, 0};
		}
		void endServerCluster() {
			auto &clusterInfo = this->clusterInfos[this->serverClusterIndex];
			if ((clusterInfo.plugs.count = this->plugCount - clusterInfo.plugs.offset) > 0)
				this->serverClusterIndex++;
		}
		void beginClientCluster(zcl::Cluster cluster) {
			auto &clusterInfo = this->clusterInfos[this->clientClusterIndex];
			clusterInfo.cluster = cluster;
			clusterInfo.plugs = {this->plugCount, 0};
		}
		void endClientCluster() {
			auto &clusterInfo = this->clusterInfos[this->clientClusterIndex];
			if ((clusterInfo.plugs.count = this->plugCount - clusterInfo.plugs.offset) > 0)
				this->clientClusterIndex--;
		}

		int size() const;
		void build(ZbEndpointData *data);

		uint8_t id;
		uint8_t plugCount = 0;
		uint8_t serverClusterIndex = 0;
		uint8_t clientClusterIndex = MAX_CLUSTER_COUNT - 1;
		MessageType plugs[MAX_PLUG_COUNT];
		ClusterInfo clusterInfos[MAX_CLUSTER_COUNT];
	};

	class ZbEndpoint {
	public:
		// takes ownership of the data
		ZbEndpoint(ZbDevice *device, ZbEndpointData *data)
			: next(device->endpoints), data(data)
		{
			device->endpoints = this;
		}

		~ZbEndpoint();

		Array<MessageType const> getPlugs() const;
		//Array<ClusterInfo const *> getServerClusters() const;
		PlugRange findServerCluster(zcl::Cluster cluster) const;
		ClusterInfo getClientCluster(int plugIndex) const;


		// next endpoint in list
		ZbEndpoint *next;

		// endpoint data that is stored in flash
		ZbEndpointData *data;

		// list of subscribers
		SubscriberList subscribers;

		//
		SystemTime time;
		uint8_t index;
		uint16_t color;
	};

	GpDevice *getGpDevice(uint8_t id) const;
	ZbEndpoint *getZbEndpoint(uint8_t id) const;
	uint8_t allocateId(int deviceCount);
	uint8_t allocateZbDeviceId();
	ZbDevice *getOrLoadZbDevice(uint8_t id);
	void eraseZbDevice(uint8_t id);

	int deviceCount = 0;
	uint8_t deviceIds[MAX_DEVICE_COUNT];
	GpDevice *gpDevices = nullptr;
	ZbDevice *zbDevices = nullptr;



	// start the interface
	Coroutine start();

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
	DataBuffer<16> const *key = nullptr;
	AesKey const *aesKey;

	// find zbee device by short address
	ZbDevice *findZbDevice(uint16_t address);

	void allocateShortAddress();

	// next device id and short address "on stock" to be fast when a device sends an association request
	uint16_t nextShortAddress;

public:

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
		 * Construct on radio packet where the length is in the first byte
		 */
		template <int N>
		PacketWriter(uint8_t (&packet)[N]) : EncryptWriter(packet + 1)
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
		 * Set send flags behind packet and length of packet to first byte (including 2 byte for crc)
		 */
		void finish(Radio::SendFlags sendFlags) {
#ifdef EMU
			assert(this->current < this->end - 1);
#endif
			*this->current = uint8_t(sendFlags);
			this->begin[-1] = (this->current - this->begin) + 2; // + 2 for crc added by hardware
		}

	protected:
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

	void writeApsDataZcl(PacketWriter &w, uint8_t dstEndpoint, zcl::Cluster clusterId, zcl::Profile profile,
		uint8_t srcEndpoint);
	void writeApsAckZcl(PacketWriter &w, uint8_t dstEndpoint, zcl::Cluster clusterId,
		zcl::Profile profile, uint8_t srcEndpoint);
	static bool writeZclCommand(PacketWriter &w, uint8_t zclCounter, int plugIndex,
		zcl::Cluster cluster, Message &message, ZbEndpoint *endpoint);
	void writeFooter(PacketWriter &w, Radio::SendFlags sendFlags);

	[[nodiscard]] AwaitableCoroutine readAttribute(uint8_t (&packet)[MESSAGE_LENGTH], ZbDevice &device,
		uint8_t dstEndpoint, zcl::Cluster clusterId, zcl::Profile profile, uint8_t srcEndpoint, uint16_t attribute);


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
	[[nodiscard]] AwaitableCoroutine handleZbCommission(uint64_t deviceAddress, Radio::SendFlags sendFlags);

	Coroutine publish();


	// true for commissioning mode, joining of new devices is allowed
	bool commissioning = false;

	AwaitableCoroutine commissionCoroutine;
	ZbDevice *tempDevice;


	struct Response {
		// response data
		int& length;
		uint8_t *response;

		// our endpoint the response is for
		uint8_t dstEndpoint;

		// expected zdp or zcl counter of the response
		uint8_t counter;

		// command we are waiting for
		uint16_t command;
	};

	// a coroutine (e.g. handleZbCommission()) waits on this barrier until a response arrives
	Waitlist<Response> responseBarrier;

	PublishInfo::Barrier publishBarrier;
};
