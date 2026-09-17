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

#include "test_executor.h"
using namespace ock::bss;

ExecutorServicePtr TestExecutor::mService = nullptr;

class TestProcessor : public ock::bss::Runnable {
public:
    TestProcessor() = default;
    TestProcessor(int a, int b) : a(a), b(b)
    {
    }
    ~TestProcessor() override = default;
    void Run() override
    {
        mSum = a + b;
    }

    int GetSum()
    {
        return mSum;
    }

private:
    int a = 0;
    int b = 0;
    int mSum = 0;
};
using TestProcessorPtr = std::shared_ptr<TestProcessor>;

void TestExecutor::SetUp() const
{
}

void TestExecutor::TearDown() const
{
}

void TestExecutor::SetUpTestCase()
{
    mService = std::make_shared<ExecutorService>(1, 100);
    ASSERT_TRUE(mService != nullptr);
    mService->SetThreadName("test");
}

void TestExecutor::TearDownTestCase()
{
    if (mService != nullptr) {
        mService->Stop();
        mService = nullptr;
    }
}

TEST_F(TestExecutor, TestTestExecutorCreate)
{
    ExecutorServicePtr service = ExecutorService::Create(102, 10000);
    ASSERT_EQ(service == nullptr, true);
}

TEST_F(TestExecutor, TestTestExecutorStart)
{
    ASSERT_TRUE(mService != nullptr);
    bool start = mService->Start();
    ASSERT_EQ(start, true);
    start = mService->Start();
    ASSERT_EQ(start, false);
}

TEST_F(TestExecutor, TestTestExecutorExecute)
{
    int a = 100;
    int b = 24;
    TestProcessorPtr processor = std::make_shared<TestProcessor>(a, b);
    ASSERT_TRUE(processor != nullptr);
    ASSERT_TRUE(mService != nullptr);
    RunnablePtr task = processor;
    ASSERT_TRUE(task != nullptr);
    bool flag = mService->Execute(task);
    ASSERT_EQ(flag, true);
    usleep(300);
    ASSERT_EQ(processor->GetSum(), 124);
}