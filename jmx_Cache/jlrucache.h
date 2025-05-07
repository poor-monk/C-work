/*
注意形参权限管理，如果只读的话就使用const
对于面向用户的接口，如果要访问类似临界区资源，要使用互斥锁，std::lock_guard<std::mutex>(mutex_)
模板类初始化make_shared<LruNodeType>(Key(),Value());
RemovNode函数出了bug，
*/
#pragma once
#include<cmath>
#include<cstring>
#include<memory> //智能指针的头文件
#include<mutex>
#include<thread>//线程管理，互斥锁等等
#include<unordered_map>
#include<list>
#include<iostream>
#include"jmxcachepolicy.h"
namespace jmxcache {
	template<typename Key, typename Value>class jmxlrucache;
	template<typename Key,typename Value>
	class lru_node {
	private:
		Key key;
		Value value;
		size_t accesscount;
		std::shared_ptr<lru_node>next;
		std::weak_ptr<lru_node>pre;
	public:
		lru_node(Key key_,Value value_)
			:key(key_)
			,value(value_)
			,accesscount(1)
		{}
		Key getkey() const { return key; }
		Value getvalue()const { return value; }
		void setvalue(const Value& value_) { value = value_; }
		size_t getaccesscount() const { return accesscount; }
		void incrementaccesscount() { accesscount++; }
		friend class jmxlrucache<Key,Value>;
	};
	template<typename Key,typename Value>
	class jmxlrucache:public jmxcacheplicy<Key,Value> {
	public:
		using LruNodeType = lru_node<Key, Value>;
		using NodePtr = std::shared_ptr<LruNodeType>;
		using NodeMap = std::unordered_map<Key,NodePtr>;
		jmxlrucache(int capacity_)
			:capacity(capacity_)
		{
			initializeList();
		}
		~jmxlrucache() override = default;
		void put(Key key, Value value)override
		{
			if (capacity <= 0)
				return;
			//std::cout << key << ' ' << value << std::endl;
			std::lock_guard<std::mutex>lock(mutex_);
			auto it = nodemap.find(key);
			if (it != nodemap.end())
			{
				UpdateExisting(it->second,value);
				return;
			}
			//std::cout << "添加" << std::endl;
			AddNewNode(key, value);
		}
		bool get(Key key, Value& value)override
		{
			if (capacity <= 0)
				return false;
			std::lock_guard<std::mutex>lock(mutex_);
			auto it = nodemap.find(key);
			if (it != nodemap.end())
			{
				value = it->second->getvalue();
				//std::cout << "lru" << value << std::endl;
				MoveToMostRecent(it->second);
				return true;
			}
			return false;
		}
		Value get(Key key)override
		{
			Value value{};
			get(key, value);
			return value;
		}
		void remove(Key key)
		{
			if (capacity <= 0)
				return;
			std::lock_guard<std::mutex>lock(mutex_);
			auto it = nodemap.find(key);
			if (it != nodemap.end())
			{
				RemoveNode(it->second);
				nodemap.erase(it);
			}
		}
	private:
		void initializeList()
		{
			dummyhead = std::make_shared<LruNodeType>(Key(),Value());
			dummytail = std::make_shared<LruNodeType>(Key(),Value());
			dummyhead->next = dummytail;
			dummytail->pre = dummyhead;
		}
		void MoveToMostRecent(NodePtr node)
		{
			RemoveNode(node);
			InsertNode(node);
		}
		void RemoveNode(NodePtr node)
		{
			if (!node->pre.expired() && node->next)
			{
				auto prev = node->pre.lock();
				prev->next = node->next;
				node->next->pre = prev;
				node->next = nullptr;
			}
		}
		void InsertNode(NodePtr node)
		{
			auto prev = dummytail->pre.lock();

			prev->next = node;
			node->next = dummytail;
			dummytail->pre = node;
			node->pre = prev;
		}
		void EvictLeastRecent()
		{
			auto leastrecent = dummyhead->next;
			RemoveNode(leastrecent);
			nodemap.erase(leastrecent->getkey());
		}
		void UpdateExisting(NodePtr node,const Value& value)
		{
			//std::cout << value << std::endl;
			node->setvalue(value);
			//std::cout << node->getvalue() << std::endl;
			MoveToMostRecent(node);
		}
		void AddNewNode(Key key, Value value)
		{
			if (nodemap.size() >= capacity)
			{
				EvictLeastRecent();
			}
			NodePtr node = std::make_shared<LruNodeType>(key, value);
			InsertNode(node);
			nodemap[key] = node;
			
		}
	private:
		int		capacity;
		NodeMap		nodemap;
		NodePtr		dummyhead;
		NodePtr		dummytail;
		std::mutex		mutex_;
	};
	template<typename Key,typename Value>
	class jmx_klrucache :public jmxlrucache<Key, Value>
	{
	public:
		~jmx_klrucache() = default;
		jmx_klrucache(int capacity,int historycapacity,int k_)
			:jmxlrucache<Key,Value>(capacity)
			,historyList(std::make_unique<jmxlrucache<Key,size_t>>(historycapacity))
			,k(k_)
		{}
		Value get(Key key)
		{
			Value value{};
			bool ismaincache = jmxlrucache<Key,Value>::get(key, value);
			size_t count = historyList->get(key);
			count++;
			historyList->put(key, count);
			//std::cout << value << ' ' << count << std::endl;
			if (ismaincache)
				return value;
			if (count >= k)
			{
				auto it = historymap.find(key);
				if (it != historymap.end())
				{
					jmxlrucache<Key,Value>::put(key, it->second);
					historyList->remove(key);
					historymap.erase(key);
					return it->second;
				}
			}
			return value;
		}
		void put(Key key, Value value)
		{
			Value tem{};
			bool ismaincache = jmxlrucache<Key,Value>::get(key,tem);
			if (ismaincache)
			{
				//std::cout << "缓存队列中" << std::endl;
				jmxlrucache<Key, Value>::put(key, value);
				return;
			}
			size_t count = historyList->get(key);
			count++;
			historyList->put(key, count);
			historymap[key] = value;
			if (count >= k)
			{
				//std::cout << ' ' << value << ' ' << key << std::endl;
				jmxlrucache<Key, Value>::put(key, value);
				historyList->remove(key);
				//std::cout << "嘿嘿嘿" << std::endl;

				historymap.erase(key);
			}
		}
	private:
		int		k;
		std::unique_ptr<jmxlrucache<Key, size_t>>		historyList;
		std::unordered_map<Key, Value>		historymap;
	};
	template<typename Key,typename Value>
	class jmx_hashlrucache {
	public:
		jmx_hashlrucache(int capacity_, int splice_num_)
			:capacity(capacity_)
			, splice_num(splice_num_ > 0 ? splice_num_ : std::thread::hardware_concurrency())
		{
			size_t splice_capacity = ceil(capacity / static_cast<double>(splice_num));
			for (int i = 0;i < splice_num;i++)
			{
				hashlru.emplace_back(std::make_unique<jmxlrucache<Key, Value>>(splice_capacity));
			}
		}
		bool get(Key key, Value& value)
		{
			size_t hashval = get_hash(key) % splice_num;
			return hashlru[hashval]->get(key, value);
		}
		Value get(Key key)
		{
			Value value{};
			size_t hashval = get_hash(key) % splice_num;
			hashlru[hashval]->get(key, value);
			return value;
		}
		void put(Key key, Value value)
		{
			size_t hashval = get_hash(key) % splice_num;
			hashlru[hashval]->put(key, value);
		}
	private:
		size_t get_hash(Key key)
		{
			std::hash<Key>hashFunc;
			return hashFunc(key);
		}
	private:
		int		splice_num;
		int		capacity;
		std::vector<std::unique_ptr<jmxlrucache<Key,Value>>>		hashlru;
	};
}