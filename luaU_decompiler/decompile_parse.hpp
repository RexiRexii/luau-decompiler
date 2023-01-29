#pragma once
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

/* Turns decompiler data into decompilation. */


#ifndef decompiler_parse_header
#define decompiler_parse_header

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
	std::string loop_variable_prefix_2 = "r";

	/* View */
	bool include_dissassembly = false;
	bool include_bytecode = false;
	
	/* If this is false it will try to produce source accurate results thought this will inline a bunch of stuff. */
	bool optimize = false;

};

extern void decompiler_parse(const std::shared_ptr<decompile_config>& config, std::shared_ptr<decompile_info>& decom_info, std::string& write);
#endif 

