#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>
#include <cstring>
#include <ostream>
#include <spcppl/assert.hpp>
#include <spcppl/ranges/fors.hpp>
#include <spcppl/numbers/bits.hpp>

template <typename N>
class BitSet {
public:
	BitSet(): v(getUnderlyingSize(size())) {
	}

	template <typename Collection>
	static BitSet fromIndices(const Collection& collection) {
		BitSet result;
		for (std::size_t index: collection) {
			result.set(index);
		}
		return result;
	}

	static BitSet fromIndices(std::initializer_list<std::size_t> collection) {
		return fromIndices<std::initializer_list<std::size_t>>(collection);
	}

	bool get(std::size_t index) const {
		SPCPPL_ASSERT(index < size());
		return static_cast<bool>((v[index >> 5] >> (index & 31)) & 1);
	}

	void set(std::size_t index, bool value) {
		if (value) {
			set(index);
		} else {
			clear(index);
		}
	}

	void set(std::size_t index) {
		SPCPPL_ASSERT(index < size());
		v[index >> 5] |= 1U << (index & 31);
	}

	void clear(std::size_t index) {
		SPCPPL_ASSERT(index < size());
		v[index >> 5] &= ~(1U << (index & 31));
	}

	void flip(std::size_t index) {
		SPCPPL_ASSERT(index < size());
		v[index >> 5] ^= 1U << (index & 31);
	}

	std::size_t firstBitFrom(std::size_t index) const {
		if (index == size()) {
			return index;
		}
		uint32_t maskFromIndex = ~((static_cast<uint32_t>(1) << (index & 31)) - 1);
		auto firstBlock = v[index >> 5];
		uint32_t value = maskFromIndex & firstBlock;
		if (value != 0) {
			return (index & ~static_cast<uint32_t>(31)) + countTrailingZeros(value);
		}
		auto it = std::find_if(v.begin() + (index >> 5) + 1, v.end(), [](auto x) {
			return x != 0;
		});
		if (it == v.end()) {
			return size();
		}
		return (it - v.begin()) * 32 + countTrailingZeros(*it);
	}

	std::size_t firstBitAfter(std::size_t index) const {
		return firstBitFrom(index + 1);
	}

	std::size_t count() const {
		std::size_t result = 0;
		for (auto block: v) {
			result += __builtin_popcount(block);
		}
		return result;
	}

	BitSet& operator|=(const BitSet& rhs) {
		for (auto i: range(v.size())) {
			v[i] |= rhs.v[i];
		}
		return *this;
	}

	BitSet& operator&=(const BitSet& rhs) {
		for (auto i: range(v.size())) {
			v[i] &= rhs.v[i];
		}
		return *this;
	}

	BitSet& operator^=(const BitSet& rhs) {
		for (auto i: range(v.size())) {
			v[i] ^= rhs.v[i];
		}
		return *this;
	}

	BitSet operator~() const& {
		auto copy = *this;
		copy.invert();
		return copy;
	}

	BitSet operator~() && {
		invert();
		return *this;
	}

	bool empty() const {
		for (uint32_t number: v) {
			if (number != 0) {
				return false;
			}
		}
		return true;
	}

	std::size_t leastBit() const {
		for (std::size_t i: range(v.size())) {
			if (v[i] != 0) {
				return i * 32 + leastSignificantBit(v[i]);
			}
		}
		return size();
	}

	BitSet& operator<<=(std::size_t rhs) {
		SPCPPL_ASSERT(rhs <= size());
		std::size_t bigShifts = rhs >> 5;
		std::memmove(&v[bigShifts], &v[0], (v.size() - bigShifts) * sizeof(int32_t));
		std::memset(&v[0], 0, bigShifts * sizeof(int32_t));
		rhs &= 31;
		uint32_t add = 0;
		if (rhs != 0) {
			for (uint32_t& element: v) {
				uint32_t next = (element >> (32 - rhs));
				element <<= rhs;
				element ^= add;
				add = next;
			}
		}
		std::size_t lastBits = size() & 31;
		if (lastBits > 0) {
			v.back() &= (1 << lastBits) - 1;
		}
		return *this;
	}

	BitSet& operator>>=(std::size_t rhs) {
		SPCPPL_ASSERT(rhs <= size());
		std::size_t bigShifts = rhs >> 5;
		std::memmove(&v[0], &v[bigShifts], (v.size() - bigShifts) * sizeof(int32_t));
		std::memset(&v[0] + v.size() - bigShifts, 0, bigShifts * sizeof(int32_t));
		rhs &= 31;
		int32_t add = 0;
		if (rhs != 0) {
			for (std::size_t i: downrange(v.size())) {
				uint32_t next = (v[i] << (32 - rhs));
				v[i] >>= rhs;
				v[i] ^= add;
				add = next;
			}
		}
		return *this;
	}

	std::size_t size() const {
		return N::value;
	}

private:
	BitSet& invert() {
		for (auto& block: v) {
			block = ~block;
		}
		std::size_t lastBits = size() & 31;
		if (size() != 0) {
			v.back() &= (1 << lastBits) - 1;
		}
		return *this;
	}

	static std::size_t countTrailingZeros(uint32_t value) {
		return __builtin_ctz(value);
	}

	static std::size_t getUnderlyingSize(std::size_t size) {
		return (size + 31) >> 5;
	}

	std::vector<uint32_t> v;

	template <typename M>
	friend bool operator==(const BitSet<M>& a, const BitSet<M>& b);

	template <typename M>
	friend std::ostream& operator<<(std::ostream& stream, const BitSet<M>& b);
};

template <typename N>
BitSet<N> operator|(const BitSet<N>& a, const BitSet<N>& b) {
	BitSet<N> copy = a;
	return copy |= b;
}

template <typename N>
BitSet<N> operator&(const BitSet<N>& a, const BitSet<N>& b) {
	BitSet<N> copy = a;
	return copy &= b;
}

template <typename N>
BitSet<N> operator^(const BitSet<N>& a, const BitSet<N>& b) {
	BitSet<N> copy = a;
	return copy ^= b;
}

template <typename N>
BitSet<N> operator<<(const BitSet<N>& a, std::size_t b) {
	BitSet<N> copy = a;
	return copy <<= b;
}

template <typename N>
BitSet<N> operator>>(const BitSet<N>& a, std::size_t b) {
	BitSet<N> copy = a;
	return copy >>= b;
}

template <typename N>
bool operator==(const BitSet<N>& a, const BitSet<N>& b) {
	return a.v == b.v;
}

template <typename N>
bool operator!=(const BitSet<N>& a, const BitSet<N>& b) {
	return !(a == b);
}

template <typename N>
std::ostream& operator<<(std::ostream& stream, const BitSet<N>& b) {
	for (std::size_t index: range(b.size())) {
		stream << (b.get(index) ? '1' : '0');
	}
	return stream;
}
