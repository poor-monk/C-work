#pragma once

#include <cmath>
#include <memory>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <vector>

#include "jmxcachepolicy.h"

namespace jmxcache
{

    template<typename Key, typename Value> class jlfucache;

    template<typename Key, typename Value>
    class FreqList
    {
    private:
        struct Node
        {
            int freq; // ����Ƶ��
            Key key;
            Value value;
            std::weak_ptr<Node> pre; // ��һ����Ϊweak_ptr����ѭ������
            std::shared_ptr<Node> next;

            Node()
                : freq(1), next(nullptr) {
            }
            Node(Key key, Value value)
                : freq(1), key(key), value(value), next(nullptr) {
            }
        };

        using NodePtr = std::shared_ptr<Node>;
        int freq_; // ����Ƶ��
        NodePtr head_; // ��ͷ���
        NodePtr tail_; // ��β���

    public:
        explicit FreqList(int n)
            : freq_(n)
        {
            head_ = std::make_shared<Node>();
            tail_ = std::make_shared<Node>();
            head_->next = tail_;
            tail_->pre = head_;
        }

        bool isEmpty() const
        {
            return head_->next == tail_;
        }

        // ���Ǽҽ�������
        void addNode(NodePtr node)
        {
            if (!node || !head_ || !tail_)
                return;

            node->pre = tail_->pre;
            node->next = tail_;
            tail_->pre.lock()->next = node; // ʹ��lock()��ȡshared_ptr
            tail_->pre = node;
        }

        void removeNode(NodePtr node)
        {
            if (!node || !head_ || !tail_)
                return;
            if (node->pre.expired() || !node->next)
                return;

            auto pre = node->pre.lock(); // ʹ��lock()��ȡshared_ptr
            pre->next = node->next;
            node->next->pre = pre;
            node->next = nullptr; // ȷ����ʽ�ÿ�nextָ�룬���׶Ͽ��ڵ������������
        }

        NodePtr getFirstNode() const { return head_->next; }

        friend class jlfucache<Key, Value>;
    };

    template <typename Key, typename Value>
    class jlfucache : public jmxcacheplicy<Key, Value>
    {
    public:
        using Node = typename FreqList<Key, Value>::Node;
        using NodePtr = std::shared_ptr<Node>;
        using NodeMap = std::unordered_map<Key, NodePtr>;

        jlfucache(int capacity, int maxAverageNum = 1000000)
            : capacity_(capacity), minFreq_(INT8_MAX), maxAverageNum_(maxAverageNum),
            curAverageNum_(0), curTotalNum_(0)
        {
        }

        ~jlfucache() override = default;

        void put(Key key, Value value) override
        {
            if (capacity_ == 0)
                return;

            std::lock_guard<std::mutex> lock(mutex_);
            auto it = nodeMap_.find(key);
            if (it != nodeMap_.end())
            {
                // ������valueֵ
                it->second->value = value;
                // �ҵ���ֱ�ӵ����ͺ��ˣ�������ȥget������һ�飬����ʵӰ�첻��
                getInternal(it->second, value);
                return;
            }

            putInternal(key, value);
        }

        // valueֵΪ��������
        bool get(Key key, Value& value) override
        {
            std::lock_guard<std::mutex> lock(mutex_);
            auto it = nodeMap_.find(key);
            if (it != nodeMap_.end())
            {
                getInternal(it->second, value);
                return true;
            }

            return false;
        }

        Value get(Key key) override
        {
            Value value;
            get(key, value);
            return value;
        }

        // ��ջ���,������Դ
        void purge()
        {
            nodeMap_.clear();
            freqToFreqList_.clear();
        }

    private:
        void putInternal(Key key, Value value); // ��ӻ���
        void getInternal(NodePtr node, Value& value); // ��ȡ����

        void kickOut(); // �Ƴ������еĹ�������

        void removeFromFreqList(NodePtr node); // ��Ƶ���б����Ƴ��ڵ�
        void addToFreqList(NodePtr node); // ��ӵ�Ƶ���б�

        void addFreqNum(); // ����ƽ�����ʵ�Ƶ��
        void decreaseFreqNum(int num); // ����ƽ�����ʵ�Ƶ��
        void handleOverMaxAverageNum(); // ����ǰƽ������Ƶ�ʳ������޵����
        void updateMinFreq();

    private:
        int                                            capacity_; // ��������
        int                                            minFreq_; // ��С����Ƶ��(�����ҵ���С����Ƶ�ν��)
        int                                            maxAverageNum_; // ���ƽ������Ƶ��
        int                                            curAverageNum_; // ��ǰƽ������Ƶ��
        int                                            curTotalNum_; // ��ǰ�������л���������� 
        std::mutex                                     mutex_; // ������
        NodeMap                                        nodeMap_; // key �� ����ڵ��ӳ��
        std::unordered_map<int, FreqList<Key, Value>*> freqToFreqList_;// ����Ƶ�ε���Ƶ�������ӳ��
    };

    template<typename Key, typename Value>
    void jlfucache<Key, Value>::getInternal(NodePtr node, Value& value)
    {
        // �ҵ�֮����Ҫ����ӵͷ���Ƶ�ε�������ɾ����������ӵ�+1�ķ���Ƶ�������У�
        // ����Ƶ��+1, Ȼ���valueֵ����
        value = node->value;
        // ��ԭ�з���Ƶ�ε�������ɾ���ڵ�
        removeFromFreqList(node);
        node->freq++;
        addToFreqList(node);
        // �����ǰnode�ķ���Ƶ���������minFreq+1��������ǰ������Ϊ�գ���˵��
        // freqToFreqList_[node->freq - 1]������node��Ǩ���Ѿ����ˣ���Ҫ������С����Ƶ��
        if (node->freq - 1 == minFreq_ && freqToFreqList_[node->freq - 1]->isEmpty())
            minFreq_++;

        // �ܷ���Ƶ�κ͵�ǰƽ������Ƶ�ζ���֮����
        addFreqNum();
    }

    template<typename Key, typename Value>
    void jlfucache<Key, Value>::putInternal(Key key, Value value)
    {
        // ������ڻ����У�����Ҫ�жϻ����Ƿ�����
        if (nodeMap_.size() == capacity_)
        {
            // ����������ɾ��������ʵĽ�㣬���µ�ǰƽ������Ƶ�κ��ܷ���Ƶ��
            kickOut();
        }

        // �����½�㣬���½����ӽ��룬������С����Ƶ��
        NodePtr node = std::make_shared<Node>(key, value);
        nodeMap_[key] = node;
        addToFreqList(node);
        addFreqNum();
        minFreq_ = std::min(minFreq_, 1);
    }

    template<typename Key, typename Value>
    void jlfucache<Key, Value>::kickOut()
    {
        NodePtr node = freqToFreqList_[minFreq_]->getFirstNode();
        removeFromFreqList(node);
        nodeMap_.erase(node->key);
        decreaseFreqNum(node->freq);
    }

    template<typename Key, typename Value>
    void jlfucache<Key, Value>::removeFromFreqList(NodePtr node)
    {
        // ������Ƿ�Ϊ��
        if (!node)
            return;

        auto freq = node->freq;
        freqToFreqList_[freq]->removeNode(node);
    }

    template<typename Key, typename Value>
    void jlfucache<Key, Value>::addToFreqList(NodePtr node)
    {
        // ������Ƿ�Ϊ��
        if (!node)
            return;

        // ��ӽ�����Ӧ��Ƶ������ǰ��Ҫ�жϸ�Ƶ�������Ƿ����
        auto freq = node->freq;
        if (freqToFreqList_.find(node->freq) == freqToFreqList_.end())
        {
            // �������򴴽�
            freqToFreqList_[node->freq] = new FreqList<Key, Value>(node->freq);
        }

        freqToFreqList_[freq]->addNode(node);
    }

    template<typename Key, typename Value>
    void jlfucache<Key, Value>::addFreqNum()
    {
        curTotalNum_++;
        if (nodeMap_.empty())
            curAverageNum_ = 0;
        else
            curAverageNum_ = curTotalNum_ / nodeMap_.size();

        if (curAverageNum_ > maxAverageNum_)
        {
            handleOverMaxAverageNum();
        }
    }

    template<typename Key, typename Value>
    void jlfucache<Key, Value>::decreaseFreqNum(int num)
    {
        // ����ƽ������Ƶ�κ��ܷ���Ƶ��
        curTotalNum_ -= num;
        if (nodeMap_.empty())
            curAverageNum_ = 0;
        else
            curAverageNum_ = curTotalNum_ / nodeMap_.size();
    }

    template<typename Key, typename Value>
    void jlfucache<Key, Value>::handleOverMaxAverageNum()
    {
        if (nodeMap_.empty())
            return;

        // ��ǰƽ������Ƶ���Ѿ����������ƽ������Ƶ�Σ����н��ķ���Ƶ��- (maxAverageNum_ / 2)
        for (auto it = nodeMap_.begin(); it != nodeMap_.end(); ++it)
        {
            // ������Ƿ�Ϊ��
            if (!it->second)
                continue;

            NodePtr node = it->second;

            // �ȴӵ�ǰƵ���б����Ƴ�
            removeFromFreqList(node);

            // ����Ƶ��
            node->freq -= maxAverageNum_ / 2;
            if (node->freq < 1) node->freq = 1;

            // ��ӵ��µ�Ƶ���б�
            addToFreqList(node);
        }

        // ������СƵ��
        updateMinFreq();
    }

    template<typename Key, typename Value>
    void jlfucache<Key, Value>::updateMinFreq()
    {
        minFreq_ = INT8_MAX;
        for (const auto& pair : freqToFreqList_)
        {
            if (pair.second && !pair.second->isEmpty())
            {
                minFreq_ = std::min(minFreq_, pair.first);
            }
        }
        if (minFreq_ == INT8_MAX)
            minFreq_ = 1;
    }

    // ��û�������ռ任ʱ�䣬���ǰ�ԭ�л����С�����˷�Ƭ��
    template<typename Key, typename Value>
    class jHashLfuCache
    {
    public:
        jHashLfuCache(size_t capacity, int sliceNum, int maxAverageNum = 10)
            : sliceNum_(sliceNum > 0 ? sliceNum : std::thread::hardware_concurrency())
            , capacity_(capacity)
        {
            size_t sliceSize = std::ceil(capacity_ / static_cast<double>(sliceNum_)); // ÿ��lfu��Ƭ������
            for (int i = 0; i < sliceNum_; ++i)
            {
                lfuSliceCaches_.emplace_back(new jlfucache <Key, Value>(sliceSize, maxAverageNum));
            }
        }

        void put(Key key, Value value)
        {
            // ����key�ҳ���Ӧ��lfu��Ƭ
            size_t sliceIndex = Hash(key) % sliceNum_;
            lfuSliceCaches_[sliceIndex]->put(key, value);
        }

        bool get(Key key, Value& value)
        {
            // ����key�ҳ���Ӧ��lfu��Ƭ
            size_t sliceIndex = Hash(key) % sliceNum_;
            return lfuSliceCaches_[sliceIndex]->get(key, value);
        }

        Value get(Key key)
        {
            Value value;
            get(key, value);
            return value;
        }

        // �������
        void purge()
        {
            for (auto& lfuSliceCache : lfuSliceCaches_)
            {
                lfuSliceCache->purge();
            }
        }

    private:
        // ��key����ɶ�Ӧ��ϣֵ
        size_t Hash(Key key)
        {
            std::hash<Key> hashFunc;
            return hashFunc(key);
        }

    private:
        size_t capacity_; // ����������
        int sliceNum_; // �����Ƭ����
        std::vector<std::unique_ptr<jlfucache<Key, Value>>> lfuSliceCaches_; // ����lfu��Ƭ����
    };

} // namespace KamaCache