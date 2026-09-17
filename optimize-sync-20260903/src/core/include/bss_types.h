/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#ifndef BOOST_STATE_STORE_TYPES_H
#define BOOST_STATE_STORE_TYPES_H

#include <cstddef>
#include <cstdint>

namespace ock {
namespace bss {
enum class MemoryType : uint8_t {
    FILE_STORE = 0,
    FRESH_TABLE = 1,
    SLICE_TABLE = 2,
    SNAPSHOT = 3,
    RESTORE = 4,
    BORROW_HEAP = 5,
    MEM_TYPE_BUTT = 6,
};

enum class BlockType : uint8_t { FILTER = 0, INDEX = 1, DATA = 2, BLOB = 3, BUTT = 4 };

constexpr uint32_t BYTE4_MAX_SLOT_SIZE = 65535;
constexpr uint32_t VALUE_INDICATOR_OFFSET = 28;
constexpr uint32_t BLOB_FILE_MAGIC_NUM = 102030405;

// blob config
constexpr uint32_t BLOB_INDEX_ENTRY_STRUCT_SIZE = 20;
constexpr uint32_t BLOB_INDEX_ENTRY_BUFFER_OFFSET = 8;
constexpr uint32_t BLOB_INDEX_ENTRY_NUMS_OFFSET = 4;
constexpr uint32_t BLOB_DATA_BLOCK_HEADER_SIZE = 8;
constexpr uint32_t BLOB_DATA_BLOCK_META_STRUCT_SIZE = 24;
constexpr char BLOB_FILE_NAME_PREFIX[] = "blob_file_";
constexpr uint32_t TOMBSTONE_MEMTABLE_SIZE = 200000;

constexpr uint64_t MAX_TIMESTAMP = 140737488355327L;
constexpr uint64_t CURRENTTS_WARNING = 58981;  // 65535 * 0.9
constexpr uint16_t NO_MAX_VALUE16 = 0xFFFFUL;
constexpr uint32_t NO_MAX_VALUE32 = 0xFFFFFFFFUL;
constexpr uint64_t NO_MAX_VALUE64 = 0xFFFFFFFFFFFFFFFFULL;
constexpr int32_t MAX_PARALLELISM = 32768;  // flink中最大并行度为32768
constexpr int32_t HEAD_BLOCK_SIZE = 9;      // lsm block头信息所占字节数

constexpr int32_t NO_MAX_INT32_VALUE_2G = 2147483647;
constexpr int32_t NO_67108864 = 67108864;  // 64M
constexpr int32_t NO_33554432 = 33554432;  // 32M
constexpr int32_t NO_16777216 = 16777216;  // 16M
constexpr int32_t NO_16777215 = 16777215;
constexpr uint32_t NO_8388608 = 8388608;  // 8M
constexpr uint32_t NO_1000000 = 1000000;
constexpr uint32_t IO_SIZE_4MB = 4194304;  // 4M
constexpr uint32_t NO_4194303 = 4194303;   // 4194303
constexpr uint32_t NO_1048576 = 1048576;   // 1M
constexpr uint32_t NO_65536 = 65536;
constexpr uint32_t NO_65535 = 65535;
constexpr uint32_t NO_10000 = 10000;
constexpr uint32_t NO_8192 = 8192;
constexpr uint32_t NO_4096 = 4096;
constexpr uint32_t NO_3000 = 3000;
constexpr uint32_t NO_2048 = 2048;
constexpr uint32_t NO_1024 = 1024;
constexpr uint32_t NO_512 = 512;

constexpr uint32_t NO_1000 = 1000;
constexpr uint32_t NO_5000 = 5000;
constexpr uint32_t NO_30000 = 30000;
constexpr uint32_t NO_50000 = 50000;
constexpr uint32_t NO_100000 = 100000;
constexpr uint32_t NO_150000 = 150000;
constexpr uint32_t NO_300000 = 300000;
constexpr uint32_t NO_256 = 256;
constexpr uint32_t NO_255 = 255;
constexpr uint32_t NO_128 = 128;
constexpr uint32_t NO_127 = 127;
constexpr uint32_t NO_300 = 300;
constexpr uint32_t NO_100 = 100;
constexpr uint32_t NO_90 = 90;
constexpr uint32_t NO_80 = 80;
constexpr uint32_t NO_79 = 79;
constexpr uint32_t NO_78 = 78;
constexpr uint32_t NO_77 = 77;
constexpr uint32_t NO_76 = 76;
constexpr uint32_t NO_75 = 75;
constexpr uint32_t NO_74 = 74;
constexpr uint32_t NO_73 = 73;
constexpr uint32_t NO_72 = 72;
constexpr uint32_t NO_71 = 71;
constexpr uint32_t NO_70 = 70;
constexpr uint32_t NO_64 = 64;
constexpr uint32_t NO_63 = 63;
constexpr uint32_t NO_60 = 60;
constexpr uint32_t NO_68 = 68;
constexpr uint32_t NO_59 = 59;
constexpr uint32_t NO_58 = 58;
constexpr uint32_t NO_56 = 56;
constexpr uint32_t NO_54 = 54;
constexpr uint32_t NO_53 = 53;
constexpr uint32_t NO_52 = 52;
constexpr uint32_t NO_51 = 51;
constexpr uint32_t NO_50 = 50;
constexpr uint32_t NO_55 = 55;
constexpr uint32_t NO_49 = 49;
constexpr uint32_t NO_48 = 48;
constexpr uint32_t NO_45 = 45;
constexpr uint32_t NO_44 = 44;
constexpr uint32_t NO_42 = 42;
constexpr uint32_t NO_41 = 41;
constexpr uint32_t NO_40 = 40;
constexpr uint32_t NO_34 = 34;
constexpr uint32_t NO_39 = 39;
constexpr uint32_t NO_38 = 38;
constexpr uint32_t NO_32 = 32;
constexpr uint32_t NO_30 = 30;
constexpr uint32_t NO_31 = 31;
constexpr uint32_t NO_29 = 29;
constexpr uint32_t NO_28 = 28;
constexpr uint32_t NO_27 = 27;
constexpr uint32_t NO_26 = 26;
constexpr uint32_t NO_25 = 25;
constexpr uint32_t NO_24 = 24;
constexpr uint32_t NO_23 = 23;
constexpr uint32_t NO_22 = 22;
constexpr uint32_t NO_21 = 21;
constexpr uint32_t NO_20 = 20;
constexpr uint32_t NO_19 = 19;
constexpr uint32_t NO_18 = 18;
constexpr uint32_t NO_17 = 17;
constexpr uint32_t NO_16 = 16;
constexpr uint32_t NO_15 = 15;
constexpr uint32_t NO_14 = 14;
constexpr uint32_t NO_13 = 13;
constexpr uint32_t NO_12 = 12;
constexpr uint32_t NO_11 = 11;
constexpr uint32_t NO_10 = 10;
constexpr uint32_t NO_7 = 7;
constexpr uint32_t NO_8 = 8;
constexpr uint32_t NO_9 = 9;
constexpr uint32_t NO_6 = 6;
constexpr uint32_t NO_5 = 5;
constexpr uint32_t NO_4 = 4;
constexpr uint32_t NO_3 = 3;
constexpr uint32_t NO_2 = 2;
constexpr uint32_t NO_1 = 1;
constexpr uint32_t NO_0 = 0;
constexpr float NO_0_6 = 0.6;
constexpr double NO_0_0_1 = 0.01;
constexpr double NO_2_0 = 2.0;
constexpr double NO_0_5 = 0.5;
constexpr double NO_0_8 = 0.8;
constexpr int NO_777 = 0777;

constexpr uint64_t NO_U64_0 = 0;
constexpr uint64_t NO_U64_1 = 1;
constexpr uint8_t NO_U8_255 = 255;

constexpr uint32_t ONE_MINUTE = 60 * 1000;

constexpr uint32_t IP_SIZE = 32;
constexpr uint32_t DEVICE_SIZE = 16;
constexpr int32_t NO_PQ_DATA = 2;

constexpr size_t IO_SIZE_1K = 1024;
constexpr size_t IO_SIZE_2K = 2 * 1024;
constexpr size_t IO_SIZE_4K = 4 * 1024;
constexpr size_t IO_SIZE_8K = 8 * 1024;
constexpr size_t IO_SIZE_16K = 16 * 1024;
constexpr size_t IO_SIZE_32K = 32 * 1024;
constexpr size_t IO_SIZE_64K = 64 * 1024;
constexpr size_t IO_SIZE_128K = 128 * 1024;
constexpr size_t IO_SIZE_256K = 256 * 1024;
constexpr size_t IO_SIZE_384K = 384 * 1024;
constexpr size_t IO_SIZE_512K = 512 * 1024;
constexpr size_t IO_SIZE_1M = 1024 * 1024;
constexpr size_t IO_SIZE_2M = 2 * 1024 * 1024;
constexpr size_t IO_SIZE_4M = 4 * 1024 * 1024;
constexpr size_t IO_SIZE_8M = 8 * 1024 * 1024;
constexpr size_t IO_SIZE_16M = 16 * 1024 * 1024;
constexpr size_t IO_SIZE_32M = 32 * 1024 * 1024;
constexpr size_t IO_SIZE_64M = 64 * 1024 * 1024;
constexpr size_t IO_SIZE_128M = 128 * 1024 * 1024;
constexpr size_t IO_SIZE_192M = 192 * 1024 * 1024;
constexpr size_t IO_SIZE_256M = 256 * 1024 * 1024;
constexpr size_t IO_SIZE_512M = 512 * 1024 * 1024;
constexpr size_t IO_SIZE_320M = 320 * 1024 * 1024;
constexpr size_t IO_SIZE_640M = 640 * 1024 * 1024;
constexpr uint64_t IO_SIZE_1G = 1024L * 1024L * 1024L;
constexpr uint64_t IO_SIZE_2G = 2L * 1024L * 1024L * 1024L;
constexpr uint64_t IO_SIZE_4G = 4L * 1024L * 1024L * 1024L;
constexpr uint64_t IO_SIZE_8G = 8L * 1024L * 1024L * 1024L;

constexpr int32_t MAGIC_NUMBER = -42;
constexpr int32_t DEPRECATED_VERSION = 1;
constexpr int32_t RANGE_FILTER_VERSION = 3;
constexpr int32_t STATE_MIGRATION_VERSION = 4;
constexpr int32_t FILE_ADDRESS_VERSION = 5;
constexpr int32_t PRIMARY_FILE_STATUS_VERSION = 7;
constexpr int32_t CURRENT_VERSION = 7;
constexpr int32_t INVALID_INDEX = -1;
constexpr uint32_t INVALID_U32_INDEX = UINT32_MAX;
constexpr uint8_t INVALID_U8 = 0xFF;
constexpr uint16_t INVALID_U16 = 0xFFFF;
constexpr uint32_t INVALID_U32 = 0xFFFFFFFF;
constexpr uint64_t INVALID_U64 = 0xFFFFFFFFFFFFFFFF;

constexpr int32_t OVERLAP_COMPARE_RESULT_CASE_0 = 0;
constexpr int32_t OVERLAP_COMPARE_RESULT_CASE_1 = 1;
constexpr int32_t OVERLAP_COMPARE_RESULT_CASE_2 = -1;
constexpr int32_t OVERLAP_COMPARE_RESULT_CASE_3 = 2;
constexpr int32_t OVERLAP_COMPARE_RESULT_CASE_4 = -2;
constexpr int32_t ERROR_CASE_DIV_BY_ZERO = -3;

constexpr uint64_t BLOCK_COMMON_MAGIC_NUM = 5126532UL;

// time constants
constexpr uint32_t ONE_MINUTE_IN_MS = 60 * 1000;
constexpr uint32_t TWO_MINUTES_IN_MS = ONE_MINUTE_IN_MS * 2;
}  // namespace bss
}  // namespace ock

#endif
