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

#ifndef BOOST_SS_SNAPSHOT_OPERATOR_COORDINATOR_H
#define BOOST_SS_SNAPSHOT_OPERATOR_COORDINATOR_H

#include <map>

#include "abstract_snapshot_operator.h"

namespace ock {
namespace bss {

class SnapshotOperatorCoordinator : public Referable {
public:
    SnapshotOperatorCoordinator() = default;

    ~SnapshotOperatorCoordinator() override = default;

    virtual BResult Start()
    {
        return BSS_ERR;
    }

    virtual void RegisterSnapshotOperator(const AbstractSnapshotOperatorRef &snapshotOperator)
    {
    }

    virtual void UnregisterSnapshotOperator(const AbstractSnapshotOperatorRef &snapshotOperator)
    {
    }

    virtual std::map<uint32_t, AbstractSnapshotOperatorRef> GetRegisterSnapshotOperator()
    {
        return {};
    }

    virtual uint32_t AllocateOperatorId()
    {
        return 0;
    }

    virtual uint64_t GetSnapshotId()
    {
        return 0;
    }

    virtual bool IsLocalSnapshot()
    {
        return false;
    }

    virtual bool IsSavepoint()
    {
        return false;
    }

    virtual PathRef GetLocalSnapshotPath()
    {
        return nullptr;
    }

    virtual BResult AcknowledgeAsyncSnapshot()
    {
        return BSS_OK;
    }

    virtual void FailSnapshot()
    {
    }

    virtual void Cancel()
    {
    }
};
using SnapshotOperatorCoordinatorRef = Ref<SnapshotOperatorCoordinator>;

}  // namespace bss
}  // namespace ock
#endif  // BOOST_SS_SNAPSHOT_OPERATOR_COORDINATOR_H