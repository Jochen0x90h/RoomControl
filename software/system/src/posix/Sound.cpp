#include "../Sound.hpp"
#include "Loop.hpp"
#include "assert.hpp"
#include "util.hpp"
#define MA_NO_DECODING
#define MA_NO_ENCODING
#define MINIAUDIO_IMPLEMENTATION
#include <miniaudio.h>
#include <opus.h>
#include <ogg/ogg.h>
#include <boost/filesystem.hpp>
#include <string>
#include <mutex>


// sample files: https://espressif-docs.readthedocs-hosted.com/projects/esp-adf/en/latest/design-guide/audio-samples.html
// mp3 to opus: ffmpeg -i input.mp3 -c:a libopus -b:a 32k -ac 1 output.opus
namespace Sound {

namespace fs = boost::filesystem;

#define CHANNEL_COUNT 1
#define SAMPLE_RATE 48000

// opus
#define FRAME_SIZE 960
#define MAX_FRAME_SIZE 6*960
#define MAX_PACKET_SIZE (3*1276)
#define MAX_PACKET_DURATION 120 // ms

// audio device
#define DEVICE_FORMAT ma_format_s16//ma_format_f32


struct Context {
	std::mutex mutex;

	// audio file
	enum State {
		IDLE,
		PLAY,
		END,
		END2
	};
	State state = IDLE;
	FILE *file;
	ogg_sync_state sync;
	ogg_page page;
	ogg_stream_state stream;
	int bufferSize = 0;
	opus_int16 buffer[(MAX_PACKET_DURATION * SAMPLE_RATE) / 1000 * CHANNEL_COUNT * 2];

	// opus
	OpusDecoder *decoder;

	// audio device
	ma_device device;
};
std::vector<Context*> contexts;
std::vector<Type> types;


// timeout to check if audio devices need to be stopped
class Timeout : public Loop::Timeout {
public:
	void activate() override {
		// next activation in 1s
		this->time += 1s;

		for (auto context : contexts) {
			std::lock_guard<std::mutex> guard(context->mutex);
			if (context->state >= Context::END) {
				if (context->state == Context::END) {
					context->state = Context::END2;
				} else {
					context->state = Context::IDLE;
					ma_device_stop(&context->device);
				}
			}
		}
	}
};
Timeout timeout;


void dataCallback(ma_device* device, void* output, const void* input, ma_uint32 frameCount) {
	assert(device->playback.channels == CHANNEL_COUNT);
	assert(device->pUserData != nullptr);
	Context *context = reinterpret_cast<Context *>(device->pUserData);
	std::lock_guard<std::mutex> guard(context->mutex);

	// opus
	auto out = reinterpret_cast<int16_t *>(output);

	if (context->state >= Context::END) {
		array::fill(frameCount * CHANNEL_COUNT, out, 0);
		return;
	}

	while (true) {
		int copyCount = min(frameCount, context->bufferSize);
		array::copy(copyCount * CHANNEL_COUNT, out, context->buffer);
		out += copyCount * CHANNEL_COUNT;
		frameCount -= copyCount;

		if (frameCount == 0) {
			// move tail of buffer
			int copyCount2 = context->bufferSize - copyCount;
			array::copy(copyCount2 * CHANNEL_COUNT, context->buffer, context->buffer + copyCount * CHANNEL_COUNT);
			context->bufferSize = copyCount2;
			break;
		} else {
			// read a packet
			ogg_packet packet;
			while (ogg_stream_packetout(&context->stream, &packet) != 1) {
				// get a page
				while (ogg_sync_pageout(&context->sync, &context->page) != 1) {
					// read new data
					auto data = ogg_sync_buffer(&context->sync, 8192);
					int readCount = fread(data, 1, 8192, context->file);
					if (readCount == 0) {
						context->bufferSize = 0;
						array::fill(frameCount * CHANNEL_COUNT, out, 0);
						context->state = Context::END;
						return;
					}
					ogg_sync_wrote(&context->sync, readCount);
				}

				// check for begin of stream
				if (ogg_page_bos(&context->page)) {
					auto serialno = ogg_page_serialno(&context->page);
					ogg_stream_init(&context->stream, serialno);
				}

				// check for end of stream
				//if (ogg_page_eos(&Audio::page)) {
				//}

				// add page to stream
				ogg_stream_pagein(&context->stream, &context->page);
			}

			// decode new samples
			auto sampleCount = opus_decode(context->decoder, packet.packet, packet.bytes,
				context->buffer, MAX_FRAME_SIZE, 0);
			context->bufferSize = max(sampleCount, 0);
		}
	}
}

int inited = 0;

struct TypeAndSuffix {
	Type type;
	char const *suffix;
};
TypeAndSuffix suffixes[] = {
	{Type::NONE, ""},
	{Type::EVENT, ".event"},
	{Type::ACTIVATION, ".activation"},
	{Type::DEACTIVATION, ".deactivation"},
	{Type::INFORMATION, ".information"},
	{Type::WARNING, ".warning"},
	{Type::DOORBELL, ".doorbell"},
	{Type::CALL, ".call"},
	{Type::ALARM, ".alarm"},
};

void init() {
	Sound::inited = 1;

	for (int count = 0; true; ++count) {
		// open file
		FILE *file;
		for (auto suffix : suffixes) {
			auto name = std::to_string(count) + suffix.suffix + ".opus";
			file = fopen(name.c_str(), "r");
			if (file != nullptr) {
				Sound::types.push_back(suffix.type);
				break;
			}
		}
		if (file == nullptr)
			break;

		// create new context
		auto context = new Context();
		Sound::contexts.push_back(context);
		context->file = file;

		// opus
		int err;
		context->decoder = opus_decoder_create(SAMPLE_RATE, CHANNEL_COUNT, &err);
		if (err < 0)
			return;

		// init audio device
		auto deviceConfig = ma_device_config_init(ma_device_type_playback);
		deviceConfig.playback.format = DEVICE_FORMAT;
		deviceConfig.playback.channels = CHANNEL_COUNT;
		deviceConfig.sampleRate = SAMPLE_RATE;
		deviceConfig.dataCallback = dataCallback;
		deviceConfig.pUserData = context;

		if (ma_device_init(nullptr, &deviceConfig, &context->device) != MA_SUCCESS)
			return;
	}

	Sound::inited = 2;

	Sound::timeout.time = Loop::now() + 1s;
	Loop::timeouts.add(Sound::timeout);
}

Array<Type> getTypes() {
	return {int(Sound::types.size()), Sound::types.data()};
}

void play(int index) {
	assert(Sound::inited != 0);
	assert(index >= 0 && index < Sound::contexts.size());

	if (Sound::inited == 2) {
		auto context = Sound::contexts[index];
		{
			std::lock_guard<std::mutex> guard(context->mutex);

			// reset file
			fseek(context->file, 0, SEEK_SET);
			ogg_sync_init(&context->sync);
			ogg_stream_init(&context->stream, 0);
			context->bufferSize = 0;
			context->state = Context::PLAY;
		}

		// start
		ma_device_start(&context->device);
	}
}

void stop(int index) {
	assert(Sound::inited != 0);
	assert(index >= 0 && index < Sound::contexts.size());

	if (Sound::inited == 2) {
		auto context = Sound::contexts[index];
		std::lock_guard<std::mutex> guard(context->mutex);
		context->state = Context::END;
	}
}

bool isPlaying(int index) {
	assert(Sound::inited != 0);
	assert(index >= 0 && index < Sound::contexts.size());

	auto context = Sound::contexts[index];
	return context->state == Context::PLAY;
}

} // Sound
