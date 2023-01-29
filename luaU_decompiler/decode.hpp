#pragma once
#include <cstdint>
#include <string>
#include "config.hpp"

/* Opcodes */
#if CONFIG_DECRYPT_OPCODE == true
#define decode_opcode(inst) std::uint8_t((inst & 0xFF) * 203u)
#else
#define decode_opcode(inst) (inst & 0xFF)
#endif

/* Regs */
#define decode_A(inst) ((inst >> 8) & 0xFF) 
#define decode_B(inst) ((inst >> 16) & 0xFF)
#define decode_C(inst) ((inst >> 24) & 0xFF)
#define decode_D(inst) (std::int32_t (inst) >> 16)
#define decode_E(inst) (std::int32_t (inst) >> 8)

extern std::uint32_t decode_int(const std::string& str, std::size_t& ip);