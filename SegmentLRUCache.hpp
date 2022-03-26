#pragma once
#include "LRU.hpp"

template<typename T>
class SegmentLRUCache
{
public:
	typedef LRUNode<T> LRUNode;

	explicit SegmentLRUCache(size_t probationCapacity, size_t protectionCapacity)
		: _probationCapacity(probationCapacity)
		, _protectionCapacity(protectionCapacity)
		, _hashmap(probationCapacity +  protectionCapacity)
	{}

	std::pair<LRUNode, bool> get(LRUNode& node)
	{
		//找不到则返回空
		auto res = _hashmap.find(node._key);
		if (res == _hashmap.end())
		{
			return std::make_pair(LRUNode(), false);
		}

		//获取数据的位置
		typename std::list<LRUNode>::iterator pos = res->second;

		//如果查找的值在PROTECTION区中，则直接移动到首部
		if (node._flag == PROTECTION)
		{
			_protectionList.erase(pos);

			_protectionList.push_front(node);
			res->second = _protectionList.begin();
			
			return std::make_pair(node, true);
		}

		//如果是PROBATION区的数据，如果PROTECTION还有空位，则将其移过去
		if (_protectionList.size() < _probationCapacity)
		{
			node._flag = PROTECTION;
			_probationList.erase(pos);

			_protectionList.push_front(node);
			res->second = _protectionList.begin();

			return std::make_pair(node, true);
		}

		//如果PROTECTION没有空位，此时就将其最后一个与PROBATION当前元素进行交换位置
		LRUNode backNode = _protectionList.back();

		std::swap(backNode._flag, node._flag);

		_probationList.erase(_hashmap[node._key]);
		_protectionList.erase(_hashmap[backNode._key]);

		_probationList.push_front(backNode);
		_protectionList.push_front(node);

		_hashmap[backNode._key] = _probationList.begin();
		_hashmap[node._key] = _protectionList.begin();


		return std::make_pair(node, true);
	}

	std::pair<LRUNode, bool> put(LRUNode& newNode)
	{
		//新节点放入PROBATION段中
		newNode._flag = PROBATION;
		LRUNode delNode;

		//如果还有剩余空间就直接插入
		if (_probationList.size() < _probationCapacity || size() < _probationCapacity + _protectionCapacity)
		{
			_probationList.push_front(newNode);
			_hashmap.insert(make_pair(newNode._key, _probationList.begin()));

			return std::make_pair(delNode, false);
		}

		//如果没有剩余空间，就需要淘汰掉末尾元素，然后再将元素插入首部
		delNode = _probationList.back();

		_hashmap.erase(delNode._key);
		_probationList.pop_back();

		_probationList.push_front(newNode);
		_hashmap.insert(make_pair(newNode._key, _probationList.begin()));

		return std::make_pair(delNode, true);
	}

	std::pair<LRUNode, bool> victim()
	{
		//如果还有剩余的空间，就不需要淘汰
		if (_probationCapacity + _protectionCapacity > size())
		{
			return std::make_pair(LRUNode(), false);
		}

		//否则淘汰_probationList的最后一个元素
		return std::make_pair(_probationList.back(), true);
	}

	int size() const
	{
		return _protectionList.size() + _probationList.size();
	}

private:
	size_t _probationCapacity;
	size_t _protectionCapacity;

	std::unordered_map<int, typename std::list<LRUNode>::iterator> _hashmap;

	std::list<LRUNode> _probationList;
	std::list<LRUNode> _protectionList;
};