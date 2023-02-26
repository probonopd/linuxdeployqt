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

// { dg-do compile }
// { dg-options "-std=c++11" }

#include <map>

void
test01()
{
  std::multimap<int, int, std::less<int*>> c;
  c.find(1);  // { dg-error "here" }
  std::multimap<int, int, std::allocator<int>> c2;
  c2.find(2); // { dg-error "here" }
}

// { dg-error "_Compare = std::(__8::)?less<int.>" "" { target *-*-* } 0 }
// { dg-error "_Compare = std::(__8::)?allocator<int>" "" { target *-*-* } 0 }
// { dg-error "comparison object must be invocable" "" { target *-*-* } 0 }
// { dg-prune-output "no match for call" }
// { dg-prune-output "invalid conversion" }

