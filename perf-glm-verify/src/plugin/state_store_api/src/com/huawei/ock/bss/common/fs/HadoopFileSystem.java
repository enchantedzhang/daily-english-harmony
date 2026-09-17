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

package com.huawei.ock.bss.common.fs;

import com.huawei.ock.bss.common.exception.BSSRuntimeException;

import org.apache.flink.core.fs.FSDataInputStream;
import org.apache.flink.core.fs.FileStatus;
import org.apache.flink.core.fs.FileSystem;
import org.apache.flink.core.fs.Path;
import org.apache.flink.util.IOUtils;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.util.List;
import java.util.stream.Collectors;
import java.util.stream.Stream;

/**
 * 用于c++层调用的使用hdfs的fs
 *
 * @since BeiMing 25.0.T1
 */
public class HadoopFileSystem {
    private static final Logger LOG = LoggerFactory.getLogger(HadoopFileSystem.class);

    private FSDataInputStream inputStream;

    private Path remotePath;

    public HadoopFileSystem(String pathString) throws IOException {
        remotePath = new Path(pathString);
        inputStream = FileSystem.get(remotePath.toUri()).open(remotePath);
        LOG.debug("Open hadoopFile :{}.", remotePath.getName());
    }

    /**
     * 关闭流
     */
    public void close() {
        IOUtils.closeQuietly(inputStream);
    }

    /**
     * 读取hdfs文件
     *
     * @param offset 读取文件起始位置
     * @param length 读取长度
     * @return 结果
     * @throws IOException 异常
     */
    public byte[] readBytes(int offset, int length) {
        try {
            inputStream.seek(offset);
            byte[] bytes = new byte[length];
            inputStream.read(bytes);
            return bytes;
        } catch (IOException e) {
            LOG.error("Read hdfs error:", e);
        }
        return new byte[0];
    }

    /**
     * 获取当前目录下的所有文件列表
     *
     * @return 文件列表
     * @throws IOException 异常
     */
    public List<String> listFiles() throws IOException {
        FileStatus[] fileStatuses = FileSystem.get(remotePath.getParent().toUri()).listStatus(remotePath.getParent());
        return Stream.of(fileStatuses).map(FileStatus::getPath).map(Path::getPath).collect(Collectors.toList());
    }

    /**
     * hdfs文件下载到指定地址
     *
     * @param localPathString 本地下载地址
     * @throws BSSRuntimeException 异常
     */
    public void download(String localPathString) throws BSSRuntimeException {
        String error = "Download from hdfs fail.";
        try {
            File file = new File(String.valueOf(localPathString));
            String canonicalPath = file.getCanonicalPath();
            file = new File(canonicalPath);
            File parentFile = file.getParentFile();
            boolean createDir = true;
            if (!parentFile.exists()) {
                createDir = parentFile.mkdir();
            }
            if (!file.exists()) {
                boolean newFile = file.createNewFile();
                if (!createDir || !newFile) {
                    LOG.error("Create local file fail.");
                    return;
                }
            }
            try (FileOutputStream outputStream = new FileOutputStream(file)) {
                final int bufferLen = 8 * 1024;
                byte[] buffer = new byte[bufferLen];
                while (true) {
                    int numBytes = inputStream.read(buffer);
                    if (numBytes == -1) {
                        break;
                    }
                    outputStream.write(buffer, 0, numBytes);
                }
            } catch (IOException e) {
                LOG.error(error, e);
                throw new BSSRuntimeException(e);
            }
        } catch (IOException exception) {
            LOG.error(error, exception);
            throw new BSSRuntimeException(error);
        }
    }

    /**
     * 删除hdfs文件
     *
     * @throws IOException 异常
     */
    public void remove() throws IOException {
        LOG.info("remove remote file:{}", remotePath.getName());
        FileSystem.get(remotePath.toUri()).delete(remotePath, false);
    }
}