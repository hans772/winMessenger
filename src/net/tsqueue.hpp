#ifndef WMNET_TS_QUEUE_HPP
#define WMNET_TS_QUEUE_HPP

#include <queue>
#include <mutex>
#include <iostream>

namespace WMNet {

	template <class T>
	class ThreadSafeQueue {
	protected:
		std::queue<T> queue;
		std::mutex mtx;
	public:

		void push(T item) {
			std::lock_guard<std::mutex> lock(mtx);
			queue.push(std::move(item));
		}
		
		bool try_pop(T& popped_item) {

			std::lock_guard<std::mutex> lock(mtx);
			if (queue.empty()) {
				return false;
			}

			popped_item = std::move(queue.front());
			queue.pop();
			return true;
		}

		bool empty() {
			std::lock_guard<std::mutex> lock(mtx);
			return queue.empty();
		}
	};

}


#endif // !WMNET_TS_QUEUE
