// Copyright (C) 2004-2013 Free Software Foundation, Inc.
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

// 21.3.6 string operations

#include <string>
#include <testsuite_hooks.h>

int test01(void)
{
  std::wstring empty;

  // data() for size == 0 is non-NULL.
  VERIFY( empty.size() == 0 );
  const std::wstring::value_type* p = empty.data();
  VERIFY( p );

  return 0;
}

int main()
{ 
  test01();
  return 0;
}

