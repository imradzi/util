#include <thread>
#include <unordered_map>
#include <mutex>

std::size_t GetThreadIndex(const std::thread::id id) {
    static std::mutex my_mutex;
    static std::size_t nextindex = 0;
    static std::unordered_map<std::thread::id, std::size_t> ids;
    std::lock_guard<std::mutex> lock(my_mutex);
    auto iter = ids.find(id);
    if(iter == ids.end())
        return ids[id] = nextindex++;
    return iter->second;
}
