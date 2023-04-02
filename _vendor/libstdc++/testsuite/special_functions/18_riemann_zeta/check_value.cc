// { dg-do run { target c++11 } }
// { dg-options "-D__STDCPP_WANT_MATH_SPEC_FUNCS__" }
//
// Copyright (C) 2016-2023 Free Software Foundation, Inc.
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

//  riemann_zeta

// This can take long on simulators, timing out the test.
// { dg-additional-options "-DMAX_ITERATIONS=5" { target simulator } }

#ifndef MAX_ITERATIONS
#define MAX_ITERATIONS (sizeof(data001) / sizeof(testcase_riemann_zeta<double>))
#endif

//  Compare against values generated by the GNU Scientific Library.
//  The GSL can be found on the web: http://www.gnu.org/software/gsl/
#include <limits>
#include <cmath>
#if defined(__TEST_DEBUG)
#  include <iostream>
#  define VERIFY(A) \
  if (!(A)) \
    { \
      std::cout << "line " << __LINE__ \
	<< "  max_abs_frac = " << max_abs_frac \
	<< std::endl; \
    }
#else
#  include <testsuite_hooks.h>
#endif
#include <specfun_testcase.h>

// Test data.
// max(|f - f_Boost|): 6.6613381477509392e-16 at index 52
// max(|f - f_Boost| / |f_Boost|): 2.1066054193139689e-15
// mean(f - f_Boost): -2.3962839288606896e-17
// variance(f - f_Boost): 1.0830580134580974e-35
// stddev(f - f_Boost): 3.2909846755311660e-18
const testcase_riemann_zeta<double>
data001[55] =
{
  { 0.0000000000000000, -10.000000000000000, 0.0 },
  { -0.0033669820451019570, -9.8000000000000007, 0.0 },
  { -0.0058129517767319012, -9.5999999999999996, 0.0 },
  { -0.0072908732290556961, -9.4000000000000004, 0.0 },
  { -0.0078420910654484390, -9.1999999999999993, 0.0 },
  { -0.0075757575757575760, -9.0000000000000000, 0.0 },
  { -0.0066476555677551889, -8.8000000000000007, 0.0 },
  { -0.0052400095350859403, -8.5999999999999996, 0.0 },
  { -0.0035434308017674907, -8.4000000000000004, 0.0 },
  { -0.0017417330388368587, -8.1999999999999993, 0.0 },
  { 0.0000000000000000, -8.0000000000000000, 0.0 },
  { 0.0015440036789213937, -7.7999999999999998, 0.0 },
  { 0.0027852131086497402, -7.5999999999999996, 0.0 },
  { 0.0036537321227995885, -7.4000000000000004, 0.0 },
  { 0.0041147930817053528, -7.1999999999999993, 0.0 },
  { 0.0041666666666666666, -7.0000000000000000, 0.0 },
  { 0.0038369975032738362, -6.7999999999999998, 0.0 },
  { 0.0031780270571782998, -6.5999999999999996, 0.0 },
  { 0.0022611282027338573, -6.4000000000000004, 0.0 },
  { 0.0011710237049390457, -6.1999999999999993, 0.0 },
  { 0.0000000000000000, -6.0000000000000000, 0.0 },
  { -0.0011576366649881868, -5.7999999999999998, 0.0 },
  { -0.0022106784318564332, -5.5999999999999996, 0.0 },
  { -0.0030755853460586913, -5.3999999999999995, 0.0 },
  { -0.0036804380477934761, -5.1999999999999993, 0.0 },
  { -0.0039682539682539680, -5.0000000000000000, 0.0 },
  { -0.0038996891301999771, -4.7999999999999998, 0.0 },
  { -0.0034551830834302698, -4.5999999999999996, 0.0 },
  { -0.0026366345018725037, -4.3999999999999995, 0.0 },
  { -0.0014687209305056920, -4.1999999999999993, 0.0 },
  { 0.0000000000000000, -4.0000000000000000, 0.0 },
  { 0.0016960463875825209, -3.7999999999999998, 0.0 },
  { 0.0035198355903356634, -3.5999999999999996, 0.0 },
  { 0.0053441503206513464, -3.3999999999999995, 0.0 },
  { 0.0070119720770910566, -3.1999999999999993, 0.0 },
  { 0.0083333333333333332, -3.0000000000000000, 0.0 },
  { 0.0090807294856852499, -2.7999999999999998, 0.0 },
  { 0.0089824623788396559, -2.5999999999999996, 0.0 },
  { 0.0077130239874243379, -2.3999999999999995, 0.0 },
  { 0.0048792123593035816, -2.1999999999999993, 0.0 },
  { 0.0000000000000000, -2.0000000000000000, 0.0 },
  { -0.0075229347765968738, -1.7999999999999989, 0.0 },
  { -0.018448986678963719, -1.5999999999999996, 0.0 },
  { -0.033764987694047545, -1.4000000000000004, 0.0 },
  { -0.054788441243880513, -1.1999999999999993, 0.0 },
  { -0.083333333333333329, -1.0000000000000000, 0.0 },
  { -0.12198707766977138, -0.79999999999999893, 0.0 },
  { -0.17459571193801349, -0.59999999999999964, 0.0 },
  { -0.24716546083171545, -0.39999999999999858, 0.0 },
  { -0.34966628059831456, -0.19999999999999929, 0.0 },
  { -0.50000000000000000, 0.0000000000000000, 0.0 },
  { -0.73392092489634220, 0.20000000000000107, 0.0 },
  { -1.1347977838669825, 0.40000000000000036, 0.0 },
  { -1.9526614482240094, 0.60000000000000142, 0.0 },
  { -4.4375384158955686, 0.80000000000000071, 0.0 },
};
const double toler001 = 2.5000000000000020e-13;

// Test data.
// max(|f - f_Boost|): 8.8817841970012523e-16 at index 1
// max(|f - f_Boost| / |f_Boost|): 2.8599739118398860e-16
// mean(f - f_Boost): -1.5313421029312504e-18
// variance(f - f_Boost): 1.6397870961151434e-38
// stddev(f - f_Boost): 1.2805417197870374e-19
const testcase_riemann_zeta<double>
data002[145] =
{
  { 5.5915824411777519, 1.2000000000000000, 0.0 },
  { 3.1055472779775810, 1.3999999999999999, 0.0 },
  { 2.2857656656801297, 1.6000000000000001, 0.0 },
  { 1.8822296181028220, 1.8000000000000000, 0.0 },
  { 1.6449340668482264, 2.0000000000000000, 0.0 },
  { 1.4905432565068935, 2.2000000000000002, 0.0 },
  { 1.3833428588407355, 2.4000000000000004, 0.0 },
  { 1.3054778090727805, 2.6000000000000001, 0.0 },
  { 1.2470314223172534, 2.7999999999999998, 0.0 },
  { 1.2020569031595942, 3.0000000000000000, 0.0 },
  { 1.1667733709844670, 3.2000000000000002, 0.0 },
  { 1.1386637757280416, 3.4000000000000004, 0.0 },
  { 1.1159890791233376, 3.6000000000000001, 0.0 },
  { 1.0975105764590043, 3.8000000000000003, 0.0 },
  { 1.0823232337111381, 4.0000000000000000, 0.0 },
  { 1.0697514772338095, 4.2000000000000002, 0.0 },
  { 1.0592817259798355, 4.4000000000000004, 0.0 },
  { 1.0505173825665735, 4.5999999999999996, 0.0 },
  { 1.0431480133351789, 4.8000000000000007, 0.0 },
  { 1.0369277551433700, 5.0000000000000000, 0.0 },
  { 1.0316598766779166, 5.2000000000000002, 0.0 },
  { 1.0271855389203539, 5.4000000000000004, 0.0 },
  { 1.0233754792270300, 5.6000000000000005, 0.0 },
  { 1.0201237683883446, 5.8000000000000007, 0.0 },
  { 1.0173430619844492, 6.0000000000000000, 0.0 },
  { 1.0149609451852231, 6.2000000000000002, 0.0 },
  { 1.0129170887121841, 6.4000000000000004, 0.0 },
  { 1.0111610141542711, 6.6000000000000005, 0.0 },
  { 1.0096503223447120, 6.8000000000000007, 0.0 },
  { 1.0083492773819229, 7.0000000000000000, 0.0 },
  { 1.0072276664807172, 7.2000000000000002, 0.0 },
  { 1.0062598756930512, 7.4000000000000004, 0.0 },
  { 1.0054241359879634, 7.6000000000000005, 0.0 },
  { 1.0047019048164696, 7.8000000000000007, 0.0 },
  { 1.0040773561979444, 8.0000000000000000, 0.0 },
  { 1.0035369583062013, 8.1999999999999993, 0.0 },
  { 1.0030691220374448, 8.4000000000000004, 0.0 },
  { 1.0026639074861505, 8.6000000000000014, 0.0 },
  { 1.0023127779098220, 8.8000000000000007, 0.0 },
  { 1.0020083928260821, 9.0000000000000000, 0.0 },
  { 1.0017444334995897, 9.2000000000000011, 0.0 },
  { 1.0015154553480514, 9.4000000000000004, 0.0 },
  { 1.0013167628052648, 9.5999999999999996, 0.0 },
  { 1.0011443029840295, 9.8000000000000007, 0.0 },
  { 1.0009945751278180, 10.000000000000000, 0.0 },
  { 1.0008645533615088, 10.200000000000001, 0.0 },
  { 1.0007516206744651, 10.400000000000000, 0.0 },
  { 1.0006535124140850, 10.600000000000001, 0.0 },
  { 1.0005682678503411, 10.800000000000001, 0.0 },
  { 1.0004941886041194, 11.000000000000000, 0.0 },
  { 1.0004298029239942, 11.200000000000001, 0.0 },
  { 1.0003738349551166, 11.400000000000000, 0.0 },
  { 1.0003251782761946, 11.600000000000001, 0.0 },
  { 1.0002828730909989, 11.800000000000001, 0.0 },
  { 1.0002460865533080, 12.000000000000000, 0.0 },
  { 1.0002140957818750, 12.200000000000001, 0.0 },
  { 1.0001862731874054, 12.400000000000000, 0.0 },
  { 1.0001620737887460, 12.600000000000001, 0.0 },
  { 1.0001410242422091, 12.800000000000001, 0.0 },
  { 1.0001227133475785, 13.000000000000000, 0.0 },
  { 1.0001067838280169, 13.200000000000001, 0.0 },
  { 1.0000929252097515, 13.400000000000000, 0.0 },
  { 1.0000808676518720, 13.600000000000001, 0.0 },
  { 1.0000703765974504, 13.800000000000001, 0.0 },
  { 1.0000612481350588, 14.000000000000000, 0.0 },
  { 1.0000533049750668, 14.200000000000001, 0.0 },
  { 1.0000463929582293, 14.400000000000000, 0.0 },
  { 1.0000403780253397, 14.600000000000001, 0.0 },
  { 1.0000351435864272, 14.800000000000001, 0.0 },
  { 1.0000305882363070, 15.000000000000000, 0.0 },
  { 1.0000266237704787, 15.200000000000001, 0.0 },
  { 1.0000231734615617, 15.400000000000000, 0.0 },
  { 1.0000201705617975, 15.600000000000001, 0.0 },
  { 1.0000175570017611, 15.800000000000001, 0.0 },
  { 1.0000152822594086, 16.000000000000000, 0.0 },
  { 1.0000133023770335, 16.200000000000003, 0.0 },
  { 1.0000115791066833, 16.399999999999999, 0.0 },
  { 1.0000100791671644, 16.600000000000001, 0.0 },
  { 1.0000087735980012, 16.800000000000001, 0.0 },
  { 1.0000076371976379, 17.000000000000000, 0.0 },
  { 1.0000066480348633, 17.199999999999999, 0.0 },
  { 1.0000057870238737, 17.400000000000002, 0.0 },
  { 1.0000050375546610, 17.600000000000001, 0.0 },
  { 1.0000043851715013, 17.800000000000001, 0.0 },
  { 1.0000038172932650, 18.000000000000000, 0.0 },
  { 1.0000033229700953, 18.199999999999999, 0.0 },
  { 1.0000028926717150, 18.400000000000002, 0.0 },
  { 1.0000025181032419, 18.600000000000001, 0.0 },
  { 1.0000021920449285, 18.800000000000001, 0.0 },
  { 1.0000019082127165, 19.000000000000000, 0.0 },
  { 1.0000016611368951, 19.199999999999999, 0.0 },
  { 1.0000014460565094, 19.400000000000002, 0.0 },
  { 1.0000012588274738, 19.600000000000001, 0.0 },
  { 1.0000010958426055, 19.800000000000001, 0.0 },
  { 1.0000009539620338, 20.000000000000000, 0.0 },
  { 1.0000008304526344, 20.200000000000003, 0.0 },
  { 1.0000007229353187, 20.400000000000002, 0.0 },
  { 1.0000006293391575, 20.600000000000001, 0.0 },
  { 1.0000005478614529, 20.800000000000001, 0.0 },
  { 1.0000004769329869, 21.000000000000000, 0.0 },
  { 1.0000004151877719, 21.200000000000003, 0.0 },
  { 1.0000003614367252, 21.400000000000002, 0.0 },
  { 1.0000003146447527, 21.600000000000001, 0.0 },
  { 1.0000002739108020, 21.800000000000001, 0.0 },
  { 1.0000002384505027, 22.000000000000000, 0.0 },
  { 1.0000002075810521, 22.200000000000003, 0.0 },
  { 1.0000001807080625, 22.400000000000002, 0.0 },
  { 1.0000001573141093, 22.600000000000001, 0.0 },
  { 1.0000001369487659, 22.800000000000001, 0.0 },
  { 1.0000001192199259, 23.000000000000000, 0.0 },
  { 1.0000001037862518, 23.200000000000003, 0.0 },
  { 1.0000000903506006, 23.400000000000002, 0.0 },
  { 1.0000000786543011, 23.600000000000001, 0.0 },
  { 1.0000000684721728, 23.800000000000001, 0.0 },
  { 1.0000000596081891, 24.000000000000000, 0.0 },
  { 1.0000000518917020, 24.200000000000003, 0.0 },
  { 1.0000000451741575, 24.400000000000002, 0.0 },
  { 1.0000000393262332, 24.600000000000001, 0.0 },
  { 1.0000000342353501, 24.800000000000001, 0.0 },
  { 1.0000000298035034, 25.000000000000000, 0.0 },
  { 1.0000000259453767, 25.200000000000003, 0.0 },
  { 1.0000000225866978, 25.400000000000002, 0.0 },
  { 1.0000000196628109, 25.600000000000001, 0.0 },
  { 1.0000000171174297, 25.800000000000001, 0.0 },
  { 1.0000000149015549, 26.000000000000000, 0.0 },
  { 1.0000000129725302, 26.200000000000003, 0.0 },
  { 1.0000000112932221, 26.400000000000002, 0.0 },
  { 1.0000000098313035, 26.600000000000001, 0.0 },
  { 1.0000000085586331, 26.800000000000001, 0.0 },
  { 1.0000000074507118, 27.000000000000000, 0.0 },
  { 1.0000000064862125, 27.200000000000003, 0.0 },
  { 1.0000000056465688, 27.400000000000002, 0.0 },
  { 1.0000000049156179, 27.600000000000001, 0.0 },
  { 1.0000000042792894, 27.800000000000001, 0.0 },
  { 1.0000000037253340, 28.000000000000000, 0.0 },
  { 1.0000000032430887, 28.200000000000003, 0.0 },
  { 1.0000000028232703, 28.400000000000002, 0.0 },
  { 1.0000000024577975, 28.600000000000001, 0.0 },
  { 1.0000000021396356, 28.800000000000001, 0.0 },
  { 1.0000000018626598, 29.000000000000000, 0.0 },
  { 1.0000000016215385, 29.200000000000003, 0.0 },
  { 1.0000000014116304, 29.400000000000002, 0.0 },
  { 1.0000000012288950, 29.600000000000001, 0.0 },
  { 1.0000000010698147, 29.800000000000001, 0.0 },
  { 1.0000000009313275, 30.000000000000000, 0.0 },
};
const double toler002 = 2.5000000000000020e-13;

template<typename Ret, unsigned int Num>
  void
  test(const testcase_riemann_zeta<Ret> (&data)[Num], Ret toler)
  {
    bool test __attribute__((unused)) = true;
    const Ret eps = std::numeric_limits<Ret>::epsilon();
    Ret max_abs_diff = -Ret(1);
    Ret max_abs_frac = -Ret(1);
    unsigned int num_datum = MAX_ITERATIONS;
    for (unsigned int i = 0; i < num_datum; ++i)
      {
	const Ret f = std::riemann_zeta(data[i].s);
	const Ret f0 = data[i].f0;
	const Ret diff = f - f0;
	if (std::abs(diff) > max_abs_diff)
	  max_abs_diff = std::abs(diff);
	if (std::abs(f0) > Ret(10) * eps
	 && std::abs(f) > Ret(10) * eps)
	  {
	    const Ret frac = diff / f0;
	    if (std::abs(frac) > max_abs_frac)
	      max_abs_frac = std::abs(frac);
	  }
      }
    VERIFY(max_abs_frac < toler);
  }

int
main()
{
  test(data001, toler001);
  test(data002, toler002);
  return 0;
}

