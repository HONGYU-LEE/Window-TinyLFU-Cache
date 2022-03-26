#pragma once
#include <shared_mutex>
#include "BloomFilter.hpp"
#include "CountMinSketch.hpp"
#include "SegmentLRUCache.hpp"

enum CACHE_ZONE
{
	SEGMENT_LRU,
	WINDOWS_LRU
};

template<typename V>
class WindowsTinyLFU
{
public:
	typedef LRUCache<V> LRUCache;
	typedef LRUNode<V> LRUNode;
	typedef SegmentLRUCache<V> SegmentLRUCache;

	explicit WindowsTinyLFU(size_t capacity)
		: _wlru(std::ceil(capacity * 0.01))
		, _slru(std::ceil(0.2 * (capacity * (1 - 0.01))), std::ceil((1 - 0.2) * (capacity * (1 - 0.01))))
		, _bloomFilter(capacity, BLOOM_FALSE_POSITIVE_RATE / 100)
		, _cmSketch(capacity)
		, _dataMap(capacity)
		, _totalVisit(0)
		, _threshold(100)
	{}

	std::pair<V, bool> Get(const std::string& key)
	{
		uint32_t keyHash = Hash(key.c_str(), key.size(), KEY_HASH_SEED);
		uint32_t conflictHash = Hash(key.c_str(), key.size(), CONFLICT_HASH_SEED);

		std::shared_lock<std::shared_mutex> rLock(_rwMutex);
		return get(keyHash, conflictHash);
	}

	std::pair<V, bool> Del(const std::string& key)
	{
		uint32_t keyHash = Hash(key.c_str(), key.size(), KEY_HASH_SEED);
		uint32_t conflictHash = Hash(key.c_str(), key.size(), CONFLICT_HASH_SEED);

		std::unique_lock<std::shared_mutex> wLock(_rwMutex);	
		return del(keyHash, conflictHash);
	}

	bool Put(const std::string& key, const V& value)
	{
		uint32_t keyHash = Hash(key.c_str(), key.size(), KEY_HASH_SEED);
		uint32_t conflictHash = Hash(key.c_str(), key.size(), CONFLICT_HASH_SEED);

		std::unique_lock<std::shared_mutex> wLock(_rwMutex);
		return put(keyHash, value, conflictHash);
	}

	static unsigned int Hash(const void* key, int len, int seed)
	{
		const unsigned int m = 0x5bd1e995;
		const int r = 24;
		unsigned int h = seed ^ len;

		const unsigned char* data = (const unsigned char*)key;
		while (len >= 4)
		{
			unsigned int k = *(unsigned int*)data;
			k *= m;
			k ^= k >> r;
			k *= m;
			h *= m;
			h ^= k;
			data += 4;
			len -= 4;
		}

		switch (len)
		{
		case 3: h ^= data[2] << 16;
		case 2: h ^= data[1] << 8;
		case 1: h ^= data[0];
			h *= m;
		};

		h ^= h >> 13;
		h *= m;
		h ^= h >> 15;
		return h;
	}


private:
	LRUCache _wlru;
	SegmentLRUCache _slru;
	BloomFilter _bloomFilter;
	CountMinSketch _cmSketch;
	std::shared_mutex _rwMutex;
	std::unordered_map<int, LRUNode> _dataMap;	//用于记录数据所在的区域
	size_t _totalVisit;
	size_t _threshold;	//保鲜机制

	static const int BLOOM_FALSE_POSITIVE_RATE = 1;		
	static const int KEY_HASH_SEED = 0xbc9f1d34;
	static const int CONFLICT_HASH_SEED = 0x9ae16a3b;


	std::pair<V, bool> get(uint32_t keyHash, uint32_t conflictHash)
	{
		//判断访问次数，如果访问次数达到限制，则触发保鲜机制
		++_totalVisit;
		if (_totalVisit >= _threshold)
		{
			_cmSketch.Reset();
			_bloomFilter.clear();
			_totalVisit = 0;
		}

		//在cmSketch对访问计数
		_cmSketch.Increment(keyHash);

		//首先查找map，如果map中没有找到则说明不存在
		auto res = _dataMap.find(keyHash);
		if (res == _dataMap.end())
		{
			return std::make_pair(V(), false);
		}

		LRUNode node = res->second;
		V value;

		//校验conflictHash是否一致，防止keyHash冲突查到错误数据
		if (node._conflict != conflictHash)
		{
			return std::make_pair (V(), false);
		}

		//判断数据位于slru还是wlru，进入对应的缓存中查询
		if (node._flag == WINDOWS_LRU)
		{
			_wlru.get(node);
		}
		else
		{
			_slru.get(node);
		}
		return std::make_pair(node._value, true);
	}

	std::pair<V, bool> del(uint32_t keyHash, uint32_t conflictHash)
	{
		auto res = _dataMap.find(keyHash);
		if (res == _dataMap.end())
		{
			return std::make_pair(V(), false);
		}

		LRUNode node = res->second;

		//再次验证conflictHash是否相同，防止由于keyHash冲突导致的误删除
		if (node._conflict != conflictHash)
		{
			return std::make_pair(V(), false);
		}
		
		_dataMap.erase(keyHash);
		return std::make_pair(node._value, true);
	}

	bool put(uint32_t keyHash, const V& value, uint32_t conflictHash)
	{

		LRUNode newNode(keyHash, value, conflictHash, WINDOWS_LRU);
		//将数据放入wlru，如果wlru没满，则直接返回
		std::pair<LRUNode, bool> res = _wlru.put(newNode);
		if (res.second == false)
		{
			_dataMap[keyHash] = newNode;
			return true;
		}

		//如果此时发生了淘汰，将淘汰节点删去
		if (_dataMap[res.first._key]._flag == WINDOWS_LRU)
		{
			_dataMap.erase(res.first._key);
		}
		

		//如果slru没满，则插入到slru中。
		newNode._flag = SEGMENT_LRU;

		std::pair<LRUNode, bool> victimRes = _slru.victim();

		if (victimRes.second == false)
		{
			_dataMap[keyHash] = newNode;
			_slru.put(newNode);
			return true;
		}

		//如果该值没有在布隆过滤器中出现过，其就不可能比victimNode高频，则插入布隆过滤器后返回
		if (!_bloomFilter.allow(keyHash))
		{
			return false;
		}

		//如果其在布隆过滤器出现过，此时就对比其和victim在cmSketch中的计数，保留大的那一个
		int victimCount = _cmSketch.getCountMin(victimRes.first._key);
		int newNodeCount = _cmSketch.getCountMin(newNode._key);

		//如果victim大于当前节点，则没有插入的必要
		if (newNodeCount < victimCount)
		{
			return false;
		}
		_dataMap[keyHash] = newNode;
		_slru.put(newNode);

		//如果满了，此时发生了淘汰，将淘汰节点删去
		if (_dataMap[res.first._key]._flag == SEGMENT_LRU)
		{
			_dataMap.erase(victimRes.first._key);
		}

		return true;
	}
};