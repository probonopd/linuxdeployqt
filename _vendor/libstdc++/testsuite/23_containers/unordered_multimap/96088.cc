// { dg-do run { target c++17 } }
// { dg-require-effective-target std_allocator_new }

// Copyright (C) 2021-2023 Free Software Foundation, Inc.
//
// This file is part of the GNU ISO C++ Library.  This library is free
// software; you can redistribute it and/or modify it under the
// terms of the GNU General Public License as published by the
// Free Software Foundation; either version 3, or (at your option)
// any later version.

// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

// You should have received a copy of the GNU General Public License along
// with this library; see the file COPYING3.  If not see
// <http://www.gnu.org/licenses/>.

// libstdc++/96088

#include <string_view>
#include <string>
#include <unordered_map>

#include <testsuite_hooks.h>
#include <replacement_memory_operators.h>

static constexpr std::initializer_list<std::pair<const char*, int>> lst = {
    {"long_str_for_dynamic_allocating", 1}
};

void
test01()
{
  __gnu_test::counter::reset();
  std::unordered_multimap<std::string, int,
			  std::hash<std::string_view>,
			  std::equal_to<std::string_view>> foo;
  foo.insert(lst.begin(), lst.end());
  VERIFY( foo.size() == 1 );

  VERIFY( __gnu_test::counter::count() == 3 );
  VERIFY( __gnu_test::counter::get()._M_increments == 3 );
}

void
test02()
{
  __gnu_test::counter::reset();
  std::unordered_multimap<std::string, int> foo;
  foo.insert(lst.begin(), lst.end());
  VERIFY( foo.size() == 1 );

  VERIFY( __gnu_test::counter::count() == 3 );
  VERIFY( __gnu_test::counter::get()._M_increments == 3 );
}

int
main()
{
  test01();
  test02();
  return 0;
}

