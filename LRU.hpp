#pragma once
#include <unordered_map>
#include <list>

enum SEGMENT_ZONE
{
	PROBATION,
	PROTECTION
};

template<typename T>
struct LRUNode
{
public:
	uint32_t _key;
	T _value;
	uint32_t _conflict;	//在key出现冲突时的辅助hash值
	bool _flag;	//用于标记在缓存中的位置

	explicit LRUNode(uint32_t key = -1, const T& value = T(), uint32_t conflict = 0, bool flag = PROBATION)
		: _key(key)
		, _value(value)
		, _conflict(conflict)
		, _flag(flag)
	{}

};

template<typename T>
class LRUCache
{
public:
	typedef LRUNode<T> LRUNode;

	explicit LRUCache(size_t capacity)
		: _capacity(capacity)
		, _hashmap(capacity)
	{}

	std::pair<LRUNode, bool> get(const LRUNode& node)
	{
		auto res = _hashmap.find(node._key);
		if (res == _hashmap.end())
		{
			return std::make_pair(LRUNode(), false);
		}

		//获取数据的位置
		typename std::list<LRUNode>::iterator pos = res->second;
		LRUNode curNode = *pos;

		//将数据移动到队首
		_lrulist.erase(pos);
		_lrulist.push_front(curNode);

		res->second = _lrulist.begin();

		return std::make_pair(curNode, true);
	}

	std::pair<LRUNode, bool> put(const LRUNode& node)
	{
		bool flag = false; //是否置换数据
		LRUNode delNode;

		//数据已满，淘汰末尾元素
		if (_lrulist.size() == _capacity)
		{
			delNode = _lrulist.back();
			_lrulist.pop_back();
			_hashmap.erase(delNode._key);

			flag = true;
		}
		//插入数据
		_lrulist.push_front(node);
		_hashmap.insert(make_pair (node._key, _lrulist.begin()));
		return std::make_pair(delNode, flag);
	}

	size_t capacity() const
	{
		return _capacity;
	}

	size_t size() const 
	{
		return _lrulist.size();
	}

private:
	size_t _capacity;
	//利用哈希表来存储数据以及迭代器，来实现o(1)的get和put
	std::unordered_map<int, typename std::list<LRUNode>::iterator> _hashmap;
	//利用双向链表来保存缓存使用情况，并保证o(1)的插入删除
	std::list<LRUNode> _lrulist;
};