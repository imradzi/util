#pragma once
#include <queue>
#include <condition_variable>
#include <mutex>
#include <unordered_map>
#include <memory>
#include <chrono>
#include <list>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/thread/concurrent_queues/sync_queue.hpp>
#include <functional>

extern boost::uuids::random_generator_mt19937 uuidGen;
using namespace std::chrono_literals;

namespace MQ {

    enum ReturnCode {
        OK,
        TimeOut,
        Closed,
    };

    constexpr std::chrono::milliseconds waitTimeOut {500};

    template<typename T>
    class Queue {
        boost::concurrent::sync_queue<T> queue;
    public:
        Queue() {}
        Queue(const Queue &) = delete;
        Queue &operator=(const Queue &) = delete;
        ~Queue() { close(); }
        void close() { queue.close(); }
        bool isClosed() { return queue.closed(); }
        ReturnCode send(const T &item) {
            auto res = queue.wait_push(item);
            if (res == boost::concurrent::queue_op_status::closed) return ReturnCode::Closed;
            return ReturnCode::OK;
        }

        ReturnCode receive(T &item) {
            auto res = queue.wait_pull(item);
            if (res == boost::concurrent::queue_op_status::closed) return ReturnCode::Closed;
            return ReturnCode::OK;
        }
    };

    constexpr std::chrono::seconds intervalToExpire {60};

    template<typename Q, typename T, typename S>
    class EventHandler {
        struct Obj {
            std::shared_ptr<Q> ob;
            std::shared_ptr<S> session;
            std::chrono::system_clock::time_point expireOn;
        };

        std::recursive_mutex mtx;
        std::unordered_map<std::string, Obj> list;
        std::function<T(const std::string &key)> fnNotifyKilling;

    public:
        EventHandler(std::function<T(const std::string &)> f): fnNotifyKilling(f) {}

        std::string add(const S &sessionRec) {
            std::lock_guard<std::recursive_mutex> __lock(mtx);
            std::string key;
            while (true) {
                //key = FNVHash32(boost::uuids::to_string(uuidGen()));
                key = boost::uuids::to_string(uuidGen());
                if (list.find(key) == list.end()) break;
            }
            auto &x = list[key];
            x.ob = std::make_shared<Q>();
            x.session = std::make_shared<S>(sessionRec);
            x.expireOn = std::chrono::system_clock::now() + intervalToExpire;
            return key;
        }

        void setExpiry(const std::string &key, std::chrono::system_clock::time_point expiry) {
            std::lock_guard<std::recursive_mutex> __lock(mtx);
            auto it = list.find(key);
            if (it != list.end())
                it->second.expireOn = expiry;
        }

        void resetExpiry(const std::string &key) {
            std::lock_guard<std::recursive_mutex> __lock(mtx);
            auto it = list.find(key);
            if (it != list.end())
                it->second.expireOn = std::chrono::system_clock::now() + intervalToExpire;
        }

        void remove(const std::string &key) {
            std::lock_guard<std::recursive_mutex> __lock(mtx);
            list.erase(key);
        }

        void removeFromList(const std::list<std::string> &keyList) {
            std::lock_guard<std::recursive_mutex> __lock(mtx);
            for (auto key : keyList) list.erase(key);
        }

        std::weak_ptr<Q> getQ(const std::string &key) {
            std::lock_guard<std::recursive_mutex> __lock(mtx);
            auto it = list.find(key);
            if (it != list.end()) {
                return it->second.ob;
            }
            return std::weak_ptr<Q>();
        }

        std::weak_ptr<S> getSession(const std::string &key) {
            std::lock_guard<std::recursive_mutex> __lock(mtx);
            auto it = list.find(key);
            if (it != list.end()) {
                return it->second.session;
            }
            return std::weak_ptr<S>();
        }

        void broadcast(const T &msg, std::function<bool(const S *dest)> fnFilter) {
            std::lock_guard<std::recursive_mutex> __lock(mtx);
            for (auto &q : list) {
                if (fnFilter(q.second.session.get())) {
                    q.second.ob->send(msg);
                }
            }
        }

        std::list<std::string> findExpired() {
            std::lock_guard<std::recursive_mutex> __lock(mtx);
            std::list<std::string> expired;
            for (auto &x : list) {
                if (x.second.expireOn <= std::chrono::system_clock::now()) {
                    x.second.ob->send(fnNotifyKilling(x.first));
                    expired.push_back(x.first);
                }
            }
            return expired;
        }

        void removeExpired() {
            auto list = findExpired();
            std::this_thread::yield();
            std::this_thread::sleep_for(300ms);
            removeFromList(list);
        }

        void closeAll() {
            std::lock_guard<std::recursive_mutex> __lock(mtx);
            for (auto &x : list) {
                if (x.second.ob) x.second.ob->close();
            }
            list.clear();
        }
    };
}
