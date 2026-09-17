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

#include "binary/fresh_binary.h"
#include "gtest/gtest.h"
#include "test_utils.h"

using namespace ock::bss;

TEST(TestFreshValueNode, TestVisit)
{
    ConfigRef config = std::make_shared<Config>();
    auto memManager = std::make_shared<MemManager>(AllocatorType::DIRECT);
    memManager->Initialize(config);
    uintptr_t buf;
    uint32_t capacity = NO_1024;
    memManager->GetMemoryDirect(MemoryType::FRESH_TABLE, capacity, buf);
    auto bufSegment = reinterpret_cast<uint8_t *>(buf);
    MemorySegmentRef memorySegment = MakeRef<MemorySegment>(capacity, bufSegment, memManager);

    uint32_t valLen = sizeof(FreshValueNode);
    FreshValueNode v1{ NodeType::COMPOSITE, valLen, ValueType::APPEND, 1 };
    FreshValueNode v2{ NodeType::COMPOSITE, valLen, ValueType::APPEND, 2 };
    FreshValueNode v3{ NodeType::SERIALIZED, valLen, ValueType::APPEND, 3 };
    uint32_t nodeLen = valLen + NO_5;
    auto nodeDistance = nodeLen + NO_4;
    memcpy_s(bufSegment, capacity, &v2, sizeof(FreshValueNode));
    memcpy_s(bufSegment + nodeDistance, capacity - nodeDistance, &v1, sizeof(FreshValueNode));
    auto pos = nodeDistance * 2;
    memcpy_s(bufSegment + pos, capacity - pos, &v3, sizeof(FreshValueNode));
    *reinterpret_cast<uint32_t *>(bufSegment + nodeDistance + nodeLen) = 0;
    *reinterpret_cast<uint32_t *>(bufSegment + nodeLen) = nodeDistance * 2;

    std::vector<FreshValueNodePtr> targetNodes{ &v1, &v2, &v3 };
    uint32_t visitIdx = 0;
    auto node = FreshValueNode::FromBuffer(bufSegment + nodeDistance);
    bool passed = true;
    node->VisitAsList(*memorySegment, [&](FreshValueNode &curNode) -> bool {
        passed &= (visitIdx < targetNodes.size());
        passed &= (targetNodes[visitIdx]->ValueSeqId() == curNode.ValueSeqId());
        visitIdx++;
        return true;
    });
    ASSERT_TRUE(passed);
}
