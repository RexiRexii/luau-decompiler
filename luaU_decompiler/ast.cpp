#include "ast.hpp"
#include "decode.hpp"
#include "dissassemble.hpp"
#include "opcodes.hpp"
#include "visualizer.hpp"
#include <algorithm>
#include <map>
#include <iostream>


/*

Branch refrence:


* If -> no jump out  ex. { 

; print ("hi")
jumpifnoteqk                   r1 199.000000 jumpifnoteqk_6
getimport                      r2 print
loadk                          r3 "hi"
call                           r2 1 0
jumpifnoteqk_6:

}

* else -> has jump out (but only one for routine) ex. -take every branch jump { 

; {

if (compare) {
	print ("hi");
} else {
	print ("hello");
}

; }

jumpifnoteqk                   r1 199.000000 jumpifnoteqk_6
getimport                      r2 print
loadk                          r3 "hi"
call                           r2 1 0
jump						   ok

jumpifnoteqk_6:
getimport                      r2 print
loadk                          r3 "hello"
call                           r2 1 0

ok:

}




* elseif -> has with routine with jump but other routine compare jump is less then or equal to first jump. {



jumpifnoteqk                   r0 10.000000 jumpifnoteqk_22
getimport                      r2 print
loadk                          r3 "xddd"
call                           r2 1 0
jump                           jump_16

jumpifnoteqk_22: 
getimport                      r1 math.random
loadn                          r2 10
loadk                          r3 1922012828.000000
call                           r1 2 1
jumpifnoteqk                   r1 69.000000 jumpifnoteqk_6 

getimport                      r1 print
loadk                          r2 "oka"
call                           r1 1 0        

jumpifnoteqk_6:
jump_16:


jump_16 :

}


*/


namespace table {

	/* Gets end of table. */
	std::uint32_t end(std::shared_ptr<decompile_info>& info, const std::uint32_t start, const std::uint32_t end) 
	{
		auto extra = 0u;
		const auto disasm = dissassemble_range(info, start, end, true); /* Dissassembly range. */
	
		/* Write fake data. *CHANGE IN FUTURE LIKE FRR */
		auto retn = std::make_shared<ast_data>();

		std::vector <std::uint32_t> cached_node_size;
		std::vector <std::uint32_t> predicted_node_size; /* Predicts node size for current table. Just gets previous power of 2. */
		std::vector <bool> entering_nested; /* So we don't get an exit and entry node size. */

		auto on_predicted = 0u; /* On predicted amount for node size though can't use back of vector cuz table issues. */
		auto array_size = 0; /* If where in a table or not. *Use table operands sub with setlist amt and settablek for one. */
		auto on_dup = false;
		auto dup_node_size = 0; /* For OP_DUPTABLE the 2nd operand is always tid not a power of 2 so we can just use that instead of predicting size. */
		auto node_size = 0; /* Used for setting info for newtable no setlist indeication at end. */
		auto valid_node = false;
		auto valid = false;
		auto prev_dest = 0u;
		auto pos = start;
		auto dism_pos = 0u;

		/* Check first if table is empty. */
		if (disasm.front ()->opcode == OP_NEWTABLE) {
			const auto node = (!(decode_B(info->proto.code[pos])) ? 0 : (1 << (decode_B(info->proto.code[pos]) - 1)));
			/* No node or table so return pos. */
			if (!info->proto.code[pos + 1u] && !node)
				return pos;
		}

		for (const auto& i : disasm) {
			
 			/* Inc/dec depending on whats it's in. */
			if (i->opcode == OP_NEWTABLE) {
				
				const auto node = (!(decode_B(info->proto.code[pos])) ? 0 : (1 << (decode_B(info->proto.code[pos]) - 1)));
				const auto l_array = signed(info->proto.code[pos + 1u]);
				
				valid = true;

				array_size += l_array; /* table_size + array_size */

				if (node) {
					node_size = node; /* Needs reset as when it exits scope itll reset. */
					valid_node = true;
					
					auto x = node;
					
					/* Henry Warrens branchless method of doing this. */
					--x;
					x = x | (x >> 1);
					x = x | (x >> 2);
					x = x | (x >> 4);
					x = x | (x >> 8);
					x = x | (x >> 16);
					x = x - (x >> 1);
					
					predicted_node_size.emplace_back(x);
					entering_nested.emplace_back(false);
					cached_node_size.emplace_back(node);
					on_predicted = x;
				}
				
			}
			else if (i->opcode == OP_SETLIST) {
				
			    auto amt = (decode_C(std::int32_t(info->proto.code[pos])) - 1);
				if (amt == -1)
					amt = prev_dest;
				
				array_size -= amt;
				valid = true;
			}
			else if (i->opcode == OP_DUPTABLE) {
				dup_node_size += std::stoi(info->proto.k[decode_D(info->proto.code[pos])].c_str());
				on_dup = true;
				valid = true;
			}

		    if (i->ancestor == ancestor_info::settable) {	
				
				/* Node size is sized to closest hash size to its power of 2. Max is 2^15 so it's a bit tricky to find the end judging on that. */
				if (node_size)
					--node_size;

				valid = true;	

				/* Dec dup size if there is any. */
				if (dup_node_size)
					--dup_node_size;
				
				if (!dup_node_size) /* Nothing to analyze just cancle. */
					on_dup = false;

				/* Hit predicted so look at future. */
				if (unsigned(node_size) <= on_predicted && on_predicted) {

					auto temp_pos = pos + i->size;
					const auto table_target = decode_B(info->proto.code[pos]);

					const auto target_1 = decode_A(info->proto.code[pos]);
					const auto target_2 = (i->opcode == OP_SETTABLE) ? decode_C(signed(info->proto.code[pos])) : -1;

					auto used_target_1 = false;
					auto used_target_2 = false;
					
					auto nested_count = 0u;
					
					/* Check future intructions and determine end by that. */
					for (auto pos_on = (dism_pos + 1u); pos_on < disasm.size(); ++pos_on) {

						const auto current = disasm[pos_on];
						
						/* Error catching if it exceeds just pass current pos. */
						if ((pos_on - 1u) == disasm.size())
							return pos;

						/* Check if targets get used if so reset. */
						if (current->has_source) {

							const auto target = current->source_reg;

							if (target == target_1)
								used_target_1 = false;

							if (target_2 != -1 && signed(target) == target_2)
								used_target_2 = false;

						}

						if (current->has_cmp_source) {

							const auto target = current->cmp_source_reg;

							if (target == target_1)
								used_target_1 = false;

							if (target_2 != -1 && signed(target) == target_2)
								used_target_2 = false;

						}

						if (current->has_value) {

							const auto target = current->value_reg;

							if (target == target_1)
								used_target_1 = false;

							if (target_2 != -1 && signed(target) == target_2)
								used_target_2 = false;

						}

						if (current->has_cmp_value) {

							const auto target = current->cmp_value_reg;

							if (target == target_1)
								used_target_1 = false;

							if (target_2 != -1 && signed(target) == target_2)
								used_target_2 = false;

						}

						/* Inc for nested. */
						if (current->opcode == OP_NEWTABLE || current->opcode == OP_DUPTABLE) {
							
							/* Duptable no need for check. */
							if (current->opcode == OP_DUPTABLE)		
								++nested_count;
							else if ((!(decode_B(info->proto.code[temp_pos])) ? 0 : (1 << (decode_B(info->proto.code[temp_pos]) - 1))) || info->proto.code[temp_pos + 1u])
								++nested_count;
						}

						/* If we hit a new table then we know its the end. */
						if (current->ancestor == ancestor_info::settable && table_target != decode_B(info->proto.code[temp_pos])) {
	
							/* Fix target. */
							if (predicted_node_size.size() > 1u) {

								if (entering_nested.back()) { /* Exiting table */
									predicted_node_size.pop_back();
									on_predicted = predicted_node_size.back();  /* Can't use front cuz of formatting issues. */
									node_size = cached_node_size.back();
									cached_node_size.pop_back();
								}
								else
									entering_nested.back() = true; /* Entering table. */
							
							}
							else {
								
								/* End of a nested table. */
								if (nested_count)
									--nested_count;
								else  /* Nested table is 0 and we entered a new table. */
									return pos;
								
							}
					
							/* Next is set for nested table. */
							if ((dism_pos + 1u) == pos_on) 
								break;
				
							/* If targets met then is okay. */
							if (used_target_1) {

								if (target_2 != -1 && used_target_2)
									break;
								else if (target_2 == -1)
									break;								
							}							
							return pos;
						}
						else if (current->ancestor == ancestor_info::settable && table_target == decode_B(info->proto.code[temp_pos])) /* Same table or nested table. */
							break;

						/* Hit a for loop not in a table anymore. */
						if (current->basic == basic_info::for_) 
							return pos;
						
						/* No return for a call or return so end. */
						if ((current->opcode == OP_CALL && !(decode_C(info->proto.code[temp_pos]) - 1u)) || current->opcode == OP_RETURN) 
							return pos;
						
						/* Check to see if targets are getting set. */
						if (current->basic != basic_info::branch &&
							current->basic != basic_info::fast_call &&
							current->opcode != OP_NOP)
							if ((current->opcode != OP_CAPTURE || current->tt != 2u)) {
								
								if (current->dest_reg == target_1) {

									/* Set twice without used. */
									if (used_target_1) 
										return pos;
															
									used_target_1 = true;
								}

								if (target_2 != -1 && signed(current->dest_reg) == target_2) {

									/* Set twice without used. */
									if (used_target_2)
										return pos;
									
									used_target_2 = true;
								}
							}
						temp_pos += current->size;
					}
				}
			}
			
			/* End. */
			if (valid && !node_size && !array_size && !on_dup && !dup_node_size) /* Can somtimes end with both ones. */
				return pos;
			
			prev_dest = i->dest_reg;
			pos += i->size;
			++dism_pos;
		}

		/* Maybe get indexs but is a empty table. */
		return start;
	}

	/* Gets smallest dest register in a table. */
	std::uint32_t smallest_reg(std::shared_ptr<decompile_info>& info, const std::uint32_t start, const std::uint32_t end) {

		auto reg = UINT_MAX; /* Current reg. */

		const auto curr_size = dissassemble(info, start)->size; /* Skip current newtable. But log its values. */
		auto pos = start + curr_size;

		const auto disasm = dissassemble_range(info, pos, end); /* Dissassembly range. */

		auto nested_table = 0u;

		for (const auto& i : disasm) {

			/* Dest is table register which we want to to skip but we skip OP_SETLIST at end pos. *Not garunteed all tables will have setlist */
			if (i->opcode == OP_SETLIST && pos == end)
				break;

			/* Capture dest along with fastcall have ints as dest but branches just nah bro. */
			if (i->opcode != OP_CAPTURE && i->basic != basic_info::fast_call && i->basic != basic_info::branch && i->basic != basic_info::for_) 
				if (i->dest_reg < reg)
					reg = i->dest_reg;

			pos += i->size;
		}

		return reg;

	}

	/* Writes table data. */
	void write (std::shared_ptr<decompile_info>& info, std::unordered_map<std::uint32_t, std::uint32_t>& buffer) {

		const auto dissassembled = dissassemble_whole(info);

		auto pos = 0u; /* Current position. */
		auto end = 0u;

		for (const auto& curr : dissassembled) {

			/* End. */
			if (end == pos)
				end = 0u;

			/* Find new table and capture it. */
			if (!end && (curr->opcode == OP_NEWTABLE || curr->opcode == OP_DUPTABLE)) {
				/* Cache buffer. */
				end = table::end(info, pos, info->proto.size_code);
				if (end != pos)
					buffer.insert(std::make_pair(pos, end));
			}

			pos += curr->size;
		}

		return;
	}

	/* Returns location of table indexs. { location } range is inside loop. */
	std::vector <std::uint32_t> indexs(std::shared_ptr<decompile_info>& info, const std::uint32_t start, const std::uint32_t end) {


		std::vector <std::uint32_t> retn;
		std::vector <std::pair<std::uint32_t, std::int32_t>> analysis; /* Locations to be analzed. {amt - {reg_1, reg_2 * if its -1 its nothing }} */


		const auto dissassembled = dissassemble_range(info, start, end);
		auto pos = start; /* Current position. */

		for (const auto& curr : dissassembled) {

			const auto bcd = info->proto.code[pos];

			switch (curr->opcode) {

				case OP_SETTABLE: {
					analysis.emplace_back(std::make_pair(decode_A(bcd), decode_C(bcd)));
					break;
				}
				
				case OP_SETTABLEN:
				case OP_SETTABLEKS: {
					analysis.emplace_back(std::make_pair(decode_A(bcd), -1));
					break;
				}

				default: {
					break;
				}

			}

			pos += curr->size;
		}

		pos = start; /* Reset */

		/* Registers found. */
		auto reg = 0u;

		for (const auto& curr : dissassembled) {

			/* Nothing more to analyze. */
			if (!analysis.size())
				return retn;
			
			const auto& on = analysis.front();

			/* Dest is first or sec. */
			if ((curr->dest_reg == on.first || curr->dest_reg == on.second) && curr->ancestor != ancestor_info::settable) {
				retn.emplace_back(pos);
				++reg;
			}

			if (reg == 2u || (reg == 1u && on.second == -1)) {
				reg = 0u;
				analysis.erase(analysis.begin());
			}


			pos += curr->size;
		}

		return retn;
	}

}

#pragma region branch_compare_core

/* Gets location of next opcode in branch if there is one. */
std::uint32_t next_opcode(std::shared_ptr<decompile_info>& info, const std::uint32_t start, const std::uint32_t end, const std::uint16_t op) {

	auto pos = start;
	const auto disasm = dissassemble_range(info, start, end);

	for (const auto& i : disasm) {

		/* Opcode matches and where past or on start. */
		if (i->opcode == op)
			return pos;

		pos += i->size;
	}

	return 0u;
}


/* First branch : parent branch */
std::unordered_map <std::uint32_t,  std::uint32_t> analyzed_cache;

/* Decides if sub statement jumps out to jmp. */
#define jump_out_if(info, pos, jmp) ((decode_D(info->proto.code[pos]) + pos + 1u) == jmp)

/* 
* Gets big jump out of branch to determine if statement size
If returns 0 branch doesnt trail. 
*/
std::uint32_t get_greatest(std::shared_ptr<decompile_info>& info, std::uint32_t pos, const std::uint32_t jmp) {

	while (pos != jmp) {

		const auto disasm = dissassemble(info, pos);
		
		pos += disasm->size;
		
		/* Iterate through if branch till we see a jump out if not we can conclude */
		if (disasm->basic == basic_info::branch) {

			const auto len = (decode_D(info->proto.code[pos]) + pos + 1u); 

			if (len > jmp)
				return len;
		}

	}

	return 0u;
}


/* Branch Compare to opposite. */
const char* const branch_opposite(const std::uint16_t op) {

	switch (op) {
		case OP_JUMPIFEQ: {
			return "~=";
		}
		case OP_JUMPIFLE: {
			return "<=";
		}
		case OP_JUMPIFLT: {
			return ">";
		}
		case OP_JUMPIFNOTEQ: {
			return "==";
		}
		case OP_JUMPIFNOTLE: {
			return ">=";
		}
		case OP_JUMPIF: {
			return "not";
		}
		case OP_JUMPIFNOT: {
			return "";
		}
		case OP_JUMPIFNOTEQK: {
			return "==";
		}
		case OP_JUMPIFEQK: {
			return "~=";
		}
		default: {
			return "==";
		}
	}
}

/* Branch Compare to string. */
const char* const branch_normal(const std::uint16_t op) {

	switch (op) {

		case OP_JUMPIFEQ: {
			return "==";
		}

		case OP_JUMPIFLE: {
			return "<=";
		}

		case OP_JUMPIFLT: {
			return "<";
		}

		case OP_JUMPIFNOTEQ: {
			return "~=";
		}

		case OP_JUMPIFNOTLE: {
			return "<=";
		}

		case OP_JUMPIF: {
			return "";
		}

		case OP_JUMPIFNOT: {
			return "not ";
		}

		case OP_JUMPIFNOTEQK: {
			return "~=";
		}

		case OP_JUMPIFEQK: {
			return "==";
		}

		default: {
			return "==";
		}

	}
	
}




/* Gets finale branch end. { Start, { { start, true for or false for and}, End } }. Pass start as starting branch. return.size = 0 no branch after.  */
std::unordered_map<std::uint32_t, std::pair<std::vector<std::pair<std::uint32_t, bool>>, std::uint32_t>> final_jump_end(std::shared_ptr<decompile_info>& info, const std::uint32_t start) {

	std::unordered_map<std::uint32_t, std::pair<std::vector<std::pair<std::uint32_t, bool>>, std::uint32_t>> retn;

	const auto full_disasm = dissassemble_whole(info);

	/* Gets previous instruction.*/
	auto get_prev = [](const std::vector <std::shared_ptr<dissassembler_data>>& dism, const std::uint32_t start) -> std::uint32_t {
		auto retn = 0u;
		for (const auto& i : dism) {
			if ((retn + i->size) == start)
				return retn;
			retn += i->size;
		}
		return 0u;
	};

	auto len = (start + decode_D(info->proto.code[start]) + 1u);
	auto prev_s = get_prev(full_disasm, len);
	auto prev_dism = dissassemble(info, prev_s);

	/* Start it off. */
	if (prev_dism->basic != basic_info::branch || !prev_dism->is_comparative)
		return retn;

	
	while (true) {
		
		if (retn.find(start) == retn.end()) {
			retn.insert(std::make_pair(start, std::make_pair(std::vector<std::pair<std::uint32_t, bool>>({ std::make_pair(prev_s, true) }), prev_s)));
		}
		else {
			retn[start].first.emplace_back(std::make_pair(prev_s, true));
			retn[start].second = prev_s;
		}

		const auto prev = get_prev(full_disasm, (prev_s + decode_D(info->proto.code[prev_s]) + 1u));
		prev_s = prev;
		prev_dism = dissassemble(info, prev);
		
		if (prev_dism->basic != basic_info::branch || !prev_dism->is_comparative) {
			retn[start].first.pop_back();
			return retn;
		}
	}

	return retn;
}

/* Returns boolean depending on rule set. */
bool logical_local(std::shared_ptr<decompile_info>& info, const std::uint32_t start, const std::uint32_t end, const std::uint32_t target, std::uint32_t& perferable_pos, std::unordered_map<std::uint32_t, std::shared_ptr<call_routine_st>>& calls_routine, std::unordered_map<std::uint32_t, std::uint32_t>& concat_routines, std::unordered_map<std::uint32_t, std::vector <std::pair<std::uint32_t, std::pair <bool, bool>>>>& loop_map, std::unordered_map<std::uint32_t, std::shared_ptr<logical_operations>>& logical_operations, std::unordered_map<std::uint32_t, std::uint32_t>& skip_locvar) {

	const auto dism = dissassemble_range(info, start, end);
	
	auto skip = -1;
	auto pos = start;
	auto checked = false;

	/* See if there dest regs greater then target. */
	for (const auto& i : dism) {

		if (i->basic != basic_info::branch &&
			i->basic != basic_info::for_ &&
			i->basic != basic_info::fast_call &&
			i->ancestor != ancestor_info::settable &&
			i->opcode != OP_NOP &&
			i->opcode != OP_RETURN)
			if ((i->opcode != OP_CAPTURE || i->tt != 2u))
				if (i->dest_reg > target) {
					checked = true;
					break;
				}

		pos += i->size;
	}

	if (!checked)
		return false;

	pos = start;
	skip = -1;
	checked = false;

	/* See if register gets overiden inside branch and gets used outside w/o overriden also check for least concat, call routine reg as per how they work. */
	auto end_branch = -1;
	 
	auto used_full = false; /* See if reg is getting used at all. */
	auto used_sv = false; /* Target is used as source or value. */
	auto importance = false; /* Inside concat/call routine. *Used for seeting dest. */
	auto target_used_before_gotten = false; /* Sees if target been used as dest before source. */
	auto importance_reg = -1;
	auto prev_dest = target;
	auto prev_dest_pos = start;
	auto prev_pos = 0u;
	auto last_target_usage = 0u;
	for (const auto& i : dism) {

		/* Log skips within logical operators. */
		if (skip_locvar.find(pos) != skip_locvar.end())
			skip = skip_locvar[pos];

		/* Log and skip logical operation. */
		if (logical_operations.find(pos) != logical_operations.end())
			skip = logical_operations[pos]->end;

		/* Log and skip loop compares. */
		if (loop_map.find(pos) != loop_map.end()) 
			skip = loop_map[pos].back().first;

		/* Go through disasm and skip any call routine. */
		if (calls_routine.find(pos) != calls_routine.end()) {
			const auto& ca = calls_routine[pos];
			skip = ca->end;
			importance = true;
		}

		/* Skip for concat routines. Refer to compiler info in configs.hpp. */
		if (concat_routines.find(pos) != concat_routines.end()) {
			skip = concat_routines[pos];
			importance = true;
		}


		/* Get lowest reg in concat and call routine. */
		if (importance &&
			i->basic != basic_info::branch &&
			i->basic != basic_info::for_ &&
			i->basic != basic_info::fast_call &&
			(i->opcode != OP_CALL || decode_C(info->proto.code[pos])) &&
			i->opcode != OP_NOP &&
			i->ancestor != ancestor_info::settable &&
			i->opcode != OP_RETURN)
			if ((i->opcode != OP_CAPTURE || i->tt != 2u)) {

				if (importance_reg == -1 || unsigned(importance_reg) > i->dest_reg) {
					importance_reg = i->dest_reg;
				}
				
			}


		/* Skip captures to check if its global. */
		if (i->opcode == OP_NEWCLOSURE) {

			/* Skip captures. */
			auto o = 1u;
			while ((*(&i + o))->opcode == OP_CAPTURE)
				++o;

			/*
			* Global closure:
			   newclosure r1 proto_0
			   capture 1 r0
			   setglobal r1 gang
			*/
			if ((*(&i + o))->opcode == OP_SETGLOBAL && (*(&i + o))->dest_reg == i->dest_reg)
				skip = (pos + o + dissassemble(info, (pos + o))->size); /* Skip routine. */

		}


		/* End of branch reset target_used_before_gotten and end_branch. */
		if (end_branch != -1 && unsigned (end_branch) <= pos) {
			end_branch = -1;
			target_used_before_gotten = false;
		}

		if (skip == -1 || pos >= unsigned (skip)) {

			importance = false;
			
			/* Check importance register. */
			if (importance_reg != -1) {

				/* Target gotten used in concat/call routine because of how they work it is automatically not local. */
				if (signed (importance_reg) == target)
					return false;

				importance_reg = -1;
			}


			/* Used as source before dest garunteed locvar. */
			if (i->has_source && i->source_reg == target && pos != start && !target_used_before_gotten)
				return true;


			/* Used as value before dest garunteed locvar. */
			if (i->has_value && i->value_reg == target && pos != start && !target_used_before_gotten)
				return true;


			/* See if target been used as source/value. */
			if ((i->has_source && i->source_reg == target) || (i->has_value && i->value_reg == target) || (i->opcode == OP_SETTABLEKS && (decode_A(info->proto.code[pos]) == target || decode_B(info->proto.code[pos]) == target))) {
				
				/* Source/Value is used twice. */
				if (used_sv)
					return true;

				used_sv = true;
				used_full = true;
			}

			/* See if target been used as cmp source/value. */
			if (i->basic == basic_info::branch && ((i->has_cmp_source && i->cmp_source_reg == target) || (i->has_cmp_value && i->cmp_value_reg == target))) {

				/* Source/Value is used twice. */
				if (used_sv)
					return true;

				used_sv = true;
				used_full = true;
			}

			/* Check reg. */
			if (i->basic != basic_info::branch &&
				i->basic != basic_info::for_ &&
				i->basic != basic_info::fast_call &&
				(i->opcode != OP_CALL || decode_C(info->proto.code[pos])) &&
				i->opcode != OP_NOP &&
				i->opcode != OP_RETURN)
				if ((i->opcode != OP_CAPTURE || i->tt != 2u)) {

					if (i->ancestor != ancestor_info::settable) {

						if (target == i->dest_reg) {

							/* Overidden once and used twice. Garunteed locvar. */
							if (!used_sv && pos != start)
								return true;

							target_used_before_gotten = true;
							used_sv = false;
							last_target_usage = pos;
							prev_dest_pos = pos;

							/* See if call has return and next is return.*/
							if (i->opcode == OP_CALL && decode_C(info->proto.code[pos]) && decode_opcode(info->proto.code[pos + i->size]) == OP_RETURN)
								return true;
						}

						/* prev_dest is greater than current but current isnt target so reg also start must be the prev dest pos. */
						if (prev_dest > target && i->dest_reg != target) {

							perferable_pos = prev_dest_pos;

							const auto target_dism = dissassemble(info, prev_dest_pos);
							if (target_dism->basic != basic_info::branch &&
								target_dism->basic != basic_info::for_ &&
								target_dism->basic != basic_info::fast_call &&
								(target_dism->opcode != OP_CALL || decode_C(info->proto.code[pos])) &&
								target_dism->opcode != OP_NOP &&
								target_dism->opcode != OP_RETURN &&
								target_dism->dest_reg == target)
								if ((target_dism->opcode != OP_CAPTURE || target_dism->tt != 2u))
									prev_dest_pos = 0u;
						

							return true;
						}
	

						prev_dest = i->dest_reg;
					}

					prev_pos = pos;
				}

			
		}

		pos += i->size;
	}

	/* Not used at alls so assume locvar. */
	if (!used_full)
		return true;
	
	/* Previous dest value is greater then target so return true. */
	if (prev_dest >= target) {

		const auto last_target_usage_dism = dissassemble(info, last_target_usage)->size;
		if ((last_target_usage + last_target_usage_dism) == end) /* Target used at end so enc. */
			last_target_usage = end;

		/* Check previous settable. */
		if (dissassemble(info, prev_pos)->ancestor == ancestor_info::settable && decode_A(info->proto.code[prev_pos]) != target && decode_B(info->proto.code[prev_pos]) != target)
			return true;

		/* Argument was used prev to end. */
		if (last_target_usage != end)
			return true;
	}

	return false;
}

/* See if branch has ful if statement jump out. */
std::uint32_t branch_count(std::shared_ptr<decompile_info>& info, std::uint32_t pos, const std::uint32_t end) {

	std::uint32_t retn = 0u;

	while (pos != end) {

		const auto disasm = dissassemble(info, pos);

		pos += disasm->size;


		if (disasm->basic == basic_info::branch)
			++retn;

	}

	return retn;
}


/* Sees if register is used in segment. *Only checks if it gets used n src or value and call routine. */
std::uint32_t register_usage_count(std::shared_ptr<decompile_info>& info, const std::uint32_t start, const std::uint32_t end, const std::uint32_t reg) {

	const auto range = dissassemble_range(info, start, end);
	auto used = 0u;

	for (const auto& i : range) {
		
		/* Found register. */
		if (i->has_source && i->source_reg == reg)
			++used;
		if (i->has_value && i->value_reg == reg)
			++used;
		

	}

	return used;
}


/* Returns mapped of current branch data *Pass pos as current branch. */
/* Pos needs to start on parent branch. */
/* Will do parnet branch no sub branches. */
void do_branch(std::shared_ptr<decompile_info>& info, std::unordered_map<std::uint32_t, std::uint32_t>& loc_var_skip, std::unordered_map<std::uint32_t, const char*>& extra_branches, std::unordered_map<std::uint32_t, std::vector <std::uint32_t>>& ast_dedicated_loops, std::unordered_map<std::uint32_t, std::shared_ptr<call_routine_st>>& calls, std::unordered_map<std::uint32_t, std::shared_ptr <branch_comparative>>& cmps, const std::uint32_t pos, std::uint32_t& buffer_end, std::unordered_map<std::uint32_t, std::uint32_t>& skip_info, std::vector<std::uint32_t>& branch_pos_scope, std::vector<std::uint32_t>& else__, std::unordered_map<std::uint32_t, std::vector <type_branch>>& retn) {

	/* Sort and get relative to pos. */
	auto relative_else_pos = 0u;
	std::sort(else__.begin(), else__.end());

	for (const auto i : else__)
		if (i >= pos)
			relative_else_pos = i;

	/* Sees if jump prev is else, if so then itll return it's locations. (For prev_branch pass final branch position.)  */
	auto is_else = [](std::shared_ptr<decompile_info>& info, const std::uint32_t on_branch, std::vector<std::uint32_t>& elses) -> std::uint32_t {

		const auto prev_branch_len = (on_branch + decode_D(info->proto.code[on_branch]) + 1u);
		
		auto temp_at = 0u;
		auto relative_else = 0u;

		/* Get biggest relative to pos. */
		for (const auto i : elses) 
			if (i <= prev_branch_len && i >= on_branch)
				relative_else = i;		
		
		for (const auto& i : dissassemble_whole(info)) {

			/* Exceeded next else scope end. */
			if (relative_else && temp_at >= relative_else)
				return 0u;

			if ((temp_at + i->size) == prev_branch_len && (i->opcode == OP_JUMP || i->opcode == OP_JUMPX)) 
				if (((i->opcode == OP_JUMP) ? decode_D(info->proto.code[on_branch]) : decode_E(info->proto.code[on_branch])) > 0)
					return temp_at;	

			temp_at += i->size; 
		}

		return 0u;
	};


	auto prev_pos = pos;
    auto len = (decode_D(info->proto.code[pos]) + signed (pos) + 1);

	
	/* Get previous pos from */
	for (const auto& i : dissassemble_range(info, pos, len)) {
		if ((prev_pos + i->size) == len)
			break;
		prev_pos += i->size;
	}
	

	const auto prev_disasm = dissassemble(info, prev_pos);

	
	/* Simple branch or has no ors, elses in it. */
	if ((!prev_disasm->is_comparative || prev_disasm->opcode == OP_JUMP) || ((decode_D(info->proto.code[prev_pos]) + prev_pos + 1u) == len)) {

		/* First map sub branches. */
		std::vector<std::uint32_t> sub_branches;
		auto skip = 0u;
		auto tpos = pos;
		for (const auto& i : dissassemble_range(info, tpos, len)) {

			/* Log. */
			if ((tpos > skip || !skip) && i->basic == basic_info::branch && i->opcode != OP_JUMP && i->opcode != OP_JUMPX) 
				sub_branches.emplace_back(tpos);

			/* Skip loops. */
			if (ast_dedicated_loops.find(tpos) != ast_dedicated_loops.end())
				skip = *std::max_element(ast_dedicated_loops[tpos].begin (), ast_dedicated_loops[tpos].end ());

			tpos += i->size;
		}


		auto prev_branch = pos;
		for (const auto i : sub_branches) {
	
			const auto curr = dissassemble(info, i);
			const auto disasm = dissassemble_range(info, prev_branch, i);
			const auto cmp_source_reg_count_sv = register_usage_count(info, prev_branch, i, curr->cmp_source_reg);
			const auto cmp_value_reg_count_sv = register_usage_count(info, prev_branch, i, curr->cmp_value_reg);
			auto end_table = 0u;
			auto pos_ = prev_branch;


			/* See if compare gets used more then once in routine. */
			/* First compare has gaunteed compare. */
			if (cmp_source_reg_count_sv > 1u)
				goto end_smp_branch;

			if (curr->has_cmp_value && cmp_value_reg_count_sv > 1u)
				goto end_smp_branch;

			/* Branches must have the same end locations. */
			if ((i + decode_D(info->proto.code[i]) + 1u) != len)
				goto end_smp_branch;



			auto cmp_source_dest_loc = 0u;
			auto cmp_source_dest_count = 0u;
			auto cmp_value_dest_loc = 0u;
			auto cmp_value_dest_count = 0u;

			/* See less registers getting used in dest. */
			if (i != pos)
				for (const auto& i : disasm) {

					/* New table. */
					if (!end_table && (i->opcode == OP_DUPTABLE || i->opcode == OP_NEWTABLE))
						end_table = table::end(info, pos_, info->proto.size_code);

					/* Target. */
					if (i->basic != basic_info::fast_call &&
						i->basic != basic_info::branch &&
						i->basic != basic_info::for_ &&
						i->opcode != OP_RETURN &&
						i->opcode != OP_NOP &&
						i->opcode != OP_CAPTURE &&
						!end_table) {
						
							/* Dest is less the cmps. */
							if ((curr->has_cmp_value && i->dest_reg < curr->cmp_value_reg) && i->dest_reg < curr->cmp_source_reg)
								goto end_smp_branch;

							/* Set count relative dest. */
							if (i->dest_reg == curr->cmp_source_reg) {
								++cmp_source_dest_count;
								cmp_source_dest_loc = pos_;
							}
							if (curr->has_cmp_value && i->dest_reg == curr->cmp_value_reg) {
								++cmp_value_dest_count;
								cmp_value_dest_loc = pos_;
							}

					}

					if (pos_ >= end_table)
						end_table = 0u;

					pos_ += i->size;
				}

			/* Cmp in value but not source. */
			if (i != pos && (cmp_value_dest_count != 1u || cmp_source_dest_count != 1u))
				if (cmp_value_dest_count && !cmp_source_dest_count) {

					/* After isnt jump. */
					if ((cmp_value_dest_loc + dissassemble(info, cmp_value_dest_loc)->size) != i)
						goto end_smp_branch;

				} else if (!cmp_value_dest_count && cmp_source_dest_count) {
	
					/* After isnt jump. */
					if ((cmp_source_dest_loc + dissassemble(info, cmp_source_dest_loc)->size) != i)
						goto end_smp_branch;

				}
				else {
					
					/* Finally see if the reg count for our target is 0 then there cant be any code between prev branch and current. */
					if ((prev_branch + dissassemble(info, prev_branch)->size) != i)
						goto end_smp_branch;

				}

			/* Finally set if. */
			auto ptr = std::make_shared<branch_comparative>();
			ptr->i = branch_data_e::and_;
			ptr->compare = branch_opposite(decode_opcode(info->proto.code[i]));
			cmps.insert(std::make_pair(i, ptr));
			
			prev_branch = i;
		}	
		
		end_smp_branch:

		/* Pos is less then next else so check len****************** to abide by encapsulation. */
		if (relative_else_pos > pos && len >= signed (relative_else_pos))
			len = relative_else_pos;
		
		buffer_end = len;
		cmps.erase(prev_branch);
		

		/* Append if. */
		if (retn.find(prev_branch) == retn.end())
			retn.insert(std::make_pair(prev_branch, std::vector <type_branch>({ type_branch::if_ })));
		else
			retn[prev_branch].emplace_back(type_branch::if_);


		loc_var_skip.insert(std::make_pair(pos, prev_branch));
		skip_info.insert(std::make_pair(pos + dissassemble(info, pos)->size, prev_branch)); /* Set skip. */
		extra_branches.insert(std::make_pair(prev_branch, branch_opposite(decode_opcode(info->proto.code[prev_branch])))); /* Set extra info for compare. */
		branch_pos_scope.emplace_back(pos);
		

		/* Doesn't count for elseif. Also fix buffer end. */
		const auto else_ = is_else(info, prev_branch, else__);
		if (else_) {
		
			if (retn.find(else_) == retn.end())
				retn.insert(std::make_pair(else_, std::vector <type_branch>({ type_branch::else_ })));
			else
				retn[else_].emplace_back(type_branch::else_);
			
			else__.emplace_back(else_);

			buffer_end = (else_ + decode_D(info->proto.code[else_]) + 1u);

		}


		return;
	}
	else {
	
		/* Routine is "complex". */
		const auto finale = final_jump_end(info, pos);
		const auto finale_ = finale.begin()->second.second;
		const auto finale_jump = (finale_ + decode_D(info->proto.code[finale_]) + 1u);

			 
		auto t_pos = pos;
		for (const auto& i : dissassemble_range(info, pos, finale_)) {

			if (i->is_comparative && i->basic == basic_info::branch) {
		
				/* Jumps to end it's or else and. */
				if ((t_pos + decode_D(info->proto.code[t_pos]) + 1u) != finale_jump) {
					auto ptr = std::make_shared<branch_comparative>();
					ptr->i = branch_data_e::or_;
					ptr->compare = (((t_pos + decode_D(info->proto.code[t_pos]) + 1u) == len) ? branch_opposite(decode_opcode(info->proto.code[t_pos])) : branch_normal(decode_opcode(info->proto.code[t_pos]))); /* Jumps to end garunteed opposite. */
					cmps.insert(std::make_pair(t_pos, ptr));
				}
				else {
					auto ptr = std::make_shared<branch_comparative>();
					ptr->i = branch_data_e::and_;
					ptr->compare = branch_normal(decode_opcode(info->proto.code[t_pos]));
					cmps.insert(std::make_pair(t_pos, ptr));
				}

			}

			t_pos += i->size;
		}

	

		buffer_end = finale_jump;
		cmps.erase(finale_);

		/* Append if info. */
		if (retn.find(finale_) == retn.end())
			retn.insert(std::make_pair(finale_, std::vector <type_branch>({ type_branch::if_ })));
		else
			retn[finale_].emplace_back(type_branch::if_);

		loc_var_skip.insert(std::make_pair(pos, finale_));
		skip_info.insert(std::make_pair(pos + dissassemble(info, pos)->size, finale_)); /* Set skip. */
		extra_branches.insert(std::make_pair(finale_, branch_opposite(decode_opcode(info->proto.code[finale_])))); /* Set extra info for compare. */
		branch_pos_scope.emplace_back(pos);


		/* Set else and fix end. */
		const auto else_ = is_else(info, prev_pos, else__);
		if (else_) {

			if (retn.find(else_) == retn.end())
				retn.insert(std::make_pair(else_, std::vector <type_branch>({ type_branch::else_ })));
			else
				retn[else_].emplace_back(type_branch::else_);

			else__.emplace_back(else_);

			buffer_end = (else_ + decode_D(info->proto.code[else_]) + 1u);

		}

	}


	return;
}


#pragma endregion


#pragma region loop_core 

enum class loop_type : std::uint8_t {
	none,
	repeat_until,
	while_end,
	for_end
};

struct loop_data {
	loop_type tt = loop_type::none;
	std::uint32_t end = 0u;
};

/* Map jumback relative location. { Jumpback location, instruction location }*/
std::vector<std::pair <std::uint32_t, std::shared_ptr<loop_data>>> map_loops(std::shared_ptr<decompile_info>& info) {

	std::vector<std::pair <std::uint32_t, std::shared_ptr<loop_data>>> retn;

	auto pos = 0u;
	auto goal = info->proto.size_code;

	std::shared_ptr<dissassembler_data> prev_disasm = nullptr;

	while (pos != goal) {

		const auto disasm = dissassemble(info, pos);

		/*
		
			For loops are standard
			Repeat will come at the end of the routine and is near jumpback instruction
			While will come at the beginging of the routine but will go past jumpback instruction.
		
		*/
		/* See for loop. */
		switch (disasm->opcode) {

			/* Repeat */
			case OP_JUMPBACK: {
				auto ptr = std::make_shared<loop_data>();
				ptr->tt = (prev_disasm != nullptr && prev_disasm->is_comparative && prev_disasm->basic == basic_info::branch) ? loop_type::repeat_until : loop_type::while_end; /* Previous instruction was compare so garunteed repeat else while. */
				ptr->end = pos;
				retn.emplace_back(std::make_pair((std::int32_t(pos + 1u) + decode_D(info->proto.code[pos])), ptr));
				break;
			}

			/* For loop. */
			case OP_FORGLOOP:
			case OP_FORNLOOP:
			case OP_FORGLOOP_NEXT:
			case OP_FORGLOOP_INEXT: {
				auto ptr = std::make_shared<loop_data>();
				ptr->tt = loop_type::for_end;
				ptr->end = pos;
				retn.emplace_back(std::make_pair((pos + decode_D(info->proto.code[pos])), ptr));
				break;
			}

			default: {
				/* Nothing to analyze. */
				break;
			}

		}

		
		prev_disasm = disasm;
		pos += disasm->size;

	}

	return retn;
}


/* Writes all loops to mapped branched data (All loops beside forgloop will be resolved on decompilation time). */
void write_loops(std::shared_ptr<decompile_info>& info, std::unordered_map<std::uint32_t, std::vector <std::uint32_t>>& dedicated_loops, std::unordered_map<std::uint32_t, std::vector <type_branch>>& retn, std::vector <std::uint32_t>& while_ends) {

	const auto map = map_loops(info);
    
	for (const auto& i : map) {

		/* Write loop by type. */
		switch (i.second->tt) {

				/* Repeat */
				case loop_type::repeat_until: {
				
					/* If no entry is found append to curr. */
					if (dedicated_loops.find(i.first) == dedicated_loops.end()) {
						dedicated_loops.insert(std::make_pair(i.first, std::vector <std::uint32_t>({ i.second->end })));
						retn.insert(std::make_pair(i.first, std::vector <type_branch> ({ type_branch::repeat })));
						retn.insert(std::make_pair(i.second->end, std::vector <type_branch>({ type_branch::until })));
					}
					else {
						dedicated_loops[i.first].emplace_back(i.second->end);
						retn[i.first].emplace_back(type_branch::repeat);

						if (retn.find(i.second->end) == retn.end())
							retn.insert(std::make_pair(i.second->end, std::vector <type_branch>({ type_branch::until })));
						else
							retn[i.second->end].emplace_back (type_branch::until);
					}

					break;
				}

				/* For loop. *End handled on decompilation time. */
				case loop_type::for_end: {

					/* If no entry is found append to curr. */
					if (dedicated_loops.find(i.first) == dedicated_loops.end()) {
						dedicated_loops.insert(std::make_pair(i.first, std::vector <std::uint32_t> ({ i.second->end })));
						retn.insert(std::make_pair(i.first, std::vector <type_branch>({ type_branch::for_ })));
					}
					else {
						dedicated_loops[i.first].emplace_back(i.second->end);
						retn[i.first].emplace_back(type_branch::for_);
					}

					break;
				}

				/* While loop *End handled on decompilation time. */
				case loop_type::while_end: {
			
					/* If no entry is found append to curr. */
					if (dedicated_loops.find(i.first) == dedicated_loops.end()) {
						dedicated_loops.insert(std::make_pair(i.first, std::vector <std::uint32_t> ({ i.second->end })));
						retn.insert(std::make_pair(i.first, std::vector <type_branch>({ type_branch::while_ })));
					}
					else {
						dedicated_loops[i.first].emplace_back(i.second->end);
						retn[i.first].emplace_back(type_branch::while_);
					}

					while_ends.emplace_back(i.second->end);

					break;
				}

				default: {
					throw std::exception("A loop wasn't properly analyzed check code/bytecode.\n");
				}

		}

	}




	return;
}



/* Returns next branch relative to pos(Pos should be current branch. */
std::uint32_t next_branch (std::shared_ptr<decompile_info>& info, std::uint32_t pos) {
	pos += dissassemble(info, pos)->size;
	const auto disasm = dissassemble_range(info, pos, info->proto.size_code);
	for (const auto& i : disasm) {
		if (i->basic == basic_info::branch && i->is_comparative)
			return pos;
		pos += i->size;
	}
	return 0u;
};

std::uint32_t next_settableks(std::shared_ptr<decompile_info>& info, std::uint32_t pos) {
	pos += dissassemble(info, pos)->size;
	const auto disasm = dissassemble_range(info, pos, info->proto.size_code);
	for (const auto& i : disasm) {
		if (i->opcode == OP_SETTABLEKS)
			return pos;
		pos += i->size;
	}
	return 0u;
};






/* Writes info on loop data. (For compare) */
void write_loop_data(std::shared_ptr<decompile_info>& info, std::unordered_map<std::uint32_t, std::vector <type_branch>>& loops, std::unordered_map<std::uint32_t, std::vector <std::uint32_t>>& dedicated_loop_pos, std::unordered_map<std::uint32_t, std::vector <std::pair<std::uint32_t, std::pair <bool, bool>>>>& buffer) {
	
	

	/*
		While - get begging of routine with jumpback address find biggest out of jump in first branch if there is any tho if not use first branch jump location as end.
		Repeat - get every jumpback and take jump to verify if its repeat then get every branch to see if exceeds that jumpback tho if not get every branch inside it and see if it exceeds the jumpback location if so get that branch parent and set as first. 
	
	    * End must be a branch. 
	*/

	const auto size = info->proto.size_code;

	
	for (const auto& i : loops) {
		auto counter = 1u;
		auto on__ = 0u;
		for (const auto b : i.second) {

			on__ = i.first;
			
			switch (b) {
		
				case type_branch::while_: {

					/* Begging is given find end. */
					const auto start = i.first;
					auto first_branch = start;
					auto first_iter = true;


					/* First find next branch. */
					for (const auto& i : dissassemble_range(info, start, size)) {
						if (i->is_comparative && i->basic == basic_info::branch)
							break;
						first_branch += i->size;
					}


					/* Last branch is always not while true if is not.*/
					if (counter != std::count(i.second.begin (), i.second.end (), type_branch::while_))
						goto while_true_statement;

					
					/* If branch exceeds size then its while true do. */
					if (first_branch < size) {

		
						const auto jmp_len = (first_branch + decode_D(info->proto.code[first_branch]) + 1u);
						auto branch_pos = start;
						auto branch_end = 0u;


						/* End is jump before end. If its jumpback we have to check. */
						for (const auto& i : dissassemble_range(info, start, jmp_len)) {
							if ((branch_pos + i->size) == jmp_len) {
								if ((*(&i + 1u))->opcode != OP_JUMPBACK) {
									branch_end = (branch_pos + decode_D(info->proto.code[branch_pos]) + 1u);
								}
								break;
							}
							branch_pos += i->size;
						}


						auto t_branch_end = start;


						/* Has given ending. */
						const auto count_je = final_jump_end(info, first_branch);
						if (count_je.size()) {
							t_branch_end = count_je.begin()->second.second;
							goto branch_end;
						}


						/* Now get prev instruction from branch_end. */
						if (branch_end)
							for (const auto& i : dissassemble_range(info, start, branch_end)) {
								if ((t_branch_end + i->size) == branch_end)
									break;
								t_branch_end += i->size;
							}
				
						/* If this not happens Previous jump from first jump doesnt exceed first jump. */
						/* Fix end. */
						if (t_branch_end >= info->proto.size_code || t_branch_end == start || decode_opcode(info->proto.code[jmp_len - 1u]) == OP_JUMPBACK || decode_opcode(info->proto.code[t_branch_end - 1u]) == OP_JUMPBACK || decode_opcode(info->proto.code[t_branch_end]) == OP_JUMPBACK) {
							
							/* Normal analysis. */
							/* Get next branch and see */
							auto on = first_branch;
							t_branch_end = start;
							while (on) {

								const auto on_first_branch = (t_branch_end == start);
								const auto jmp_on = (on + decode_D(info->proto.code[on]) + 1u);

								/* Get next branch and first see if contents in that branch get used or not and it's jump is constant. */
								if (jmp_on == jmp_len) {
									
									const auto disasm = dissassemble_range(info, ((first_iter)  ? t_branch_end : t_branch_end + dissassemble(info, t_branch_end)->size), on);
									first_iter = false;

									/* This is just too check validity everything to get set will be done later. */

									const auto prev_branch = dissassemble(info, on);
									const auto target_1 = prev_branch->cmp_source_reg;
									const auto target_2 = ((prev_branch->has_cmp_value) ? signed(prev_branch->cmp_value_reg) : -1);

									auto marked_target_1 = false;
									auto marked_target_2 = false;
									for (const auto& i : disasm) {
										
										/* Garunteed end. */
										if (i->basic == basic_info::for_ ||
											i->opcode == OP_RETURN ||
											i->opcode == OP_NOP)
											if (on_first_branch)
												goto while_true_statement;
											else
												goto branch_end;
										
										
										/* Source compare register is garunteed. */
										if (marked_target_1) {
										
											/* End of out branch without failure. */
											if (i->basic == basic_info::branch && i->is_comparative)
												if (target_2 != -1 && marked_target_2)
													break;
												else if (target_2 == -1)
													break;
										
											/* End.  target 1 and 2 have been set but there isnt  branch following it. */
											if (target_2 != -1 && marked_target_2)
												if (on_first_branch) /* If there is double marked on the first branch it's a while true do. */
													goto while_true_statement;
												else
													goto branch_end;
										
											/* End. Target 1 has been set no branch following it also no target 2. */
											if (target_2 == -1)
												if (on_first_branch)
													goto while_true_statement;
												else
													goto branch_end;
										}
										
										
										/* Source isnt set and it's a branch.*/
										if (!marked_target_1 && i->basic == basic_info::branch && i->is_comparative)
											break;
										
										
										
										/* Set values. */
										if (i->basic != basic_info::fast_call && i->opcode != OP_CAPTURE) {
										
											if (i->dest_reg == target_1)
												marked_target_1 = true;
											else if (target_2 != -1 && i->dest_reg == unsigned(target_2))
												marked_target_2 = true;
											else {
										
												/* If dest reg is lower then first then end. */
												if (i->dest_reg < target_1)
													if (on_first_branch)
														goto while_true_statement;
													else
														goto branch_end;
										
												/* If value reg is lower then first then end. */
												if (target_2 != -1 && i->dest_reg < unsigned(target_2))
													if (on_first_branch)
														goto while_true_statement;
													else
														goto branch_end;
											}
										
									}

								}

								}
								else
									break;


								t_branch_end = on;
								on = next_branch(info, on);

							}

						}
						
					branch_end:
					    
						/* Log. */
						if (buffer.find(start) == buffer.end())
							buffer.insert(std::make_pair(start, std::vector <std::pair<std::uint32_t, std::pair <bool, bool>>>({ std::make_pair(t_branch_end, std::make_pair(false, false)) })));
						else
							buffer[start].emplace_back(std::make_pair(t_branch_end, std::make_pair(false, false)));

					
					}
					else {
		
					while_true_statement:

						if (buffer.find(start) == buffer.end())
							buffer.insert(std::make_pair(start, std::vector <std::pair<std::uint32_t, std::pair <bool, bool>>>({ std::make_pair(start, std::make_pair(false, true)) })));
						else
							buffer[start].emplace_back(std::make_pair(start, std::make_pair(false, true)));

					}

					++counter;

					break;
				}

				case type_branch::until: {

					/* End is given find beggining. */
					const auto end = i.first;
					const auto end_addr_jmp = (dissassemble(info, end)->size + end);
					const auto start = (signed(end) + decode_D(info->proto.code[end]) + 1); /* Take jumpback jump. */
					
					auto begin = 0u;			
					auto routine_end = start;
					auto at = start;
					auto temp_at = start;
					

					const auto ren_dism = dissassemble_range(info, start, end);
				
					/* Get prev from end is routine_end. */
					for (const auto& i : ren_dism) {
						if ((routine_end + i->size) == end)
							break;
						routine_end += i->size;
					}


					/* Find begin. */
					for (const auto& i : ren_dism) {
		
						/* Anlyze each branch and use a rule ste to see if it apples to next branch or next is end. */
						if (at >= temp_at && i->is_comparative && i->basic == basic_info::branch) {

							/* Next is end. */
							if ((at + i->size) == end) {						
								if (!begin) /* Indication of one compare. */
									begin = at;
								goto repeat_end;
							}

							/* More then one compare get next. */
							const auto next_branch_pos = next_branch(info, at);
							const auto next_dism = dissassemble(info, next_branch_pos);

							auto target_1 = next_dism->cmp_source_reg;
							auto target_2 = ((next_dism->has_cmp_value) ? signed(next_dism->cmp_value_reg) : -1);

							auto marked_target_1 = false;
							auto marked_target_2 = false;
					
							const auto fixed_at = (at + i->size);

							begin = at;
							temp_at = fixed_at;

							for (const auto& i : dissassemble_range(info, fixed_at, end)) {
						
								/* Hit jumpback end. */
								if ((temp_at + i->size) == end)
									goto repeat_end;

								/* Garunteed end. */
								if (i->basic == basic_info::for_ ||
									i->opcode == OP_RETURN ||
									i->opcode == OP_NOP)
										goto repeat_routine_end;


								/* Source compare register is garunteed. */
								if (marked_target_1) {

									/* End of out branch without failure. **Reset */
									if (i->basic == basic_info::branch && i->is_comparative) {

										if (target_2 != -1 && marked_target_2) {

											const auto next_jmp = next_branch(info, temp_at);
											if (!next_jmp) /* No next jump. */
												goto repeat_end;

											const auto next_dism = dissassemble(info, next_jmp);

											marked_target_1 = false;
											marked_target_2 = false;
											target_1 = next_dism->cmp_source_reg;
											target_2 = ((next_dism->has_cmp_value) ? signed(next_dism->cmp_value_reg) : -1);

										}
										else if (target_2 == -1) {

											const auto next_jmp = next_branch(info, temp_at);
											if (!next_jmp) /* No next jump. */
												goto repeat_end;

											const auto next_dism = dissassemble(info, next_jmp);

											marked_target_1 = false;
											target_1 = next_dism->cmp_source_reg;
											target_2 = ((next_dism->has_cmp_value) ? signed(next_dism->cmp_value_reg) : -1);

										}

									}
									else if ((target_2 != -1 && marked_target_2) || target_2 == -1)
										goto repeat_routine_end;

								}
		

								/* Set values. */
								if (i->basic != basic_info::branch && i->basic != basic_info::fast_call && i->opcode != OP_CAPTURE) {

									if (i->dest_reg == target_1) {

										/* Gets used twice. */
										if (marked_target_1)
											goto repeat_routine_end;

										marked_target_1 = true;
									}
									else if (target_2 != -1 && i->dest_reg == unsigned(target_2)) {

										/* Gets used twice. */
										if (marked_target_2)
											goto repeat_routine_end;

										marked_target_2 = true;
									}
									else {

										/* If dest reg is lower then first then end. */
										if (i->dest_reg < target_1)
											goto repeat_routine_end;

										/* If value reg is lower then first then end. */
										if (target_2 != -1 && i->dest_reg < unsigned(target_2))
											goto repeat_routine_end;
									}

								}

								temp_at += i->size;
							}

						}
	
						/* Invalid branch to cache begin. */
						repeat_routine_end:
							at += i->size;
					
					}


				repeat_end:
	
					/* Fix begin to encapsulate compare regs. */
					const auto disasm = dissassemble_range(info, start, begin);
					const auto on_begin = dissassemble(info, begin);
					const auto target_1 = on_begin->cmp_source_reg;
					const auto target_2 = ((on_begin->has_cmp_value) ? signed(on_begin->cmp_value_reg) : -1);

					auto marked_target_1 = false; /* At least one register compare is garunteed. */
					auto marked_target_2 = false;

					auto temp_begin = start;
					for (const auto& i : disasm) {

						/* Set values. */
						if (i->basic != basic_info::fast_call && i->opcode != OP_CAPTURE) {

							if (i->dest_reg == target_1)
								marked_target_1 = true;
							else if (target_2 != -1 && i->dest_reg == unsigned(target_2))
								marked_target_2 = true;

						}

						/* End. */
						if (marked_target_1) {

							if (target_2 != -1 && marked_target_2) {
								begin = temp_begin;
								marked_target_2 = false;
								marked_target_1 = false;
							}
							else if (target_2 == -1) {
								begin = temp_begin;
								marked_target_1 = false;
							}

						}

						temp_begin += i->size;
					}
					
					/* Log. */
					if (buffer.find(begin) == buffer.end())
						buffer.insert(std::make_pair(begin, std::vector <std::pair<std::uint32_t, std::pair <bool, bool>>>({ std::make_pair(routine_end, std::make_pair(true, false)) })));
					else
						buffer[begin].emplace_back(std::make_pair(routine_end, std::make_pair(true, false)));


					break;
				}

				default: {
					break;
				}

			}
			
		}
		

		/* Put non while true do loop in the back. */
		if (buffer.find (on__) != buffer.end ())
			for (auto& b : buffer[on__]) {

				/* Check for while and make sure its not while true do. */
				if (!b.second.first && b.first != on__ && buffer[on__].back() != b) {
					std::swap(b, buffer[on__].back());
					break;
				}

			}

	}

	return;
}


#pragma endregion



#pragma region logical_operator_core 

/* Start is start of branch routine. */
std::uint32_t largest_branch_jump(std::shared_ptr<decompile_info>& info, const std::uint32_t start, const std::uint32_t end) {
	
	auto pos = start;
	auto retn = (start + decode_D(info->proto.code[start]) + 1u);

	const auto disasm = dissassemble_range(info, start, end);
	for (const auto& i : disasm) {
		if (i->basic == basic_info::branch && i->opcode != OP_JUMP && i->opcode != OP_JUMPX && i->opcode != OP_JUMPBACK && (pos + decode_D(info->proto.code[pos] + 1u) > retn))
			retn = (pos + decode_D(info->proto.code[pos]) + 1u);
		pos += i->size;
	}

	return retn; /* Can't find one so return end jump.*/
}

/* Get branch code section. */
std::uint32_t get_branch_code_sect_pos(std::shared_ptr<decompile_info>& info, std::unordered_map<std::uint32_t, std::uint32_t>& dedicated_loops, const std::uint32_t start) {
	
	const auto target = dissassemble(info, start);
	const auto len = (start + decode_D(info->proto.code[start]) + 1u);

	/* Now check if previous instruction is a jump. */
	auto at = start;
	auto send_next = false;

	for (const auto& i : dissassemble_range(info, start, len)) {
		if ((at + i->size) == len && i->is_comparative && i->basic == basic_info::branch) {
			return (at + decode_D(info->proto.code[at]) + 1u);  /* Segment is gaurunteed to be code section. */
		}
		if (i->is_comparative && i->basic == basic_info::branch && !send_next && i->cmp_source_reg == target->cmp_source_reg)
			send_next = true;
		if (i->is_comparative&& i->basic == basic_info::branch && send_next)
			return (at + decode_D(info->proto.code[at]) + 1u);

		at += i->size;
	}


	return len;
}


/* Gets unkown comparative start address of a logical operation. */
std::uint32_t get_logical_op_unk_comparative(std::shared_ptr<decompile_info>& info, const std::vector <std::shared_ptr<dissassembler_data>>& disasm, const std::uint32_t loc) {


	const auto end_space = (loc + 1u); /* End of our logical operation. */


	std::uint32_t first_occurance = 0u; /* First occurange jump of logical operation refrence. */
	std::uint32_t pos = 0u;


	/* First we need to get the range of it. */
	for (const auto& i : disasm) {


		/* Also find first occurance. */
		if (!first_occurance) {

			switch (i->opcode) {

							case OP_JUMPIFEQK:
							case OP_JUMPIFNOTEQK:
							case OP_JUMPIFEQ:
							case OP_JUMPIFLE:
							case OP_JUMPIFLT:
							case OP_JUMPIFNOTEQ:
							case OP_JUMPIFNOTLE:
							case OP_JUMPIFNOTLT:
							case OP_JUMP:
							case OP_JUMPX:
							case OP_JUMPIFNOT:
							case OP_JUMPIF: {

									/* Check for JumpX. */
									const auto jump = ((i->opcode != OP_JUMPX) ? decode_D(info->proto.code[pos]) : decode_E(info->proto.code[pos]));

									if (jump == loc || jump == end_space)
										first_occurance = pos;
									
									break;
							}

			}

		}

		/* Found. */
		if (pos == loc)
			break;

		pos += i->size;

	}


	/* Now that we have data we need to focus on first_occurance and any jumps to intercept first_occurance below */
	auto next_intercept = [=]() -> std::uint32_t {

			std::uint32_t pos = 0u;

		    for (const auto& i : disasm) {

				    switch (i->opcode) {
				    
				             case OP_JUMPIFEQK:
				             case OP_JUMPIFNOTEQK:
				             case OP_JUMPIFEQ:
				             case OP_JUMPIFLE:
				             case OP_JUMPIFLT:
				             case OP_JUMPIFNOTEQ:
				             case OP_JUMPIFNOTLE:
				             case OP_JUMPIFNOTLT:
				             case OP_JUMP:
							 case OP_JUMPX:
				             case OP_JUMPIFNOT:
				             case OP_JUMPIF: {
									
								 /* Check for JumpX. */
								 const auto jump = ((i->opcode != OP_JUMPX) ? unsigned(decode_D(info->proto.code[pos])) : unsigned(decode_E(info->proto.code[pos])));

								 /* Found. */
								 if (jump < first_occurance)
									 return pos;

								 break;
				             }
				    
				    }

					/* End. */
					if (pos == first_occurance)
						return 0u;

					pos += i->size;
			}

			return pos;
	};


	auto retn = 0u;

	while (next_intercept())
		retn = next_intercept ();

	return retn;
}


void write_logical_operators(std::shared_ptr<decompile_info>& info, std::unordered_map<std::uint32_t, std::shared_ptr<call_routine_st>>& calls, std::unordered_map<std::uint32_t, std::shared_ptr<logical_operations>>& buffer) {

	const auto disasm = dissassemble_whole(info);

	/* Gets register of next loadb jump. */
	auto end_logical_operation = [=](const std::uint32_t start, const std::uint32_t end, std::shared_ptr<decompile_info>& info) -> std::int32_t {


		std::uint32_t pos = 0u;

		const auto range = dissassemble_range(info, start, end);


		for (const auto& i : disasm) {

			switch (i->opcode) {

				case OP_SETGLOBAL:
				case OP_RETURN: {
					return -1;
				}

				case OP_CALL: {

					const auto retns = (std::int32_t(decode_C(info->proto.code[pos])) - 1);
					if (!retns)
						return -1;

					break;
				}

				case OP_LOADB: {

					/* Not jump log*/
					if ((decode_C(info->proto.code[pos]))) 
						return decode_A(info->proto.code[pos]);
					
					break;
				}

			}

			pos += i->size;
		}

		return -1;
	};


	/* Finds 3rd method of logical operation */
	auto write_3rd_logial_operation = [=](std::shared_ptr<decompile_info>& info, std::unordered_map<std::uint32_t, std::shared_ptr<call_routine_st>>& calls, const std::vector <std::shared_ptr<dissassembler_data>>& dissassembly, std::unordered_map<std::uint32_t, std::shared_ptr<logical_operations>>& buffer) -> void {

		const auto size = info->proto.size_code;

		std::uint32_t pos = 0u;


		/* Branches for pre analysis. */
		std::unordered_map<std::uint32_t, std::uint32_t> branches; /* {Start, End}  */
		std::unordered_map<std::uint32_t, std::uint32_t> branches_2; /* {Start, End} *For analysis append. */
		std::unordered_map<std::uint32_t, std::uint32_t> branch_temp; /* {Start, End} */
		std::vector<std::uint32_t> appending_remove; /* { pos }*/
		auto branch_on = 0u;
		auto start_branch = 0u;
		
		/* Get each branch in dissassembly and get any jump outs encapsulating current branch. */
		for (const auto& i : dissassembly) {

			 /* Add to branches. */
			 for (const auto& i : branch_temp)
			 	if (i.second <= pos && branches_2.find(i.first) == branches_2.end()) {
					branches_2.insert(std::make_pair(i.first, i.second));
			 		appending_remove.emplace_back(i.first);
			 	}
			 
			 /* Remove used. */
			 for (const auto i : appending_remove) 
				 branch_temp.erase(i);
			

		     appending_remove.clear();

			 if (i->basic == basic_info::branch && i->opcode != OP_JUMP && i->opcode != OP_JUMPX && i->opcode != OP_JUMPBACK) {
			 
				/* See if current exceded on jump. */
				auto append = false;
			 	for (auto& b : branch_temp) {
					
					if (b.second < (pos + decode_D(info->proto.code[pos]) + 1u))
						b.second = (pos + decode_D(info->proto.code[pos]) + 1u);
					else
						append = true;
			
				}

				/* Append. */
				if (append || !branch_temp.size ())
					branch_temp.insert(std::make_pair(pos, (pos + decode_D(info->proto.code[pos]) + 1u)));		
			
			 }
			 

			pos += i->size;
		}

		/* Cache values to look for. */
		std::vector<std::uint32_t> searching_sec;
		for (const auto& i : branches_2)
			if (std::find (searching_sec.begin (), searching_sec.end (), i.second) == searching_sec.end ())
				searching_sec.emplace_back(i.second);

		/* Remove duplicated with first being lowest. */
		for (const auto s : searching_sec) {
			auto start = UINT_MAX;
			for (const auto& i : branches_2)
				if (start > i.first && i.second == s)
					start = i.first;
			branches.insert(std::make_pair(start, s));
		}


		
		/* Now we need to see if any of the branches is logical operation. */
		for (const auto& i : branches) {

			/* Clear */
			auto on = i.first;
			pos = on;


			auto end = i.second;
			const auto end_cache = end;


			auto branch_disasm = dissassemble_range(info, on, end);
			auto ending_disasm = dissassemble_range(info, end, size);

			const auto on_dism = dissassemble(info, on);

			auto on_reg = 0u;
			auto bool_reg = UINT_MAX;
			auto hit_loadb = false;




			
			/* Get current logical operation reg. */
			for (const auto& curr : branch_disasm) {

				/* Garunteed. */
				if (curr->opcode == OP_JUMPIF || curr->opcode == OP_JUMPIFNOT)
					on_reg = decode_A(info->proto.code[pos]);


				/* Lowest reg for loadb. */
				if (curr->opcode == OP_LOADB && curr->dest_reg < bool_reg) {
					bool_reg = curr->dest_reg;
					hit_loadb = true;
				}

				/* End. */
				if ((pos + curr->size) >= end) {

					if (hit_loadb)
						on_reg = bool_reg;

					/* Fix up */
					end = pos;


					/* Now find last usage of target reg before routine. */
					auto temp_pos = 0u;
					const auto range = dissassemble_range(info, temp_pos, on);
					for (const auto& i : range) {
						if (i->dest_reg == on_reg && i->basic != basic_info::branch) /* Ignore branch and find target as dest. */
							on = temp_pos;
						temp_pos += i->size;
					}

					/* See first branch compare if it uses locvar as first compare use info else reset. */
					const auto first = dissassemble(info, i.first);
					if (first->opcode == OP_JUMPIF || first->opcode == OP_JUMPIFNOT)
						branch_disasm = dissassemble_range(info, on, end);
					else
						on = i.first;


					ending_disasm = dissassemble_range(info, end, size);
					break;
				}

				pos += curr->size;
			}

			/* Fix on. */
			const auto dissassemble_f = dissassemble(info, i.first);
			if (dissassemble_f->cmp_source_reg != on_reg || (dissassemble_f->has_cmp_value && dissassemble_f->cmp_value_reg != on_reg)) {

				auto dissam_pos = 0u;

				/* See if theres next. */
				for (const auto& a : dissassemble_range(info, 0u, i.first)) {
					if ((dissam_pos + a->size) == i.first)
						break;
					dissam_pos += a->size;
				}

				/* Check prev if lowests gets source gets used if no value. */
				const auto on_pos_dism = dissassemble(info, dissam_pos);
				if (!dissassemble_f->has_cmp_value) {

					if (on_pos_dism->dest_reg == dissassemble_f->cmp_source_reg)
						on = dissam_pos;
					else
						on = i.first;

				}
				else {

				}

				branch_disasm = dissassemble_range(info, on, end);
			}
		
			auto check = false;
			auto is_bool = false; /* Logical operation contains boolean. */


			/* Can't have a logical operation with only one branch. */
			auto branch_count = 0u;
			for (const auto& curr : branch_disasm)
				branch_count += (curr->basic == basic_info::branch);


			/* Only one branch so end */
			if (!branch_count)
				continue;


			/* Check if next to last instruction in branch matches with current reg to dest and if dest is greater then on_reg, etc. */
			pos = on;
			for (const auto& curr : branch_disasm) {
				
				/* Dest is an var that is lower then current so garunteed. */
				if (curr->basic != basic_info::fast_call &&
					curr->basic != basic_info::branch &&
					curr->basic != basic_info::for_ &&
					curr->opcode != OP_RETURN &&
					curr->opcode != OP_NOP &&
					curr->opcode != OP_CAPTURE &&
					curr->dest_reg < on_reg)
						goto end_routine;



				/* Set is bool must be loadb and source must be true with dest being on reg. */
				if (curr->opcode == OP_LOADB && curr->dest_reg == on_reg)
					is_bool = true;



				/* End of branch. Last dest instruction must be target. */
				if (pos == end && decode_A(info->proto.code[pos]) == on_reg)
					check = true;



				/* Garunteed end. */
				if (curr->basic == basic_info::for_)
					goto end_routine;



				/* If no return for call garunteed end or not out branch. */
				if ((curr->opcode == OP_CALL && !(std::int32_t(decode_C(info->proto.code[pos])) - 1)) || (curr->opcode == OP_LOADB && decode_C(info->proto.code[pos])))
					goto end_routine;



				/* Check if where on a bracnh with valid compare and see if jump matches with end if so check to see if source compare matches with reg if not garunteed end. */
				if (!is_bool && curr->basic == basic_info::branch && curr->opcode != OP_JUMP && curr->opcode != OP_JUMPBACK) {

					const auto jump = (decode_D(info->proto.code[pos]) + pos + 1u);

					/* Jump out if end matches with jump and source isn't target. */
					if (jump == end_cache) {
						if (decode_A(info->proto.code[pos]) != on_reg)
							goto end_routine;
					}
					else {

						/* Jump exceeds end so end. */
						if (jump > end_cache)
							goto end_routine;

						/* Jump, jump's somewhere in the routine so check if source isn't using reg. */
						if (decode_A(info->proto.code[pos]) == on_reg)
							goto end_routine;

					}

				}

				/* Garunteed end. */
				if (pos != end_cache) /* Only check before end. */
					switch (curr->opcode) {

						case OP_RETURN:
						case OP_SETGLOBAL:
						case OP_SETLIST:
						case OP_SETTABLE:
						case OP_JUMPBACK:
						case OP_SETTABLEKS:
						case OP_SETTABLEN:
						case OP_SETUPVAL: {
							goto end_routine;
						}

						default: {
							break;
						}

					}

				pos += curr->size;
			}


			/* Valid logical operation. */
			if (check) {
				auto ptr = std::make_shared<logical_operations>();
				ptr->end = end;
				ptr->reg = on_reg;
				ptr->end_jump = i.second;
				buffer.insert(std::make_pair(on, ptr));
			}


		end_routine:
			continue;
		}

		return;
	};


	auto begin = 0u;
	auto reg = 0u;
	auto pos = 0u;
	auto target = 0u;
	auto valid = false; /* Stages ignored. */

	const auto dissassemble_size = disasm.size();

	/* Write 3rd message (any other stuff will be ignored). */
	write_3rd_logial_operation(info, calls, disasm, buffer);

	/*
	* Count above comparisions as beggingins as to not collide with locvars.
	* Anything else is a normal branch.
	Ways
		1:
		instructions
		loadb a, 1, +2
		loadb a, 0
		2:
		cmp
		loadb r1, 0
		instructions
		loadb a, 1, +2
		loadb a, 0
		3:
		* No loadb +jump
		* Reg follows compare
		* Always reg in branch
		* Max branch jump is the end *Start with first dont go after end.
		* Ends with the reg.
		* End jump has reg exclusive to it (Unless loadb which will have reg every branch with 2nd operand being true).
	*/
	for (auto o = 0u; o < dissassemble_size; ++o) {

		const auto i = disasm[o];


		if (pos >= target)
			if (buffer.find(pos) != buffer.end())
				target = buffer[pos]->end;
			else {

			switch (i->opcode) {

				/* Check for assignment. */
			case OP_ADDK:
			case OP_ADD:
			case OP_SUBK:
			case OP_SUB:
			case OP_DIVK:
			case OP_DIV:
			case OP_MULK:
			case OP_MUL:
			case OP_POWK:
			case OP_POW:
			case OP_MODK:
			case OP_MOD:
			case OP_ANDK:
			case OP_AND:
			case OP_ORK:
			case OP_OR: {

				const auto code = info->proto.code[pos];

				/* Reset if dest and source1 regs are the same. */
				if (decode_A(code) == decode_B(code)) {
					begin = 0u;
					reg = 0u;
					valid = false;
				}

				break;
			}

					  /* Reset. */
			case OP_SETUPVAL:
			case OP_SETGLOBAL:
			case OP_RETURN: {
				begin = 0u;
				reg = 0u;
				valid = false;
				break;
			}



			case OP_CALL: {

				const auto retns = (std::int32_t(decode_C(info->proto.code[pos])) - 1);
				if (!retns) {
					begin = 0u;
					reg = 0u;
					valid = false;
				}

				break;
			}



			case OP_LOADB: {


				/* Get current register*/
				const auto curr_info = info->proto.code[pos];
				const auto curr_reg = decode_A(curr_info);


				/* Not jump log. */
				if (!(decode_C(curr_info)) && !valid) {

					begin = pos;

					const auto rend = end_logical_operation(begin, info->proto.size_code, info); /* Loadb jump register.*/
					reg = (rend == -1) ? curr_reg : rend;

					/* Found cmp loadb but doesnt loadb jump same register. */
					if (reg != curr_reg) {
						begin = 0u;
						reg = 0u;
						valid = false;
						break;
					}

					if ((disasm[o - 1u])->is_comparative) {
						valid = true;
						begin = (pos - (disasm[o - 1u])->size); /* Log begin as cmp address. */
					}

				}
				else if (decode_C(curr_info)) {

					/* Begin got reset so set. */
					if ((disasm[o - 1u])->is_comparative && !begin) {
						begin = (pos - 2u);
						reg = decode_A(info->proto.code[pos]);
					}

					// Has jump check. 
					const auto next = disasm[o + 1u];
					const auto next_info = info->proto.code[pos + 1];
					const auto next_reg = decode_A(next_info);


					// instruction.
					// loadb rz, 0, +2
					// loadb rz, 1
					// JMP:
					// continue;

					// If it has a jump with a comparative above we need to find if it links to any comparatives and get the first comparative.
					// designed to handle expressions where loadb only gets called once after routine. 
					// last compare ignores other compare if reg is 0 but if not checks to make sure there wasnt a dummy loadb in the control flow. 
					if (!begin && !reg && curr_reg == next_reg && (!reg || reg != curr_reg)) {
						auto ptr = std::make_shared<logical_operations>();
						ptr->end = (pos + i->size);
						ptr->reg = reg;
						buffer.insert(std::make_pair(get_logical_op_unk_comparative(info, disasm, pos), ptr));
					}




					// OP_LOADB
					// instructions
					// designed to take multiple loadbs in a expression. 
					if (next->opcode == OP_LOADB && curr_reg == next_reg) {

						auto ptr = std::make_shared<logical_operations>();
						ptr->end = (pos + i->size);
						ptr->reg = reg;

						buffer.insert(std::make_pair(begin, ptr));
						valid = false;
						begin = 0u;
						reg = 0u;
					}


				}

				break;
			}

			}

		}


		pos += i->size;
	}




	return;
}


/* Determine if a compare in logical operation (Also determines branch stuff and loop compare stuff) is and or plane etc. */
void determine_logical_operation_cmp(std::shared_ptr<decompile_info>& info, std::unordered_map<std::uint32_t, std::shared_ptr<logical_operations>>& logical_operations, std::unordered_map<std::uint32_t, std::vector<std::pair<std::uint32_t, std::pair <bool, bool>>>>& loop_map, std::unordered_map<std::uint32_t, std::shared_ptr <branch_comparative>>& buffer) {


	/* Map logical comparisions. */
	for (const auto& a : logical_operations) {

		auto ranged_idx = 0u;


		const auto dissassemble = dissassemble_range(info, a.first, a.second->end);
		

		/* Loadb +jump are garunteed so there stored as end but anything else is end_jump. */
		const auto end = (a.second->end_jump) ? a.second->end_jump : a.second->end;


		/* Only useful for loadb +jump. */
		const auto end_ = (end - 1u);
		const auto end_jump = (end + 1u);

		
		const auto reg = a.second->reg;

		/* True good bad false*/
		auto good = (decode_B(info->proto.code[end])) ? end : end_ ;
		auto bad = (good == end) ? end_ : end;

	
		branch_data_e last = branch_data_e::none;

		auto pos = a.first;

		auto prem_true = false; /* Given true within chunk. */

		/* Current bool. */
		const auto cu_bool = ((dissassemble.front()->opcode == OP_LOADB) ? decode_B(info->proto.code[a.first]) : decode_B(info->proto.code[a.first + dissassemble.front()->size]));


		auto parent_jump = 0u;

		auto bcount_end = 0u; /* Branch count end. */
		auto target_pos = 0u; /* Make pos  = target_pos*/


		/* Gets next jump pos before end. Returns bool and position thought if first value in pair is -1 it's undecisive. 0 on pos means that there is no next cmp.  */
		auto next_relative_cmp_pos = [=](const std::uint32_t end, const std::uint32_t idx, const std::uint32_t pos) ->std::pair <std::int8_t, std::uint32_t> {

			auto retn = std::make_pair(-1, 0);

			auto dummy_pos = pos + 1u;

			std::int8_t boolean = -1;


			for (auto i = (idx + 1); i < dissassemble.size(); ++i) {

				const auto& on = dissassemble[i];


				switch (on->opcode) {

							case OP_LOADB: {
								/* Log it. */
								boolean = decode_B(info->proto.code[dummy_pos]);
								break;
							}

				
							case OP_JUMPIFEQK:
							case OP_JUMPIFNOTEQK:
							case OP_JUMPIFEQ:
							case OP_JUMPIFLE:
							case OP_JUMPIFLT:
							case OP_JUMPIFNOTEQ:
							case OP_JUMPIFNOTLE:
							case OP_JUMPIFNOTLT:
							case OP_JUMP:
							case OP_JUMPIFNOT:
							case OP_JUMPIF: {

								/* Return. */

								retn.first = boolean;
								retn.second = dummy_pos;
								return retn;

							}

				}

				/* End. */
				if (dummy_pos == end)
					return retn;

				dummy_pos += on->size;

			}


			return retn;
		};

		for (const auto& i : dissassemble) {

 			const auto curr_op = i->opcode;

			if (target_pos) {

				if (target_pos == pos)
					target_pos = 0u;

			}
			else
			  switch (curr_op) {


							case OP_LOADB: {

								const auto curr = info->proto.code[pos];
								if (decode_A(curr) == reg)
									prem_true = decode_B(curr);

								break;
							}

							case OP_JUMPIFEQK:
							case OP_JUMPIFNOTEQK:
							case OP_JUMPIFEQ:
							case OP_JUMPIFLE:
							case OP_JUMPIFLT:
							case OP_JUMPIFNOTEQ:
							case OP_JUMPIFNOTLE:
							case OP_JUMPIFNOTLT:
							case OP_JUMP:
							case OP_JUMPX:
							case OP_JUMPIFNOT:
							case OP_JUMPIF: {

								/*
								    jumps with 0 = and
								    jumps with 1 is or 
									** else end **

									|| if its undecisive find next jump 
								*/
								
								/* Check for JumpX. */
								const auto jump = (((curr_op == OP_JUMPX) ? decode_E(info->proto.code[pos]) : decode_D(info->proto.code[pos]))  + pos + 1u);

								/* Set parent jump. */
								if (!parent_jump)
									parent_jump = jump;


						    	auto ptr = std::make_shared<branch_comparative>();


								/* Hit. */
								if (!bcount_end && logical_operations.find(pos) != logical_operations.end() && (branch_count(info, pos, logical_operations[pos]->end) == 1u || branch_count(info, pos, logical_operations[pos]->end) == 0u)) {
									target_pos = logical_operations[pos]->end;
									break;
								} else 
									bcount_end = ((logical_operations.find(pos) != logical_operations.end()) ? logical_operations[pos]->end : 0u);
			
								/* Branch jumps to good or bad. *Must be loadb +jump */
								if (!a.second->end_jump && (jump == good || jump == bad)) {
		
									const auto next_jmp = next_relative_cmp_pos(end_jump, ranged_idx, pos);


									/* We have to treat them differently. */
									if (next_jmp.second) {

										/* If it has a second jump usally means or. */

										/* Check and see if there is a parent jump. */
										if (parent_jump < next_jmp.second) {

										}
										else {

										}

									}
									else {

										/* Close out the statement thought if last is none do nothing. */
										ptr->i = ((last == branch_data_e::none) ? branch_data_e::none : branch_data_e::close);


										/* If it's good we can keep it though if it's bad change it. */
										ptr->compare = ((jump == good) ? branch_normal(curr_op) : branch_opposite(curr_op));

									}

									last = ptr->i;
									
									buffer.insert(std::make_pair(pos, ptr));

									break;
								}

								/* Jumps out of routine so what ever current loadb it is. */
								/* End of jump is more exact for this. */
								if (jump == end_jump || a.second->end_jump) {

									const auto next_jmp = next_relative_cmp_pos(end_jump, ranged_idx, pos);

									// Jumps out of routine with false so do opposite. Or there is end jump and jump is inside it.
									if (!cu_bool || (a.second->end_jump && jump < a.second->end_jump)) {

										/* Has second jump so do oppoite and place and. */
										/* Also first jump so open though if theres a second jump. If not find next jump place and its the first jump place and if there is no next and its not first just close it. */
										ptr->i = ((last == branch_data_e::none && next_jmp.second) ? branch_data_e::and_open : ((next_jmp.second) ? branch_data_e::and_ : ((last == branch_data_e::none) ? branch_data_e::and_ : branch_data_e::and_close)));

										/* Just do and if it's a 3rd method.*/
										if (a.second->end_jump)
											ptr->i = branch_data_e::and_;

										/* Because it jumps out with false we want it to be opposite. */
										ptr->compare = branch_opposite(curr_op);
									
									}
									else {

										/* Jumps out with positive so like with we did with and we want this to be or basically we do the same thing except keep compare original. */
										ptr->i = ((last == branch_data_e::none && next_jmp.second) ? branch_data_e::or_open : ((next_jmp.second) ? branch_data_e::or_ : ((last == branch_data_e::none) ? branch_data_e::or_ : branch_data_e::or_close)));

										/* Just do or if it's a 3rd method.*/
										if (a.second->end_jump)
											ptr->i = branch_data_e::or_;

										/* Because it jumps out with false we want it to be opposite. */
										ptr->compare = branch_normal(curr_op);
									
									}
									
									buffer.insert(std::make_pair(pos, ptr));

									last = ptr->i;
									
									break;
								}

								parent_jump = jump;

								/* Are jump, jumps to somewhere in the branch. */

								break;
							}

			}

			if (bcount_end == pos)
				bcount_end = 0u;

			pos += i->size;
			++ranged_idx;

		}

	}


	/* Map loop info. */
	for (const auto& a : loop_map) {

		auto pos = a.first;
		const auto end = a.second.back().first;
		const auto dissassemble = dissassemble_range(info, pos, end);

		for (const auto& i : dissassemble) {

			const auto curr_op = i->opcode;

			switch (curr_op) {

				case OP_JUMPIFEQK:
				case OP_JUMPIFNOTEQK:
				case OP_JUMPIFEQ:
				case OP_JUMPIFLE:
				case OP_JUMPIFLT:
				case OP_JUMPIFNOTEQ:
				case OP_JUMPIFNOTLE:
				case OP_JUMPIFNOTLT:
				case OP_JUMP:
				case OP_JUMPX:
				case OP_JUMPIFNOT:
				case OP_JUMPIF: {


					/* Check for JumpX. */
					const auto jump = (((curr_op == OP_JUMPX) ? decode_E(info->proto.code[pos]) : decode_D(info->proto.code[pos])) + pos + 1u);

					auto ptr = std::make_shared<branch_comparative>();
					
					/* Jumps too end so or. */
					if (jump >= end) {

						ptr->i = ((pos != end) ? branch_data_e::or_ : branch_data_e::none);
						ptr->compare = ((pos != end) ? branch_normal(curr_op) : branch_opposite(curr_op));

					}
					else {

						ptr->i = ((pos != end) ? branch_data_e::and_ : branch_data_e::none);
						ptr->compare = branch_opposite(curr_op);

					}

					buffer.insert(std::make_pair(pos, ptr));

					break;
				}

			}



			pos += i->size;

		}

	}

	return;
}

#pragma endregion


#pragma region localizer



/* Sees if an register is used as a call/global/table/branch_compare or used no where else afterwards but doesnt happen if its last usage isnt in dest operand. 
garunteed with call/global/table
*/
bool register_used_vital(std::shared_ptr<decompile_info>& info, std::unordered_map<std::uint32_t, std::shared_ptr<call_routine_st>>& calls_routine, const std::vector <std::shared_ptr<dissassembler_data>>& disasm, const std::uint32_t target) {

	
	auto pos = 0u;
	auto end = 0u;
	auto lo = 0u;

	for (const auto& i : disasm) {

		if (!end && calls_routine.find(pos) != calls_routine.end())
			end = calls_routine[pos]->end;




		/* See if where inside a table. */
		if (!lo && (i->opcode == OP_NEWTABLE || i->opcode == OP_DUPTABLE))
			lo = table::end(info, pos, info->proto.size_code);

		/* End of current table. */
		if (lo == pos)
			lo = 0u;




		/* Gaunteed. */
		if (end || lo) {
			/* used in  call/table */
			if ((i->has_source && i->source_reg == target) || (i->has_value && i->value_reg == target))
				return true;
		}

		if (end < pos)
			end = 0u;
		
		pos += i->size;
	}

	return false;
}


/* Determines when a instruction should localize its register. Recreates locvar info which is stored in proto. */
void register_localize(std::shared_ptr<decompile_info>& info, std::unordered_map<std::uint32_t, std::shared_ptr<call_routine_st>>& calls_routine, std::unordered_map<std::uint32_t, std::vector <type_branch>>& branches, std::unordered_map<std::uint32_t, std::vector <std::uint32_t>>& dedicated_loops, std::unordered_map<std::uint32_t, std::shared_ptr<logical_operations>>& logical_operations, std::unordered_map<std::uint32_t, std::vector <std::pair<std::uint32_t, std::pair <bool, bool>>>>& loop_map, std::unordered_map<std::uint32_t, std::uint32_t>& skip_locvar, std::unordered_map<std::uint32_t, std::uint32_t>& concat_routines, std::vector <std::uint32_t>& ends) {

	const auto disasm = dissassemble_whole(info);

	std::unordered_map<std::uint32_t, std::uint32_t> reset_on_scope; /* When pos is a key, on_reg is sey to it's value {pos, on_register}. */
	std::vector<std::uint32_t> last_if; /* Back is last if *Used for elseif, else. */

	/* Sees if register is used in dest operand in call routine. */
	auto reg_used_in_call_start = [=](const std::uint32_t reg, const std::uint32_t start, std::unordered_map<std::uint32_t, std::shared_ptr<call_routine_st>>& cr, std::unordered_map<std::uint32_t, std::vector <type_branch>>& branches, std::vector <std::uint32_t>& ends) -> std::uint32_t {

		auto lam_pos = 0u; /* Position. */
		auto end = 0u; /* End of call routine */
		auto branch_counter = 0u;
	
		for (const auto& i : disasm) {
			
			if (start > lam_pos) {
				lam_pos += i->size;
				continue;
			}


			/* Log end. */
			if (std::find(ends.begin(), ends.end(), lam_pos) != ends.end()) {

				/* Exceeded end so just return. */
				if (!branch_counter)
					return 0u;

				branch_counter -= std::count(ends.begin(), ends.end(), lam_pos);
			}

			/* Log for branch should be fine for encapsulation. */
			if (branches.find(lam_pos) != branches.end()) {

				/* Any if loops we will skip there statement routine. */
				for (const auto tb : branches[lam_pos])
					switch (tb) {

						case type_branch::if_: {
							++branch_counter;
							break;
						}

						/* Fixes on_register. */
						case type_branch::else_:
						case type_branch::elseif_: {
							
							/* End of else routine so just return. */
							if (!branch_counter)
								return 0u;

							break;
						}

					}

			}


		

			/* Inside call routine. */
			if (cr.find(lam_pos) != cr.end()) {
				const auto& ca = cr[lam_pos];
				end = ca->end;
			}
			
			/* End. */
			if (end == lam_pos)
				end = 0u;
			
			/* Where inside a call and destreg we hit corresponds to given reg. (Capture has int for dest operand). */
			if (end && i->dest_reg == reg &&
				i->opcode != OP_CAPTURE &&
				i->basic != basic_info::fast_call &&
				i->ancestor != ancestor_info::settable &&
				i->basic != basic_info::branch &&
				i->basic != basic_info::for_)
					return lam_pos;
			


			lam_pos += i->size;
		}

		return 0u;
	};


	/* Gets next loop prep location. */
	auto next_loop_prep = [=](const std::uint32_t start, std::shared_ptr<decompile_info>& info) -> std::uint32_t {

		auto lam_pos = 0u;

		for (const auto& i : disasm) {

			/* Start at target start. */
			if (start <= lam_pos)
				switch (i->opcode) {

						/* Loop preps. */
						case OP_FORGPREP:
						case OP_FORGPREP_INEXT:
						case OP_FORGPREP_NEXT:
						case OP_FORNPREP: {
							return lam_pos;
						}

						/* No loops afterwards*/
						case OP_FORGLOOP_INEXT:
						case OP_FORGLOOP_NEXT:
						case OP_FORNLOOP: {
							return 0;
						}


						/* If prep is not garunteed. */
						case OP_JUMPX:
						case OP_JUMP: {

							/* Jump leads to forgloop so mark it. */

							/* Check for JUMPX. */
							const auto jump = (((i->opcode == OP_JUMP) ? decode_D(info->proto.code[lam_pos]) : decode_E(info->proto.code[lam_pos])) + lam_pos + 1u);
							if (dissassemble(info, jump)->basic == basic_info::for_)
								return jump;

							break;
						}

				}



			lam_pos += i->size;
		}

		return 0u;
	};


	/* Next register used in assignment. */
	auto next_register_assignment = [=](const std::uint32_t start) -> std::int32_t {

		auto idx = 0u;
		auto lam_pos = 0u;
		const auto end = disasm.size();

		for (const auto& i : disasm) {

			/* end */
			if ((idx + 1u) >= end)
				return -1;

			if (start <= lam_pos) {

				/* Garunteed assignment if its setglobal or setupvalue. */
				if ((*(&i + 1))->opcode == OP_SETGLOBAL || (*(&i + 1))->opcode == OP_SETUPVAL)
					return (*(&i + 1))->dest_reg;

			}

			lam_pos += i->size;
			++idx;
		}

		return -1;
	};



	auto null_pos_table = false; /* If empty table at begginging accept it. */
	auto pos = 0u; /* Current position. */
	auto target = 0u; /* Target position. *Usally used as call routine end. */
	auto on_register = info->proto.closure.arg_count; /* Current target dest register. */
	auto limit_reg = -1; /* on_register aka target can not exceed this value. */
	auto idx = 0u; /* Dissassembly index. */
	auto target_amt = 0u; /* Target position for validity. */
	auto curr_target_amt = 0u; /* Target (ip + amt). */
	auto curr_target_reg = 0u; /* What reg were looking for; for tables. */
	auto next_loop = 0; /* Next loop. If it's -1 no loops after position. */
	auto secondary_limit_reg = 0u; /* Refere to limit_reg. */
	auto in_table = 0u; /* Inside table? *End of current table. */
	auto table_reg = 0u; /* Dest register of parent register. */
	auto table_parent = 0u; /* Parent pos of table. */
	auto prev_dest = 0u; /* Prevois dest operand. */
	auto table_no_setlist = false; /* No setlist after the table. */
	auto garunteed_till = 0u; /* Grunteed pos that will be lovar. */
	const auto disasm_end = disasm.size(); /* End of code. */

	std::vector <std::int32_t> loop_blacklist; /* Blacklisted registers that cant be variables r+2 fornloop. -1 is nothing used as place holder. */

	for (const auto& i : disasm) {
		
		prev_dest = i->dest_reg;

		/* End of range we want to analyze. */
		if ((idx + 1u) == disasm_end)
			break;
	
		/* Log skips within logical operators. */
		if (skip_locvar.find(pos) != skip_locvar.end())
			target = skip_locvar[pos];

		/* Log and skip logical operation. */
		if (logical_operations.find(pos) != logical_operations.end())
			target = logical_operations[pos]->end;
		
		/* Log and skip loop compares. */
		if (loop_map.find(pos) != loop_map.end())
			target = loop_map[pos].back().first;


		/* End of scope. */
		if (reset_on_scope.find(pos) != reset_on_scope.end()) {
			on_register = reset_on_scope[pos];
			reset_on_scope.erase(pos); /* Remove cache. */
		}
	
		/* Begin of a scope. */
		if (branches.find(pos) != branches.end()) {

			/* Any if loops we will skip there statement routine. */
			for (const auto tb : branches[pos])
				switch (tb) {

					/* For, while, repeat loops. */
					case type_branch::for_:
					case type_branch::while_:
					case type_branch::repeat: {

						/* Add scope end. */
						for (const auto on : dedicated_loops[pos])
							reset_on_scope.insert(std::make_pair(on, on_register));
				
						const auto curr_code = info->proto.code[dedicated_loops[pos].front()];

						/* Add loop. */
						switch (decode_opcode(info->proto.code[dedicated_loops[pos].front ()])) { /* The same address will be accounted for beging loop. */


							case OP_FORNLOOP: {
								on_register = (decode_A(curr_code) + 3u);
								loop_blacklist.emplace_back(signed(on_register) - 1); /* +2 cant be mutated. */
								break;
							}
							case OP_FORGLOOP_INEXT:
							case OP_FORGLOOP_NEXT: {
								on_register = (decode_A(curr_code) + 5u);
								loop_blacklist.emplace_back(-1);
								break;
							}
							case OP_FORGLOOP: {
								on_register += (info->proto.code[dedicated_loops[pos].front () + 1u] + 3u); /* You cant have multiple for loops at the same address. */
								loop_blacklist.emplace_back(-1);
								break;
							}

							default: {
								break;
							}

						}

						break;
					}
						
					case type_branch::if_:{

						const auto jmp = (decode_D(info->proto.code[pos]) + signed(pos) + 1);

						if (reset_on_scope.find (jmp) == reset_on_scope.end())
							reset_on_scope.insert(std::make_pair(jmp, on_register));

						last_if.emplace_back(pos);
						break;
					}
					
					/* Fixes on_register. */
					case type_branch::else_:
					case type_branch::elseif_: {
						
						const auto jmp_pos = last_if.back();
						const auto jmp = (decode_D(info->proto.code[jmp_pos]) + signed(jmp_pos) + 1);
						on_register = reset_on_scope[jmp];

						break;
					}
				

				}

		}


		/* Set next loop. *Some preps require future setting but we will resolve cache when we hit prep. */    
		next_loop = next_loop_prep(pos, info);

		if (next_loop){

			/* Scopes ranges are handled in earlyer code. */
			/* Set limit with prep 1st reg operand. */
			limit_reg = decode_A(info->proto.code[next_loop]);
	    
		}


		/* Check table. *Skip table is analyzed later. */
		if (!in_table && (i->opcode == OP_NEWTABLE || i->opcode == OP_DUPTABLE)) {
			in_table = table::end(info, pos, info->proto.size_code);
			table_reg = decode_A(info->proto.code[pos]);
			table_parent = pos;
			if (!in_table)
				null_pos_table = true;
		}


		/* End of table with pos so set to 0 for next table. *Make sure there is a in_table before resetting it. */
		if ((in_table || null_pos_table) && pos == in_table) {
			in_table = 0u;
			if (i->opcode != OP_SETLIST)
				table_no_setlist = true;
		}


		/* Skip captures to check if its global. */
		if (i->opcode == OP_NEWCLOSURE) {

			/* Skip captures. */
			auto o = 1u;
			while ((*(&i + o))->opcode == OP_CAPTURE)
				++o;

			/*
			* Global closure:
			   newclosure r1 proto_0
			   capture 1 r0
			   setglobal r1 gang
			*/
			if ((*(&i + o))->opcode == OP_SETGLOBAL && (*(&i + o))->dest_reg == i->dest_reg)
				target = (pos + o + dissassemble(info, (pos + o))->size); /* Skip routine. */

		}


		/* Go through disasm and skip any call routine. */
		if (calls_routine.find(pos) != calls_routine.end() && !target) {
			const auto& ca = calls_routine[pos];
			target = ca->end;
		}

		/* Skip for concat routines. Refer to compiler info in configs.hpp. */
		if (concat_routines.find(pos) != concat_routines.end())
			target = concat_routines[pos];

		/* End target. */
		if (target <= pos) 
			target = 0u;


		/* Second compare when end of table isn't OP_SETLIST. */
		if (!target && table_no_setlist && table_reg == on_register && !reg_used_in_call_start(table_reg, pos, calls_routine, branches, ends)) {
			
			/* See if ending table emblacement matches with parent emblacement. */
			const auto poss = (decode_A(info->proto.code[table_parent]) == ((i->ancestor == ancestor_info::settable) ? decode_B(info->proto.code[pos]) : decode_A(info->proto.code[pos]))) ? pos : table_parent;

			auto ptr = std::make_shared<loc_var>();
			ptr->end = poss;
			ptr->reg = on_register;
			info->proto.locvars.emplace_back(ptr);
			++on_register;
			
			

			/* Reset for next table. */
			table_reg = 0u;
			table_no_setlist = false;
			table_parent = 0u;
		}

		/*
		 Found if:
		  * Not inside table
		  * If there is a secondary_limit check to see if on_register is less than it.
		  * Next opcode isn't return.
		  * No branches or fastcall (May chang ein the future for now we can ignore it).
		  * No limit register and on_register is less than limit.
		  * Opcode isnt nop, capture (used for setting upvalues so dont need to check)
		  * Current dest = target dest
		  * Not looking for a target position (used for skipping tables, calls, etc)
		  * Register isnt used in a call routine.
		  * If call make sure it has return.
		  * No value operand.
		  * No repeat loop aftwards. 
		  * No settable. 
		*/
		const auto next_reg_used = reg_used_in_call_start(on_register, pos, calls_routine, branches, ends);
		if (!in_table &&
			(!secondary_limit_reg || on_register < secondary_limit_reg) &&
			i->basic != basic_info::fast_call &&
			i->basic != basic_info::branch &&
			i->basic != basic_info::for_ &&
			i->ancestor != ancestor_info::settable &&
			(!next_loop || on_register < unsigned(limit_reg)) &&
			i->opcode != OP_NOP &&
			i->opcode != OP_CAPTURE &&
			i->opcode != OP_RETURN &&
			i->dest_reg == on_register &&
			(i->opcode != OP_CALL || (decode_C(std::int32_t(info->proto.code[pos])) - 1)) &&
			!target &&
			(!next_reg_used || (i->opcode == OP_NAMECALL && (pos + i->size) == next_reg_used)) &&
			(loop_map.find(pos + i->size) == loop_map.end() || !loop_map[pos + i->size].back().second.first)) {

				/* Skip setglobal. */
				const auto next = next_register_assignment(((*(&i + 1))->opcode == OP_SETGLOBAL || (*(&i + 1))->opcode == OP_SETUPVAL) ? (pos + i->size + (*(&i + 1))->size) : pos);


				/* Check return. */
				if ((*(&i + 1u))->opcode == OP_RETURN) {

					const auto retn = (decode_B(std::int32_t(info->proto.code[pos + i->size])) - 1);
					const auto reg = decode_A(info->proto.code[pos + i->size]);
					
					/* Reg used in return. */
					if (retn == -1)
						goto locvar_ast_end;
					
					/* Reg used in return. */
					if (retn)
						for (auto i = 0u; i < unsigned(retn); ++i) 
							if ((i + reg) == on_register)
								goto locvar_ast_end;

					/* Isn't used in return so locvar. */
					goto set_locvar_2;
				}
	

				/* On blacklist. *Used for limiting for prep registers. */
				for (const auto b : loop_blacklist)
					if (b == signed(i->source_reg)) { /* Only for like 2 operands. */
						/* Remove and inc on_register. */
						loop_blacklist.pop_back();
						++on_register;
						goto locvar_ast_end;
					}
			
				/*
					namecall r2, r2, 0
					call r2, 1, 1
				*/
				if (i->opcode == OP_NAMECALL && i->dest_reg == i->source_reg && (*(&i + 1u))->opcode == OP_CALL && (*(&i + 1u))->dest_reg == i->dest_reg && decode_C(info->proto.code[pos + i->size])) {
					
	
					if (logical_local(info, pos, info->proto.size_code, on_register, garunteed_till, calls_routine, concat_routines, loop_map, logical_operations, skip_locvar))
						goto set_locvar_1;
					else
						goto locvar_ast_end;


				set_locvar_1:
				
							if (garunteed_till) { /* Respect garunteed_till. */
								if (pos != garunteed_till) {
									target = garunteed_till;
									goto locvar_ast_end;
							    }
								garunteed_till = 0u;
								target = 0u;
							}
							
							/* No next and target isnt found in locvars. */
							if (next == -1 || std::uint32_t(next) >= on_register) {
								auto ptr = std::make_shared<loc_var>();
								ptr->end = (pos + i->size);
								ptr->reg = on_register;
								info->proto.locvars.emplace_back(ptr);
								++on_register;
							}		

				}
				else {


					if (logical_local(info, pos, info->proto.size_code, on_register, garunteed_till, calls_routine, concat_routines, loop_map, logical_operations, skip_locvar))
						goto set_locvar_2;
					else
						goto locvar_ast_end;

						
					if (i->opcode == OP_GETVARARGS && (decode_B(info->proto.code[pos]) - 1) != 1) {

								/* Respect OP_GETVARARGS 2nd operand. */
								auto amt = (decode_B(std::int32_t(info->proto.code[pos])) - 1);

								if (amt == -1)
									amt = prev_dest;

								if (next == -1 || std::uint32_t(next) >= on_register) {
									auto ptr = std::make_shared<loc_var>();
									ptr->end = pos;
									ptr->reg = on_register;
									info->proto.locvars.emplace_back(ptr);
								}

								on_register += amt;
							}
					else {

					set_locvar_2:
									if (garunteed_till) { /* Respect garunteed_till. */
										if (pos != garunteed_till) {
											target = garunteed_till;
											goto locvar_ast_end;
										}
										garunteed_till = 0u;
										target = 0u;
									}

									/* No next and target isnt found in locvars. */
									if (next == -1 || std::uint32_t(next) >= on_register) {									
										auto ptr = std::make_shared<loc_var>();
										ptr->end = pos;
										ptr->reg = on_register;
										info->proto.locvars.emplace_back(ptr);
										++on_register;
									}
					}
					
					

					

				}

		}


		locvar_ast_end:

		/* Add register to locvar if targets are met. */
		if (!target && target_amt && (i->opcode == OP_SETTABLEKS || i->opcode == OP_SETTABLEN) && curr_target_amt++ == target_amt) {

			/* Clear targets. */
			target_amt = 0u;
			curr_target_amt = 0u;

			/* Make sure register isnt used in call. */
			if (!reg_used_in_call_start(curr_target_reg, pos, calls_routine, branches, ends)) {

				/* Skip setglobal. */
				const auto next = next_register_assignment(((*(&i + 1))->opcode == OP_SETGLOBAL || (*(&i + 1))->opcode == OP_SETUPVAL) ? (pos + i->size + (*(&i + 1))->size) : pos);

				/* Add locvar. */
				if (next == -1 || std::uint32_t(next) >= curr_target_reg) {
					auto ptr = std::make_shared<loc_var>();
					ptr->end = pos;
					ptr->reg = curr_target_reg;
					info->proto.locvars.emplace_back(ptr);
					++on_register;
				}

			}


		}

		/* Clear loop. */
		if (next_loop == pos) {
			next_loop = 0;
			limit_reg = -1;
		}

		null_pos_table = false;
		pos += i->size;
		++idx;
	}


	/* Set up everything to iterate again for table. */
	/* Get locvars for table. ex. {1, (1 + a), (aa + o)}. */

	in_table = 0u;
	pos = 0u;

	auto curr_table_on = 0u;
	auto curr_table_limit = 0u;
	auto curr_end = 0u;

	std::vector <std::uint32_t> indexs_map;

	/* Iterate again setting info for table in locvar. */
	for (const auto& i : disasm) {

		/* 
		
		 Locvar 
	
		* Not fastcall, for loop, branch
		* OP_NOP, OP_CAPTURE
		* Current target is dest
		* Current dest doesn't exceed limit.
	
		*/
		if (curr_end &&
			i->basic != basic_info::fast_call &&
			i->basic != basic_info::branch &&
			i->basic != basic_info::for_ &&
			i->opcode != OP_NOP &&
			i->opcode != OP_CAPTURE &&
			curr_table_on == i->dest_reg &&
			curr_table_on <= curr_table_limit) {


				/* Found pos as an index. ex. _test = "1000", */
				if (std::find(indexs_map.begin(), indexs_map.end(), pos) != indexs_map.end())
					goto for_continue;

				auto ptr = std::make_shared<loc_var>();
				ptr->end = pos;
				ptr->reg = i->dest_reg;
				ptr->in_table = true;
				info->proto.locvars.emplace_back(ptr);
				++curr_table_on;

		}
		

		for_continue:

		/* End. */
		if (curr_end == pos) {
			curr_end = 0u;
			indexs_map.clear();
		}


		/* Set data on table entry. */
		if (i->opcode == OP_NEWTABLE || i->opcode == OP_DUPTABLE) {
			const auto end = table::end(info, pos, info->proto.size_code); /* Location of end of table. */

			/* Set cache. */
			if (!curr_end) {
				indexs_map = table::indexs(info, pos, end);
				curr_end = end;
			}


			curr_table_on = table::smallest_reg(info, pos, end); /* Set as smallest. */
			curr_table_limit = (curr_table_on + info->proto.code[pos + 1u]); /* Set smallest + table amount as limit. */

		}


		/* Set cache relative to parent. */
		if (i->opcode == OP_SETLIST && curr_end) {
			curr_table_on = table::smallest_reg(info, pos, curr_end); /* Set as smallest. */
			curr_table_limit = (curr_table_on + info->proto.code[pos + 1u]); /* Set smallest + table amount as limit. */
		}


		pos += i->size;
	}

	/* Remove duplicates. */
	std::vector<std::shared_ptr<loc_var>> new_locvars;
	for (const auto& locvar : info->proto.locvars) {

		auto found = false;

		for (const auto& new_locvar : new_locvars) 
			if (new_locvar->end == locvar->end) {
				found = true;
				break;
			}
		

		if (!found) 
			new_locvars.emplace_back(locvar);
		
	}

	
	info->proto.locvars = new_locvars;


	/* Finally sort it by ends. */
	std::sort(info->proto.locvars.begin(), info->proto.locvars.end(), [=](const auto& a, const auto& b) {return a->end < b->end; });


	return; 
}




#pragma endregion


#pragma  region call_routine 

/* Writes call routines *Solve encapsulation during decompilation time */
void write_call_routines(std::shared_ptr<decompile_info>& info, std::unordered_map<std::uint32_t, std::shared_ptr<call_routine_st>>& buffer) {

	const auto dissassembled = dissassemble_whole(info);

	auto pos = 0u;
	auto idx = 0u;

	/* Gets first time register gets used as dest. */
	auto first_dest = [=](const std::uint32_t reg, const std::uint32_t end_pos) -> std::int32_t {
		
		auto start = 0u;
		auto pos_ = 0u;

		for (const auto& i : dissassembled) {

			if (pos_ == end_pos)
				return start;
			else if (i->dest_reg == reg && i->basic != basic_info::branch) {  
				/* curr dest = target, not a branch and no namecall the call. */

				if (i->opcode == OP_NAMECALL) {

					const auto& next = (*(&i + 1u));

					/*
						  namecall  r5 r4 oop
						  call      r5 2 0
					*/
					if (next->opcode != OP_CALL || (next->opcode == OP_CALL && i->dest_reg != i->source_reg))
						start = pos_;

				}
				else
					start = pos_;
			
			}

			pos_ += i->size;
		}

		return 0u;
	};

	for (const auto& i : dissassembled) {

		if (i->opcode == OP_CALL) {
			
			auto ptr = std::make_shared<call_routine_st>();
			ptr->end = pos;
			ptr->idx = idx;
			buffer.insert(std::make_pair(first_dest(decode_A(info->proto.code[pos]), pos), ptr));

		}

		pos += i->size;
		++idx;

	}



	return;
}

#pragma endregion


/* Make into namespace aswell as other functions in the future. */
void write_concat_routines(std::shared_ptr<decompile_info>& info, std::unordered_map<std::uint32_t, std::uint32_t>& buffer) {
	std::vector<std::uint32_t> concat_map;

	const auto dism = dissassemble_whole(info);

	/* Find all OP_CONCATS. */
	auto pos = 0u;
	for (const auto& i : dism) {
		if (i->opcode == OP_CONCAT)
			concat_map.emplace_back(pos);
		pos += i->size;
	}

	for (const auto i : concat_map) {

		pos = 0u;
		auto found = 0u;
		const auto target = decode_B(info->proto.code[i]);
		
		/* Find last dest that uses target and log. */
		for (const auto& o : dism) {

			/* End. */
			if (pos == i)
				break;

			if (o->dest_reg == target)
				found = pos;
			
			pos += o->size;
		}

		buffer.insert(std::make_pair(found, i));
	}

	return;
}

#pragma region closure_core 


/* Gets arguments of a closure. */
std::uint32_t get_args(std::shared_ptr<decompile_info>& info) {

	std::vector <std::uint32_t> dests; /* Registers used in dest.*/
	std::vector <std::uint32_t> source_no_dest; /* Registers used in source, value but not dest. */

	const auto disasm = dissassemble_whole(info);
	const auto loops = map_loops(info);

	auto prev_dest = 0u;
	auto pos = 0u;
	for (const auto& i : disasm) {

		/* See if pos is a loop. */
		for (const auto& a : loops)
			if (a.first == pos && a.second->tt == loop_type::for_end) {
			
				const auto on = a.second->end;
				const auto target = decode_A(info->proto.code[on]);
				const auto curr_op = decode_opcode(info->proto.code[on]);

				auto iteration = 0u;

				switch (curr_op) {

					case OP_FORGLOOP: {
						iteration = (decode_A(info->proto.code[on]) + 4u);
						break;
					}
					case OP_FORNLOOP: {
						iteration = (decode_A(info->proto.code[on]) + 2u);
						break;
					}
					case OP_FORGLOOP_INEXT: {
						iteration = (decode_A(info->proto.code[on]) + 4u);
						break;
					}
					case OP_FORGLOOP_NEXT: {
						iteration = (decode_A(info->proto.code[on]) + 4u);
						break;
					}

					default: {
						break;
					}

				}
				
				/* Add vars from those loops. */
				for (auto i = target; i < (iteration + 1u); ++i) {
					if (std::find(dests.begin(), dests.end(), i) == dests.end())
						dests.emplace_back(i);
				}

			}

		
		/* Fix OP_SETTABLEKS as using this in disasm can cause behavioral issues in some functions. */
		if (i->opcode == OP_SETTABLEKS) {
			
			i->source_reg = decode_B(info->proto.code[pos]);
			i->has_source = true;

			/* Place dest. */
			if (std::find(dests.begin(), dests.end(), i->dest_reg) == dests.end() &&
				std::find(source_no_dest.begin(), source_no_dest.end(), i->dest_reg) == source_no_dest.end())
				source_no_dest.emplace_back(i->dest_reg);
		}
			

		switch (i->basic) {
				
				/* Theese get set ahead of time. */
				case basic_info::for_: {
					break;
				}

				case basic_info::branch: {
				
					/* Check for 1st compare. */
					if (i->has_cmp_source &&
						std::find(dests.begin(), dests.end(), i->cmp_source_reg) == dests.end() &&
						std::find(source_no_dest.begin(), source_no_dest.end(), i->cmp_source_reg) == source_no_dest.end())
							source_no_dest.emplace_back(i->cmp_source_reg);

					/* Check for 2nd compare. */
					if (i->has_cmp_value &&
						std::find(dests.begin(), dests.end(), i->cmp_value_reg) == dests.end() &&
						std::find(source_no_dest.begin(), source_no_dest.end(), i->cmp_value_reg) == source_no_dest.end())
							source_no_dest.emplace_back(i->cmp_value_reg);

					break;
				}

				default: {

					if (i->opcode == OP_RETURN) {

						/* Add every return var. */
						const auto reg = decode_A(info->proto.code[pos]);
					    auto retn = (decode_B(std::int32_t(info->proto.code[pos])) - 1);
						
						if (retn) {

							if (retn == -1)
								retn = prev_dest;

							for (auto i = 0u; i < unsigned (retn); ++i) {

								const auto a = (i + reg);

								if (std::find(dests.begin(), dests.end(), a) == dests.end() &&
									std::find(source_no_dest.begin(), source_no_dest.end(), a) == source_no_dest.end())
									source_no_dest.emplace_back(a);
							}

						}

					}
					else if (i->opcode == OP_GETVARARGS) {

						auto amt = (decode_B(std::int32_t(info->proto.code[pos])) - 1);

						if (amt == -1)
							amt = prev_dest;

						for (auto a = i->dest_reg; a < (i->dest_reg + amt); ++a)
							if (std::find(dests.begin(), dests.end(), a) == dests.end())
								dests.emplace_back(a);

					}
					else {

						/* Make sure its not indexing an upvalue, nop, or for, branch. */
						if (i->opcode != OP_NOP && (i->opcode != OP_CAPTURE || i->tt != 2u)) {

							/* Log dest. */
							if (i->opcode != OP_CALL && i->opcode != OP_SETTABLEKS && i->opcode != OP_CAPTURE && i->basic != basic_info::fast_call && std::find(dests.begin(), dests.end(), i->dest_reg) == dests.end())
								dests.emplace_back(i->dest_reg);


							/* This for namecall. */
							if (i->opcode == OP_NAMECALL) {

								if (std::find(dests.begin(), dests.end(), i->dest_reg) == dests.end())
									dests.emplace_back(i->dest_reg);

								if (std::find(dests.begin(), dests.end(), i->dest_reg + 1u) == dests.end())
									dests.emplace_back(i->dest_reg + 1u);

							}



							/* Log call arguments. */
							if (i->opcode == OP_CALL) {

								auto args = (decode_B(std::int32_t(info->proto.code[pos])) - 1);
								const auto dest = i->dest_reg;

								if (args == -1)
									args = (prev_dest - dest);

								/* Iterate through args and see if if arg is not getting used in dest. */
								for (auto i = 0; i < args; ++i) {

									const auto arg = (i + dest + 1u);

									if (std::find(dests.begin(), dests.end(), arg) == dests.end() &&
										std::find(source_no_dest.begin(), source_no_dest.end(), arg) == source_no_dest.end())
										source_no_dest.emplace_back(arg);


								}

								/* Add placement for call. */
								if (std::find(dests.begin(), dests.end(), dest) == dests.end() &&
									std::find(source_no_dest.begin(), source_no_dest.end(), dest) == source_no_dest.end())
									source_no_dest.emplace_back(dest);


							}



							/* Used as source yet not dest. */
							if (i->has_source &&
								std::find(dests.begin(), dests.end(), i->source_reg) == dests.end() &&
								std::find(source_no_dest.begin(), source_no_dest.end(), i->source_reg) == source_no_dest.end()) {
								source_no_dest.emplace_back(i->source_reg);
							}


							/* Used as value yet not dest. */
							if (i->has_value &&
								std::find(dests.begin(), dests.end(), i->value_reg) == dests.end() &&
								std::find(source_no_dest.begin(), source_no_dest.end(), i->value_reg) == source_no_dest.end()) {
								source_no_dest.emplace_back(i->value_reg);
							}



							/* Is for multret. */
							if (i->opcode == OP_CAPTURE) /* Capture has int for dest. */
								prev_dest = 0u;
							else if ((i->opcode != OP_NAMECALL || dissassemble(info, (pos + i->size))->opcode != OP_CALL) && i->basic != basic_info::fast_call) /* Ignore namecall followed by call. */
								prev_dest = i->dest_reg;

						}
					}

					break;
				}
		}

		pos += i->size;
	}

	/* If there is no dest that hasnt been written too return else get maximum in the vector + 1 for format and return. */
	if (!source_no_dest.size())
		return 0u;

	return (*std::max_element (source_no_dest.begin (), source_no_dest.end ()) + 1u);
}


/* Sets child proto types and name. */
void set_proto_type_name(std::shared_ptr<decompile_info>& info, std::unordered_map<std::uint32_t, std::shared_ptr<call_routine_st>>& call_routine) {

	const auto disasm = dissassemble_whole(info);

	auto pos = 0u;
	auto target = 0u;
	auto on_table = 0u; /* End of current table. */

	for (const auto& curr : disasm) {




		/* See if where inside a table. */
		if (!on_table && (curr->opcode == OP_NEWTABLE || curr->opcode == OP_DUPTABLE))
			on_table = table::end(info, pos, info->proto.size_code);
		
		/* End of table. */
		if (on_table == pos)
			on_table = 0u;




		/* Routine has varargs*/
		if (curr->opcode == OP_GETVARARGS)
			info->proto.closure.varargs = true;


		/* Set target end for closure. */
		if (call_routine.find(pos) != call_routine.end())
			target = call_routine[pos]->end;

		/* Determin type. */
		if (curr->opcode == OP_NEWCLOSURE || curr->opcode == OP_DUPCLOSURE) {

			const auto& curr_data = info->proto.code[pos];
			const auto p = decode_D(curr_data); /* Should be proto id for newclosure. */
			auto ptr = ((curr->opcode == OP_DUPCLOSURE) ? nullptr : info->proto.p[p]); /* Dupclosure gives us id through K. */


			/* Remove func_ identifier and get proto from it. */
			if (ptr == nullptr) {
				auto str = std::string(info->proto.k[p]);
				str.erase(str.find("func_"), (sizeof("func_") - 1u));
				const auto id = unsigned(std::stoi(str.c_str()) - 0);
				ptr = ((info->proto.p.size() <= id) ? info->proto.p.back () : info->proto.p[id]);
			}


			/* Make sure it hasnt been set yet. */
			if (ptr->proto.closure.tt == closure_type::none) {

				/* newclosure, global or local */

				/* newclosure */
				if (target || on_table) {
					ptr->proto.closure.tt = closure_type::newclosure;
					ptr->proto.closure.name.clear();
				}
				else {

					/* Skip capture */
					auto o = 1u;
					while ((*(&curr + o))->opcode == OP_CAPTURE)  
						++o;

					/* Global */
					if ((*(&curr + o))->opcode == OP_SETGLOBAL && (*(&curr + o))->dest_reg == curr->dest_reg) {

						ptr->proto.closure.tt = closure_type::global;

						/* Get name and format then set. */
						auto& str = info->proto.k[info->proto.code[pos + o + 1u]];
						str.erase(std::remove(str.begin(), str.end(), '\"'), str.end());
						ptr->proto.closure.name = str;
					}
					else {
						/* local */

						ptr->proto.closure.tt = closure_type::local;

						/* Get name and format then set. */
						auto& str = info->proto.k[p];
						str.erase(std::remove(str.begin(), str.end(), '\"'), str.end());
						ptr->proto.closure.name = str;
					}
				}

				

			}

		}

		
		if (pos == target)
			target = 0u;

		pos += curr->size;
	}

	return;
}


/* Writes proto args, names, type (includes children proto). */
void write_proto_info(std::shared_ptr<decompile_info>& info, std::unordered_map<std::uint32_t, std::shared_ptr<call_routine_st>>& call_routine) {

	auto unique_indentifier = 1u;

	std::vector <std::shared_ptr<decompile_info>> pending_analysis;
	set_proto_type_name(info, call_routine); /* Set current types and names. */

	std::unordered_map<std::uint32_t, std::shared_ptr<call_routine_st>> dup_call_routine; 

	/* Kick off analysis. */
	for (auto i = 0u; i < info->proto.size_proto; ++i) {

		/* Get current */
		auto& ptr = info->proto.p[i];
		
		/* Current calls. */
		dup_call_routine.clear();
		write_call_routines(ptr, dup_call_routine);
	
		/* Write args. */
		ptr->proto.closure.arg_count = get_args(ptr);
		ptr->proto.closure.args_set = true;
		ptr->proto.unique_indentifier = unique_indentifier++;

		/* Append child proto. */
		for (const auto& child : ptr->proto.p) 		
			pending_analysis.emplace_back(child);

		/* Set current type and names. */
		set_proto_type_name(ptr, call_routine);

	}


	while (pending_analysis.size()) {

		std::vector <std::shared_ptr<decompile_info>> temp_analysis;

		for (auto& ptr : pending_analysis) {

			/* Write args. */
			ptr->proto.closure.arg_count = get_args(ptr);
			ptr->proto.closure.args_set = true;
			ptr->proto.unique_indentifier = unique_indentifier++;

			/* Current calls. */
			dup_call_routine.clear();
			write_call_routines(ptr, dup_call_routine);
			
			/* Append child proto. */
			for (const auto& child : ptr->proto.p) 
				if (!child->proto.closure.args_set) /* Safe check. */
					temp_analysis.emplace_back(child);

			/* Set current type and names. */
			set_proto_type_name(ptr, dup_call_routine);

		}

		/* Clear for next. */
		pending_analysis.clear();
		
		/* End if no more closures to analyze. */
		if (!temp_analysis.size())
			break;

		/* Reset cache to children. */
		pending_analysis = temp_analysis;

		temp_analysis.clear();
	} 

	return;
}


/* Parse closures.  */
void write_closures(std::shared_ptr<decompile_info>& info, std::unordered_map<std::uint32_t, std::shared_ptr<call_routine_st>>& call_routine, const std::shared_ptr<decompile_config>& config) {


	/* Make sure protos exists and args havent been set yet.  */
	if (info->proto.p.size() && !info->proto.p.front()->proto.closure.args_set) {

		/* Write basic proto info. */
		write_proto_info(info, call_routine);

		/* Pending analysis of proto children. */
		std::vector <std::vector <std::shared_ptr<decompile_info>>> pending_analysis;


		auto id = 0u; /* Parent id. */


		/* Get child proto of first to kick off getting child of child and so on of proto. */
		std::unordered_map<std::uint32_t, std::vector <std::vector <std::shared_ptr<decompile_info>>>> cached; /* { id, { proto_vector, proto_vector_1 } }*/
		pending_analysis.emplace_back(info->proto.p);



		/* Insert first cache. */
		cached.insert(std::make_pair(id++, pending_analysis));


		/* Set proto children. */
		while (pending_analysis.size()) {

			std::vector <std::vector <std::shared_ptr<decompile_info>>> temp_analysis;

			/* Iterate through proto children and place there children in a temporary vector. */
			for (const auto& i : pending_analysis)
				for (const auto& o : i)
					temp_analysis.emplace_back(o->proto.p);


			/* Clear and set. */
			pending_analysis.clear();
			pending_analysis = temp_analysis;
			if (pending_analysis.size())
				cached.insert(std::make_pair(id++, pending_analysis));
			temp_analysis.clear();

		}


		auto retn = std::make_shared<ast_data>();
		std::unordered_map<std::uint32_t, std::uint32_t> skip_info; /* Positions to jump too when we hit positions. *Used to skip types in elseif. */
		std::unordered_map<std::uint32_t, std::uint32_t> skip_locvar; /* Skips locvar info. */

		/* Write all locvar info for each proto. (We do this so we can set each upvalue name when we decompile. */
		for (auto& i : cached)
			for (auto& o : i.second)
				for (auto& info : o) 
					if (!info->proto.debug_info && !info->proto.closure.pre_anlyzed){ /* Make sure there are no debug info and its first name. */

						auto& omorfia = retn->branches;

						table::write(info, retn->forming_tables);
						write_concat_routines(info, retn->concat_routines);
						write_call_routines(info, retn->call_routine);
						write_loops(info, retn->dedicated_loops, omorfia, retn->while_ends);
						write_logical_operators(info, retn->call_routine, retn->logical_operations);
						write_closures(info, retn->call_routine, config);
						write_loop_data(info, omorfia, retn->dedicated_loops, retn->loop_map);

						determine_logical_operation_cmp(info, retn->logical_operations, retn->loop_map, retn->branch_comparisons);


						const auto diasm = dissassemble_whole(info);
						auto ip = 0u;
						auto ignore_till = 0u; /* Skips all instructions until ip = this var. */
						for (auto iter = 0u; iter < diasm.size (); ++iter) {

							const auto& i = diasm[iter];

							/* Ignore till ip == ignore_till. */
							/* We skip logical operations, loop compares, elseif from parents. */
							if (!ignore_till || ignore_till < ip) {

								/* Skip. */
								if (retn->logical_operations.find(ip) != retn->logical_operations.end())
									ignore_till = retn->logical_operations[ip]->end;

								else if (retn->loop_map.find(ip) != retn->loop_map.end())
									ignore_till = (retn->loop_map[ip].back().first + dissassemble(info, retn->loop_map[ip].back().first)->size);

								else if (skip_info.find(ip) != skip_info.end())
									ignore_till = skip_info[ip];

								else if (retn->concat_routines.find(ip) != retn->concat_routines.end())
									ignore_till = retn->concat_routines[ip];

								else {

									/* Iterate through if branch till we see a jump if its a jump out of rotuine dont proceed will get handled during parsing. */
									if (i->is_comparative && i->basic == basic_info::branch) {

										std::uint32_t end = 0u;
										do_branch(info, skip_locvar, retn->extra_branches, retn->dedicated_loops, retn->call_routine, retn->branch_comparisons, ip, end, skip_info, retn->branch_pos_scope, retn->else_routine_begin, omorfia);
										if (end) /* Make sure valid end before using it. */
											retn->ends.emplace_back(end);

									}

								}

							}

							ip += i->size;
						}

						/* Sort for binary search. */
						std::sort(retn->branch_pos_scope.begin(), retn->branch_pos_scope.end());

						register_localize(info, retn->call_routine, omorfia, retn->dedicated_loops, retn->logical_operations, retn->loop_map, skip_locvar, retn->concat_routines, retn->ends);
				
						info->proto.closure.pre_anlyzed = true; /* First time so mark it. */
						retn = std::make_shared<ast_data>();
						skip_info.clear();
						skip_locvar.clear();

					}


		/* Set parent vars. */
		auto& omorfia = retn->branches;

		table::write(info, retn->forming_tables);
		write_concat_routines(info, retn->concat_routines);
		write_call_routines(info, retn->call_routine);
		write_loops(info, retn->dedicated_loops, omorfia, retn->while_ends);
		write_logical_operators(info, retn->call_routine, retn->logical_operations);
		write_closures(info, retn->call_routine, config);
		write_loop_data(info, omorfia, retn->dedicated_loops, retn->loop_map);

		determine_logical_operation_cmp(info, retn->logical_operations, retn->loop_map, retn->branch_comparisons);


		const auto diasm = dissassemble_whole(info);
		auto ip = 0u;
		auto ignore_till = 0u; /* Skips all instructions until ip = this var. */
		for (auto iter = 0u; iter < diasm.size(); ++iter) {

			const auto& i = diasm[iter];

			/* Ignore till ip == ignore_till. */
			/* We skip logical operations, loop compares, elseif from parents. */
			if (!ignore_till || ignore_till < ip) {

				/* Skip. */
				if (retn->logical_operations.find(ip) != retn->logical_operations.end())
					ignore_till = retn->logical_operations[ip]->end;

				else if (retn->loop_map.find(ip) != retn->loop_map.end())
					ignore_till = (retn->loop_map[ip].back ().first + dissassemble(info, retn->loop_map[ip].back().first)->size);

				else if (skip_info.find(ip) != skip_info.end())
					ignore_till = skip_info[ip];

				else if (retn->concat_routines.find(ip) != retn->concat_routines.end())
					ignore_till = retn->concat_routines[ip];

				else {

					/* Iterate through if branch till we see a jump if its a jump out of rotuine dont proceed will get handled during parsing. */
					if (i->is_comparative && i->basic == basic_info::branch) {
						std::uint32_t end = 0u;
						do_branch(info, skip_locvar, retn->extra_branches, retn->dedicated_loops, retn->call_routine, retn->branch_comparisons, ip, end, skip_info, retn->branch_pos_scope, retn->else_routine_begin, omorfia);
						if (end) /* Make sure valid end before using it. */
							retn->ends.emplace_back(end);
					}

				}

			}

			ip += i->size;
		}

		/* Sort for binary search. */
		std::sort(retn->branch_pos_scope.begin(), retn->branch_pos_scope.end());

		register_localize(info, retn->call_routine, omorfia, retn->dedicated_loops, retn->logical_operations, retn->loop_map, skip_locvar, retn->concat_routines, retn->ends);


		/* First to kick off upvalue anlysis we need to set first order upvalues based on single parent proto. */
		auto pos = 0u;
		auto upvalue_suffix = 0u;
		for (const auto& i : dissassemble_whole(info)) {


			/* All upvalues set or routine after newclosure. */
			if (i->opcode == OP_NEWCLOSURE || i->opcode == OP_DUPCLOSURE) {


				/* Proto ID with temp pos. */
				auto p = decode_D(info->proto.code[pos]);


				/* Dupclosure. */
				if (i->opcode == OP_DUPCLOSURE) {
					auto str = std::string(info->proto.k[decode_D(info->proto.code[pos])]);
					str.erase(str.find("func_"), (sizeof("func_") - 1u));
					const auto id = unsigned(std::stoi(str.c_str()) - 0);
					p = ((info->proto.p.size() <= id) ? (info->proto.p.size() - 1u) : id);
				}


				/* Child proto with temp position. */
				const auto& child = info->proto.p[p];
				auto temp_pos = (pos + 1u);


				/* Go throught each capture. */
				auto o = 1u;
				while ((*(&i + o))->opcode == OP_CAPTURE) {


					/* Current data and source*/
					const auto curr_data = info->proto.code[temp_pos];
					const auto source = decode_B(curr_data);


					/* Upvalue. */
					if (decode_A(curr_data) == 2u) 
						child->proto.upvalues.emplace_back(info->proto.upvalues[source]); /* Add upvalue to child proto. */
					else {

					    auto upvalue_name = config->upvalue_prefix + std::to_string(info->proto.unique_indentifier) + '_' + std::to_string(upvalue_suffix);

						
						/* Find locvar by register, make sure it hasnt been set before. */
						for (const auto& l : info->proto.locvars)
							if (l->reg == source && !l->is_upvalue) {
								l->name = upvalue_name;
								l->is_upvalue = true;
							}
							else if (l->reg == source) /* Override given upvalue name with current one. */
								upvalue_name = l->name;


						++upvalue_suffix;
						child->proto.upvalues.emplace_back(upvalue_name); /* Add upvalue to child proto. */

					}

					++o;
					++temp_pos; /* Capture size. */

				}

			}

			pos += i->size;
		}


		/* Now we have locvar info we need to get each parent proto and mark upvalue info for relative child proto. */
		for (auto iter = 0u; iter < cached.size(); ++iter) {

			auto& i = cached[iter];

			for (auto& o : i)
				for (auto& info : o) {


					auto pos = 0u;
					auto upvalue_suffix = 0u;
					for (const auto& i : dissassemble_whole(info)) {


						/* All upvalues set or routine after newclosure. */
						if (i->opcode == OP_NEWCLOSURE || i->opcode == OP_DUPCLOSURE) {

							/* Proto ID with temp pos. */
							auto p = decode_D(info->proto.code[pos]);

							/* Dupclosure. */
							if (i->opcode == OP_DUPCLOSURE) {
								auto str = std::string(info->proto.k[decode_D(info->proto.code[pos])]);
								str.erase(str.find("func_"), (sizeof("func_") - 1u));
								const auto id = unsigned(std::stoi(str.c_str()) - 0);
								p = ((info->proto.p.size() <= id) ? (info->proto.p.size() - 1u) : id);
							}


							/* Child proto with temp position. */
							const auto& child = info->proto.p[p];
							auto temp_pos = (pos + 1u);


							/* Go throught each capture. */
							auto o = 1u;
							while ((*(&i + o))->opcode == OP_CAPTURE) {


								/* Current data and source*/
								const auto curr_data = info->proto.code[temp_pos];
								const auto source = decode_B(curr_data);


								/* Upvalue. */
								if (decode_A(curr_data) == 2u)
									child->proto.upvalues.emplace_back(info->proto.upvalues[source]); /* Add upvalue to child proto. */
								else {

								    auto upvalue_name = config->upvalue_prefix + std::to_string(info->proto.unique_indentifier) + '_' + std::to_string(upvalue_suffix);


									/* Find locvar by register, make sure it hasnt been set before. */
									for (const auto& l : info->proto.locvars)
										if (l->reg == source && !l->is_upvalue) {
											l->name = upvalue_name;
											l->is_upvalue = true;
										}
										else if (l->reg == source) /* Override given upvalue name with current one. */
											upvalue_name = l->name;


									++upvalue_suffix;
									child->proto.upvalues.emplace_back(upvalue_name); /* Add upvalue to child proto. */
								}

								++o;
								++temp_pos; /* Capture size. */

							}

						}

						pos += i->size;
					}

				}

		}

		--id;

		/* Go through and start at the back of the unordered map and decompile data for it then append to its closure. */
		while (cached.size()) {

			/* Get biggest value in unordered map.*/
		    auto& max = cached[id];

			/* Decompile each value. */
			for (auto& i : max)
				for (auto& o : i) 
					decompiler_parse(config, o, o->proto.closure.data);

			/* Remove it. */
			cached.erase(id--);

		}

	}


	
	return;
}


#pragma endregion




std::shared_ptr<ast_data> create_ast(std::shared_ptr<decompile_info>& info, const std::shared_ptr<decompile_config>& config) {

	auto retn = std::make_shared<ast_data>();

	auto& omorfia = retn->branches;


	table::write(info, retn->forming_tables);  
	write_concat_routines(info, retn->concat_routines);
    write_call_routines(info, retn->call_routine);
	write_loops(info, retn->dedicated_loops, omorfia, retn->while_ends);
	write_logical_operators(info, retn->call_routine, retn->logical_operations);
	write_closures(info, retn->call_routine, config);
	write_loop_data(info, omorfia, retn->dedicated_loops, retn->loop_map);
	

	determine_logical_operation_cmp(info, retn->logical_operations, retn->loop_map, retn->branch_comparisons);


	/* Write branches. */ 

	auto ip = 0u;

	const auto dissassembly = dissassemble_whole(info);
	const auto dissassembly_size = dissassembly.size();
	std::unordered_map<std::uint32_t, std::uint32_t> skip_info; /* Positions to jump too when we hit positions. *Used to skip types in elseif. */
	std::unordered_map<std::uint32_t, std::uint32_t> skip_locvar; /* Skips locvar info. */

	auto ignore_till = 0u; /* Skips all instructions until ip = this var. */


	for (auto iter = 0u; iter < dissassembly_size; ++iter) {

		const auto& i = dissassembly[iter];

		/* Ignore till ip == ignore_till. */
		/* We skip logical operations, loop compares, elseif from parents. */
		if (!ignore_till || ignore_till < ip) { 

			/* Skip. */
			if (retn->logical_operations.find(ip) != retn->logical_operations.end()) 
				ignore_till = retn->logical_operations[ip]->end;

			else if (retn->loop_map.find(ip) != retn->loop_map.end()) 
				ignore_till = retn->loop_map[ip].back().first;		

			else if (skip_info.find(ip) != skip_info.end()) 
				ignore_till = skip_info[ip];
			
			else if (retn->concat_routines.find(ip) != retn->concat_routines.end()) 
				ignore_till = retn->concat_routines[ip];		

			else {
			
				/* Iterate through if branch till we see a jump if its a jump out of rotuine dont proceed will get handled during parsing. */
				if (i->is_comparative && i->basic == basic_info::branch) {	
					
					std::uint32_t end = 0u;
					do_branch(info, skip_locvar, retn->extra_branches, retn->dedicated_loops, retn->call_routine, retn->branch_comparisons, ip, end, skip_info, retn->branch_pos_scope, retn->else_routine_begin, omorfia);			
					if (end) /* Make sure valid end before using it. */
						retn->ends.emplace_back(end);
					
				}

			}

		}

		ip += i->size;
	}


	/* Sort for binary search. */
	std::sort(retn->branch_pos_scope.begin(), retn->branch_pos_scope.end());

	/* Has no debug info so we need to make psuedo info. */
	if (!info->proto.closure.pre_anlyzed) 
		register_localize(info, retn->call_routine, omorfia, retn->dedicated_loops, retn->logical_operations, retn->loop_map, skip_locvar, retn->concat_routines, retn->ends);



	/* Fix locvar info for branches*/
	auto tpos = 0u;
	auto on = 0u;
	for (const auto& i : dissassembly) {

		/* End of range. */
		if (on && tpos >= on)
			on = 0u;

		/* Set range. */
		if (!on && skip_locvar.find(tpos) != skip_locvar.end())
			on = skip_locvar[tpos];

		/* Check for locvars in range. */
		if (on) 
			for (auto o = 0u; o < info->proto.locvars.size(); ++o)
				if (info->proto.locvars[o]->end >= tpos && info->proto.locvars[o]->end <= on)
					info->proto.locvars.erase(info->proto.locvars.begin() + o);
		

		tpos += i->size;
	}
	

	std::sort(retn->ends.begin(), retn->ends.end());

	return retn;
}