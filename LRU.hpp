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
	uint32_t _conflict;	//��key���ֳ�ͻʱ�ĸ���hashֵ
	bool _flag;	//���ڱ���ڻ����е�λ��

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

		//��ȡ���ݵ�λ��
		typename std::list<LRUNode>::iterator pos = res->second;
		LRUNode curNode = *pos;

		//�������ƶ�������
		_lrulist.erase(pos);
		_lrulist.push_front(curNode);

		res->second = _lrulist.begin();

		return std::make_pair(curNode, true);
	}

	std::pair<LRUNode, bool> put(const LRUNode& node)
	{
		bool flag = false; //�Ƿ��û�����
		LRUNode delNode;

		//������������̭ĩβԪ��
		if (_lrulist.size() == _capacity)
		{
			delNode = _lrulist.back();
			_lrulist.pop_back();
			_hashmap.erase(delNode._key);

			flag = true;
		}
		//��������
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
	//���ù�ϣ�����洢�����Լ�����������ʵ��o(1)��get��put
	std::unordered_map<int, typename std::list<LRUNode>::iterator> _hashmap;
	//����˫�����������滺��ʹ�����������֤o(1)�Ĳ���ɾ��
	std::list<LRUNode> _lrulist;
};