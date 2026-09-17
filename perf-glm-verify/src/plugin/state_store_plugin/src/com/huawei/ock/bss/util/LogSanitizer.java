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

package com.huawei.ock.bss.util;

/**
 * LogSanitizer
 *
 * @since BeiMing 25.0.T1
 */
public class LogSanitizer {
    private static final String[] REPLACEMENTS = {
        "%0a", "%0d", "%09", "%0b", "%0c",
        "%08", "%7f", "\n", "\r", "\t", "\f", "\u000B"
    };

    /**
     * 过滤日志中可能导致注入的问题字符
     *
     * @param input 原始日志内容
     * @return 清洗后的日志内容
     */
    public static String sanitize(String input) {
        String result = input;
        if (result == null) {
            return "null";
        }

        for (String target : REPLACEMENTS) {
            result = replaceIgnoreCase(result, target, "");
        }

        return result;
    }

    /**
     * replaceIgnoreCase
     *
     * @param source      source
     * @param target      target
     * @param replacement replacement
     * @return java.lang.String
     */
    private static String replaceIgnoreCase(String source, String target, String replacement) {
        StringBuilder result = new StringBuilder();
        int i = 0;
        int len = target.length();
        while (i < source.length()) {
            if (i + len <= source.length() && source.substring(i, i + len).equalsIgnoreCase(target)) {
                result.append(replacement);
                i += len;
            } else {
                result.append(source.charAt(i));
                i++;
            }
        }
        return result.toString();
    }
}
