#pragma once
#include"../jmxcachepolicy.h"
#include"jarclfu.h"
#include"jarclru.h"
#include<memory>
#include<unordered_map>
namespace jmxcache {
	template<typename Key,typename Value>
	class jarccache:public jmxcacheplicy<Key,Value> {
	public:
		explicit jarccache(size_t capacity_ = 10,size_t threshold_ = 3)
			:capacity(capacity_)
			,threshold(threshold_)
			,arclru(std::make_unique<ArcLru<Key,Value>>(capacity_,threshold_))
			,arclfu(std::make_unique<ArcLfu<Key, Value>>(capacity_))

		{}
		void put(Key key, Value value)override
		{
			if (capacity <= 0)
				return;
			bool inghots = checkGhotsCache(key);
			if(!inghots)
			{
				if (arclru->put(key, value))
				{
					arclfu->put(key, value);
				}
			}
			else
			{
				arclru->put(key, value);
			}
		}
		bool get(Key key, Value& value)override
		{
			//std::cout << "*" << key << ' ' << value << '\n';
			checkGhotsCache(key);
			bool shold = false;
			if (arclru->get(key, value, shold))
			{
				if (shold)
				{
					arclfu->put(key, value);
				}
				//std::cout << "*" << 1 << '\n';
				return true;
			}
			//std::cout << "**" << 1 << '\n';
			return arclfu->get(key, value);
		}
		Value get(Key key)override
		{
			Value value{};
			get(key, value);
			return value;
		}
	private:
		bool checkGhotsCache(Key key)
		{
			bool inghots = false;
			if (arclru->checkGhots(key))
			{
				
				if (arclfu->decreaseCapacity())
					arclru->increaseCapacity();
				inghots = true;
			}
			else if (arclfu->checkGhots(key))
			{
				if (arclru->decreaseCapacity())
					arclfu->increaseCapacity();
				inghots = true;
			}
			return inghots;
		}
	private:
		size_t capacity;
		size_t threshold;

		std::unique_ptr<ArcLru<Key, Value>> arclru;
		std::unique_ptr<ArcLfu<Key, Value>> arclfu;

	};
}