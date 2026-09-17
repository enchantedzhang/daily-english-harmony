/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#ifndef BOOST_SS_STRING_UTIL_H
#define BOOST_SS_STRING_UTIL_H

#include <sstream>
#include <string>
#include <vector>

namespace ock {
namespace bss {
class StringUtil {
public:
    static std::vector<std::string> Split(const std::string &input, const std::string &delimiter)
    {
        std::vector<std::string> result;
        auto start = 0;
        auto end = input.find(delimiter);
        while (end != std::string::npos) {
            std::string token = input.substr(start, end - start);
            token = Trim(token);
            if (!token.empty()) {
                result.push_back(token);
            }
            start = static_cast<int32_t>(end + delimiter.length());
            end = input.find(delimiter, start);
        }

        std::string lastToken = Trim(input.substr(start));
        if (!lastToken.empty()) {
            result.emplace_back(lastToken);
        }

        return result;
    }

    static std::string Trim(const std::string &s)
    {
        auto first = s.find_first_not_of(' ');
        if (first == std::string::npos)
            return "";
        auto last = s.find_last_not_of(' ');
        return s.substr(first, last - first + 1);
    }

    // 将 std::vector<std::string> 转换为 "item1,item2,item3"
    static inline std::string MergeVectorToString(const std::vector<std::string> &stringArray,
                                                  uint32_t vectorMaxLength = UINT32_MAX)
    {
        std::ostringstream oss;
        uint32_t index = 0;
        for (const auto &stringItem : stringArray) {
            oss << stringItem << ",";
            if (index++ > vectorMaxLength) {
                break;
            }
        }
        std::string outputStr = oss.str();
        if (!outputStr.empty() && outputStr.back() == ',') {
            outputStr.pop_back();
        }
        return outputStr;
    }

    // 将字符串 "item1,item2,item3" 转换为 std::vector<std::string>
    static inline void SplitStringToVector(const std::string &str, std::vector<std::string> &strVecter,
                                           uint32_t vectorMaxLength = UINT32_MAX)
    {
        std::stringstream ss(str);
        std::string token;
        uint32_t index = 0;
        while (std::getline(ss, token, ',')) {
            strVecter.emplace_back(token);
            if (index++ > vectorMaxLength) {
                break;
            }
        }
    }
};
}  // namespace bss
}  // namespace ock

#endif  // BOOST_SS_STRING_UTIL_H
