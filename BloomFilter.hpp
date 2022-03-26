#pragma once
#include <string>
#include <cmath>
#include <vector>

class BloomFilter
{
public:
	explicit BloomFilter(size_t capacity, double fp)
		: _bitsPerKey((getBitsPerKey(capacity, fp) < 32 ? 32 : _bitsPerKey) + 31)
		, _hashCount(getHashCount(capacity, _bitsPerKey))
		, _bits(_bitsPerKey, 0)
	{
		//如果哈希函数的数量超过上限，此时准确率就非常低
		if (_hashCount > MAX_HASH_COUNT)
		{
			_hashCount = MAX_HASH_COUNT;
		}
	}

	void put(uint32_t hash)
	{
		int delta = hash >> 17 | hash << 15;

		for (int i = 0; i < _hashCount; i++)
		{
			int bitPos = hash % _bitsPerKey;
			_bits[bitPos / 32] |= (1 << (bitPos % 32));
			hash += delta;
		}
	}

	bool contains(uint32_t hash) const 
	{
		//参考leveldb的做法，通过delta来打乱哈希值，模拟进行多次哈希函数
		int delta = hash >> 17 | hash << 15;	//右旋17位，打乱哈希值
		int bitPos = 0;
		for (int i = 0; i < _hashCount; i++) 
		{
			bitPos = hash % _bitsPerKey;
			if ((_bits[bitPos / 32] & (1 << (bitPos % 32))) == 0) 
			{
				return false;
			}
			hash += delta;
		}

		return true;
	}

	bool allow(uint32_t hash)
	{
		if (contains(hash) == false)
		{
			put(hash);
			return false;
		}
		return true;
	}

	void clear() 
	{
		for (auto& it : _bits) 
		{
			it = 0;
		}
	}

private:
	size_t _bitsPerKey;
	size_t _hashCount;
	std::vector<int> _bits;

	static const int MAX_HASH_COUNT = 30;

	int getBitsPerKey(int capacity, double fp) const
	{
		return std::ceil(-1 * capacity * std::log(fp) / std::pow(std::log(2), 2));
	}

	int getHashCount(int capacity, int bitsPerKey) const
	{
		return std::round(std::log(2) * bitsPerKey / double(capacity));
	}
};