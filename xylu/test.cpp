#include <xylu/xystring/fmt.h>
#include <xylu/xyrange/ops.h>

int main()
{
    xyu::StringView sv = "hello_world";
    xyu::String str;
    for (auto c : sv.range()
        | xyu::RangeOps::split([](char c) { return c == '_'; })
        | xyu::RangeOps::join(',') | xyu::RangeOps::concat(xyu::StringView{"!"}))
    str << c;
    str.replacec(str.data(), "hw", 0xE0u);
    xyu::xyout << str;

    return 0;
}
