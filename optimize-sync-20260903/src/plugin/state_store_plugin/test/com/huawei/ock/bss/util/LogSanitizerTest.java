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

import static org.junit.Assert.assertEquals;

import org.junit.Test;

/**
 * LogSanitizerTest
 *
 * @since BeiMing 25.0.T1
 */
public class LogSanitizerTest {
    @Test
    public void test_logSanitizer() {
        assertEquals("null", LogSanitizer.sanitize(null));
    }

    @Test
    public void testEmptyString() {
        assertEquals("", LogSanitizer.sanitize(""));
    }

    @Test
    public void testUrlEncodedNewline() {
        String input = "username=admin%0a[INJECTED]";
        String expected = "username=admin[INJECTED]";
        assertEquals(expected, LogSanitizer.sanitize(input));
    }

    @Test
    public void testMultipleEncodedControlChars() {
        String input = "a%0d%0c%0b%7f%08z";
        String expected = "az";
        assertEquals(expected, LogSanitizer.sanitize(input));
    }

    @Test
    public void testRawControlChars() {
        String input = "line1\nline2\rline3\tline4\fline5\u000Bend";
        String expected = "line1line2line3line4line5end";
        assertEquals(expected, LogSanitizer.sanitize(input));
    }

    @Test
    public void testMixedEncodedAndRawChars() {
        String input = "input%0aINJECT\n%0dLOG\r%0c\f%7fEND";
        String expected = "inputINJECTLOGEND";
        assertEquals(expected, LogSanitizer.sanitize(input));
    }
}
