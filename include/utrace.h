#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <tuple>
#include "flowbook_entry.h"

// 定义FlowData结构体，包含flowkey, pkt_len, timestamp, queue_len
struct FlowData {
    flow_key flowkey;
    int pkt_len;
    long timestamp;
    int queue_len;
};

class FlowArray {
private:
    std::vector<FlowData> data; // 存储流数据的数组
    int capacity; // 数组的最大容量

public:
    // 构造函数，指定数组大小
    FlowArray(int size) : capacity(size) {
        data.reserve(size);
    }

    // 向数组中添加元素
    bool addElement(const FlowData& flow_data) {
        if (data.size() >= capacity) {
            std::cerr << "Error: Array is full." << std::endl;
            return false;
        }
        data.push_back(flow_data);
        return true;
    }

    // 将数组保存为CSV格式的文件
    void saveToFile(const std::string& filename) {
        std::ofstream file(filename);
        if (!file.is_open()) {
            std::cerr << "Error: Could not open file." << std::endl;
            return;
        }
        // 写入CSV头部
        file << "flowid,pkt_len,timestamp,queue_len\n";
        // 遍历数组，写入每个元素的数据
        for (const auto& flow_data : data) {
            // 使用flowkey的src_port作为flowid
            int flow_id = flow_data.flowkey._srcport;
            file << flow_id << "," 
                 << flow_data.pkt_len << ","
                 << flow_data.timestamp << ","
                 << flow_data.queue_len << "\n";
        }
        file.close();
    }

    static FlowArray mergeAndSort(const FlowArray& array1, const FlowArray& array2) {
        // 创建一个新的FlowArray对象，其容量是两个数组的总和
        FlowArray mergedArray(array1.data.size() + array2.data.size());

        // 将两个数组的元素添加到新的FlowArray对象中
        mergedArray.data.insert(mergedArray.data.end(), array1.data.begin(), array1.data.end());
        mergedArray.data.insert(mergedArray.data.end(), array2.data.begin(), array2.data.end());

        // 使用lambda表达式根据timestamp对元素进行排序
        std::sort(mergedArray.data.begin(), mergedArray.data.end(),
                  [](const FlowData& a, const FlowData& b) {
                      return a.timestamp < b.timestamp;
                  });

        return mergedArray;
    }
};