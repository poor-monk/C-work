#pragma once
#include<memory>
#include<mutex>
#include<unordered_map>
#include<list>
#include"jarcnode.h"
namespace jmxcache {
template<typename Key,typename Value>
class ArcLfu {
public:
	using NodeType = jmxcache::ArcNode<Key, Value>;
	using NodePtr = std::shared_ptr<NodeType>;
	using NodeMap = std::unordered_map<Key, NodePtr>;
	using FreqMap = std::unordered_map<size_t, std::list<NodePtr>>;
	ArcLfu(size_t capacity_)
		:capacity(capacity_)
		, ghotscapacity(capacity_)
		, minfreq(0)
	{
		initlist();
	}
	bool put(Key key, Value value)
	{
		if (capacity <= 0)
			return false;
		std::lock_guard<std::mutex>lock(mutex_);
		auto it = mainmap.find(key);
		if (it != mainmap.end())
		{
			return updateExistingnode(it->second,value);
		}
		return addNewNode(key, value);
	}
	bool get(Key key, Value& value)
	{
		std::lock_guard<std::mutex>lock(mutex_);
		auto it = mainmap.find(key);
		if (it != mainmap.end())
		{
			value = it->second->getvalue();
			updateFreqnode(it->second);
			return true;
		}
		return false;
	}
	bool checkGhots(Key key)
	{
		auto it = ghotsmap.find(key);
		if (it != ghotsmap.end())
		{
			removeghotsnode(it->second);
			ghotsmap.erase(it->second->getkey());
			return true;
		}
		return false;
	}
	void increaseCapacity() { capacity++; }
	bool decreaseCapacity() {
		if (capacity == 0)
			return false;
		if (mainmap.size() == capacity)
		{
			evictLeastFreq();
		}
		capacity--;
		return true;
	}
private:
	void initlist()
	{
		ghotshead = std::make_shared<NodeType>();
		ghotstail = std::make_shared<NodeType>();
		
		ghotshead->next = ghotstail;
		ghotstail->pre = ghotshead;
	}
	void remove(NodePtr node)
	{
		if (!node->pre.expired() && node->next)
		{
			auto prev = node->pre.lock();
			prev->next = node->next;
			node->next->pre = prev;
			node->next = nullptr;
		}
	}
	bool updateExistingnode(NodePtr node, Value value)
	{
		node->setvalue(value);
		updateFreqnode(node);
		return true;
	}
	void updateFreqnode(NodePtr node)
	{
		size_t oldfreq = node->getaccesscount();
		node->incrementaccesscount();
		size_t newfreq = node->getaccesscount();
		auto& oldlist = freqmap[oldfreq];
		oldlist.remove(node);
		if (oldlist.empty())
		{
			freqmap.erase(oldfreq);
			if (oldfreq == minfreq)
				minfreq = newfreq;
		}
		if (freqmap.find(newfreq) == freqmap.end())
		{
			freqmap[newfreq] = std::list<NodePtr>();
		}
		freqmap[newfreq].push_back(node);
	}
	bool addNewNode(Key key, Value value)
	{
		if (mainmap.size() >= capacity)
		{
			evictLeastFreq();
		}
		NodePtr newnode = std::make_shared<NodeType>(key, value);
		mainmap[key] = newnode;
		if (freqmap.find(1) == freqmap.end())
			freqmap[1] = std::list<NodePtr>();
		freqmap[newnode->getaccesscount()].push_back(newnode);
		minfreq = 1;
		return true;
	}
	void evictLeastFreq()
	{
		if (freqmap.empty())
			return;
		auto& minfreqlist = freqmap[minfreq];
		if (minfreqlist.empty())
			return;
		auto leastnode = minfreqlist.front();
		minfreqlist.pop_front();
		if (minfreqlist.empty())
		{
			freqmap.erase(minfreq);
			if(!freqmap.empty())
			minfreq = freqmap.begin()->first;
		}
		if (ghotsmap.size() >= ghotscapacity)
		{
			removeghotsnode(ghotshead->next);
		}
		mainmap.erase(leastnode->getkey());
		addtoghots(leastnode);
		ghotsmap[leastnode->getkey()] = leastnode;
	}
	void addtoghots(NodePtr node)
	{
		auto prev = ghotstail->pre.lock();
		prev->next = node;
		node->next = ghotstail;
		ghotstail->pre = node;
		node->pre = prev;
	}
	void removeghotsnode(NodePtr node)
	{
		if (!node || node == ghotstail)
			return;
		remove(node);
	}
private:
	size_t capacity;
	size_t ghotscapacity;
	size_t minfreq;
	std::mutex mutex_;

	FreqMap freqmap;

	NodeMap mainmap;
	NodeMap ghotsmap;
	
	NodePtr ghotshead;
	NodePtr ghotstail;
};
}