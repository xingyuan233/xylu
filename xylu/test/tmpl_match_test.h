#include "../detail/xytraits/kind.h"

using namespace xyu;

template <typename T, typename = t_enable<T::value <= 1>>
size_c<1> match_test(char&&);
template <typename T, typename... Args, typename = t_enable<T::value <= 2>>
size_c<2> match_test(char&&, Args...);
template <typename T, typename Arg, typename = t_enable<T::value <= 3>>
size_c<3> match_test(Arg&&);
template <typename T, typename Arg, typename... Args, typename = t_enable<T::value <= 4>>
size_c<4> match_test(Arg&&, Args...);
template <typename T, typename... Args, typename = t_enable<T::value <= 5>>
size_c<5> match_test(Args&&...);

template <typename T, typename = t_enable<T::value <= 6>>
size_c<6> match_test(const char&&);
template <typename T, typename... Args, typename = t_enable<T::value <= 7>>
size_c<7> match_test(const char&&, Args...);
template <typename T, typename Arg, typename = t_enable<T::value <= 8>>
size_c<8> match_test(const Arg&&);
template <typename T, typename Arg, typename... Args, typename = t_enable<T::value <= 9>>
size_c<9> match_test(const Arg&&, Args...);
template <typename T, typename... Args, typename = t_enable<T::value <= 10>>
size_c<10> match_test(const Args&&...);

template <typename T, typename = t_enable<T::value <= 11>>
size_c<11> match_test(const char&);
template <typename T, typename... Args, typename = t_enable<T::value <= 12>>
size_c<12> match_test(const char&, Args...);
template <typename T, typename Arg, typename = t_enable<T::value <= 13>>
size_c<13> match_test(const Arg&);
template <typename T, typename Arg, typename... Args, typename = t_enable<T::value <= 14>>
size_c<14> match_test(const Arg&, Args...);
template <typename T, typename... Args, typename = t_enable<T::value <= 15>>
size_c<15> match_test(const Args&...);

template <typename T, typename = t_enable<T::value <= 20>>
size_c<20> match_test(int&&);
template <typename T, typename... Args, typename = t_enable<T::value <= 21>>
size_c<21> match_test(int&&, Args...);
template <typename T, typename = t_enable<T::value <= 22>>
size_c<22> match_test(const int&&);
template <typename T, typename... Args, typename = t_enable<T::value <= 23>>
size_c<23> match_test(const int&&, Args...);
template <typename T, typename = t_enable<T::value <= 24>>
size_c<24> match_test(const int&);
template <typename T, typename... Args, typename = t_enable<T::value <= 25>>
size_c<25> match_test(const int&, Args...);

template <typename T, typename = t_enable<T::value <= 30>>
size_c<30> match_test(double&&);
template <typename T, typename... Args, typename = t_enable<T::value <= 31>>
size_c<31> match_test(double&&, Args...);
template <typename T, typename = t_enable<T::value <= 32>>
size_c<32> match_test(const double&&);
template <typename T, typename... Args, typename = t_enable<T::value <= 33>>
size_c<33> match_test(const double&&, Args...);
template <typename T, typename = t_enable<T::value <= 34>>
size_c<34> match_test(const double&);
template <typename T, typename... Args, typename = t_enable<T::value <= 35>>
size_c<35> match_test(const double&, Args...);

template <typename T>
size_c<0> match_test(...);


struct S { static constexpr int value = 30; };
void test_mathc()
{
    constexpr auto level = decltype(match_test<S>('0'))::value;
}
