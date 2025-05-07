#include <iostream>
#include <string>
#include <chrono>
#include <vector>
#include <iomanip>
#include <random>
#include <algorithm>
#include<array>
#include "jmxcachepolicy.h"
#include "jlfucache.h"
#include "jlrucache.h"
#include "jARCcache/jarccache.h"

class Timer {
public:
    Timer() : start_(std::chrono::high_resolution_clock::now()) {}

    double elapsed() {
        auto now = std::chrono::high_resolution_clock::now();
        return std::chrono::duration_cast<std::chrono::milliseconds>(now - start_).count();
    }

private:
    std::chrono::time_point<std::chrono::high_resolution_clock> start_;
};

// ������������ӡ���
void printResults(const std::string& testName, int capacity,
    const std::vector<int>& get_operations,
    const std::vector<int>& hits) {
    std::cout << "=== " << testName << " ������� ===" << std::endl;
    std::cout << "�����С: " << capacity << std::endl;

    // �����Ӧ���㷨�������ڲ��Ժ����ж���
    std::vector<std::string> names;
    if (hits.size() == 3) {
        names = { "LRU", "LFU", "ARC" };
    }
    else if (hits.size() == 4) {
        names = { "LRU", "LFU", "ARC", "LRU-K" };
    }
    else if (hits.size() == 5) {
        names = { "LRU", "LFU", "ARC", "LRU-K", "LFU-Aging" };
    }

    for (size_t i = 0; i < hits.size(); ++i) {
        double hitRate = 100.0 * hits[i] / get_operations[i];
        std::cout << (i < names.size() ? names[i] : "Algorithm " + std::to_string(i + 1))
            << " - ������: " << std::fixed << std::setprecision(2)
            << hitRate << "% ";
        // ��Ӿ������д������ܲ�������
        std::cout << "(" << hits[i] << "/" << get_operations[i] << ")" << std::endl;
    }

    std::cout << std::endl;  // ��ӿ��У�ʹ���������
}

void testHotDataAccess() {
    std::cout << "\n=== ���Գ���1���ȵ����ݷ��ʲ��� ===" << std::endl;

    const int CAPACITY = 20;         // ��������
    const int OPERATIONS = 500000;   // �ܲ�������
    const int HOT_KEYS = 20;         // �ȵ���������
    const int COLD_KEYS = 5000;      // ����������

    jmxcache::jmxlrucache<int, std::string> lru(CAPACITY);
    jmxcache::jlfucache<int, std::string> lfu(CAPACITY);
    jmxcache::jarccache<int, std::string> arc(CAPACITY);
    // ΪLRU-K���ú��ʵĲ�����
    // - �����������������㷨��ͬ
    // - ��ʷ��¼������Ϊ���ܷ��ʵ����м�����
    // - k=2��ʾ���ݱ�����2�κ�Ż���뻺�棬�ʺ������ȵ��������
    jmxcache::jmx_klrucache<int, std::string> lruk(CAPACITY, HOT_KEYS + COLD_KEYS, 2);
    jmxcache::jlfucache<int, std::string> lfuAging(CAPACITY, 20000);

    std::random_device rd;
    std::mt19937 gen(rd());

    // ����ָ��ָ��������������LFU-Aging
    std::array<jmxcache::jmxcacheplicy<int, std::string>*, 5> caches = { &lru, &lfu, &arc, &lruk, &lfuAging };

    std::vector<int> hits(5, 0);
    std::vector<int> get_operations(5, 0);
    std::vector<std::string> names = { "LRU", "LFU", "ARC", "LRU-K", "LFU-Aging" };

    // Ϊ���еĻ�����������ͬ�Ĳ������в���
    for (int i = 0; i < caches.size(); ++i) {
        // ��Ԥ�Ȼ��棬����һЩ����
        for (int key = 0; key < HOT_KEYS; ++key) {
            std::string value = "value" + std::to_string(key);
            caches[i]->put(key, value);
        }

        // �������put��get������ģ����ʵ����
        for (int op = 0; op < OPERATIONS; ++op) {
            // ���������ϵͳ�ж�������д����Ƶ��
            // ��������30%���ʽ���д����
            bool isPut = (gen() % 100 < 30);
            int key;

            // 70%���ʷ����ȵ����ݣ�30%���ʷ���������
            if (gen() % 100 < 70) {
                key = gen() % HOT_KEYS; // �ȵ�����
            }
            else {
                key = HOT_KEYS + (gen() % COLD_KEYS); // ������
            }

            if (isPut) {
                // ִ��put����
                std::string value = "value" + std::to_string(key) + "_v" + std::to_string(op % 100);
                caches[i]->put(key, value);
            }
            else {
                // ִ��get��������¼�������
                std::string result;
                get_operations[i]++;
                if (caches[i]->get(key, result)) {
                    hits[i]++;
                }
            }
        }
    }

    // ��ӡ���Խ��
    printResults("�ȵ����ݷ��ʲ���", CAPACITY, get_operations, hits);
}

void testLoopPattern() {
    std::cout << "\n=== ���Գ���2��ѭ��ɨ����� ===" << std::endl;

    const int CAPACITY = 50;          // ��������
    const int LOOP_SIZE = 500;        // ѭ����Χ��С
    const int OPERATIONS = 200000;    // �ܲ�������

    jmxcache::jmxlrucache <int, std::string> lru(CAPACITY);
    jmxcache::jlfucache <int, std::string> lfu(CAPACITY);
    jmxcache::jarccache<int, std::string> arc(CAPACITY);
    // ΪLRU-K���ú��ʵĲ�����
    // - ��ʷ��¼������Ϊ��ѭ����С�����������Ƿ�Χ�ںͷ�Χ�������
    // - k=2������ѭ�����ʣ�����һ���������ֵ
    jmxcache::jmx_klrucache<int, std::string> lruk(CAPACITY, LOOP_SIZE * 2, 2);
    jmxcache::jlfucache<int, std::string> lfuAging(CAPACITY, 3000);

    std::array<jmxcache::jmxcacheplicy<int, std::string>*, 5> caches = { &lru, &lfu, &arc, &lruk, &lfuAging };
    std::vector<int> hits(5, 0);
    std::vector<int> get_operations(5, 0);
    std::vector<std::string> names = { "LRU", "LFU", "ARC", "LRU-K", "LFU-Aging" };

    std::random_device rd;
    std::mt19937 gen(rd());

    // Ϊÿ�ֻ����㷨������ͬ�Ĳ���
    for (int i = 0; i < caches.size(); ++i) {
        // ��Ԥ��һ�������ݣ�ֻ����20%�����ݣ�
        for (int key = 0; key < LOOP_SIZE / 5; ++key) {
            std::string value = "loop" + std::to_string(key);
            caches[i]->put(key, value);
        }

        // ����ѭ��ɨ��ĵ�ǰλ��
        int current_pos = 0;

        // ������ж�д������ģ����ʵ����
        for (int op = 0; op < OPERATIONS; ++op) {
            // 20%������д������80%�����Ƕ�����
            bool isPut = (gen() % 100 < 20);
            int key;

            // ���ղ�ͬģʽѡ���
            if (op % 100 < 60) {  // 60%˳��ɨ��
                key = current_pos;
                current_pos = (current_pos + 1) % LOOP_SIZE;
            }
            else if (op % 100 < 90) {  // 30%�����Ծ
                key = gen() % LOOP_SIZE;
            }
            else {  // 10%���ʷ�Χ������
                key = LOOP_SIZE + (gen() % LOOP_SIZE);
            }

            if (isPut) {
                // ִ��put��������������
                std::string value = "loop" + std::to_string(key) + "_v" + std::to_string(op % 100);
                caches[i]->put(key, value);
            }
            else {
                // ִ��get��������¼�������
                std::string result;
                get_operations[i]++;
                if (caches[i]->get(key, result)) {
                    hits[i]++;
                }
            }
        }
    }

    printResults("ѭ��ɨ�����", CAPACITY, get_operations, hits);
}

void testWorkloadShift() {
    std::cout << "\n=== ���Գ���3���������ؾ��ұ仯���� ===" << std::endl;

    const int CAPACITY = 30;            // ��������
    const int OPERATIONS = 80000;       // �ܲ�������
    const int PHASE_LENGTH = OPERATIONS / 5;  // ÿ���׶εĳ���

    jmxcache::jmxlrucache<int, std::string> lru(CAPACITY);
    jmxcache::jlfucache<int, std::string> lfu(CAPACITY);
    jmxcache::jarccache<int, std::string> arc(CAPACITY);
    jmxcache::jmx_klrucache<int, std::string> lruk(CAPACITY, 500, 2);
    jmxcache::jlfucache<int, std::string> lfuAging(CAPACITY, 10000);

    std::random_device rd;
    std::mt19937 gen(rd());
    std::array<jmxcache::jmxcacheplicy<int, std::string>*, 5> caches = { &lru, &lfu, &arc, &lruk, &lfuAging };
    std::vector<int> hits(5, 0);
    std::vector<int> get_operations(5, 0);
    std::vector<std::string> names = { "LRU", "LFU", "ARC", "LRU-K", "LFU-Aging" };

    // Ϊÿ�ֻ����㷨������ͬ�Ĳ���
    for (int i = 0; i < caches.size(); ++i) {
        // ��Ԥ�Ȼ��棬ֻ����������ʼ����
        for (int key = 0; key < 30; ++key) {
            std::string value = "init" + std::to_string(key);
            caches[i]->put(key, value);
        }

        // ���ж�׶β��ԣ�ÿ���׶��в�ͬ�ķ���ģʽ
        for (int op = 0; op < OPERATIONS; ++op) {
            // ȷ����ǰ�׶�
            int phase = op / PHASE_LENGTH;

            // ÿ���׶εĶ�д������ͬ 
            int putProbability;
            switch (phase) {
            case 0: putProbability = 15; break;  // �׶�1: �ȵ���ʣ�15%д�������
            case 1: putProbability = 30; break;  // �׶�2: ��Χ�����д����Ϊ30%
            case 2: putProbability = 10; break;  // �׶�3: ˳��ɨ�裬10%д�뱣�ֲ���
            case 3: putProbability = 25; break;  // �׶�4: �ֲ��������΢��Ϊ25%
            case 4: putProbability = 20; break;  // �׶�5: ��Ϸ��ʣ�����Ϊ20%
            default: putProbability = 20;
            }

            // ȷ���Ƕ�����д����
            bool isPut = (gen() % 100 < putProbability);

            // ���ݲ�ͬ�׶�ѡ��ͬ�ķ���ģʽ����key - �Ż���ķ��ʷ�Χ
            int key;
            if (op < PHASE_LENGTH) {  // �׶�1: �ȵ���� - �ȵ�����5��ʹ�ȵ������
                key = gen() % 5;
            }
            else if (op < PHASE_LENGTH * 2) {  // �׶�2: ��Χ��� - ��Χ400�����ʺ�30��С�Ļ���
                key = gen() % 400;
            }
            else if (op < PHASE_LENGTH * 3) {  // �׶�3: ˳��ɨ�� - ����100����
                key = (op - PHASE_LENGTH * 2) % 100;
            }
            else if (op < PHASE_LENGTH * 4) {  // �׶�4: �ֲ������ - �Ż��ֲ��������С
                // ����5���ֲ�����ÿ�������СΪ15�������뻺���С20�ӽ�����С
                int locality = (op / 800) % 5;  // ����Ϊ5���ֲ�����
                key = locality * 15 + (gen() % 15);  // ÿ����15����
            }
            else {  // �׶�5: ��Ϸ��� - �����ȵ���ʱ���
                int r = gen() % 100;
                if (r < 40) {  // 40%���ʷ����ȵ㣨��30%���ӣ�
                    key = gen() % 5;  // 5���ȵ��
                }
                else if (r < 70) {  // 30%���ʷ����еȷ�Χ
                    key = 5 + (gen() % 45);  // ��С�еȷ�ΧΪ50����
                }
                else {  // 30%���ʷ��ʴ�Χ����40%���٣�
                    key = 50 + (gen() % 350);  // ��ΧҲ��Ӧ��С
                }
            }

            if (isPut) {
                // ִ��д����
                std::string value = "value" + std::to_string(key) + "_p" + std::to_string(phase);
                caches[i]->put(key, value);
            }
            else {
                // ִ�ж���������¼�������
                std::string result;
                get_operations[i]++;
                if (caches[i]->get(key, result)) {
                    hits[i]++;
                }
            }
        }
    }

    printResults("�������ؾ��ұ仯����", CAPACITY, get_operations, hits);
}

int main() {
    testHotDataAccess();
    testLoopPattern();
    testWorkloadShift();
    return 0;
}