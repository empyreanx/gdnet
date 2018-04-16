/* gdnet_rb.h */

#ifndef GDNET_QUEUE_H
#define GDNET_QUEUE_H

#include "os/memory.h"
#include "os/mutex.h"

template<class T, int SIZE = 1024>
class GDNetQueue {
	T* items[SIZE + 1];

	int read_pos;
	int write_pos;

	Mutex* mutex;

public:

	bool is_empty() {
		bool empty;

		mutex->lock();
		empty = (read_pos == write_pos);
		mutex->unlock();

		return empty;
	}

	bool is_full() {
		bool full;

		mutex->lock();
		full = ((write_pos + 1) % (SIZE + 1) == read_pos);
		mutex->unlock();

		return full;
	}

	int size() {
		int count;

		mutex->lock();

		if (write_pos > read_pos)
			count = write_pos - read_pos;
		else if (write_pos < read_pos)
			count = (SIZE - read_pos + 1) + write_pos;
		else
			count = 0;

		mutex->unlock();

		return count;
	}

	void push(T* item) {
		ERR_FAIL_COND(is_full());

		mutex->lock();

		items[write_pos] = item;
		write_pos = (write_pos + 1) % (SIZE + 1);

		mutex->unlock();
	}

	T* pop() {
		ERR_FAIL_COND_V(is_empty(), NULL);

		T* item;

		mutex->lock();

		item = items[read_pos];
		read_pos = (read_pos + 1) % (SIZE + 1);

		mutex->unlock();

		return item;
	}

	void clear() {
		mutex->lock();

		while (!is_empty()) {
			memdelete(pop());
		}

		mutex->unlock();
	}

	GDNetQueue() : mutex(NULL) {
		read_pos = write_pos = 0;
		mutex = Mutex::create();
	}

	~GDNetQueue() {
		memdelete(mutex);
	}
};

#endif
