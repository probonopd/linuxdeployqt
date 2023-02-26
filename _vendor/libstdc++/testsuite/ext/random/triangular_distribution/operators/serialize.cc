// { dg-do run { target c++11 } }
// { dg-require-cstdint "" }
//
// 2009-08-14  Edward M. Smith-Rowland  <3dw4rd@verizon.net>
// 2013-03-02  Ulrich Drepper  <drepper@gmail.com>
//
// Copyright (C) 2013-2023 Free Software Foundation, Inc.
//
// This file is part of the GNU ISO C++ Library.  This library is free
// software; you can redistribute it and/or modify it under the
// terms of the GNU General Public License as published by the
// Free Software Foundation; either version 3, or (at your option)
// any later version.
//
// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License along
// with this library; see the file COPYING3.  If not see
// <http://www.gnu.org/licenses/>.

#include <ext/random>
#include <sstream>
#include <testsuite_hooks.h>

void
test01()
{
  std::stringstream str;
  __gnu_cxx::triangular_distribution<double> u(1.5, 3.0, 4.5), v;
  std::minstd_rand0 rng;

  u(rng); // advance
  str << u;

  str >> v;
  VERIFY( u == v );
}

int main()
{
  test01();
  return 0;
}

