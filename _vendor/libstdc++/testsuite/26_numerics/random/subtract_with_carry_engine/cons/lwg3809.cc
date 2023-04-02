// { dg-do run { target c++11 } }
#include <random>
#include <testsuite_hooks.h>

// LWG 3809. Is std::subtract_with_carry_engine<uint16_t> supposed to work?
// PR 107466 - invalid -Wnarrowing error with std::subtract_with_carry_engine

int main()
{
  // It should be possible to construct this engine with a 16-bit result_type:
  std::subtract_with_carry_engine<uint16_t, 12, 5, 12> s16;
  std::subtract_with_carry_engine<uint32_t, 12, 5, 12> s32;
  // It should have been seeded with the same sequence as the 32-bit version
  // and produce random numbers in the same range, [0, 1<<12).
  for (int i = 0; i < 10; ++i)
    VERIFY( s16() == s32() );
  // The default seed should be usable without truncation to uint16_t:
  s16.seed();
  s32.seed();
  for (int i = 0; i < 10; ++i)
    VERIFY( s16() == s32() );
  s16.seed(101);
  s32.seed(101);
  for (int i = 0; i < 10; ++i)
    VERIFY( s16() == s32() );
}

