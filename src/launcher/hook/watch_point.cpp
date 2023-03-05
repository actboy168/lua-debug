#include <hook/watch_point.h>

namespace luadebug::autoattach {
    watch_point::operator bool() const {
        return !!address;
    }
    bool watch_point::find_symbol(const lua::resolver& resolver) {
        address = (void*)resolver.find(funcname);
        return !!address;
    }
}
