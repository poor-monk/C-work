#pragma once
#include<memory>
namespace jmxcache {
	template<typename Key,typename Value>
	class ArcNode {
	private:
		size_t						accesscount;
		Key							key;
		Value						value;
		std::shared_ptr<ArcNode>	next;
		std::weak_ptr<ArcNode>		pre;
	public:
		ArcNode()
			:accesscount(1)
			, next(nullptr)
		//	, pre(nullptr)
		{}
		ArcNode(Key key_,Value value_)
			:accesscount(1)
			,value(value_)
			,key(key_)
			,next(nullptr)
			//,pre(nullptr)
		{}

		Value getvalue() const { return value; }
		Key getkey() const { return key; }
		size_t getaccesscount() const { return accesscount; }

		void setvalue(const Value& value_) { value = value_; }
		void incrementaccesscount() { ++accesscount; }
		template<typename K, typename V> friend class ArcLru;
		template<typename K, typename V> friend class ArcLfu;
	};
}