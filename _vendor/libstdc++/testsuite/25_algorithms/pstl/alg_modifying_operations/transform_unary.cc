// -*- C++ -*-
// { dg-options "-ltbb" }
// { dg-do run { target c++17 } }
// { dg-timeout-factor 3 }
// { dg-require-effective-target tbb_backend }

//===-- transform_unary.pass.cpp ------------------------------------------===//
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

template <typename InputIterator, typename OutputIterator>
void
check_and_reset(InputIterator first, InputIterator last, OutputIterator out_first)
{
    typedef typename std::iterator_traits<OutputIterator>::value_type Out;
    typename std::iterator_traits<OutputIterator>::difference_type k = 0;
    for (; first != last; ++first, ++out_first, ++k)
    {
        // check
        Out expected = 1 - *first;
        Out actual = *out_first;
        EXPECT_EQ(expected, actual, "wrong value in output sequence");
        // reset
        *out_first = k % 7 != 4 ? 7 * k - 5 : 0;
    }
}

struct test_one_policy
{
    template <typename Policy, typename InputIterator, typename OutputIterator, typename UnaryOp>
    void
    operator()(Policy&& exec, InputIterator first, InputIterator last, OutputIterator out_first,
               OutputIterator out_last, UnaryOp op)
    {
        auto orr = std::transform(exec, first, last, out_first, op);
        EXPECT_TRUE(out_last == orr, "transform returned wrong iterator");
        check_and_reset(first, last, out_first);
    }
};

template <typename Tin, typename Tout>
void
test()
{
    for (size_t n = 0; n <= 100000; n = n <= 16 ? n + 1 : size_t(3.1415 * n))
    {
        Sequence<Tin> in(n, [](int32_t k) { return k % 5 != 1 ? 3 * k - 7 : 0; });

        Sequence<Tout> out(n);

        const auto flip = Complement<Tin, Tout>(1);
        invoke_on_all_policies(test_one_policy(), in.begin(), in.end(), out.begin(), out.end(), flip);
        invoke_on_all_policies(test_one_policy(), in.cbegin(), in.cend(), out.begin(), out.end(), flip);
    }
}

template <typename T>
struct test_non_const
{
    template <typename Policy, typename InputIterator, typename OutputInterator>
    void
    operator()(Policy&& exec, InputIterator input_iter, OutputInterator out_iter)
    {
        invoke_if(exec, [&]() { transform(exec, input_iter, input_iter, out_iter, non_const(std::negate<T>())); });
    }
};

int32_t
main()
{
    test<int32_t, int32_t>();
    test<int32_t, float32_t>();
    test<uint16_t, float32_t>();
    test<float32_t, float64_t>();
    test<float64_t, float64_t>();

    test_algo_basic_double<int32_t>(run_for_rnd_fw<test_non_const<int32_t>>());

    std::cout << done() << std::endl;
    return 0;
}

