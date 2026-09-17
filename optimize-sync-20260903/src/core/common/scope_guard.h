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
#pragma once

#include <memory>
#include <utility>

template <typename F> class ScopeGuard {
public:
    constexpr explicit ScopeGuard(F &&func) : function(std::move(func))
    {
    }
    ~ScopeGuard()
    {
        std::move(function)();
    }

private:
    F function;
};

template <typename F> inline ScopeGuard<F> make_scope_guard(F &&function_)
{
    return ScopeGuard<F>(std::forward<F>(function_));
}

#define SCOPE_EXIT_CONCAT(n, ...) const auto scope_exit##n = make_scope_guard([&] { __VA_ARGS__; })
#define SCOPE_EXIT_FWD(n, ...) SCOPE_EXIT_CONCAT(n, __VA_ARGS__)
#define SCOPE_EXIT(...) SCOPE_EXIT_FWD(__LINE__, __VA_ARGS__)
