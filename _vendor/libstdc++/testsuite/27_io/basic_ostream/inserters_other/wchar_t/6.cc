// { dg-do run { target c++11 } }

// Copyright (C) 2016-2023 Free Software Foundation, Inc.
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

// C++11 27.7.3.9 Rvalue stream insertion [ostream.rvalue]

#include <sstream>

template<typename Ostream, typename T, typename = void>
  struct is_insertable
  : std::false_type
  { };

template<typename> using void_t = void;

template<typename Ostream, typename T>
  using insert_result
    = decltype(std::declval<Ostream>() << std::declval<const T&>());

template<typename Ostream, typename T>
  struct is_insertable<Ostream, T, void_t<insert_result<Ostream, T>>>
  : std::true_type
  { };

struct X {};
std::wostream& operator<<(std::wostream&, const X&) = delete;

struct Y {};
std::wostream& operator<<(std::wostream& os, const Y&) {return os;}
std::wostream& operator<<(std::wostream&& os, const Y&) {throw 1;} // not used

struct Z{};

void test01()
{
  Y y;
  std::wostringstream os;
  os << y;
  os << Y();
  std::wostringstream() << y;
  std::wostringstream() << Y();
  static_assert(!is_insertable<std::wostream&, X&>::value, "");
  static_assert(!is_insertable<std::wostream&&, X&>::value, "");
  static_assert(!is_insertable<std::wostream&, X&&>::value, "");
  static_assert(!is_insertable<std::wostream&&, X&&>::value, "");
  static_assert(is_insertable<std::wostream&, Y&>::value, "");
  static_assert(is_insertable<std::wostream&&, Y&&>::value, "");
  static_assert(is_insertable<std::wostream&, Y&>::value, "");
  static_assert(is_insertable<std::wostream&&, Y&&>::value, "");
  static_assert(!is_insertable<std::wostream&, Z&>::value, "");
  static_assert(!is_insertable<std::wostream&&, Z&>::value, "");
  static_assert(!is_insertable<std::wostream&, Z&&>::value, "");
  static_assert(!is_insertable<std::wostream&&, Z&&>::value, "");
}

int main()
{
  test01();
}

