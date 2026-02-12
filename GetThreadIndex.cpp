#include <thread>
#include <unordered_map>
#include <mutex>

std::size_t GetThreadIndex(const std::thread::id id) {
    static std::mutex my_mutex;
    static std::size_t nextindex = 0;
    static std::unordered_map<std::thread::id, std::size_t> ids;
    std::lock_guard<std::mutex> lock(my_mutex);
    auto [iter, inserted] = ids.try_emplace(id, nextindex);
    if (inserted) ++nextindex;
    return iter->second;
}
