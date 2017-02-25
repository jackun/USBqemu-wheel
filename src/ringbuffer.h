#ifndef RINGBUFFER_H
#define RINGBUFFER_H

#include <algorithm> // for std::min
#include <cstdint>

class RingBuffer
{
	RingBuffer(RingBuffer&) = delete;
public:
	RingBuffer();
	RingBuffer(size_t capacity);
	~RingBuffer();

	//size_t write(const char *data, size_t bytes);
	//size_t read(char *data, size_t bytes);

	// Overwrites old data if nbytes > size()
	void write(uint8_t *src, size_t nbytes);
	size_t read(uint8_t *dst, size_t nbytes);

	// just move pointers around
	void write(size_t bytes);
	void read(size_t bytes);

	void reserve(size_t size);
	// if you care about old data, check how much can be written
	// may need to call available/write twice in case write pointer wraps
	size_t peek_write(bool overwrite = false) const;
	size_t peek_read() const;
	// amount of valid data, may need to read twice
	size_t size() const;
	size_t capacity() const { return m_capacity; }
	char* front(){ return m_data + m_begin; }
	char* back(){ return m_data + m_end; }

private:
	bool overrun;
	size_t m_begin, m_end, m_capacity;
	char *m_data;
};

#endif