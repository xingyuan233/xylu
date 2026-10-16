#include "detail/xysystem/path.h"

using namespace xyu;

int main()
{
    try {

        auto walker = Path{}.walk(Path::WT_DFS);
        for (auto info : walker.range())
            xyout << info.name << '\n';

    } catch (...) {
        xyerr << "error catched\n";
    }
    return 0;
}
