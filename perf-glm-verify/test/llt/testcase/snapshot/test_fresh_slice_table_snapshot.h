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

#ifndef BOOST_SS_TEST_FRESH_SLICE_TABLE_SNAPSHOT_H
#define BOOST_SS_TEST_FRESH_SLICE_TABLE_SNAPSHOT_H

#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cerrno>
#include <iostream>
#include <random>
#include <string>

#include "common/bss_log.h"
#include "fresh_table/fresh_table.h"
#include "gtest/gtest.h"
#include "include/boost_state_db.h"
#include "slice_table/slice_table.h"
#include "slice_table/test_slice_table_manager.h"
#include "test_utils.h"

using namespace ock::bss;
class TestFreshSliceTableSnapshot : public testing::Test {
public:
    TestFreshSliceTableSnapshot() = default;
    ~TestFreshSliceTableSnapshot() override = default;
    // TestCase only enter once
    static void SetUpTestCase();
    static void TearDownTestCase();
    // every TEST_F macro will enter one
    void SetUp() override;
    void TearDown() override;
    static BoostStateDBImpl *mBoostStateDB;
    static ConfigRef mConfig;
    static MemManagerRef mMemManager;
    static uint64_t mCheckpointId;
    static std::vector<QueryKey> originKeyList;
    static std::vector<Value> originValueList;

    static inline void PutDataToFreshSliceTable(std::vector<QueryKey> &originKeyList,
                                                std::vector<Value> &originValueList, uint32_t keyCount)
    {
        uint32_t index = 0;
        uint64_t num = 0;
        //  写keyCount条数据
        while (index < keyCount) {
            uint8_t *keyData;
            uint32_t keyLength = NO_1024;
            uint8_t *valueData;
            uint32_t valueLength = NO_1024;

            // 1、生成key
            TestSliceTableManager::GenerateData(keyData, keyLength);
            uint32_t hashCode = test::HashForTest(keyData, keyLength);
            BinaryData priKey(keyData, keyLength);

            // 2、生成value
            TestSliceTableManager::GenerateData(valueData, valueLength);
            BinaryData value(valueData, valueLength);
            uint16_t stateId = VALUE << NO_13;
            QueryKey queryKey(stateId, hashCode, priKey);
            Value putVal;
            putVal.Init(ValueType::PUT, valueLength, valueData, num++);

            // 3、数据暂存到对应的kv list，用于后续查找比较
            originKeyList.push_back(queryKey);
            originValueList.push_back(putVal);
            TestFreshSliceTableSnapshot::mBoostStateDB->GetFreshTable()->Put(queryKey, putVal);
            index++;
        }
    }

    static bool DeleteDirectoryContents(const std::string &dirPath)
    {
        DIR *dir = opendir(dirPath.c_str());
        if (!dir) {
            LOG_ERROR("无法打开目录: " << dirPath);
            return false;
        }

        struct dirent *entry;
        while ((entry = readdir(dir)) != nullptr) {
            std::string fileName = entry->d_name;
            if (fileName == "." || fileName == "..") {
                continue;  // 跳过当前目录和父目录
            }

            std::string filePath = dirPath + "/" + fileName;
            struct stat fileStat;
            if (lstat(filePath.c_str(), &fileStat) == -1) {
                LOG_ERROR("无法获取文件信息: " << filePath);
                continue;
            }

            if (S_ISREG(fileStat.st_mode)) {  // 判断是否为普通文件
                if (remove(filePath.c_str()) == 0) {
                    LOG_INFO("删除文件: " << filePath);
                } else {
                    LOG_ERROR("删除文件失败: " << filePath);
                }
            } else if (S_ISDIR(fileStat.st_mode)) {  // 子目录
                if (!DeleteDirectoryContents(filePath)) {
                    LOG_ERROR("删除目录失败: " << filePath);
                    continue;
                }
                // 删除空目录
                if (rmdir(filePath.c_str()) != 0) {
                    LOG_ERROR("删除目录失败: " << filePath);
                    continue;
                } else {
                    LOG_INFO("删除空目录: " << filePath);
                }
            } else if (S_ISLNK(fileStat.st_mode)) {  // 符号链接
                if (remove(filePath.c_str()) != 0) {
                    LOG_ERROR("删除符号链接失败: " << filePath);
                } else {
                    LOG_INFO("删除符号链接: " << filePath);
                }
            }
        }
        closedir(dir);
        return true;
    }

    static bool IsAllSliceEvicted()
    {
        uint32_t bucketNum = mBoostStateDB->GetSliceTable()->GetSliceBucketIndex()->GetIndexCapacity();
        for (uint32_t i = 0; i < bucketNum; ++i) {
            auto chain = mBoostStateDB->GetSliceTable()->GetSliceBucketIndex()->GetLogicChainedSlice(i);
            if (chain->IsEmpty()) {
                continue;
            }
            if (chain->GetSliceAddress(i) == nullptr || chain->GetSliceAddress(i)->IsEvicted()) {
                continue;
            } else {
                return false;
            }
        }
        return true;
    }

    static void TestAllKeysFindInSliceAndLsm()
    {
        for (auto &key : originKeyList) {
            // 进行查询，lsm store 中是否有对应的key，这里能保证slice中没有数据，所以get找到的值一定在lsm store中
            Value valueInLsmStore{};
            bool ret = mBoostStateDB->GetSliceTable()->Get(key, valueInLsmStore);
            ASSERT_TRUE(ret);
        }
    }

    // 线程1的任务：执行 TryEvict
    static void EvictTask()
    {
        while (!IsAllSliceEvicted()) {
            LOG_INFO("EvictTask");
            mBoostStateDB->GetSliceTable()->TryCurrentDBEvict(0, false, true);
        }
    }

    // 线程2的任务：执行 TestAllKeysFindInSliceAndLsm
    static void TestTask()
    {
        while (!IsAllSliceEvicted()) {
            LOG_INFO("GetTask");
            TestAllKeysFindInSliceAndLsm();
        }
    }
};

#endif  // BOOST_SS_TEST_FRESH_SLICE_TABLE_SNAPSHOT_H
