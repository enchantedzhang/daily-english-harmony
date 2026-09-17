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

package com.huawei.ock.bss.iterator;

import org.apache.commons.io.IOUtils;

import java.io.Closeable;
import java.util.Collections;
import java.util.Iterator;
import java.util.NoSuchElementException;
import java.util.function.Consumer;

import javax.annotation.Nonnull;

/**
 * 自定义可释放资源的迭代器
 *
 * @param <T> 迭代器迭代的类型
 * @since BeiMing 25.0.T1
 */
public interface CloseableIterator<T> extends Iterator<T>, AutoCloseable {
    /**
     * 空迭代器
     */
    CloseableIterator<?> EMPTY_INSTANCE = adapterForIterator(Collections.emptyIterator());

    /**
     * adapterForIterator
     *
     * @param <T> T
     * @param iterator iterator
     * @return CloseableIterator
     */
    @Nonnull
    static <T> CloseableIterator<T> adapterForIterator(@Nonnull Iterator<T> iterator) {
        return adapterForIterator(iterator, () -> {

        });
    }

    /**
     * adapterForIterator
     *
     * @param <T> T
     * @param iterator iterator
     * @param closeable closeable
     * @return CloseableIterator
     */
    @Nonnull
    static <T> CloseableIterator<T> adapterForIterator(@Nonnull Iterator<T> iterator, Closeable closeable) {
        return new IteratorAdapter<>(iterator, closeable);
    }

    /**
     * empty
     *
     * @param <T> T
     * @return CloseableIterator
     */
    static <T> CloseableIterator<T> empty() {
        return adapterForIterator(Collections.emptyIterator());
    }

    /**
     * singletonIterator
     *
     * @param <T> T
     * @param element element
     * @return CloseableIterator
     */
    static <T> CloseableIterator<T> singletonIterator(T element) {
        return new CloseableIterator<T>() {
            private boolean hasNext = true;

            /**
             * hasNext
             *
             * @return boolean
             */
            public boolean hasNext() {
                return this.hasNext;
            }

            /**
             * next
             *
             * @return T
             */
            public T next() {
                if (this.hasNext) {
                    this.hasNext = false;
                    return (T) element;
                }
                throw new NoSuchElementException();
            }

            /**
             * close
             *
             * @throws Exception exception
             */
            public void close() throws Exception {
            }
        };
    }

    /**
     * IteratorAdapter
     *
     * @param <E> E
     */
    public static final class IteratorAdapter<E> implements CloseableIterator<E> {
        @Nonnull
        private final Iterator<E> delegate;

        private final Closeable closeable;

        IteratorAdapter(@Nonnull Iterator<E> delegate, Closeable closeable) {
            this.delegate = delegate;
            this.closeable = closeable;
        }

        /**
         * hasNext
         *
         * @return boolean
         */
        public boolean hasNext() {
            return this.delegate.hasNext();
        }

        /**
         * next
         *
         * @return 下一个元素
         */
        public E next() {
            return this.delegate.next();
        }

        /**
         * remove
         */
        public void remove() {
            this.delegate.remove();
        }

        /**
         * forEachRemaining
         *
         * @param action The action to be performed for each element
         */
        public void forEachRemaining(Consumer<? super E> action) {
            this.delegate.forEachRemaining(action);
        }

        /**
         * close
         */
        public void close() {
            IOUtils.closeQuietly(this.closeable);
        }
    }
}
