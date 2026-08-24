#include "gmk/core/error.hpp"

namespace gmk {

std::string Error::what() const
{
    std::string result;
    result.reserve(message_.size() + 32);
    result.append(gmk::to_string(code_));
    result.append(": ");
    result.append(message_);
    if (native_ != 0) {
        result.append(" (native code ");
        result.append(std::to_string(native_));
        result.push_back(')');
    }
    return result;
}

}  // namespace gmk
