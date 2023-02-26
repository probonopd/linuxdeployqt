// { dg-do run { target c++17 } }

// Copyright (C) 2014-2023 Free Software Foundation, Inc.
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

// 8.6 path non-member functions [path.non-member]

#include <filesystem>
#include <testsuite_fs.h>
#include <testsuite_hooks.h>

using std::filesystem::path;

void
test01()
{
  VERIFY( hash_value(path("a//b")) == hash_value(path("a/b")) );
  VERIFY( hash_value(path("a/")) == hash_value(path("a//")) );
}

void
test02()
{
  for (const path p : __gnu_test::test_paths)
  {
    path pp = p.native();
    VERIFY( hash_value(p) == hash_value(pp) );
  }
}

void
test03()
{
  std::hash<path> h;
  // LWG 3657. std::hash<std::filesystem::path> is not enabled
  for (const path p : __gnu_test::test_paths)
    VERIFY( h(p) == hash_value(p) );
}

int
main()
{
  test01();
  test02();
  test03();
}

