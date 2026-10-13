#pragma once

#include "openssl/bn.h"

#include "../detail/xystring/fmt.h"
#include "../detail/xycoll/nearlist.h"
#include "../detail/xyrange/ops.h"
#include "../detail/xymath/random.h"
#include "../detail/xymath/num.h"

using namespace xyu;

void test_conv(int len, int n, int m)
{
    xyout << xyfmt("test: len={}, n={}, m={}\n", len, n, m);

    Clock clock;
    volatile char* buf = alloc<char>(len + 1);

    RandInt<RandPCG32> rand;
    char table[16 + 1] = "1123456789abcdef";
    for (int i=0; i<len; ++i) buf[i] = table[rand(0, 16)];
    buf[len] = '\0';
    if (len <= 128) xyout << (char*)buf << '\n';

    BIGNUM* bignum = BN_new();
    BigNum bn{};

    auto past = clock.stop();

    for (int i=0; i<n; ++i)
    {
        if (i == m) clock.start();
        BN_bin2bn((uint8*)buf, len, bignum);
        if (i == 0 && len <= 128) xyout << BN_bn2hex(bignum) << '\n';
    }
    past = clock.stop();
    xyout << "BIGNUM - from_bebin: " << past << " " << (double)past.ns() / n << "ns\n";

    for (int i=0; i<n; ++i)
    {
        if (i == m) clock.start();
        bn.from_bin_be({(char*)buf, (uint)len});
        if (i == 0 && len <= 128) xyout << bn.to_hex() << '\n';
    }
    past = clock.stop();
    xyout << "xyu - from_bebin: " << past << " " << (double)past.ns() / n << "ns\n";

    for (int i=0; i<n; ++i)
    {
        if (i == m) clock.start();
        volatile auto p = xyu::alloc<uint8>(len);
        BN_bn2bin(bignum, p);
        if (i == 0 && len <= 128)
        {
            auto q = p;
            while (*q) xyout << (int)*q++ << ',';
            xyout << '\n';
        }
        xyu::dealloc(p, len);
    }
    past = clock.stop();
    xyout << "BIGNUM - to_bebin: " << past << " " << (double)past.ns() / n << "ns\n";

    for (int i=0; i<n; ++i)
    {
        if (i == m) clock.start();
        volatile String str = bn.to_bin_be();
        if (i == 0 && len <= 128)
        {
            for (int c : ((String&)str).range()) xyout << c << ',';
            xyout << '\n';
        }
    }
    past = clock.stop();
    xyout << "xyu - to_bebin: " << past << " " << (double)past.ns() / n << "ns\n";

    for (int i=0; i<n; ++i)
    {
        if (i == m) clock.start();
        BN_lebin2bn((uint8*)buf, len, bignum);
        if (i == 0 && len <= 128) xyout << BN_bn2hex(bignum) << '\n';
    }
    past = clock.stop();
    xyout << "BIGNUM - from_lebin: " << past << " " << (double)past.ns() / n << "ns\n";

    for (int i=0; i<n; ++i)
    {
        if (i == m) clock.start();
        bn.from_bin_le((char*)buf);
        if (i == 0 && len <= 128) xyout << bn.to_hex() << '\n';
    }
    past = clock.stop();
    xyout << "xyu - from_lebin: " << past << " " << (double)past.ns() / n << "ns\n";

    for (int i=0; i<n; ++i)
    {
        if (i == m) clock.start();
        volatile auto p = xyu::alloc<uint8>(len);
        BN_bn2lebinpad(bignum, p, len);
        if (i == 0 && len <= 128)
        {
            auto q = p;
            while (*q) xyout << (int)*q++ << ',';
            xyout << '\n';
        }
        xyu::dealloc(p, len);
    }
    past = clock.stop();
    xyout << "BIGNUM - to_lebin: " << past << " " << (double)past.ns() / n << "ns\n";

    for (int i=0; i<n; ++i)
    {
        if (i == m) clock.start();
        volatile String str = bn.to_bin_le();
        if (i == 0 && len <= 128)
        {
            for (int c : ((String&)str).range()) xyout << c << ',';
            xyout << '\n';
        }
    }
    past = clock.stop();
    xyout << "xyu - to_lebin: " << past << " " << (double)past.ns() / n << "ns\n";

    for (int i=0; i<n; ++i)
    {
        if (i == m) clock.start();
        BN_hex2bn(&bignum, (char*)buf);
    }
    past = clock.stop();
    xyout << "BIGNUM - from_hex: " << past << " " << (double)past.ns() / n << "ns\n";

    for (int i=0; i<n; ++i)
    {
        if (i == m) clock.start();
        bn.from_hex((char*)buf);
    }
    past = clock.stop();
    xyout << "xyu - from_hex: " << past << " " << (double)past.ns() / n << "ns\n";

    for (int i=0; i<n; ++i)
    {
        if (i == m) clock.start();
        volatile char* obuf = BN_bn2hex(bignum);
    }
    past = clock.stop();
    xyout << "BIGNUM - to_hex: " << past << " " << (double)past.ns() / n << "ns\n";

    for (int i=0; i<n; ++i)
    {
        if (i == m) clock.start();
        volatile String str = bn.to_hex();
    }
    past = clock.stop();
    xyout << "xyu - to_hex: " << past << " " << (double)past.ns() / n << "ns\n";

    BN_free(bignum);
    dealloc((char*)buf, len + 1);
    xyout << '\n';
}

void test_bit(int len, int n, int m)
{
    Clock clock;
    xyout << xyfmt("test: len={}, n={}, m={}\n", len, n, m);
    auto past = clock.stop();

    volatile char* buf = alloc<char>(len + 1);

    RandInt<RandPCG32> rand;
    char table[16 + 1] = "1123456789abcdef";
    for (int i=0; i<len; ++i) buf[i] = table[rand(0, 16)];
    buf[len] = '\0';

    BigNum oor;
    oor.from_hex(String{}.append('f', 16));
    BigNum oand;
    oand.from_hex(String{}.append('f', len - 14).append('0', 14));

    BIGNUM* bignum = BN_new();
    BN_hex2bn(&bignum, (char*)buf);
    BigNum bn;
    bn.from_hex({(char*)buf, (uint)len});
    if (len <= 128) xyout << bn.count() << ' ' << bn.words() << ' ' << bn.to_hex() << '\n';
    // xyout << bn.to_hex() << '\n';

    BigNum bn2;
    BIGNUM* bignum2 = BN_new();

    for (int i=0; i<n; ++i)
    {
        if (i == m) clock.start();
        bn.bit_not(bn2);
        // if (i == 0 && len <= 128) xyout << "xyu: " << bn2.to_hex() << '\n';
    }
    past = clock.stop();
    xyout << "xyu not: " << past << " " << (double)past.ns() / n << "ns\n";

    for (int i=0; i<n; ++i)
    {
        if (i == m) clock.start();
        bn.bit_and(oand, bn2);
        if (i == 0 && len <= 128) xyout << "xyu: " << bn2.to_hex() << '\n';
    }
    past = clock.stop();
    xyout << "xyu and: " << past << " " << (double)past.ns() / n << "ns\n";

    for (int i=0; i<n; ++i)
    {
        if (i == m) clock.start();
        bn.bit_or(oor, bn2);
        if (i == 0 && len <= 128) xyout << "xyu: " << bn2.to_hex() << '\n';
    }
    past = clock.stop();
    xyout << "xyu or: " << past << " " << (double)past.ns() / n << "ns\n";

    for (int i=0; i<n; ++i)
    {
        if (i == m) clock.start();
        bn.bit_xor(oor, bn2);
        if (i == 0 && len <= 128) xyout << "xyu: " << bn2.to_hex() << '\n';
    }
    past = clock.stop();
    xyout << "xyu xor: " << past << " " << (double)past.ns() / n << "ns\n";

    int off = 1;

    for (int i=0; i<n; ++i)
    {
        if (i == m) clock.start();
        bn.bit_lshift(off, bn2);
        if (i == 0 && len <= 128) xyout << "xyu: " << bn2.to_hex(true, false) << '\n';
    }
    past = clock.stop();
    xyout << "xyu l - " << off << " : " << past << " " << (double)past.ns() / n << "ns\n";

    for (int i=0; i<n; ++i)
    {
        if (i == m) clock.start();
        BN_lshift(bignum2, bignum, off);
        if (i == 0 && len <= 128)
        {
            auto p = BN_bn2hex(bignum2);
            xyout << "BIGNUM: " << p << '\n';
        }
    }
    past = clock.stop();
    xyout << "BIGNUM l - " << off << " : " << past << " " << (double)past.ns() / n << "ns\n";

    for (int i=0; i<n; ++i)
    {
        if (i == m) clock.start();
        BN_lshift1(bignum2, bignum);
        if (i == 0 && len <= 128)
        {
            auto p = BN_bn2hex(bignum2);
            xyout << "BIGNUM: " << p << '\n';
        }
    }
    past = clock.stop();
    xyout << "BIGNUM l1: " << past << " " << (double)past.ns() / n << "ns\n";

    for (int i=0; i<n; ++i)
    {
        if (i == m) clock.start();
        bn.bit_rshift(off, bn2);
        if (i == 0 && len <= 128) xyout << "xyu: " << bn2.to_hex(true, false) << '\n';
    }
    past = clock.stop();
    xyout << "xyu r - " << off << " : " << past << " " << (double)past.ns() / n << "ns\n";

    for (int i=0; i<n; ++i)
    {
        if (i == m) clock.start();
        BN_rshift(bignum2, bignum, off);
        if (i == 0 && len <= 128)
        {
            auto p = BN_bn2hex(bignum2);
            xyout << "BIGNUM: " << p << '\n';
        }
    }
    past = clock.stop();
    xyout << "BIGNUM r - " << off << " : " << past << " " << (double)past.ns() / n << "ns\n";

    for (int i=0; i<n; ++i)
    {
        if (i == m) clock.start();
        BN_rshift1(bignum2, bignum);
        if (i == 0 && len <= 128)
        {
            auto p = BN_bn2hex(bignum2);
            xyout << "BIGNUM: " << p << '\n';
        }
    }
    past = clock.stop();
    xyout << "BIGNUM r1: " << past << " " << (double)past.ns() / n << "ns\n";

    BN_free(bignum2);
    BN_free(bignum);
    dealloc((char*)buf, len + 1);
    xyout << '\n';
}

void test_calc(int len, int n, int m)
{
    Clock clock;
    xyout << xyfmt("test: len={}, n={}, m={}\n", len, n, m);
    auto past = clock.stop();

    volatile char* buf = alloc<char>(len + 1);

    RandInt<RandPCG32> rand;
    char table[16 + 1] = "1123456789abcdef";
    for (int i=0; i<len; ++i) buf[i] = table[rand(0, 16)];
    buf[len] = '\0';

    BigNum oor;
    oor.from_hex(String{}.append('f', 16));
    BigNum oand;
    oand.from_hex(String{}.append('f', len - 14).append('0', 14));

    BIGNUM* bignum = BN_new();
    BN_hex2bn(&bignum, (char*)buf);
    BigNum bn;
    bn.from_hex({(char*)buf, (uint)len});
    if (len <= 128) xyout << bn.count() << ' ' << bn.words() << ' ' << bn.to_hex() << '\n';
    // xyout << bn.to_hex() << '\n';

    BigNum bn2;
    BIGNUM* bignum2 = BN_new();

    for (int i=0; i<n; ++i)
    {
        if (i == m) clock.start();
        bn.uadd(bn, bn2);
        if (i == 0 && len <= 128) xyout << "xyu: " << bn2.to_hex() << '\n';
    }
    past = clock.stop();
    xyout << "xyu add: " << past << " " << (double)past.ns() / n << "ns\n";

    for (int i=0; i<n; ++i)
    {
        if (i == m) clock.start();
        bn.uadd2(bn, bn2);
        if (i == 0 && len <= 128) xyout << "xyu: " << bn2.to_hex() << '\n';
    }
    past = clock.stop();
    xyout << "xyu add2: " << past << " " << (double)past.ns() / n << "ns\n";

    for (int i=0; i<n; ++i)
    {
        if (i == m) clock.start();
        BN_uadd(bignum2, bignum, bignum);
        if (i == 0 && len <= 128)
        {
            auto p = BN_bn2hex(bignum2);
            xyout << "BIGNUM: " << p << '\n';
        }
    }
    past = clock.stop();
    xyout << "BIGNUM add: " << past << " " << (double)past.ns() / n << "ns\n";

    BN_free(bignum2);
    BN_free(bignum);
    dealloc((char*)buf, len + 1);
    xyout << '\n';
}
