#pragma once

#include <cstdint>


template <int N, int W>
class BitField {
public:

	void clear() {
		for (uint32_t &d : this->data)
			d = 0;
	}
	
	void set() {
		uint32_t mask = 0xffffffff >> (32 - (32 / W) * W);
		for (uint32_t &d : this->data)
			d = mask;
	}
	
	BitField &set(int index, int value) {
		uint32_t &d = this->data[index / (32 / W)];
		int shift = (index % (32 / W)) * W;
		uint32_t mask = ~(0xffffffff << W);
		d = (d & ~(mask << shift)) | ((value & mask) << shift);
		return *this;
	}
	
	int get(int index) const {
		uint32_t d = this->data[index / (32 / W)];
		int shift = (index % (32 / W)) * W;
		uint32_t mask = ~(0xffffffff << W);
		return (d >> shift) & mask;
	}

	BitField operator ~() const {
		BitField r;
		uint32_t mask = 0xffffffff >> (32 - (32 / W) * W);
		for (int i = 0; i < array::count(this->data); ++i) {
			r.data[i] = this->data[i] ^ mask;
		}
		return r;
	}

	BitField operator &(BitField const &b) const {
		BitField r;
		for (int i = 0; i < array::count(this->data); ++i) {
			r.data[i] = this->data[i] & b.data[i];
		}
		return r;
	}

	int findFirstNonzero() const {
		int pos = 0;
		for (uint32_t d : this->data) {
			if (d != 0) {
				for (int i = (32 / W) / 2; i >= 1; i /= 2) {
					uint32_t mask = ~(0xffffffff << i * W);
					if ((d & mask) == 0) {
						d >>= i * W;
						pos += i;
					}
				}
				return pos;
			}
			pos += 32 / W;
		}
		return -1;
	}
	
protected:

	uint32_t data[(N + (32 / W - 1)) / (32 / W)];
};
