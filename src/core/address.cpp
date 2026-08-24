#include "gmk/core/address.hpp"

#include <cstdio>

namespace gmk {

std::string to_string(Address address)
{
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "0x%llx",
                  static_cast<unsigned long long>(address.value()));
    return buffer;
}

}  // namespace gmk
