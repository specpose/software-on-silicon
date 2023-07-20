#pragma once
#include<stdexcept>

namespace SFA {
    namespace util{
		struct logic_error : std::logic_error {
		logic_error(std::string what, std::string file_name, std::string function_name) : std::logic_error(what),fi_name(file_name), fu_name(function_name) {}
		std::string fi_name;
		std::string fu_name;
	    };
        struct runtime_error : std::runtime_error {
		runtime_error(std::string what, std::string file_name, std::string function_name) : std::runtime_error(what),fi_name(file_name), fu_name(function_name) {}
		std::string fi_name;
		std::string fu_name;
	    };
    }
}
