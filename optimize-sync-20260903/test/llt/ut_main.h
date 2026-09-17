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

#ifndef BOOST_SS_UT_MAIN_H
#define BOOST_SS_UT_MAIN_H

extern int32_t g_testSeed;

extern void CleanupSstFiles();

extern void CleanupBlobFiles();

extern void CleanupTombstoneFiles(void);

extern std::string GetCurrentWorkingDirectory();

extern int RemoveFile(const char *fpath, const struct stat *sb, int typeflag, struct FTW *ftwbuf);
#endif  // BOOST_SS_UT_MAIN_H
