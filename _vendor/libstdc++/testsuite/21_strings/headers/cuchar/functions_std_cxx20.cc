// { dg-options "-std=c++20" }
// { dg-do compile { target c++20 } }

#include <cuchar>

namespace gnu
{
#if _GLIBCXX_USE_UCHAR_C8RTOMB_MBRTOC8_CXX20
  using std::mbrtoc8;
  using std::c8rtomb;
#endif // _GLIBCXX_USE_UCHAR_C8RTOMB_MBRTOC8_CXX20
}

