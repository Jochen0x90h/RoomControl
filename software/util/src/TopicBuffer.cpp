#include "TopicBuffer.hpp"


TopicBuffer &TopicBuffer::operator /=(String str) {
	if (!str.isEmpty()) {
		char *data = this->data + MAX_PREFIX_LENGTH;
		if (this->index < MAX_TOPIC_LENGTH)
			data[this->index++] = '/';
		char *dst = data + this->index;
		int l = min(str.length, MAX_TOPIC_LENGTH - this->index);
		array::copy(dst, dst + l, str.begin());
		#ifdef DEBUG
		dst[l] = 0;
		#endif
		this->index += l;
	}
	return *this;
}

String TopicBuffer::state() {
	char *data = this->data + MAX_PREFIX_LENGTH - 4;
	array::copy(data, data + 4, "stat");
	return {4 + this->index, data};
}

String TopicBuffer::command() {
	char *data = this->data + MAX_PREFIX_LENGTH - 4;
	array::copy(data, data + 4, "cmnd");
	return {4 + this->index, data};
}

String TopicBuffer::enumeration() {
	char *data = this->data + MAX_PREFIX_LENGTH - 4;
	array::copy(data, data + 4, "enum");
	return {4 + this->index, data};
}

void TopicBuffer::removeLast() {
	char *data = this->data + MAX_PREFIX_LENGTH;
	do
		--this->index;
	while (this->index > 0 && data[this->index] != '/');
	#ifdef DEBUG
	data[this->index] = 0;
	#endif
}

int TopicBuffer::getElementCount() {
	char *data = this->data + MAX_PREFIX_LENGTH;
	int count = 0;
	for (int i = 0; i < this->index; ++i) {
		if (data[i] == '/')
			++count;
	}
	return count;
}
