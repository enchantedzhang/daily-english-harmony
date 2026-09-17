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

package com.huawei.ock.bss.metric;

/**
 * SnapshotMetric
 *
 * @since BeiMing 25.2
 */
public enum SnapshotMetric {
    SNAPSHOT_TOTAL_TIME,
    SNAPSHOT_UPLOAD_TIME,
    SNAPSHOT_FILE_COUNT,
    SNAPSHOT_INCREMENTAL_SIZE,
    SNAPSHOT_FILE_SIZE,
    SNAPSHOT_SLICE_FILE_COUNT,
    SNAPSHOT_SLICE_INCREMENTAL_FILE_SIZE,
    SNAPSHOT_SLICE_FILE_SIZE,
    SNAPSHOT_SST_FILE_COUNT,
    SNAPSHOT_SST_INCREMENTAL_FILE_SIZE,
    SNAPSHOT_SST_FILE_SIZE
}