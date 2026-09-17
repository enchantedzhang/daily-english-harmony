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

#include "com_huawei_ock_bss_table_AbstractTable.h"

#include <memory>

#include "common/scope_guard.h"
#include "include/boost_state_db.h"
#include "kv_helper.h"

using namespace ock::bss;

namespace {

std::string GetClassName(JNIEnv *env, jobject jObj, jclass objClass)
{
    auto getClassMethod = env->GetMethodID(objClass, "getClass", "()Ljava/lang/Class;");
    auto jClassObj = env->CallObjectMethod(jObj, getClassMethod);
    auto jClassCls = env->GetObjectClass(jClassObj);
    auto getNameMethod = env->GetMethodID(jClassCls, "getName", "()Ljava/lang/String;");
    auto jClassName = (jstring)env->CallObjectMethod(jClassObj, getNameMethod);
    const auto *className = env->GetStringUTFChars(jClassName, nullptr);
    SCOPE_EXIT({ env->ReleaseStringUTFChars(jClassName, className); });
    return className;
}

void GetTypeSerializer(JNIEnv *env, jobject jTypeSerializer, TypeSerializer &typeSerializer)
{
    static const std::string ROW_DATA_SERIALIZER{ "org.apache.flink.table.runtime.typeutils.RowDataSerializer" };
    static const std::string BIG_INT_TYPE{ "org.apache.flink.table.types.logical.BigIntType" };
    static const std::string TIMESTAMP_TYPE{ "org.apache.flink.table.types.logical.TimestampType" };

    auto typeSerializerClass = env->GetObjectClass(jTypeSerializer);
    auto className = GetClassName(env, jTypeSerializer, typeSerializerClass);
    if (ROW_DATA_SERIALIZER == className) {
        typeSerializer.mSerType = SerializerType::BINARY_ROW_DATA_SERIALIZER;
        auto typesFieldId = env->GetFieldID(typeSerializerClass, "types",
                                            "[Lorg/apache/flink/table/types/logical/LogicalType;");
        auto jTypes = reinterpret_cast<jobjectArray>(env->GetObjectField(jTypeSerializer, typesFieldId));
        for (int i = 0; i < env->GetArrayLength(jTypes); i++) {
            auto jElemType = env->GetObjectArrayElement(jTypes, i);
            if (!jElemType) {
                LOG_ERROR("impossible null element type");
                continue;
            }
            auto typeClassName = GetClassName(env, jElemType, env->GetObjectClass(jElemType));
            if (BIG_INT_TYPE == typeClassName) {
                typeSerializer.mFields.push_back({ NO_8, true });
            } else if (TIMESTAMP_TYPE == typeClassName) {
                typeSerializer.mFields.push_back({ NO_8, true });
            } else {
                typeSerializer.mFields.push_back({ 0, false });
            }
        }
    } else {
        typeSerializer.mSerType = SerializerType::UNKNOWN;
    }
}

BResult GetTableSerializer(JNIEnv *env, jobject jTableDesc, jclass tableDescClass, TableSerializer &out)
{
    auto getTableSerializerMethod = env->GetMethodID(tableDescClass, "getTableSerializer",
                                                     "()Lcom/huawei/ock/bss/common/serialize/TableSerializer;");
    if (!getTableSerializerMethod) {
        return BSS_ERR;
    }
    auto jTblSerializer = env->CallObjectMethod(jTableDesc, getTableSerializerMethod);
    auto tblSerializerClass = env->GetObjectClass(jTblSerializer);

    // 生成key serializer
    auto getKeySerializerMethod = env->GetMethodID(tblSerializerClass, "getKeySerializer",
                                                   "()Lorg/apache/flink/api/common/typeutils/TypeSerializer;");
    auto jKeyTypeSerializer = env->CallObjectMethod(jTblSerializer, getKeySerializerMethod);
    GetTypeSerializer(env, jKeyTypeSerializer, out.mKeySerializer);

    // 生成key2 serializer
    auto getKey2SerializerMethod = env->GetMethodID(tblSerializerClass, "getKey2Serializer",
                                                    "()Lorg/apache/flink/api/common/typeutils/TypeSerializer;");
    // 如果是KMap类型，tblSerializerClass实际上是SubTableSerializer
    if (getKey2SerializerMethod) {
        auto jKey2TypeSerializer = env->CallObjectMethod(jTblSerializer, getKey2SerializerMethod);
        GetTypeSerializer(env, jKey2TypeSerializer, out.mKey2Serializer);
    } else {
        env->ExceptionClear();
    }
    return BSS_OK;
}

TableDescriptionRef GetTableDes(JNIEnv *env, StateType type, const Config &config, jobject jTableDesc)
{
    if (config.mStartGroup > config.mEndGroup) {
        return nullptr;
    }
    // com.huawei.ock.bss.table.description.ITableDescription
    jclass tableDescClass = env->GetObjectClass(jTableDesc);
    jmethodID getTableNameMethod = env->GetMethodID(tableDescClass, "getTableName", "()Ljava/lang/String;");
    auto jTableName = reinterpret_cast<jstring>(env->CallObjectMethod(jTableDesc, getTableNameMethod));
    const char *tableName = env->GetStringUTFChars(jTableName, nullptr);
    if (!tableName) {
        LOG_ERROR("Get table name failed");
        return nullptr;
    }
    SCOPE_EXIT({ env->ReleaseStringUTFChars(jTableName, tableName); });

    jmethodID getTableTTLMethod = env->GetMethodID(tableDescClass, "getTableTTL", "()J");
    auto tableTTL = env->CallLongMethod(jTableDesc, getTableTTLMethod);

    TableSerializer tblSerializer;
    if (GetTableSerializer(env, jTableDesc, tableDescClass, tblSerializer) != BSS_OK) {
        LOG_ERROR("Get Table Serializer failed");
        return nullptr;
    }

    return std::make_shared<TableDescription>(type, tableName, tableTTL, tblSerializer, config);
}

}  // namespace

JNIEXPORT jlong JNICALL Java_com_huawei_ock_bss_table_AbstractTable_open(JNIEnv *env, jclass, jlong jDBPath,
                                                                         jstring jTableType, jobject jTableDesc)
{
    if (UNLIKELY(env == nullptr || jTableType == nullptr)) {
        LOG_ERROR("parameter is nullptr.");
        return 0;
    }

    const char *tableType = env->GetStringUTFChars(jTableType, nullptr);
    if (tableType == nullptr) {
        return 0;
    }
    SCOPE_EXIT({ env->ReleaseStringUTFChars(jTableType, tableType); });
    StateType keyedStateType;
    if (!StringToKeyedStateType(tableType, keyedStateType)) {
        return 0;
    }

    auto *boostStateDB = reinterpret_cast<BoostStateDB *>(jDBPath);
    if (UNLIKELY(boostStateDB == nullptr)) {
        LOG_ERROR("BoostStateDB is nullptr.");
        return 0;
    }
    auto tableDesc = GetTableDes(env, keyedStateType, boostStateDB->GetConfig(), jTableDesc);
    if (tableDesc == nullptr) {
        return 0;
    }
    TableRef table = boostStateDB->GetTableOrCreate(tableDesc);
    if (table == nullptr) {
        return 0;
    }
    auto address = static_cast<jlong>(reinterpret_cast<size_t>(table.get()));
    LOG_INFO("success open table:" << tableType << ",tableName:" << tableDesc->GetTableName());

    return address;
}

JNIEXPORT void JNICALL Java_com_huawei_ock_bss_table_AbstractTable_updateTtl(JNIEnv *env, jclass, jlong jDBPath,
                                                                             jstring jTableType, jobject jTableDesc)
{
    if (UNLIKELY(env == nullptr)) {
        LOG_ERROR("Input env is nullptr.");
        return;
    }

    const char *tableType = env->GetStringUTFChars(jTableType, nullptr);
    if (tableType == nullptr) {
        return;
    }
    SCOPE_EXIT({ env->ReleaseStringUTFChars(jTableType, tableType); });
    StateType keyedStateType;
    if (!StringToKeyedStateType(tableType, keyedStateType)) {
        return;
    }

    auto *boostStateDB = reinterpret_cast<BoostStateDB *>(jDBPath);
    if (UNLIKELY(boostStateDB == nullptr)) {
        LOG_ERROR("BoostStateDB is nullptr.");
        return;
    }
    auto tableDesc = GetTableDes(env, keyedStateType, boostStateDB->GetConfig(), jTableDesc);
    if (!tableDesc) {
        return;
    }

    BResult result = boostStateDB->UpdateTtlConfig(tableDesc);
    if (result != BSS_OK) {
        LOG_ERROR("Failed to update TTL, " << tableDesc->ToString() << ", ret: " << result);
    } else {
        LOG_INFO("Success update TTL, " << tableDesc->ToString());
    }
}
