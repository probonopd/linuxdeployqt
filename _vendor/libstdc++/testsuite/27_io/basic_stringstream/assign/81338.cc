// Copyright (C) 2017-2023 Free Software Foundation, Inc.
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

// { dg-do run { target c++11 } }

#include <sstream>
#include <testsuite_hooks.h>

void
test01()
{
  std::stringstream ss;
  for (int i = 0; i < 100; ++i)
  {
    ss << 'a';
    VERIFY( static_cast<bool>(ss) );
    VERIFY( ss.str() == "a" );
    ss = std::stringstream();
  }
}

int
main()
{
  test01();
}

