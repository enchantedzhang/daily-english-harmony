/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include "snapshot_compression_utils.h"

#include "bss_log.h"
#include "compressor_utils.h"
#include "securec.h"

namespace ock {
namespace bss {
BResult SnapshotCompressionUtils::CopyRaw(const uint8_t *rawData, uint32_t rawLength, uint8_t *storedData,
                                          uint32_t storedCapacity, CompressAlgo &storedAlgo, uint32_t &storedLength)
{
    if (UNLIKELY(rawData == nullptr || storedData == nullptr || storedCapacity < rawLength)) {
        LOG_ERROR("Snapshot compression copy raw failed, rawLength:" << rawLength
                                                                     << ", storedCapacity:" << storedCapacity);
        return BSS_INVALID_PARAM;
    }
    auto ret = memcpy_s(storedData, storedCapacity, rawData, rawLength);
    if (UNLIKELY(ret != BSS_OK)) {
        LOG_ERROR("Snapshot compression copy raw failed, ret:" << ret << ", rawLength:" << rawLength);
        return BSS_ERR;
    }
    storedAlgo = CompressAlgo::NONE;
    storedLength = rawLength;
    return BSS_OK;
}

BResult SnapshotCompressionUtils::TryCompressInto(CompressAlgo policy, const uint8_t *rawData, uint32_t rawLength,
                                                  uint8_t *storedData, uint32_t storedCapacity,
                                                  CompressAlgo &storedAlgo, uint32_t &storedLength)
{
    storedAlgo = CompressAlgo::NONE;
    storedLength = 0;
    if (UNLIKELY(rawLength == 0)) {
        return BSS_OK;
    }
    if (policy == CompressAlgo::NONE) {
        return CopyRaw(rawData, rawLength, storedData, storedCapacity, storedAlgo, storedLength);
    }
    if (UNLIKELY(rawData == nullptr || storedData == nullptr || storedCapacity == 0)) {
        LOG_ERROR("Snapshot compression invalid param, rawLength:" << rawLength
                                                                   << ", storedCapacity:" << storedCapacity);
        return BSS_INVALID_PARAM;
    }
    CompressAlgo codec = policy;
    if (!CompressorUtils::IsSupportCodec(codec)) {
        LOG_WARN("Snapshot compression codec is not supported, fallback raw, codec:" << static_cast<uint32_t>(codec));
        return CopyRaw(rawData, rawLength, storedData, storedCapacity, storedAlgo, storedLength);
    }
    CompressorRef compressor = CompressorUtils::InitCompressor(codec);
    if (compressor == nullptr) {
        LOG_WARN("Snapshot compression init compressor failed, fallback raw, codec:" << static_cast<uint32_t>(policy));
        return CopyRaw(rawData, rawLength, storedData, storedCapacity, storedAlgo, storedLength);
    }

    uint32_t outputSize = compressor->Compress(storedData, storedCapacity, rawData, rawLength);
    if (outputSize == 0 || outputSize >= rawLength) {
        LOG_DEBUG("Snapshot compression has no benefit, fallback raw, rawLength:" << rawLength
                                                                                  << ", outputSize:" << outputSize);
        return CopyRaw(rawData, rawLength, storedData, storedCapacity, storedAlgo, storedLength);
    }
    storedAlgo = codec;
    storedLength = outputSize;
    return BSS_OK;
}

BResult SnapshotCompressionUtils::Decompress(CompressAlgo storedAlgo, const uint8_t *storedData, uint32_t storedLength,
                                             uint8_t *rawData, uint32_t rawLength)
{
    if (UNLIKELY(rawLength == 0)) {
        return storedLength == 0 ? BSS_OK : BSS_INVALID_PARAM;
    }
    if (UNLIKELY(storedData == nullptr || rawData == nullptr)) {
        LOG_ERROR("Snapshot decompression invalid null buffer, algo:" << static_cast<uint32_t>(storedAlgo)
                                                                      << ", rawLength:" << rawLength
                                                                      << ", storedLength:" << storedLength);
        return BSS_INVALID_PARAM;
    }
    if (storedAlgo == CompressAlgo::NONE) {
        if (UNLIKELY(storedLength != rawLength)) {
            LOG_ERROR("Snapshot decompression invalid none length, rawLength:" << rawLength
                                                                               << ", storedLength:" << storedLength);
            return BSS_INVALID_PARAM;
        }
        auto ret = memcpy_s(rawData, rawLength, storedData, storedLength);
        if (UNLIKELY(ret != BSS_OK)) {
            LOG_ERROR("Snapshot decompression copy raw failed, ret:" << ret << ", rawLength:" << rawLength);
            return BSS_ERR;
        }
        return BSS_OK;
    }
    if (UNLIKELY(storedAlgo != CompressAlgo::LZ4 || storedLength == 0 || storedLength >= rawLength)) {
        LOG_ERROR("Snapshot decompression invalid metadata, algo:" << static_cast<uint32_t>(storedAlgo)
                                                                   << ", rawLength:" << rawLength
                                                                   << ", storedLength:" << storedLength);
        return BSS_INVALID_PARAM;
    }
    CompressAlgo codec = storedAlgo;
    CompressorRef compressor = CompressorUtils::InitCompressor(codec);
    if (UNLIKELY(compressor == nullptr || codec != storedAlgo)) {
        LOG_ERROR("Snapshot decompression init compressor failed, algo:" << static_cast<uint32_t>(storedAlgo));
        return BSS_NOT_SUPPORTED;
    }
    uint32_t decompressedSize = compressor->Decompress(rawData, rawLength, storedData, storedLength);
    if (UNLIKELY(decompressedSize != rawLength)) {
        LOG_ERROR("Snapshot decompression failed, algo:" << static_cast<uint32_t>(storedAlgo) << ", rawLength:"
                                                         << rawLength << ", storedLength:" << storedLength
                                                         << ", decompressedSize:" << decompressedSize);
        return BSS_ERR;
    }
    return BSS_OK;
}
}  // namespace bss
}  // namespace ock
