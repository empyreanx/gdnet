/* gdnet_rb.h */

#ifndef GDNET_QUEUE_H
#define GDNET_QUEUE_H

#include "core/os/memory.h"

template<class T, int SIZE = 1024>
class GDNetQueue {
	T* items[SIZE];

	int read_pos;
	int write_pos;

public:

	bool is_empty() {
		return (read_pos == write_pos);
	}

	bool is_full() {
		return ((write_pos + 1) % SIZE == read_pos);
	}

	int size() {
		if (write_pos > read_pos)
			return write_pos - read_pos;
		else if (write_pos < read_pos)
			return (SIZE - read_pos) + write_pos;
		else
			return 0;
	}

	void push(T* item) {
		ERR_FAIL_COND(is_full());

		items[write_pos] = item;
		write_pos = (write_pos + 1) % SIZE;
	}

	T* pop() {
		ERR_FAIL_COND_V(is_empty(), NULL);

		T* item = items[read_pos];
		read_pos = (read_pos + 1) % SIZE;
		return item;
	}

	void clear() {
		while (!is_empty()) {
			memdelete(pop());
		}
	}

	GDNetQueue() { read_pos = write_pos = 0; }
};

#endif
