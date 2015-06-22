/* gdnet_rb.h */

#ifndef GDNET_QUEUE_H
#define GDNET_QUEUE_H

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

	void clear() { read_pos = write_pos = 0; }

	GDNetQueue() { clear(); }
};

#endif
