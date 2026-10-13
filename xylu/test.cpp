// #include "test.h"
// #include "openssl/bn.h"
// #include "test/string_tallyc_test.h"
// #include <iostream>

#include "filesystem"
#include "detail/xycoll/refval.h"
#include "detail/xymath/random.h"
#include "detail/xystring/fmt.h"
#include "detail/xystring/strfixed.h"
#include "detail/xystring/strheap.h"
#include "detail/xysystem/file.h"
#include "detail/xysystem/path.h"

using namespace xyu;

namespace xylu::xystring::__
{

}
using namespace xylu::xystring::__;

void main2()
{


    auto path = Path{}.get_dir();
    xyout << path.get() << '\n';
    // path = "../release/./1.txt/a/b/../../c";
    path = "/../a/";
    // path.concat("/devoding/Java/../C++");
    xyout << path.get() << '\n';
    // path.set_dir();
    // xyout << path.get_dir().get() << '\n';

    xyout << path.to_absolute().get() << '\n';

    // String str = R"(\\tmp\xingyuan)";
    // String tmp;
    // // replace_char(str, tmp, '\\', '/');
    // xyout << tmp;
}


void test()
{

    int m = 100, n = 100000;
    int lens[] { 32, 64, 256, 512, 1024, 2048, 4096, 4096*4, 4096*16 };
    // int lens[] { 4, 8, 12, 16, 20, 32, 40, 48, 56, 64 };

    RandInt<RandPCG32> rand;
    Clock clock;
    volatile size_t x = 0;
    for (auto len : lens)
    {
        xyout << xyfmt("test m={}, n={}, len={}\n", m, n, len );

        String str;
        char c = rand('a', 'z');
        // for (int i=0; i<len; ++i) str.append(c);
        for (int i=0; i<len+32; ++i)
        {
            if (i < len - len / 5) str.append(rand('a', 'z'));
            else str.append(rand('A', 'z'));
        }
        size_t start = str.size() - str.size() / 5;
        char target = '\xaa';
        StringView cs = "*%#0123456789/*z";
        // str[rand(start, str.size() - 1)] = target;

        String buf(str.size() * 3);
        char* p = (char*)(((size_t)buf.data() + 31) & ~31);
        char* s = (char*)(((size_t)str.data() + 31) & ~31) + 4;

        {

        }

        for (int i=0; i<n; ++i)
        {
            if (i == m) clock.start();
            // replace(s, len, s, 'c', '.');
            //int r = contrast2(str.data(), str.size(), str.data());
            // memxfind(str.data(), 'A', str.size());
            // int r = __builtin_memcmp(str.data(), str.data(), str.size());
            // if (i == 0) xyout << r << ' ';
        }
        xyout << xyfmt("func{}: {}\n", "-inline", clock.stop() / double(n - m));

        for (int i=0; i<n; ++i)
        {
            if (i == m) clock.start();


        }
        xyout << xyfmt("func{}: {}\n", "-1", clock.stop() / double(n - m));

        for (int i=0; i<n; ++i)
        {
            if (i == m) clock.start();


        }
        xyout << xyfmt("func{}: {}\n", "-2", clock.stop() / double(n - m));

        xyout << '\n';

    }

}

int a = 0;

int main()
{
    try {

        Path path;
        path.get_dir();
        path.concat("../a/e");
        // path.set("./a../b/c/../../");
        xyout << path.get() << '\n';
        xyout << path.to_absolute() << '\n';
        xyout << path.to_relative() << '\n';

        // test();

        // main2();

    } catch (...) {
        xyerr << "error catched\n";
    }
    return 0;
}
