#ifndef WMNET_MESSAGE_HPP
#define WMNET_MESSAGE_HPP

#include <cstdint>
#include <vector>

#include "net/internals.hpp"
namespace WMNet {

	template <typename T>
	struct Serializer;

	template <class T>
	struct Message {
		T data;
		msg_id_t id;

		std::vector<byte> serialize() const {
			return Serializer<T>::serialize(*this);
		}
	};
}

#endif