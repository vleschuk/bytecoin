// Copyright (c) 2012-2018, The CryptoNote developers, The Bytecoin developers.
// Licensed under the GNU Lesser General Public License. See LICENSE for details.

#include "MemoryStreams.hpp"
#include <algorithm>
#include <cassert>
#include <cstring>
#include <stdexcept>
#include <thread>
#include "Invariant.hpp"

using namespace common;

MemoryInputStream::MemoryInputStream(const void *buffer, size_t buffer_size)
    : buffer(static_cast<const char *>(buffer)), buffer_size(buffer_size), in_position(0) {}

size_t MemoryInputStream::read_some(void *data, size_t size) {
	//BC_CREATE_LOCK(lock, memory_mutex, "memory");
	invariant(in_position <= buffer_size, "jump over the end of buffer");
	size = std::min(size, buffer_size - in_position);

	if (size > 0)
		memcpy(data, buffer + in_position, size);
	in_position += size;
	return size;
}

size_t StringInputStream::read_some(void *data, size_t size) {
	BC_CREATE_LOCK(lock, in_mutex, "in");
	invariant(in_position <= in->size(), "jump over the end of buffer");
	size = std::min(size, in->size() - in_position);

	memcpy(data, in->data() + in_position, size);
	in_position += size;
	return size;
}

size_t StringInputStream::copy_to(IOutputStream &out, size_t max_count) {
	size_t total_count = 0;
	while (true) {
		BC_CREATE_LOCK(lock, in_mutex, "in");
		size_t rc = std::min(in->size() - in_position, max_count);
		if (rc == 0)
			break;
		size_t count = out.write_some(in->data() + in_position, rc);
		in_position += count;
		max_count -= count;
		total_count += count;
		if (count == 0)
			break;
	}
	return total_count;
}

size_t VectorInputStream::read_some(void *data, size_t size) {
	BC_CREATE_LOCK(lock, in_mutex, "in");
	invariant(in_position <= in->size(), "jump over the end of buffer");
	size = std::min(size, in->size() - in_position);

	memcpy(data, in->data() + in_position, size);
	in_position += size;
	return size;
}

size_t VectorInputStream::copy_to(IOutputStream &out, size_t max_count) {
	size_t total_count = 0;
	while (true) {
		BC_CREATE_LOCK(lock, in_mutex, "in");
    auto size = in->size();
		size_t rc = std::min(size - in_position, max_count);
//    fprintf(stderr, "%llu: in->size(): %zu, in_position: %zu, max_count: %zu, rc: %zu\n",
//        std::this_thread::get_id(), size, in_position, max_count, rc);
		if (rc == 0)
			break;
		size_t count = out.write_some(in->data() + in_position, rc);
		in_position += count;
		max_count -= count;
		total_count += count;
//    fprintf(stderr, "%llu: count: %zu, in_position(addr: %p): %zu, max_count: %zu, total_count: %zu\n",
//        std::this_thread::get_id(), count, (void *)&in_position, in_position, max_count, total_count);
		if (count == 0)
			break;
	}
	return total_count;
}

size_t StringOutputStream::write_some(const void *data, size_t size) {
	BC_CREATE_LOCK(lock, out_mutex, "out");
	out->append(static_cast<const char *>(data), size);
	return size;
}

size_t VectorOutputStream::write_some(const void *data, size_t size) {
	BC_CREATE_LOCK(lock, out_mutex, "out");
	append(*out, static_cast<const uint8_t *>(data), static_cast<const uint8_t *>(data) + size);
	return size;
}

size_t CircularBuffer::read_some(void *data, size_t size) {
	BC_CREATE_LOCK(lock, read_mutex, "read_impl");
	size_t rc = std::min(size, read_count());
	memcpy(data, read_ptr(), rc);
	did_read(rc);
	return rc;
}

size_t CircularBuffer::write_some(const void *data, size_t size) {
	BC_CREATE_LOCK(lock, write_mutex, "write_impl");
	size_t rc = std::min(size, write_count());
  void *dest = write_ptr();
//  fprintf(stderr, "dest: %p, data: %p, size: %zu\n", dest, data, size);
	memcpy(dest, data, rc);
	did_write(rc);
	return rc;
}

void CircularBuffer::did_write(size_t count) {
	BC_CREATE_LOCK(lock, write_mutex, "write_impl");
	write_pos += count;
	invariant(write_pos <= read_pos + impl.size(), "Writing past end of Buffer");
}

void CircularBuffer::did_read(size_t count) {
	BC_CREATE_LOCK(rlock, read_mutex, "read_impl");
	BC_CREATE_LOCK(wlock, write_mutex, "write_impl");
	read_pos += count;
//	fprintf(stderr, "DIDREAD: thread %llu, buffer: %p, read_pos: %zu, write_pos: %zu, count: %zu\n",
//			std::this_thread::get_id(), (void *)this, read_pos, write_pos, count);
	invariant(read_pos <= write_pos, "Reading past end of Buffer");
	if (read_pos >= impl.size()) {
		read_pos -= impl.size();
		write_pos -= impl.size();
	}
}

void CircularBuffer::copy_from(IInputStream &in) {
	while (true) {
		BC_CREATE_LOCK(lock, write_mutex, "write_impl");
		size_t wc = write_count();
		if (wc == 0)
			break;
		size_t count = in.read_some(write_ptr(), wc);
		did_write(count);
		if (count == 0)
			break;
	}
}

size_t CircularBuffer::copy_to(IOutputStream &out, size_t max_count) {
	size_t total_count = 0;
	while (true) {
		BC_CREATE_LOCK(lock, read_mutex, "read_impl");
		size_t rc = std::min(read_count(), max_count);
		if (rc == 0)
			break;
		size_t count = out.write_some(read_ptr(), rc);
		did_read(count);
		max_count -= count;
		total_count += count;
		if (count == 0)
			break;
	}
	return total_count;
}
