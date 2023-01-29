#include <iostream>
#include <unordered_map>
#include "decode.hpp"
#include "decompile_parse.hpp"
#include "dissassemble.hpp"
#include "opcodes.hpp"
#include "visualizer.hpp"

struct branch
{
	std::string name = "";
	std::uint32_t lenght = 0u;
};

static std::shared_ptr<branch> branch_name(std::shared_ptr<decompile_info>& info, const std::shared_ptr<dissassembler_data>& disasm, const std::uint32_t offset) {
	
	auto retn = std::make_shared<branch>();

	switch (disasm->opcode)
	{
			case OP_JUMPIFEQK: {
				const auto len = decode_D(info->proto.code[offset]);
				retn->name = ("jumpifeqk_" + std::to_string(len) + ':');
				retn->lenght = len;
				break;
			}
			case OP_JUMPIFNOTEQK: {
				const auto len = decode_D(info->proto.code[offset]);
				retn->name = ("jumpifnoteqk_" + std::to_string(len) + ':');
				retn->lenght = len;
				break;
			}
			case OP_JUMPIF: {
				const auto len = decode_D(info->proto.code[offset]);
				retn->name = ("jumpif_" + std::to_string(len) + ':');
				retn->lenght = len;
				break;
			}
			case OP_JUMPIFNOT: {
				const auto len = decode_D(info->proto.code[offset]);
				retn->name = ("jumpifnot_" + std::to_string(len) + ':');
				retn->lenght = len;
				break;
			}
			case OP_JUMPIFEQ: {
				const auto len = decode_D(info->proto.code[offset]);
				retn->name = ("jumpifeq_" + std::to_string(len) + ':');
				retn->lenght = len;
				break;
			}
			case OP_JUMPIFLE: {
				const auto len = decode_D(info->proto.code[offset]);
				retn->name = ("jumpifle" + std::to_string(len) + ':');
				retn->lenght = len;
				break;
			}
			case OP_JUMPIFLT: {
				const auto len = decode_D(info->proto.code[offset]);
				retn->name = ("jumpiflt_" + std::to_string(len) + ':');
				retn->lenght = len;
				break;
			}
			case OP_JUMPIFNOTEQ: {
				const auto len = decode_D(info->proto.code[offset]);
				retn->name = ("jumpifnoteq_" + std::to_string(len) + ':');
				retn->lenght = len;
				break;
			}
			case OP_JUMPIFNOTLE: {
				const auto len = decode_D(info->proto.code[offset]);
				retn->name = ("jumpifnotle_" + std::to_string(len) + ':');
				retn->lenght = len;
				break;
			}
			case OP_JUMPIFNOTLT: {
				const auto len = decode_D(info->proto.code[offset]);
				retn->name = ("jumpifnotlt_" + std::to_string(len) + ':');
				retn->lenght = len;
				break;
			}
			case OP_JUMP: {
				const auto len = decode_D(info->proto.code[offset]);
				retn->name = ("jump_" + std::to_string(len) + ':');
				retn->lenght = len;
				break;
			}
	}

	return retn;
}

void dissassemble_textualize (std::shared_ptr<decompile_info>& decom_info, std::string& write) {
	
	write.clear();

	auto pos = 0u;
	const auto bytecode = decom_info->proto.code;

	std::unordered_map <std::uint32_t, std::string> mapped_branches;

	/* Iterate through bytecode with offset. */
	for (auto offset = 0u; offset < bytecode.size();) 
	{
	    auto disasm = dissassemble(decom_info, offset, true);

		std::string str_op = "";
	
		/* Split instruction between operands while getting the position of the split. */
		std::int32_t o = 0u;
		while (disasm->data[o] != ' ') 
		{
			str_op += disasm->data[o];
			++o;
		}

		/* Format with position of spacing between instruction and operands. */
		const auto format_lenght_r = (format_lenght - str_op.length());
		std::string s_str = "";
		s_str.resize(format_lenght_r, ' ');
		disasm->data.insert(o, s_str);

		/* Log branch jumps. */
		if (disasm->basic == basic_info::branch)
		{
			const auto branch = branch_name(decom_info, disasm, offset);
			const auto key = (offset + branch->lenght + 1);

			if (mapped_branches.find(key) == mapped_branches.end ()) 
				mapped_branches.insert(std::make_pair(key, (branch->name + '\n')));

			else if (mapped_branches.find(key) != mapped_branches.end() && mapped_branches[key] != branch->name) 
				mapped_branches[key] = (mapped_branches[key] + branch->name + '\n');
		}

		/* If where at a branch write jump. */
		if (mapped_branches.find(offset) != mapped_branches.end())
			write += mapped_branches[offset];

		/* Write format and data. */
		write += std::to_string(offset) + "  " + disasm->data + '\n';
		offset += disasm->size;
	}
}