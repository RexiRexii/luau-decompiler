#include "decode.hpp"


std::uint32_t decode_int(const std::string& str, std::size_t& ip)
{
    std::uint8_t ip_byte = 0u;
    auto l_shift_imm = 0u;
    auto retn = 0u;

    do
    {
        ip_byte = str.at (ip++);
        retn |= ((ip_byte & 127) << l_shift_imm);
        l_shift_imm += 7u;

    } while (ip_byte & 128);

    return retn;
}