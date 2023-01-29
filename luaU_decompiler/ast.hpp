#pragma once
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include "decompile_parse.hpp"


namespace table {

	/* Gets end of table. */
	std::uint32_t end(std::shared_ptr<decompile_info>& info, const std::uint32_t start, const std::uint32_t end);

}


/* Types in branch */
enum class branch_data_e : std::uint8_t {

	none,

	and_,  /* Basic and. */
	and_close, /* Close parenthesis with and. */
	and_open,  /* Open parenthesis with and. */
	and_truncate, /* Close expression with and. */

	or_,  /* Basic or. */
	or_close, /* Close parenthesis with or. */
	or_open,  /* Open parenthesis with or. */
	or_truncate,  /* Close expression with or. */

	close /* Close expression. */
};


/* Type of branch. */
enum class type_branch : std::uint8_t {
	if_,
	elseif_,
	else_,
	repeat,
	while_,
	until,
	for_,
	none
};


struct closure_data {
	/* Args get resolved during parse. */
	std::uint32_t start = 0u;
	std::uint32_t end = 0u;
	bool is_global = false;
	std::uint32_t closure_id = 0u;
};


struct branch_comparative {
	branch_data_e i = branch_data_e::none;
	std::string compare = "";
};


struct logical_operations {
	std::uint32_t end = 0u;
	std::uint32_t reg = 0u;
	std::uint32_t end_jump = 0u; /* End jump. */
};


struct call_routine_st {
	std::uint32_t end = 0u;
	std::uint32_t idx = 0u;
};

struct ast_data {

	/* Expressions are in info->proto.locvars. */

	std::vector <std::uint32_t> ends; /* Ends for branches etc. */
	std::vector <std::uint32_t> while_ends; /* Ends for while. */
	std::vector <std::uint32_t> else_routine_begin; /* Used to set scoping for else if, else routines. */
	std::unordered_map<std::uint32_t, std::shared_ptr<call_routine_st>> call_routine; /* Full Call routines. {Start, End} also dont worry about encapsulation we will solve that during decompilation time. *Doesn't count fastcalls becuase those are volatile.  *Begging of routine is only there to stop vars being formed. */
	std::unordered_map<std::uint32_t, std::uint32_t> concat_routines; /* Concat routines see compiler info in configs.hpp for more info. */

	std::unordered_map<std::uint32_t, std::shared_ptr<logical_operations>> logical_operations; /* Logical operations, { Begin, shared }*/

	std::unordered_map<std::uint32_t, std::vector <type_branch>> branches;  /* Has to be seperate members because logical operations.  */
	std::unordered_map<std::uint32_t, std::vector <std::uint32_t>> dedicated_loops; /* Used for certain loops. (Only useful for loops so throwing it in branch won't make any sense). */
	std::unordered_map<std::uint32_t, std::vector <std::pair <std::uint32_t, std::pair <bool, bool>>>> loop_map; /* Map of loop compare routine. {Start, { **Encapsulation data.** {End, {true for repeat false for while, true if while true loop else not its false }}}} *Due to structure of architecture any non forever repeating loops will be at the back. */
	std::unordered_map<std::uint32_t, std::shared_ptr <branch_comparative>> branch_comparisons; /* Each and, or in a branch routine will be placed here besides logical operations if the branch end it will add a hanging or, and then append data to it. But it will not cache end only if it's logical operation it will cache end if not it will hang and, or. */
	std::unordered_map<std::uint32_t, const char*> extra_branches;/* If a branch needs a custom compare it will be here *Only used if logical operations isnt used on the branch. */
	std::vector<std::uint32_t> branch_pos_scope; /* Branch position scope. */

	std::unordered_map<std::uint32_t, std::uint32_t> forming_tables; /* Formed tables {start, end} */

};


std::shared_ptr<ast_data> create_ast(std::shared_ptr<decompile_info>& info, const std::shared_ptr<decompile_config>& config);