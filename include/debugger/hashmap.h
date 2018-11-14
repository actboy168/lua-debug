#pragma once

#include <stdint.h>

namespace vscode {
	template <class T>
	struct hashnode {
		uintptr_t key;
		T*        value;
	};

	template <class T, size_t N>
	struct hashchunk {
		hashnode<T>      data[N];
		hashchunk<T, N>* next = 0;
	};

	template <class T, size_t N>
	class hashbucket {
	public:
		typedef hashchunk<T, N> chunk_type;

		hashbucket()
			: front_chunk(new chunk_type)
			, back_chunk(front_chunk)
			, pos(0)
		{ }

        ~hashbucket() {
            clear();
            delete front_chunk;
        }

		bool get(uintptr_t key, T*& t) {
			hashnode<T>* node = get_node(key);
			if (node) {
				t = node->value;
				return true;
			}
			return false;
		}

		T* get(uintptr_t key) {
			hashnode<T>* node = get_node(key);
			if (node) {
				return node->value;
			}
			return 0;
		}

		void put(uintptr_t key, T* value) {
			hashnode<T>* node = get_node(key);
			if (node) {
				node->value = value;
				return;
			}
			if (pos == N) {
				chunk_type* c = new chunk_type;
				back_chunk->next = c;
				back_chunk = c;
				pos = 0;
			}
			back_chunk->data[pos].key = key;
			back_chunk->data[pos].value = value;
			pos++;
		}

		void del(uintptr_t key) {
			hashnode<T>* node = get_node(key);
			if (!node) {
				return;
			}
			node->key = back_chunk[pos].key;
			node->value = back_chunk[pos].value;
			del_back();
		}

        void clear() {
            while (front_chunk->next) {
                chunk_type* t = front_chunk->next;
                front_chunk->next = t->next;
                delete t;
            }
	    back_chunk = front_chunk;
            pos = 0;
        }

	private:
		hashnode<T>* get_node(uintptr_t key) {
			for (chunk_type* c = front_chunk; c != back_chunk; c = c->next) {
				for (size_t i = 0; i < N; ++i) {
					if (c->data[i].key == key) {
						return &(c->data[i]);
					}
				}
			}
			for (size_t i = 0; i < pos; ++i) {
				if (back_chunk->data[i].key == key) {
					return &(back_chunk->data[i]);
				}
			}
			return 0;
		}

		void del_back() {
			if (pos > 1) {
				pos--;
				return;
			}
			if (front_chunk == back_chunk) {
				pos = 0;
				return;
			}
			chunk_type* c = front_chunk;
			for (; c->next != back_chunk; c = c->next)
			{ }
			delete back_chunk;
			back_chunk = c;
			back_chunk->next = 0;
			pos = N;
		}

	private:
		chunk_type* front_chunk;
		chunk_type* back_chunk;
		size_t      pos;
	};

	template <class T, size_t BucketSize = 8191, size_t ChunkSize = 64>
	class hashmap {
	public:
		bool get(uintptr_t key, T*& t) {
			return buckets[key % BucketSize].get(key, t);
		}
		T* get(uintptr_t key) {
			return buckets[key % BucketSize].get(key);
		}
		void put(uintptr_t key, T* value) {
			return buckets[key % BucketSize].put(key, value);
		}
		void del(uintptr_t key) {
			return buckets[key % BucketSize].del(key);
		}
        void clear() {
            for (size_t i = 0; i < BucketSize; ++i)
                buckets[i].clear();
        }
	private:
		hashbucket<T, ChunkSize> buckets[BucketSize];
	};
}
