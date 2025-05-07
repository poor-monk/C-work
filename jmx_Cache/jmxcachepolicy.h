#pragma once
namespace jmxcache {
	template<typename Key, typename Value> 
	class jmxcacheplicy {
	public:
		virtual ~jmxcacheplicy() {};
		virtual void put(Key key, Value value) = 0;
		virtual bool get(Key key, Value &value) = 0;
		virtual Value get(Key key) = 0;
	};
}