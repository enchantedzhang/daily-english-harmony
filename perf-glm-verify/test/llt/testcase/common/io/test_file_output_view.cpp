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
#include "common/io/file_input_view.h"
#include "common/io/file_output_view.h"
#include "test_utils.h"

namespace ock {
namespace bss {
namespace test {
namespace test_file_output_view {

const std::string testFilePath = "test_output.txt";
FileOutputView fileOutput;
FileInputView fileInput;

class TestFileOutputView : public ::testing::Test {
public:
    static void SetUpTestCase();
    static void TearDownTestCase();

    TestFileOutputView() = default;

    void SetUp() override
    {
    }
    void TearDown() override
    {
    }

    static MemManagerRef mDirectMemManager;
};
MemManagerRef TestFileOutputView::mDirectMemManager = nullptr;

void TestFileOutputView::SetUpTestCase()
{
    Uri uri(testFilePath);
    PathRef path = std::make_shared<Path>(uri);
    ConfigRef config = std::make_shared<Config>();
    ASSERT_EQ(fileOutput.Init(path, config, FileOutputView::WriteMode::OVERWRITE), BSS_OK);
    ASSERT_EQ(fileInput.Init(FileSystemType::LOCAL, path), BSS_OK);

    mDirectMemManager = std::make_shared<MemManager>(AllocatorType::DIRECT);
    mDirectMemManager->Initialize(config);
}

void TestFileOutputView::TearDownTestCase()
{
    fileOutput.Close();
    fileInput.Close();
}

TEST_F(TestFileOutputView, WriteOperations)
{
    std::random_device rd;  // 随机设备
    std::mt19937 gen(rd());

    // 写入整数
    std::uniform_int_distribution<uint8_t> dist8(NO_0, UINT8_MAX);
    uint8_t num8 = dist8(gen);
    ASSERT_EQ(fileOutput.WriteUint8(num8), BSS_OK);

    std::uniform_int_distribution<uint32_t> dist16(NO_0, UINT16_MAX);
    uint16_t num16 = dist16(gen);
    ASSERT_EQ(fileOutput.WriteUint16(num16), BSS_OK);

    std::uniform_int_distribution<uint32_t> dist32(NO_0, UINT32_MAX);
    uint32_t num32 = dist32(gen);
    ASSERT_EQ(fileOutput.WriteUint32(num32), BSS_OK);

    std::uniform_int_distribution<uint32_t> dist64(NO_0, UINT32_MAX);  // 范围 [0, 2^32-1]
    uint64_t num64 = dist32(gen);
    ASSERT_EQ(fileOutput.WriteUint64(num64), BSS_OK);

    // 写入字符串
    std::string str = "Hello, World!";
    ASSERT_EQ(fileOutput.WriteUTF(str), BSS_OK);

    // 刷新并关闭文件
    ASSERT_EQ(fileOutput.Flush(), BSS_OK);

    // 读取数据并验证
    fileInput.Seek(0);
    uint8_t readNum8 = 0;
    ASSERT_EQ(fileInput.Read(readNum8), BSS_OK);
    ASSERT_EQ(readNum8, num8);

    uint16_t readNum16;
    ASSERT_EQ(fileInput.Read(readNum16), BSS_OK);
    ASSERT_EQ(readNum16, num16);

    uint32_t readNum32;
    ASSERT_EQ(fileInput.Read(readNum32), BSS_OK);
    ASSERT_EQ(readNum32, num32);

    uint64_t readNum64;
    ASSERT_EQ(fileInput.Read(readNum64), BSS_OK);
    ASSERT_EQ(readNum64, num64);

    // 读取字符串
    std::string readStr;
    ASSERT_EQ(fileInput.ReadUTF(readStr), BSS_OK);
    ASSERT_EQ(readStr, str);
}

TEST_F(TestFileOutputView, ByteBuffer)
{
    auto data = GetRandomData();
    auto length = data.size();
    ByteBufferRef byteBuffer = MakeRef<ByteBuffer>(data.data(), length);
    ASSERT_EQ(fileOutput.WriteByteBuffer(byteBuffer, NO_0, length), BSS_OK);
    ByteBufferRef byteBuffer1 = MakeRef<ByteBuffer>(length, MemoryType::FILE_STORE, mDirectMemManager);
    ASSERT_EQ((fileInput.ReadByteBuffer(NO_0, byteBuffer1, NO_0, length)), BSS_OK);
    ASSERT_EQ(byteBuffer->Compare(byteBuffer1, NO_0, NO_0, length, length), 0);
}

}  // namespace test_file_output_view
}  // namespace test
}  // namespace bss
}  // namespace ock