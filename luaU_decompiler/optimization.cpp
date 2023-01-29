#include "optimization.hpp"
#include <algorithm>
#include <sstream>

#pragma region core 

/* Turns string into vector by line. */
std::vector<std::string> split_lines(std::string str) 
{
    std::vector<std::string> retn;
    auto pos = 0u;

    while ((pos = str.find('\n')) != std::string::npos) 
    {
        retn.emplace_back(str.substr(0u, pos));
        str.erase(0, pos + sizeof ('\n'));
    }
    
    return retn;
}

/* Line is a local variable? */
bool line_has_localvar(const std::string& str) {

    if (str.find("local") != std::string::npos)
        return true;

    return false;
}

/* Gets tha name of a variable. */
std::string variable_name(std::string str) {
    str.erase(str.find("local "), sizeof ("local "));
    str.substr(0, str.find("="));
    str.erase(std::remove(str.begin(), str.end(), ' '), str.end());
    return str;
}

/* Sees if variable is getting refrenced in the line. */
std::uint32_t variable_line_refrence_count(const std::string& str, const std::string& var_name)
{
    return 0u;
}

/* Gets variable refrence count in the whole code. */
std::uint32_t variable_refrence_count(const std::vector<std::string>& vect, const std::string& var_name) 
{
    auto retn = 0u;

    for (const auto& i : vect)
        variable_line_refrence_count(i, var_name);

    return retn;
}

#pragma endregion

void post_optization(std::string& write, const std::shared_ptr<decompile_config>& config) {

    const auto lines = split_lines(write);

    for (const auto& i : lines) {

    }
}