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

#ifndef BOOST_SS_INPUT_SORTED_RUN_H
#define BOOST_SS_INPUT_SORTED_RUN_H

#include <vector>

#include "lsm_store/file/file_meta_data.h"
#include "merging_iterator.h"

namespace ock {
namespace bss {
struct GroupRangeRefHash {
    std::size_t operator()(const GroupRangeRef &groupRange) const
    {
        return groupRange->HashCode();
    }
};

struct GroupRangeRefEqual {
    bool operator()(const GroupRangeRef &a, const GroupRangeRef &b) const
    {
        return a->Equals(b);
    }
};

class InputSortedRun;
using InputSortedRunRef = std::shared_ptr<InputSortedRun>;
class InputSortedRun : public std::enable_shared_from_this<InputSortedRun> {
public:
    class FileIteratorWriter;
    using FileIteratorWriterRef = std::shared_ptr<FileIteratorWriter>;

    InputSortedRun()
    {
        mFileMetaDataList.clear();
    }

    ~InputSortedRun()
    {
        mFileMetaDataList.clear();
    }

    inline KeyValueIteratorRef GetIterator(const FileIteratorWriterRef &builder)
    {
        return std::make_shared<InputSortedRunIterator>(shared_from_this(), builder);
    }

    inline static std::vector<InputSortedRunRef> BuildInputSortedRun(
        const std::vector<FileMetaDataRef> &fileList, FileMetaData::FileMetaDataComparator &fileMetaDataComparator)
    {
        std::unordered_map<GroupRangeRef, InputSortedRunRef, GroupRangeRefHash, GroupRangeRefEqual> inputSortedRunMap;
        inputSortedRunMap.reserve(fileList.size());
        std::vector<InputSortedRunRef> result;
        for (auto &fileMetaData : fileList) {
            auto tmpKey = fileMetaData->GetGroupRange();
            auto it = inputSortedRunMap.find(tmpKey);
            if (it != inputSortedRunMap.end()) {
                it->second->AddFileMetaData(fileMetaData);
            } else {
                InputSortedRunRef inputSortedRun = std::make_shared<InputSortedRun>();
                inputSortedRun->AddFileMetaData(fileMetaData);
                inputSortedRunMap.emplace(tmpKey, inputSortedRun);
                result.push_back(inputSortedRun);
            }
        }
        for (InputSortedRunRef &inputSortedRun : result) {
            inputSortedRun->Sort(fileMetaDataComparator);
        }
        return result;
    }

    inline static KeyValueIteratorRef BuildInputSortedRunIterator(const FileMetaDataRef &fileMetaData,
                                                                  const FileIteratorWriterRef &builder)
    {
        InputSortedRunRef inputSortedRun = std::make_shared<InputSortedRun>();
        inputSortedRun->AddFileMetaData(fileMetaData);
        return inputSortedRun->GetIterator(builder);
    }

    inline static KeyValueIteratorRef BuildInputIterator(const std::vector<FileMetaDataRef> &fileMetaDatas,
                                                         const FileIteratorWriterRef &builder,
                                                         MemManagerRef &memManager, bool reverseOrder, bool sectionRead,
                                                         FileProcHolder holder)
    {
        return std::make_shared<InputSortedRunLevel0Iterator>(fileMetaDatas, builder, memManager, reverseOrder,
                                                              sectionRead, holder);
    }

    class InputSortedRunIterator : public Iterator<KeyValueRef> {
    public:
        InputSortedRunIterator(const InputSortedRunRef &inputSortedRun, const FileIteratorWriterRef &builder)
            : mBuilder(builder), mInputSortedRun(inputSortedRun), mCurrentIndex(0)
        {
            InitFileIterator();
        }

        bool HasNext() override
        {
            if (!mInitialized) {
                mInitialized = true;
                mNextPair = Advance();
            }
            return (mNextPair != nullptr);
        }

        KeyValueRef Next() override
        {
            KeyValueRef currentPair = mNextPair;
            mPrev = mCurrent;
            mNextPair = Advance();
            return currentPair;
        }

        void PrintUsefulInfo() override
        {
            if (mPrev != nullptr) {
                LOG_INFO("iterators size:" << mInputSortedRun->Size() << ", prev iterator info:");
                mPrev->PrintUsefulInfo();
            }
        }

    private:
        KeyValueRef Advance()
        {
            while ((mCurrent == nullptr || !mCurrent->HasNext()) &&
                   static_cast<uint32_t>(mCurrentIndex) < (mInputSortedRun->Size() - 1)) {
                mCurrentIndex++;
                InitFileIterator();
            }
            if (mCurrent != nullptr && mCurrent->HasNext()) {
                return mCurrent->Next();
            }
            return nullptr;
        }

    private:
        inline void InitFileIterator()
        {
            if (mCurrent != nullptr) {
                mCurrent->Close();
            }

            mCurrent = mBuilder->Build(mInputSortedRun->GetFileMetaData(mCurrentIndex));
        }

    private:
        FileIteratorWriterRef mBuilder = nullptr;
        InputSortedRunRef mInputSortedRun = nullptr;
        KeyValueIteratorRef mCurrent = nullptr;
        KeyValueIteratorRef mPrev = nullptr;
        KeyValueRef mNextPair = nullptr;
        int32_t mCurrentIndex = -1;
        bool mInitialized = false;
    };

    using BuilderFunc = std::function<KeyValueIteratorRef(const FileMetaDataRef &)>;
    class FileIteratorWriter {
    public:
        inline KeyValueIteratorRef Build(const FileMetaDataRef &fileMetaData) const
        {
            return mBuilderFunc(fileMetaData);
        }

        inline static FileIteratorWriterRef Of(BuilderFunc builderFunc)
        {
            FileIteratorWriterRef fileIteratorBuilder = std::make_shared<FileIteratorWriter>();
            fileIteratorBuilder->mBuilderFunc = builderFunc;
            return fileIteratorBuilder;
        }

    private:
        BuilderFunc mBuilderFunc = nullptr;
    };

    class InputSortedRunLevel0Iterator : public Iterator<KeyValueRef> {
    public:
        InputSortedRunLevel0Iterator(const std::vector<FileMetaDataRef> &metas, const FileIteratorWriterRef &builder,
                                     const MemManagerRef &memManager, bool reverseOrder, bool sectionRead,
                                     FileProcHolder holder)
            : mBuilder(builder), mMetas(metas), mMemManager(memManager), mReverseOrder(reverseOrder)
        {
            InitFileIterator(sectionRead, holder);
        }

        bool HasNext() override
        {
            if (!mInitialized) {
                mInitialized = true;
                mNextPair = Advance();
            }
            return (mNextPair != nullptr);
        }

        KeyValueRef Next() override
        {
            KeyValueRef currentPair = mNextPair;
            mNextPair = Advance();
            return currentPair;
        }

    private:
        KeyValueRef Advance()
        {
            auto nextKey = mMergeIter->NextKey();
            auto iter = mMetas.begin();
            if (nextKey == nullptr) {
                if (iter == mMetas.end()) {
                    return nullptr;
                }
                mMergeIter->AddIter(mBuilder->Build(*iter));
                mSmallKey = *(*iter)->GetSmallest();
                iter = mMetas.erase(iter);
            } else {
                mSmallKey = nextKey->key;
            }
            while (iter != mMetas.end()) {
                if (mSmallKey.Compare(*(*iter)->GetSmallest()) >= 0) {
                    mMergeIter->AddIter(mBuilder->Build(*iter));
                    iter = mMetas.erase(iter);
                    continue;
                }
                break;
            }

            return mMergeIter != nullptr && mMergeIter->HasNext() ? mMergeIter->Next() : nullptr;
        }

    private:
        inline void InitFileIterator(bool sectionRead, FileProcHolder holder)
        {
            std::sort(mMetas.begin(), mMetas.end(), FileMetaDataGroup::CreateFileMetaDataComparator(mReverseOrder));
            std::vector<KeyValueIteratorRef> iters;
            mSmallKey = *(*mMetas.begin())->GetSmallest();
            auto iter = mMetas.begin();
            while (iter != mMetas.end()) {
                if (mSmallKey.Compare(*(*iter)->GetSmallest()) == 0) {
                    iters.emplace_back(mBuilder->Build(*iter));
                    iter = mMetas.erase(iter);
                    continue;
                }
                break;
            }
            mMergeIter = std::make_shared<MergingIterator>(iters, mMemManager, nullptr, nullptr, false, holder,
                                                           sectionRead);
        }

    private:
        FileIteratorWriterRef mBuilder = nullptr;
        std::vector<FileMetaDataRef> mMetas;
        MemManagerRef mMemManager = nullptr;
        Key mSmallKey;
        KeyValueRef mNextPair = nullptr;
        MergingIteratorRef mMergeIter;
        bool mReverseOrder = false;
        bool mInitialized = false;
    };

private:
    inline void AddFileMetaData(const FileMetaDataRef &fileMetaData)
    {
        mFileMetaDataList.push_back(fileMetaData);
    }
    inline FileMetaDataRef GetFileMetaData(int32_t currentIndex)
    {
        if (currentIndex < 0 || static_cast<uint32_t>(currentIndex) >= mFileMetaDataList.size()) {
            return nullptr;
        }
        return mFileMetaDataList.at(currentIndex);
    }

    inline void Sort(FileMetaData::FileMetaDataComparator &fileMetaDataComparator)
    {
        if (mFileMetaDataList.size() > 1) {
            std::sort(mFileMetaDataList.begin(), mFileMetaDataList.end(), fileMetaDataComparator);
        }
    }

    inline uint32_t Size() const
    {
        return mFileMetaDataList.size();
    }

private:
    std::vector<FileMetaDataRef> mFileMetaDataList;
};

}  // namespace bss
}  // namespace ock
#endif  // BOOST_SS_INPUT_SORTED_RUN_H