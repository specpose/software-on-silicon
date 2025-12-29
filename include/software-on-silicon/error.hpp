#pragma once
#include <stdexcept>

namespace SFA {
namespace util {
    enum error_code : unsigned char;
    const std::string error_message(error_code what);
    void logic_error(error_code what, std::string file_name, std::string function_name, const char* modname = nullptr);
    void runtime_error(error_code what, std::string file_name, std::string function_name, const char* modname = nullptr);
}
}
