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

#include "test_table.h"

namespace ock {
namespace bss {
namespace test {
namespace test_table {

std::string GetCurrentWorkingDirectory()
{
    std::cout << "inside GetCurrentWorkingDirectory" << std::endl;
    char buffer[1024];
    if (getcwd(buffer, sizeof(buffer)) != nullptr) {
        return std::string(buffer);
    }
    return "";
}

void CleanupSstFiles()
{
    std::cout << "inside CleanupSstFiles" << std::endl;
    std::string currentDir = GetCurrentWorkingDirectory();
    DIR *dirp = opendir(currentDir.c_str());
    if (dirp) {
        struct dirent *entry;
        while ((entry = readdir(dirp)) != nullptr) {
            std::string fileName = entry->d_name;
            if (fileName.find("sst") != std::string::npos) {
                std::string filePath = currentDir + "/" + fileName;
                remove(filePath.c_str());
            }
        }
        closedir(dirp);
    }
}

TableRef CreateFromDB(KeyedStateType keyedStateType, std::string tableName, BoostStateDB *db)
{
    std::cout << "inside create from db" << std::endl;
    return db->GetTableOrCreate(keyedStateType, tableName, -1L);
}

void TestTable::SetUp()
{
    std::cout << "inside setUp" << std::endl;
    ConfigRef config = std::make_shared<Config>();
    config->mMemorySegmentSize = IO_SIZE_16M;
    config->SetEvictMinSize(IO_SIZE_16M);
    config->Init(NO_0, NO_127, NO_128);
    config->SetFileStoreL0NumTrigger(NO_2);
    config->SetLsmStoreCompactionSwitch(true);

    mDB = BoostStateDBFactory::Create();
    mDB->Open(config);

    mDB1 = BoostStateDBFactory::Create();
    mDB1->Open(config);

    kVTable = std::dynamic_pointer_cast<KVTable>(CreateFromDB(KeyedStateType::VALUE, "kVTable", mDB));
    nsKVTable = std::dynamic_pointer_cast<NsKVTable>(CreateFromDB(KeyedStateType::SUBKEYED_VALUE, "nsKVTable", mDB));
    kMapTable = std::dynamic_pointer_cast<KMapTable>(CreateFromDB(KeyedStateType::MAP, "kMapTable", mDB));
    nsKMapTable =
        std::dynamic_pointer_cast<NsKMapTable>(CreateFromDB(KeyedStateType::SUBKEYED_MAP, "nsKMapTable", mDB));
    kListTable = std::dynamic_pointer_cast<KListTable>(CreateFromDB(KeyedStateType::LIST, "kListTable", mDB));
    nsKListTable =
        std::dynamic_pointer_cast<NsKListTable>(CreateFromDB(KeyedStateType::SUBKEYED_LIST, "nsKListTable", mDB));

    CleanupSstFiles();
}

void TestTable::SetUpTestCase()
{
}

void TestTable::TearDown()
{
    std::cout << "inside TearDown" << std::endl;
    kVTable.reset();
    nsKVTable.reset();
    kMapTable.reset();
    nsKMapTable.reset();
    kListTable.reset();
    nsKListTable.reset();

    sourceKey.clear();
    sourceSubKey.clear();
    sourceNS.clear();

    mDB->Close();
    delete mDB;
    if (mDB1 != nullptr) {
        mDB1->Close();
        delete mDB1;
    }
    CleanupSstFiles();

    LOG_INFO("TestTable::TearDownTestCase finish.");
}

TEST_F(TestTable, test_kv_table_put_ok)
{
    std::cout << "inside test_kv_table_put_ok" << std::endl;
    DT_Enable_Leak_Check(0, 0);
    DT_Set_Running_Time_Second(10800);
    int times = 30000000;
    int count = 1;

    DT_FUZZ_START(0, times, "test_kv_table_put_ok", 0)
    {
        PutKv(IO_SIZE_16M);
        count++;
    }
    DT_FUZZ_END()
    std::cout << "test_kv_table_put_ok times: " << (count - 1) << std::endl;
}

TEST_F(TestTable, test_kv_table_get_ok)
{
    std::cout << "inside test_kv_table_get_ok" << std::endl;
    DT_Enable_Leak_Check(0, 0);
    DT_Set_Running_Time_Second(10800);
    int putTimes = 30000000;
    int getTimes = 30000000;
    int count = 1;
    DT_FUZZ_START(0, putTimes, "put", 0)
    {
        PutKv(IO_SIZE_16M);
        count++;
    }
    DT_FUZZ_END()
    std::cout << "put times: " << (count - 1) << std::endl;

    count = 1;
    DT_FUZZ_START(0, getTimes, "get", 0)
    {
        GetKv(IO_SIZE_16M);
        count++;
    }
    DT_FUZZ_END()
    std::cout << "get times: " << (count - 1) << std::endl;
}

TEST_F(TestTable, test_kv_table_contain_ok)
{
    std::cout << "inside test_kv_table_contain_ok" << std::endl;
    DT_Enable_Leak_Check(0, 0);
    DT_Set_Running_Time_Second(10800);
    int putTimes = 30000000;
    int containTimes = 30000000;
    int count = 1;
    DT_FUZZ_START(0, putTimes, "put", 0)
    {
        PutKv(IO_SIZE_16M);
        count++;
    }
    DT_FUZZ_END()
    std::cout << "put times: " << (count - 1) << std::endl;

    count = 1;
    DT_FUZZ_START(0, containTimes, "contain", 0)
    {
        ContainKv(IO_SIZE_16M);
        count++;
    }
    DT_FUZZ_END()
    std::cout << "contain times: " << (count - 1) << std::endl;
}

TEST_F(TestTable, test_kv_table_remove_ok)
{
    std::cout << "inside test_kv_table_remove_ok" << std::endl;
    DT_Enable_Leak_Check(0, 0);
    DT_Set_Running_Time_Second(10800);
    int putTimes = 30000000;
    int removeTimes = 30000000;
    int count = 1;
    DT_FUZZ_START(0, putTimes, "put", 0)
    {
        PutKv(IO_SIZE_16M);
        count++;
    }
    DT_FUZZ_END()
    std::cout << "put times: " << (count - 1) << std::endl;

    count = 1;
    DT_FUZZ_START(0, removeTimes, "remove", 0)
    {
        RemoveKv(IO_SIZE_16M);
        count++;
    }
    DT_FUZZ_END()
    std::cout << "remove times: " << (count - 1) << std::endl;
}

TEST_F(TestTable, test_klist_table_put_ok)
{
    std::cout << "test_klist_table_put_ok" << std::endl;
    DT_Enable_Leak_Check(0, 0);
    DT_Set_Running_Time_Second(10800);
    int times = 30000000;
    int count = 1;

    DT_FUZZ_START(0, times, "test_klist_table_put_ok", 0)
    {
        PutKList(IO_SIZE_16M);
        count++;
    }
    DT_FUZZ_END()
    std::cout << "test_klist_table_put_ok times: " << (count - 1) << std::endl;
}

TEST_F(TestTable, test_klist_table_get_ok)
{
    std::cout << "inside test_klist_table_get_ok" << std::endl;
    DT_Enable_Leak_Check(0, 0);
    DT_Set_Running_Time_Second(10800);
    int putTimes = 30000000;
    int getTimes = 30000000;
    int count = 1;

    DT_FUZZ_START(0, putTimes, "test_klist_table_get_ok--put", 0)
    {
        PutKList(IO_SIZE_16M);
        count++;
    }
    DT_FUZZ_END()
    std::cout << "test_klist_table_get_ok--put getTimes: " << (count - 1) << std::endl;

    count = 1;
    DT_FUZZ_START(0, getTimes, "test_klist_table_get_ok--get", 0)
    {
        GetKList(IO_SIZE_16M);
        count++;
    }
    DT_FUZZ_END()
    std::cout << "test_klist_table_get_ok--get getTimes: " << (count - 1) << std::endl;
}

TEST_F(TestTable, test_klist_table_contain_ok)
{
    std::cout << "inside test_klist_table_contain_ok" << std::endl;
    DT_Enable_Leak_Check(0, 0);
    DT_Set_Running_Time_Second(10800);
    int putTimes = 30000000;
    int containTimes = 30000000;
    int count = 1;

    DT_FUZZ_START(0, putTimes, "test_klist_table_contain_ok--put", 0)
    {
        PutKList(IO_SIZE_16M);
        count++;
    }
    DT_FUZZ_END()
    std::cout << "test_klist_table_contain_ok--put containTimes: " << (count - 1) << std::endl;

    count = 1;
    DT_FUZZ_START(0, containTimes, "test_klist_table_contain_ok--contain", 0)
    {
        ContainKList(IO_SIZE_16M);
        count++;
    }
    DT_FUZZ_END()
    std::cout << "test_klist_table_contain_ok--contain containTimes: " << (count - 1) << std::endl;
}

TEST_F(TestTable, test_klist_table_add_ok)
{
    std::cout << "inside test_klist_table_add_ok" << std::endl;
    DT_Enable_Leak_Check(0, 0);
    DT_Set_Running_Time_Second(10800);
    int times = 30000000;
    int count = 1;

    DT_FUZZ_START(0, times, "test_klist_table_add_ok--contain", 0)
    {
        AddKList(IO_SIZE_16M);
        count++;
    }
    DT_FUZZ_END()
    std::cout << "test_klist_table_add_ok--contain times: " << (count - 1) << std::endl;
}

TEST_F(TestTable, test_klist_table_remove_ok)
{
    std::cout << "inside test_klist_table_remove_ok" << std::endl;
    DT_Enable_Leak_Check(0, 0);
    DT_Set_Running_Time_Second(10800);
    int putTimes = 30000000;
    int removeTimes = 30000000;
    int count = 1;

    DT_FUZZ_START(0, putTimes, "test_klist_table_remove_ok--put", 0)
    {
        PutKList(IO_SIZE_16M);
        count++;
    }
    DT_FUZZ_END()
    std::cout << "test_klist_table_remove_ok--put removeTimes: " << (count - 1) << std::endl;

    count = 1;
    DT_FUZZ_START(0, removeTimes, "test_klist_table_remove_ok--remove", 0)
    {
        RemoveKList(IO_SIZE_16M);
        count++;
    }
    DT_FUZZ_END()
    std::cout << "test_klist_table_remove_ok--remove times: " << (count - 1) << std::endl;
}

TEST_F(TestTable, test_klist_table_clean_resource_ok)
{
    std::cout << "inside test_klist_table_clean_resource_ok" << std::endl;
    DT_Enable_Leak_Check(0, 0);
    DT_Set_Running_Time_Second(10800);
    int times = 30000000;
    int count = 1;

    DT_FUZZ_START(0, times, "test_klist_table_clean_resource_ok--clean", 0)
    {
        int index = 0;
        uint32_t resId = *(uint32_t *)DT_SetGetU32(&g_Element[index++], 0x1);
        kListTable->CleanResource(resId);
        count++;
    }
    DT_FUZZ_END()
    std::cout << "test_klist_table_clean_resource_ok--clean times: " << (count - 1) << std::endl;
}

TEST_F(TestTable, test_kmap_table_put_ok)
{
    std::cout << "inside test_kmap_table_put_ok" << std::endl;
    DT_Enable_Leak_Check(0, 0);
    DT_Set_Running_Time_Second(10800);
    int times = 30000000;
    int count = 1;

    DT_FUZZ_START(0, times, "test_kmap_table_put_ok", 0)
    {
        AddKMap(IO_SIZE_16M);
        count++;
    }
    DT_FUZZ_END()
    std::cout << "test_kmap_table_put_ok times: " << (count - 1) << std::endl;
}

TEST_F(TestTable, test_kmap_table_contain_ok)
{
    std::cout << "inside test_kmap_table_contain_ok" << std::endl;
    DT_Enable_Leak_Check(0, 0);
    DT_Set_Running_Time_Second(10800);
    int putTimes = 30000000;
    int containTimes = 30000000;
    int count = 1;

    DT_FUZZ_START(0, putTimes, "test_kmap_table_contain_ok--put", 0)
    {
        AddKMap(IO_SIZE_16M);
        count++;
    }
    DT_FUZZ_END()
    std::cout << "test_kmap_table_contain_ok--put removeTimes: " << (count - 1) << std::endl;

    count = 1;
    DT_FUZZ_START(0, containTimes, "test_kmap_table_contain_ok--contain", 0)
    {
        ContainKMap(IO_SIZE_16M);
        count++;
    }
    DT_FUZZ_END()
    std::cout << "test_kmap_table_contain_ok--contain times: " << (count - 1) << std::endl;
}

TEST_F(TestTable, test_kmap_table_contain1_ok)
{
    std::cout << "inside test_kmap_table_contain1_ok" << std::endl;
    DT_Enable_Leak_Check(0, 0);
    DT_Set_Running_Time_Second(10800);
    int putTimes = 30000000;
    int containTimes = 30000000;
    DT_FUZZ_START(0, putTimes, "test_kmap_table_contain1_ok--put", 0)
    {
        AddKMap(IO_SIZE_16M);
    }
    DT_FUZZ_END()

    DT_FUZZ_START(0, containTimes, "test_kmap_table_contain1_ok--contain", 0)
    {
        ContainKMap1(IO_SIZE_16M);
    }
    DT_FUZZ_END()
}

TEST_F(TestTable, test_kmap_table_get_ok)
{
    std::cout << "inside test_kmap_table_get_ok" << std::endl;
    DT_Enable_Leak_Check(0, 0);
    DT_Set_Running_Time_Second(10800);
    int putTimes = 30000000;
    int getTimes = 30000000;
    DT_FUZZ_START(0, putTimes, "test_kmap_table_get_ok--put", 0)
    {
        AddKMap(IO_SIZE_16M);
    }
    DT_FUZZ_END()

    DT_FUZZ_START(0, getTimes, "test_kmap_table_get_ok--get", 0)
    {
        GetKMap(IO_SIZE_16M);
    }
    DT_FUZZ_END()
}

TEST_F(TestTable, test_kmap_table_remove_ok)
{
    std::cout << "inside test_kmap_table_remove_ok" << std::endl;
    DT_Enable_Leak_Check(0, 0);
    DT_Set_Running_Time_Second(10800);
    int putTimes = 30000000;
    int removeTimes = 30000000;
    DT_FUZZ_START(0, putTimes, "test_kmap_table_remove_ok--put", 0)
    {
        AddKMap(IO_SIZE_16M);
    }
    DT_FUZZ_END()

    DT_FUZZ_START(0, removeTimes, "test_kmap_table_remove_ok--remove", 0)
    {
        RemoveKMap(IO_SIZE_16M);
    }
    DT_FUZZ_END()
}

TEST_F(TestTable, test_kmap_table_remove_all_ok)
{
    std::cout << "inside test_kmap_table_remove_all_ok" << std::endl;
    DT_Enable_Leak_Check(0, 0);
    DT_Set_Running_Time_Second(10800);
    int putTimes = 30000000;
    int removeAllTimes = 30000000;
    DT_FUZZ_START(0, putTimes, "test_kmap_table_remove_all_ok--put", 0)
    {
        AddKMap(IO_SIZE_16M);
    }
    DT_FUZZ_END()

    DT_FUZZ_START(0, removeAllTimes, "test_kmap_table_remove_all_ok--remove-all", 0)
    {
        RemoveAllKMap(IO_SIZE_16M);
    }
    DT_FUZZ_END()
}

TEST_F(TestTable, test_open_table_ok)
{
    std::cout << "inside test_open_table_ok" << std::endl;
    DT_Enable_Leak_Check(0, 0);
    DT_Set_Running_Time_Second(10800);
    int times = 30000000;
    DT_FUZZ_START(0, times, "test_open_table_ok--put", 0)
    {
        int index = 0;
        auto keyedStateType = *(uint32_t *)DT_SetGetNumberRange(&g_Element[index++], 0x0, 0x0, 0x5);
        const std::string tableName = DT_SetGetString(&g_Element[index++], 31, 0, "test_open_table_ok--table-name");
        uint64_t ttl = *(uint64_t *)DT_SetGetU64(&g_Element[index++], 0x0);
        mDB->GetTableOrCreate(static_cast<KeyedStateType>(keyedStateType), tableName, ttl);
    }
    DT_FUZZ_END()
}

TEST_F(TestTable, test_update_ttl_config_ok)
{
    std::cout << "inside test_update_ttl_config_ok" << std::endl;
    DT_Enable_Leak_Check(0, 0);
    DT_Set_Running_Time_Second(10800);
    int times = 30000000;
    DT_FUZZ_START(0, times, "test_update_ttl_config_ok", 0)
    {
        UpdateTtlConfig();
    }
    DT_FUZZ_END()
}

TEST_F(TestIterator, test_open_key_iterator_ok)
{
    std::cout << "inside test_open_key_iterator_ok" << std::endl;
    DT_Enable_Leak_Check(0, 0);
    DT_Set_Running_Time_Second(10800);
    int times = 30000000;
    DT_FUZZ_START(0, times, "test_open_key_iterator_ok", 0)
    {
        OpenKeysIterator();
    }
    DT_FUZZ_END()
}

TEST_F(TestIterator, test_open_sub_key_iterator_ok)
{
    std::cout << "inside test_open_sub_key_iterator_ok" << std::endl;
    DT_Enable_Leak_Check(0, 0);
    DT_Set_Running_Time_Second(10800);
    int times = 30000000;
    DT_FUZZ_START(0, times, "test_open_sub_key_iterator_ok", 0)
    {
        OpenSubKeyIterator();
    }
    DT_FUZZ_END()
}

TEST_F(TestIterator, test_open_sub_entry_iterator_ok)
{
    std::cout << "inside test_open_sub_entry_iterator_ok" << std::endl;
    DT_Enable_Leak_Check(0, 0);
    DT_Set_Running_Time_Second(10800);
    int times = 30000000;
    DT_FUZZ_START(0, times, "test_open_sub_entry_iterator_ok", 0)
    {
        OpenSubEntryIterator();
    }
    DT_FUZZ_END()
}

TEST_F(TestTable, test_boost_state_db_open)
{
    std::cout << "inside test_boost_state_db_open" << std::endl;
    DT_Enable_Leak_Check(0, 0);
    DT_Set_Running_Time_Second(10800);
    int times = 30000000;

    DT_FUZZ_START(0, times, "test_boost_state_db_open", 0)
    {
        std::string basePath = "/home/workspace/w30061906/self/boost_state_store/tmp/";
        BoostStateDB *dbforOpen = BoostStateDBFactory::Create();
        dbforOpen->Open(FuzzConfig(basePath));
        dbforOpen->Close();
        delete dbforOpen;
        dbforOpen = nullptr;
    }
    DT_FUZZ_END()
}

TEST_F(TestTable, test_boost_state_db_change_heap_available_size)
{
    std::cout << "inside test_boost_state_db_change_heap_available_size" << std::endl;
    DT_Enable_Leak_Check(0, 0);
    DT_Set_Running_Time_Second(10800);
    int times = 30000000;

    DT_FUZZ_START(0, times, "test_boost_state_db_change_heap_available_size", 0)
    {
        int index = 0;
        uint64_t newSize = *(uint64_t *)DT_SetGetU64(&g_Element[index++], 0x0);
        BoostStateDB::ChangeHeapAvailableSize(newSize);
    }
    DT_FUZZ_END()
}

TEST_F(TestTable, test_boost_state_db_notify_db_snapshot_abort)
{
    std::cout << "inside test_boost_state_db_notify_db_snapshot_abort" << std::endl;
    DT_Enable_Leak_Check(0, 0);
    DT_Set_Running_Time_Second(10800);
    int times = 30000000;

    DT_FUZZ_START(0, times, "test_boost_state_db_notify_db_snapshot_abort", 0)
    {
        int index = 0;
        uint64_t checkpointId = *(uint64_t *)DT_SetGetU64(&g_Element[index++], 0x0);
        mDB->NotifyDBSnapshotAbort(checkpointId);
    }
    DT_FUZZ_END()
}

TEST_F(TestTable, test_checkpoint_ok)
{
    std::cout << "inside test_checkpoint_ok" << std::endl;
    DT_Enable_Leak_Check(0, 0);
    DT_Set_Running_Time_Second(10800);
    int putTimes = 30000000;
    int checkpointTimes = 30000000;
    DT_FUZZ_START(0, putTimes, "test_checkpoint_not_ok--put", 0)
    {
        PutKv(IO_SIZE_4K);
        PutKList(IO_SIZE_4K);
        AddKMap(IO_SIZE_4K);
        AddNSKMap(IO_SIZE_4K);
    }
    DT_FUZZ_END()

    DT_FUZZ_START(0, checkpointTimes, "test_checkpoint_not_ok--checkpoint", 0)
    {
        std::string path = "/home/workspace/w30061906/self/boost_state_store/tmp/";
        mkdir(path.c_str(), mode_t(777));
        uint64_t checkpointId = SyncCheckpoint(path);
        AsyncCheckpoint(checkpointId);
        DeleteCpFile();
    }
    DT_FUZZ_END()
}

TEST_F(TestTable, test_checkpoint_not_ok)
{
    std::cout << "inside test_checkpoint_not_ok" << std::endl;
    DT_Enable_Leak_Check(0, 0);
    DT_Set_Running_Time_Second(10800);
    int putTimes = 30000000;
    int checkpointTimes = 30000000;
    DT_FUZZ_START(0, putTimes, "test_checkpoint_not_ok--put", 0)
    {
        PutKv(IO_SIZE_4K);
        PutKList(IO_SIZE_4K);
        AddKMap(IO_SIZE_4K);
        AddNSKMap(IO_SIZE_4K);
    }
    DT_FUZZ_END()

    DT_FUZZ_START(0, checkpointTimes, "test_checkpoint_not_ok--checkpoint", 0)
    {
        int index = 0;
        std::string path = "/home/workspace/w30061906/self/boost_state_store/tmp/";
        uint64_t checkpointId = SyncCheckpoint(path);
        uint64_t asyncCheckpointId = *(uint64_t *)DT_SetGetU64(&g_Element[index++], 0x0);
        AsyncCheckpoint(asyncCheckpointId);
        // must do this because memory is alloc in sync part and release in async part.
        mDB->CreateAsyncCheckpoint(checkpointId, false);
        DeleteCpFile();
    }
    DT_FUZZ_END()
}

TEST_F(TestTable, test_restore_not_ok)
{
    std::vector<std::string> metaPaths;
    std::unordered_map<std::string, std::string> lazyPathMapping;
    std::cout << "inside test_restore_not_ok" << std::endl;
    DT_Enable_Leak_Check(0, 0);
    DT_Set_Running_Time_Second(10800);
    int times = 30000000;
    DT_FUZZ_START(0, times, "test_restore_not_ok--meta", 0)
    {
        bool isLazy = GetRestoreParam(metaPaths, lazyPathMapping);
        mDB->Restore(metaPaths, lazyPathMapping, isLazy);
    }
    DT_FUZZ_END()
}

}  // namespace test_table
}  // namespace test
}  // namespace bss
}  // namespace ock
