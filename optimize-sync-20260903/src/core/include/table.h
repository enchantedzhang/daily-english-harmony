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

#ifndef BOOST_SS_TABLE_H
#define BOOST_SS_TABLE_H

#include <vector>

#include "auto_closeable.h"
#include "binary_data.h"
#include "bss_err.h"

namespace ock {
namespace bss {
class TableDescription;
using TableDescriptionRef = std::shared_ptr<TableDescription>;

class Table : public AutoCloseable {
public:
    /**
     * Put one key value pair to table, for KV/KMAP/KLIST
     * @param keyHashCode key hash code.
     * @param key single key boost.
     * @param value value boost.
     * @return if success return BSS_OK, else return others.
     */
    virtual BResult Put(uint32_t keyHashCode, const BinaryData &key, const BinaryData &value)
    {
        return BSS_NOT_SUPPORTED;
    }

    /**
     * Add key value pair to table, append to exist value, for KLIST
     * @param keyHashCode key hash code.
     * @param key single key boost.
     * @param value value boost.
     * @return if success return BSS_OK, else return others.
     */
    virtual BResult Add(uint32_t keyHashCode, const BinaryData &key, const BinaryData &value)
    {
        return BSS_NOT_SUPPORTED;
    }

    /**
     * Put values with the same key to table, for KLIST
     * @param keyHashCode key hash code.
     * @param key single key boost.
     * @param values value boost vector.
     * @return if success return BSS_OK, else return others.
     */
    virtual BResult Put(uint32_t keyHashCode, const BinaryData &key, const std::vector<BinaryData> &values)
    {
        return BSS_NOT_SUPPORTED;
    }

    /**
     * Add values with the same key to table, append to exist value, for KLIST
     * @param keyHashCode key hash code.
     * @param key single key boost.
     * @param values value boost vector.
     * @return if success return BSS_OK, else return others.
     */
    virtual BResult Add(uint32_t keyHashCode, const BinaryData &key, const std::vector<BinaryData> &values)
    {
        return BSS_NOT_SUPPORTED;
    }

    /**
     * Put one key value pair to table, for KMAP
     * @param keyHashCode key hash code.
     * @param priKey primary key boost.
     * @param secKey secondary key boost.
     * @param value value binay.
     * @return if success return BSS_OK, else return others.
     */
    virtual BResult Put(uint32_t keyHashCode, const BinaryData &priKey, const BinaryData &secKey,
                        const BinaryData &value)
    {
        return BSS_NOT_SUPPORTED;
    }

    /**
     * Get value by single key, for KV/KLIST
     * @param keyHashCode key hash code.
     * @param key single key boost.
     * @param value return value boost.
     * @return if success return BSS_OK, not exist return BSS_NOT_EXIST, else return others.
     */
    virtual BResult Get(uint32_t keyHashCode, const BinaryData &key, BinaryData &value)
    {
        return BSS_NOT_SUPPORTED;
    }

    /**
     * Get value by dual key, for KMAP
     * @param keyHashCode key hash code.
     * @param priKey primary key boost.
     * @param secKey secondary key boost.
     * @param value return value boost.
     * @return if success return BSS_OK, not exist return BSS_NOT_EXIST, else return others.
     */
    virtual BResult Get(uint32_t keyHashCode, const BinaryData &priKey, const BinaryData &secKey, BinaryData &value)
    {
        return BSS_NOT_SUPPORTED;
    }

    /**
     * Get value by single key, for KLIST
     * @param keyHashCode key hash code.
     * @param key single key boost.
     * @param values return value boost vector.
     * @return if success return BSS_OK, not exist return BSS_NOT_EXIST, else return others.
     */
    virtual BResult Get(uint32_t keyHashCode, const BinaryData &key, std::vector<BinaryData> &values)
    {
        return BSS_NOT_SUPPORTED;
    }

    /**
     * Determine whether contain the specified key, for KV/KMAP/KLIST,
     * if it is KMap, Not all elements in the Map have been deleted, then return exist.
     * @param keyHashCode key hash code.
     * @param key single key boost.
     * @return if exist return true, else return false.
     */
    virtual bool Contain(uint32_t keyHashCode, const BinaryData &key)
    {
        return false;
    }

    /**
     * Determine whether contain the specified key, for KMAP
     * @param keyHashCode key hash code.
     * @param priKey primary key boost.
     * @param secKey secondary key boost.
     * @return if exist return true, else return false.
     */
    virtual bool Contain(uint32_t keyHashCode, const BinaryData &priKey, const BinaryData &secKey)
    {
        return false;
    }

    /**
     * Remove key, for KV/KMAP/KLIST
     * @param keyHashCode key hash code.
     * @param key single key boost.
     * @return if success return BSS_OK, else return others.
     */
    virtual BResult Remove(uint32_t keyHashCode, const BinaryData &key)
    {
        return BSS_NOT_SUPPORTED;
    }

    /**
     * Remove key, for KMAP
     * @param keyHashCode key hash code.
     * @param priKey primary key boost.
     * @param secKey secondary key boost.
     * @return if success return BSS_OK, else return others.
     */
    virtual BResult Remove(uint32_t keyHashCode, const BinaryData &priKey, const BinaryData &secKey)
    {
        return BSS_NOT_SUPPORTED;
    }

    /**
     * Get table description.
     * @return table description instance.
     */
    virtual TableDescriptionRef GetTableDescription() const
    {
        return nullptr;
    }
};
using TableRef = std::shared_ptr<Table>;
}  // namespace bss
}  // namespace ock

#endif
