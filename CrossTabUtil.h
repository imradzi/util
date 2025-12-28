#pragma once
#include "boost/tokenizer.hpp"
#include "boost/algorithm/string.hpp"
#include <vector>
struct CrossTabEntryDef {
    std::string title;
    std::vector<std::string> filterList;
    int index {-1};

public:
    bool FindPartialString(const std::string& key) const {
        if (key.empty()) return false;
        for (auto const& x : filterList) {
            if (boost::icontains(key, x)) return true;
        }
        return false;
    }

    std::string toString() const {
        std::string res = title + "~";
        std::string delim;
        for (const auto& v : filterList) {
            res += delim + v;
            delim = ",";
        }
        return res;
    }

    static std::string toString(const std::vector<CrossTabEntryDef>& list) {
        std::string res;
        std::string delim;
        for (const auto& v : list) {
            res += delim + v.toString();
            delim = ";";
        }
        return res;
    }

    // format of the string = title~f1,f2,f3;coltitle2~f21,f22,f23,f24;coltitle3~f44
    static std::vector<CrossTabEntryDef> fromString(const std::string& v) {
        boost::tokenizer<boost::char_separator<char>> tok(v, boost::char_separator<char>(";", "", boost::drop_empty_tokens));
        std::vector<CrossTabEntryDef> result;
        int idx = 0;
        for (auto const& set : tok) {
            boost::tokenizer<boost::char_separator<char>> tokLine(set, boost::char_separator<char>("~", "", boost::keep_empty_tokens));
            CrossTabEntryDef rec;
            auto iter = tokLine.begin();
            rec.title = iter != tokLine.end() ? *iter : "";
            iter++;
            if (iter != tokLine.end()) {
                rec.index = idx++;
                boost::tokenizer<boost::char_separator<char>> tokList(*iter, boost::char_separator<char>(",", "", boost::drop_empty_tokens));
                for (auto const& t : tokList) {
                    const auto& v = boost::trim_copy(t);
                    if (!v.empty()) rec.filterList.push_back(v);
                }
            }
            result.push_back(rec);
        }
        return result;
    }
};
