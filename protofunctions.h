#pragma once
#include <google/protobuf/util/json_util.h>
namespace Protobuf {
    template<typename T>
    std::string ToJSON(const T& proto) {
        std::string json;
        auto ret = google::protobuf::util::MessageToJsonString(proto, &json);
        if (ret.ok())
            return json;
        else
            return "";
    }

    template<typename T, typename S>
    T FromJSON(const S &json) {
        T proto;
        auto ret = google::protobuf::util::JsonStringToMessage(json, &proto);
        return proto;
    }

    template<typename T, typename S=std::string>
    bool FromJSON(T& proto, const S &json) {
        auto ret = google::protobuf::util::JsonStringToMessage(json, &proto);
        return ret.ok();
    }

}
