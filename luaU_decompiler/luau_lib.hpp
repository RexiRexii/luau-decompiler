#pragma once
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#pragma region decompiler 

#pragma region decompiler_data

enum class closure_type : std::uint8_t 
{
	none,
	local, /* local function test () */
	global, /* function test () */
	newclosure /* (function()  end)*/
};

struct closure_info
{
	std::string name = ""; /* Name of closure. */
	bool is_child_proto = false; /* Is child proto is true if its not main proto. */
	bool args_set = false; /* Sees if args have been analyzed. */
	std::uint32_t arg_count = 0u; /* Arg count for current closure. */
	closure_type tt = closure_type::none; /* Type of closure. */
	std::string data = ""; /* Decompilation data of closure. */
	bool pre_anlyzed = false; /* If closure has already been analyzed before decompilation. */
	bool varargs = false; /* Routine has varargs. */
};

struct loc_var 
{
	std::string name = "";
	std::uint32_t end = 0u;
	std::uint32_t reg = 0u;
	bool in_table = false;
	bool is_upvalue = false;
};

struct decompile_info;
struct proto_decompilation
{
	std::vector <std::string> upvalues;
	std::vector <std::string> k;
	std::vector <std::uint32_t> code;
	std::vector <std::shared_ptr<decompile_info>> p;
	std::vector <std::shared_ptr<loc_var>> locvars; /* Logical operation info will be partially put here as to determine wether it's an var or not. */
	std::uint8_t debug_info = 0u;
	std::uint32_t main_proto = 0u;
	std::size_t size_proto = 0u;
	std::size_t size_code = 0u;
	std::uint32_t unique_indentifier = 0u; /* Unique identifier for upvalues etc. ex. Upvalue_{iden}_0 */
	closure_info closure;
};

struct decompile_info
{
	proto_decompilation proto;
	std::vector <std::string> pcode_bytes;
};

struct decompile_config 
{
	/* Prefixs. */
	std::string iterator_prefix = "i";
	std::string variable_prefix = "v";
	std::string global_variable_prefix = "g";
	std::string argument_prefix = "a";
	std::string function_prefix = "func_";
	std::string upvalue_prefix = "upv";
	std::string loop_variable_prefix = "k";
	std::string loop_variable_prefix_2 = "v";

	/* View */
	bool include_dissassembly = false;
	bool include_bytecode = false;

	/* If this is false it will try to produce source accurate results thought this will inline a bunch of stuff. */
	bool optimize = false;
};

#pragma endregion

extern std::string luaU_decompile(const std::string& bytecode);

extern std::shared_ptr<decompile_info> build_decompiler_info(const std::string& bytecode, const std::size_t len);

#pragma endregion

#pragma region dissassemble

#pragma region dissassemble_data
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

#pragma endregion

/* Dissassemble address. */
extern std::shared_ptr<dissassembler_data> dissassemble(std::shared_ptr<decompile_info>& info, const std::uint32_t offset, const bool detail_info = false);

/* Dissassemble whole code. */
extern std::vector <std::shared_ptr<dissassembler_data>> dissassemble_whole(std::shared_ptr<decompile_info>& info, const bool detail_info = false);

/* Dissassemble range. */
extern std::vector <std::shared_ptr<dissassembler_data>> dissassemble_range(std::shared_ptr<decompile_info>& info, const std::uint32_t start, const std::uint32_t end, const bool detail_info = false);

#pragma endregion