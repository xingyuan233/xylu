#pragma once

/**
 * @file    stringheap_test.h
 * @brief   LString基础功能测试
 * @date    2025-12-28
 */

#include <iostream>
#include "../detail/xystring/strheap.h"

using namespace xyu;

std::ostream& operator<<(std::ostream& os, const LString& s)
{ return os << s.data(); }

int main2()
{
    std::cout << "=== Testing LString (Heap String) ===\n\n";

    {
        std::cout << "1. Construction & Assignment:\n";
        LString s1;
        std::cout << "  Default: '" << s1 << "' (sz=" << s1.size() << ", cap=" << s1.capacity() << ")\n";

        LString s2("Hello");
        std::cout << "  From C-Str: '" << s2 << "' (sz=" << s2.size() << ", cap=" << s2.capacity() << ")\n";

        LString s3 = s2;
        std::cout << "  Copy Construct: '" << s3 << "'\n";

        LString s4 = std::move(s3);
        std::cout << "  Move Construct: '" << s4 << "' (s3 should be empty: '" << s3 << "')\n";
    }
    std::cout << "\n";

    {
        std::cout << "2. Append & Expansion:\n";
        LString s("Hello");
        std::cout << "  Init: '" << s << "' (cap=" << s.capacity() << ")\n";

        s.append(" World");
        std::cout << "  Append ' World': '" << s << "' (sz=" << s.size() << ", cap=" << s.capacity() << ")\n";

        s.append("! This is a long string to trigger reallocation.");
        std::cout << "  Append Long: '" << s << "'\n";
        std::cout << "  Current Cap: " << s.capacity() << "\n";
    }
    std::cout << "\n";

    {
        std::cout << "3. Insert & Replace:\n";
        LString s("0123456789");
        std::cout << "  Init: '" << s << "'\n";

        s.insert(5, "ABC");
        std::cout << "  Insert(5, 'ABC'): '" << s << "'\n";

        s.replace(0, 2, "Start");
        std::cout << "  Replace(0, 2, 'Start'): '" << s << "'\n";
    }
    std::cout << "\n";

    {
        std::cout << "4. Resize & Clear:\n";
        LString s("Test");
        s.resize(10, '.');
        std::cout << "  Resize(10, '.'): '" << s << "'\n";

        s.resize(2);
        std::cout << "  Resize(2): '" << s << "'\n";

        s.clear();
        std::cout << "  Clear: '" << s << "' (sz=" << s.size() << ")\n";
    }
    std::cout << "\n";

    {
        std::cout << "5. Search (inherited from StringViewBase):\n";
        LString s("The quick brown fox");
        std::cout << "  String: '" << s << "'\n";
        std::cout << "  find('quick'): " << s.find(StringView("quick")) << "\n";
        std::cout << "  find('lazy'): " << s.find(StringView("lazy")) << " (-1 expected)\n";

        // 验证 operator[]
        std::cout << "  s[4]: " << s[4] << "\n";
    }

    return 0;
}