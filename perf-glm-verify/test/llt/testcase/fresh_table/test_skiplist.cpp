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

#include "test_skiplist.h"

void TestSkiplist::SetUp()
{
    uint32_t capacity = 1024 * 1024 * 8;
    void *addr = malloc(capacity);
    ASSERT_TRUE(addr != nullptr);
    memoryPool = MakeRef<FixedSizeMemoryPool>(reinterpret_cast<uint8_t *>(addr), capacity, true);
}

void TestSkiplist::TearDown()
{
}

TEST_F(TestSkiplist, test_basic)
{
    IntComparator intComp;
    SkipList<int, IntComparator> intSkipList(intComp, memoryPool, 1);
    ASSERT_EQ(intSkipList.Initialize(), BSS_OK);
    std::vector<int> keys = { 10, 5, 15, 3, 7, 12, 17, 4, 2, 1, 6, 8, 11, 13, 16, 14 };
    for (const auto &item : keys) {
        ASSERT_EQ(intSkipList.Put(item), BSS_OK);
    }
    ASSERT_FALSE(intSkipList.Empty());
    ASSERT_EQ(keys.size(), intSkipList.Size());
    ASSERT_TRUE(intSkipList.Contains(5));
    ASSERT_FALSE(intSkipList.Contains(20));
    int firstKey = 0;
    int lastKey = 0;
    intSkipList.First(firstKey);
    ASSERT_EQ(firstKey, 1);
    intSkipList.Last(lastKey);
    ASSERT_EQ(lastKey, 17);

    // 迭代遍历
    auto it = intSkipList.NewIterator();
    std::sort(keys.begin(), keys.end(), [](int a, int b) { return a < b; });
    uint32_t index = 0;
    while (it.HasNext()) {
        auto next = it.Next();
        ASSERT_EQ(next, keys[index]);
        index++;
    }
}

TEST_F(TestSkiplist, test_string_key)
{
    StringComparator strComp;
    SkipList<std::string, StringComparator> strSkipList(strComp, memoryPool, 1);
    ASSERT_EQ(strSkipList.Initialize(), BSS_OK);
    std::vector<std::string> keys = { "apple", "banana", "cherry" };
    for (const auto &item : keys) {
        strSkipList.Put(item);
    }
    ASSERT_TRUE(strSkipList.Contains("banana"));
}
