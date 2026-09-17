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

#include "binary/value/value.h"
#include "gtest/gtest.h"
#include "lsm_store/file/merging_iterator.h"

using namespace ock::bss;

namespace {
KeyValueRef MakeListKeyValue(uint8_t valueType, uint32_t valueLen, uint64_t seqId)
{
    static uint8_t keyData = 1;
    static uint8_t valueData = 1;
    auto keyValue = MakeRef<KeyValue>();
    uint16_t stateId = StateId::Of(NO_1, StateType::LIST);
    PriKeyNode priKeyNode(stateId, NO_1, &keyData, NO_1);
    keyValue->key.Init(priKeyNode, nullptr);
    keyValue->value.Init(valueType, valueLen, &valueData, seqId);
    return keyValue;
}

MergingIteratorRef MakeSectionMergingIterator(std::vector<KeyValueRef> &&keyValues)
{
    auto vectorIterator = std::make_shared<KeyValueVectorIterator>(std::move(keyValues));
    std::vector<KeyValueIteratorRef> iterators{ vectorIterator };
    return std::make_shared<MergingIterator>(iterators, nullptr, FileProcHolder::FILE_STORE_COMPACTION, true);
}
}  // namespace

TEST(TestValue, test_value_size)
{
    ASSERT_EQ(sizeof(Value), NO_40);
}

TEST(TestValue, test_large_list_put_discards_older_value_during_section_compaction)
{
    auto newerPut = MakeListKeyValue(ValueType::PUT, IO_SIZE_4M + NO_1, NO_2);
    auto olderPut = MakeListKeyValue(ValueType::PUT, IO_SIZE_4M + NO_1, NO_1);
    auto iterator = MakeSectionMergingIterator({ newerPut, olderPut });

    ASSERT_TRUE(iterator->HasNext());
    EXPECT_EQ(iterator->Next()->value.SeqId(), NO_2);
    EXPECT_FALSE(iterator->HasNext());
}

TEST(TestValue, test_large_list_append_remains_sectioned_during_compaction)
{
    auto newerAppend = MakeListKeyValue(ValueType::APPEND, IO_SIZE_4M + NO_1, NO_2);
    auto olderPut = MakeListKeyValue(ValueType::PUT, IO_SIZE_4M + NO_1, NO_1);
    auto iterator = MakeSectionMergingIterator({ newerAppend, olderPut });

    ASSERT_TRUE(iterator->HasNext());
    EXPECT_EQ(iterator->Next()->value.SeqId(), NO_2);
    ASSERT_TRUE(iterator->HasNext());
    EXPECT_EQ(iterator->Next()->value.SeqId(), NO_1);
    EXPECT_FALSE(iterator->HasNext());
}
