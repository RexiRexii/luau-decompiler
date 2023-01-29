#pragma once
#include <memory>
#include <string>
#include <vector>
#include "config.hpp"
#include "decompile_parse.hpp"


enum class ancestor_info : std::uint8_t 
{
	none,
	settable
};

enum class basic_info : std::uint16_t
{
	none,
	math,
	load,
	branch,
	fetch,
	set,
	call,
	fast_call,
	return_,
	closure,
	for_
};

struct dissassembler_data 
{
	std::uint16_t opcode = 0u;
	std::uint16_t size = 0u;

	std::string data = ""; /* Dissassembly */

	basic_info basic = basic_info::none; /* Basic data on the opcode. */
	ancestor_info ancestor = ancestor_info::none; /* Common info of opcode. */

	std::uint32_t dest_reg = 0u;

	/* Only used for concatable instructions. {ONLY FOR REGISTERS NOT CONSTANTS}. */
	std::uint32_t source_reg = 0u;
	bool has_source = false; /* Has source reg? */
	std::uint32_t value_reg = 0u;
	bool has_value = false; /* Has value reg? */


	std::uint32_t cmp_source_reg = 0u; /* Compare register operand 1. */
	bool has_cmp_source = true; /* Has value compare reg? */
	std::uint32_t cmp_value_reg = 0u; /* Compare register operand 2. */
	bool has_cmp_value = false; /* Has value compare reg? */

	std::uint32_t tt = 0u; /* Type used in any operand. */

	bool is_comparative = false; /* If opcode compares something. */

	std::string byte_code = ""; /* Bytecode represented as fromatted string for decompiler. */
};

#if CONFIG_TESTING_BUILD == true  

/* Dissassemble address. */
std::shared_ptr<dissassembler_data> dissassemble(std::shared_ptr<decompile_info>& info, const std::uint32_t offset, const bool detail_info = false);

/* Dissassemble whole code. */
std::vector <std::shared_ptr<dissassembler_data>> dissassemble_whole(std::shared_ptr<decompile_info>& info, const bool detail_info = false);

/* Dissassemble range. */
std::vector <std::shared_ptr<dissassembler_data>> dissassemble_range(std::shared_ptr<decompile_info>& info, const std::uint32_t start, const std::uint32_t end, const bool detail_info = false);

#endif

#if CONFIG_TESTING_BUILD == false  

/* Dissassemble address. */
extern __declspec(dllexport) std::shared_ptr<dissassembler_data> dissassemble(std::shared_ptr<decompile_info>& info, const std::uint32_t offset, const bool detail_info = false);

/* Dissassemble whole code. */
extern __declspec(dllexport) std::vector <std::shared_ptr<dissassembler_data>> dissassemble_whole(std::shared_ptr<decompile_info>& info, const bool detail_info = false);

/* Dissassemble range. */
extern __declspec(dllexport) std::vector <std::shared_ptr<dissassembler_data>> dissassemble_range(std::shared_ptr<decompile_info>& info, const std::uint32_t start, const std::uint32_t end, const bool detail_info = false);

#endif
