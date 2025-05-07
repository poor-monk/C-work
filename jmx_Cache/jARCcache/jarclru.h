#pragma once
#include"jarcnode.h"
#include<memory>
#include<unordered_map>
#include<mutex>
#include<iostream>
namespace jmxcache {
	template<typename Key,typename Value>
	class ArcLru {
	public:
		using NodeType = jmxcache::ArcNode<Key, Value>;
		using NodePtr = std::shared_ptr<NodeType>;
		using LruMap = std::unordered_map<Key, NodePtr>;
		ArcLru(int capacity_,int transformthreshold_)
			:capacity(capacity_)
			,ghotscapacity(capacity_)
			,transformthreshold(transformthreshold_)
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
				//std::cout<<key<<' '<<value<<'\n';
				return updateExistingNode(it->second, value);
			}
			return addNewNode(key,value);
		}
		bool get(Key key, Value& value, bool& threshold)
		{
			if (capacity <= 0)
				return false;
			std::lock_guard<std::mutex>lock(mutex_);
			auto it = mainmap.find(key);
			if (it != mainmap.end())
			{
				threshold = updateaccessNode(it->second);
				value = it->second->getvalue();
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
				ghotsmap.erase(key);
				return true;
			}
			return false;
		}
		void increaseCapacity() { ++capacity; }
		bool decreaseCapacity() {
			if (capacity <= 0)
				return false;
			if (mainmap.size() == capacity)
			{
				evictLeastRecent();
			}
			--capacity;
			return true;
		}
	private:
		void initlist()
		{
			mainhead = std::make_shared<NodeType>();
			maintail = std::make_shared<NodeType>();
			mainhead->next = maintail;
			maintail->pre = mainhead;

			ghotshead = std::make_shared<NodeType>();
			ghotstail = std::make_shared<NodeType>();
			ghotshead->next = ghotstail;
			ghotstail->pre = ghotshead;
		}
		bool updateExistingNode(NodePtr node, const Value& value)
		{
			node->setvalue(value);
			movetorecent(node);
			return true;
		}
		bool updateaccessNode(NodePtr node)
		{
			movetorecent(node);
			node->incrementaccesscount();
			return node->getaccesscount() >= transformthreshold;
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
		void movetorecent(NodePtr node)
		{
			remove(node);
			//std::cout << "*"<<node->getkey() << '\n';
			insertmainnode(node);
		}
		bool addNewNode(Key key,Value value) {
			auto newnode = std::make_shared<NodeType>(key, value);
			if (mainmap.size() >= capacity)
			{
				evictLeastRecent();
			}
			insertmainnode(newnode);
			mainmap[key] = newnode;
			return true;
		}
		void insertmainnode(NodePtr node)
		{
			auto prev = maintail->pre.lock();
			prev->next = node;
			node->next = maintail;
			maintail->pre = node;
			node->pre = prev;
		}
		void evictLeastRecent()
		{
			auto node = mainhead->next;
			if (!node||node == maintail)
				return;
			remove(node);
			mainmap.erase(node->getkey()); 
			addtoghots(node);
		}
		void addtoghots(NodePtr node)
		{
			if (ghotscapacity <= ghotsmap.size())
			{
				auto frontnode = ghotshead->next;
				if (!frontnode || frontnode == ghotstail)
					return;
				remove(node);
				ghotsmap.erase(frontnode->getkey());
			}
			insertghotsnode(node);
			ghotsmap[node->getkey()] = node;
		}
		void insertghotsnode(NodePtr node)
		{
			auto prev = ghotstail->pre.lock();
			prev->next = node;
			node->next = ghotstail;
			ghotstail->pre = node;
			node->pre = prev;
		}
		void removeghotsnode(NodePtr node)
		{
			remove(node);
			ghotsmap.erase(node->getkey());
		}
	private:
		size_t		capacity;
		size_t		ghotscapacity;
		size_t		transformthreshold;
		std::mutex	mutex_;
		NodePtr		mainhead;
		NodePtr		maintail;
		NodePtr		ghotshead;
		NodePtr		ghotstail;

		LruMap		mainmap;
		LruMap		ghotsmap;
	};
}