// -*- C++ -*-
// { dg-options "-ltbb" }
// { dg-do run { target c++17 } }
// { dg-timeout-factor 3 }
// { dg-require-effective-target tbb_backend }

//===-- is_sorted.pass.cpp ------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "pstl/pstl_test_config.h"

#ifdef PSTL_STANDALONE_TESTS

#include "pstl/execution"
#include "pstl/algorithm"
#else
#include <execution>
#include <algorithm>
#endif // PSTL_STANDALONE_TESTS

#include "pstl/test_utils.h"

using namespace TestUtils;

struct test_is_sorted
{
    template <typename Policy, typename Iterator>
    void
    operator()(Policy&& exec, Iterator first, Iterator last, bool exam)
    {
        using namespace std;
        typedef typename std::iterator_traits<Iterator>::value_type T;

        //try random-access iterator
        bool res = is_sorted(exec, first, last);
        EXPECT_TRUE(exam == res, "is_sorted wrong result for random-access iterator");
        auto iexam = is_sorted_until(first, last);
        auto ires = is_sorted_until(exec, first, last);
        EXPECT_TRUE(iexam == ires, "is_sorted_until wrong result for random-access iterator");

        //try random-access iterator with a predicate
        res = is_sorted(exec, first, last, std::less<T>());
        EXPECT_TRUE(exam == res, "is_sorted wrong result for random-access iterator");
        iexam = is_sorted_until(first, last, std::less<T>());
        ires = is_sorted_until(exec, first, last, std::less<T>());
        EXPECT_TRUE(iexam == ires, "is_sorted_until wrong result for random-access iterator");
    }
};

template <typename T>
void
test_is_sorted_by_type()
{

    Sequence<T> in(99999, [](size_t v) -> T { return T(v); }); //fill 0..n

    invoke_on_all_policies(test_is_sorted(), in.begin(), in.end(), std::is_sorted(in.begin(), in.end()));
    invoke_on_all_policies(test_is_sorted(), in.cbegin(), in.cend(), std::is_sorted(in.begin(), in.end()));

    in[in.size() / 2] = -1;
    invoke_on_all_policies(test_is_sorted(), in.begin(), in.end(), std::is_sorted(in.begin(), in.end()));
    invoke_on_all_policies(test_is_sorted(), in.cbegin(), in.cend(), std::is_sorted(in.begin(), in.end()));

    in[1] = -1;
    invoke_on_all_policies(test_is_sorted(), in.begin(), in.end(), std::is_sorted(in.begin(), in.end()));
    invoke_on_all_policies(test_is_sorted(), in.cbegin(), in.cend(), std::is_sorted(in.begin(), in.end()));

    //an empty container
    Sequence<T> in0(0);
    invoke_on_all_policies(test_is_sorted(), in0.begin(), in0.end(), std::is_sorted(in0.begin(), in0.end()));
    invoke_on_all_policies(test_is_sorted(), in0.cbegin(), in0.cend(), std::is_sorted(in0.begin(), in0.end()));

    //non-descending order
    Sequence<T> in1(9, [](size_t v) -> T { return T(0); });
    invoke_on_all_policies(test_is_sorted(), in1.begin(), in1.end(), std::is_sorted(in1.begin(), in1.end()));
    invoke_on_all_policies(test_is_sorted(), in1.cbegin(), in1.cend(), std::is_sorted(in1.begin(), in1.end()));
}

template <typename T>
struct test_non_const
{
    template <typename Policy, typename Iterator>
    void
    operator()(Policy&& exec, Iterator iter)
    {
        is_sorted(exec, iter, iter, std::less<T>());
        is_sorted_until(exec, iter, iter, std::less<T>());
    }
};

int32_t
main()
{

    test_is_sorted_by_type<int32_t>();
    test_is_sorted_by_type<float64_t>();

    test_is_sorted_by_type<Wrapper<int32_t>>();

    test_algo_basic_single<int32_t>(run_for_rnd_fw<test_non_const<int32_t>>());

    std::cout << done() << std::endl;
    return 0;
}

