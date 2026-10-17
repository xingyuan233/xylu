#include "detail/xystring/fmt.h"

using namespace xyu;

int main()
{
    try {

        for (int i = 0; i <= 3; ++i)
            xyout << format("{ {} ? {}{}{}}\n", i, 1, 2, 3);

    } catch (...) {
        xyerr << "error catched\n";
    }
    return 0;
}
