#include <algorithm>
#include <iostream>
#include "decode.hpp"
#include "dissassemble.hpp"
#include "opcodes.hpp"

#pragma region arith 


/* Gets arith instruction. */
const char* const do_arith_instruction(const std::uint16_t op)
{
	switch (op) {

			case OP_ADDK: {
				return "addk ";
			}
			case OP_ADD: {
				return "add ";
			}

			case OP_SUBK: {
				return "subk ";
			}
			case OP_SUB: {
				return "sub ";
			}

			case OP_DIVK: {
				return "divk ";
			}
			case OP_DIV: {
				return "div ";
			}

			case OP_MULK: {
				return "mulk ";
			}
			case OP_MUL: {
				return "mul ";
			}

			case OP_POWK: {
				return "powk ";
			}
			case OP_POW: {
				return "pow ";
			}

			case OP_MODK: {
				return "modk ";
			}
			case OP_MOD: {
				return "mod ";
			}

			case OP_ANDK: {
				return "andk ";
			}
			case OP_AND: {
				return "and ";
			}

			case OP_ORK: {
				return "ork ";
			}
			case OP_OR: {
				return "or ";
			}
	}

	return "add ";
}



/* Checks if arith is constant. */
bool is_arith_constant_dis(const std::uint16_t op) 
{
	switch (op) {
				case OP_ADDK:
				case OP_SUBK:
				case OP_DIVK:
				case OP_MULK:
				case OP_POWK:
				case OP_MODK:
				case OP_ANDK:
				case OP_ORK: {
					return true;
				}

				default: {
					return false;
				}
	}

	return false;
}


const char* const jump_2_d(const std::uint16_t op)
{
	switch (op)
	{
		/* Normal. */
	case OP_JUMPIFEQ: {
		return "jumpifeq";
	}

	case OP_JUMPIFLE: {
		return "jumpifle";
	}

	case OP_JUMPIFLT: {
		return "jumpiflt";
	}

	case OP_JUMPIFNOTEQ: {
		return "jumpifnoteq";
	}

	case OP_JUMPIFNOTLE: {
		return "jumpifnotle";
	}

	case OP_JUMPIFNOTLT: {
		return "jumpifnotlt";
	}

					   /* Constant. */
	case OP_JUMPIFEQK: {
		return "jumpifeqk";
	}

	case OP_JUMPIFNOTEQK: {
		return "jumpifnoteqk";
	}
	}

	return "jumpifnoteqk";
}

#pragma endregion

std::shared_ptr<dissassembler_data> dissassemble(std::shared_ptr<decompile_info>& info, const std::uint32_t offset, const bool detail_info)
{
	const auto bytecode = info->proto.code;
	std::uint16_t len = 0u;

	auto retn = std::make_shared <dissassembler_data>();

	const auto curr = bytecode[offset];
	const auto opcode = decode_opcode(curr);

	switch (opcode)
	{
	/* Comparatives. */
	case OP_JUMPIFEQK:
	case OP_JUMPIFNOTEQK: {

		const auto jump = jump_2_d(opcode);

		if (detail_info)
			retn->data = (std::string(jump) + " r" + std::to_string(decode_A(curr)) + " " + info->proto.k[bytecode[offset + 1]] + ' ' + jump + '_' + std::to_string(decode_D(curr)));

		else
			retn->data = (std::string(jump) + " r" + std::to_string(decode_A(curr)) + " k" + std::to_string(bytecode[offset + 1]) + " " + std::to_string(decode_D(curr)));

		retn->size = 2u;
		retn->opcode = opcode;
		retn->is_comparative = true;
		retn->basic = basic_info::branch;
		retn->cmp_source_reg = decode_A(curr);

		break;
	}
	case OP_JUMPIFEQ:
	case OP_JUMPIFLE:
	case OP_JUMPIFLT:
	case OP_JUMPIFNOTEQ:
	case OP_JUMPIFNOTLE:
	case OP_JUMPIFNOTLT: {

		const auto jump = jump_2_d(opcode);

		if (detail_info)
			retn->data = (std::string(jump) + " r" + std::to_string(decode_A(curr)) + " r" + std::to_string(bytecode[offset + 1]) + ' ' + jump + '_' + std::to_string(decode_D(curr)));

		else
			retn->data = (std::string(jump) + " r" + std::to_string(decode_A(curr)) + " r" + std::to_string(bytecode[offset + 1]) + ' ' + std::to_string(decode_D(curr)));


		retn->size = 2u;
		retn->opcode = opcode;
		retn->is_comparative = true;
		retn->basic = basic_info::branch;
		retn->cmp_source_reg = decode_A(curr);
		retn->cmp_value_reg = bytecode[offset + 1];
		retn->has_cmp_value = true;
		break;
	}
	case OP_RETURN: {
		retn->data = ("return r" + std::to_string(decode_A(curr)) + " " + std::to_string(decode_B(std::int32_t(curr)) - 1));
		retn->size = 1u;
		retn->opcode = OP_RETURN;
		retn->basic = basic_info::return_;
		retn->byte_code = info->pcode_bytes[offset];
		break;
	}
	/* Jumps*/
	case OP_JUMP: {

		if (detail_info)
			retn->data = ("jump jump_" + std::to_string(decode_D(curr)));
		else
			retn->data = ("jump " + std::to_string(decode_D(curr)));


		retn->byte_code = info->pcode_bytes[offset];
		retn->basic = basic_info::branch;
		retn->size = 1u;
		retn->opcode = OP_JUMP;
		retn->has_cmp_source = false;
		break;
	}
	case OP_JUMPX: {

		if (detail_info)
			retn->data = ("jumpx jumpx_" + std::to_string(decode_E(curr)));
		else
			retn->data = ("jumpx " + std::to_string(decode_E(curr)));


		retn->byte_code = info->pcode_bytes[offset];
		retn->basic = basic_info::branch;
		retn->size = 1u;
		retn->opcode = OP_JUMPX;
		retn->has_cmp_source = false;
		break;
	}
	case OP_JUMPBACK: {

		if (detail_info)
		{

			retn->data = ("jumpback jump_" + std::to_string(decode_D(curr)));
			retn->size = 1u;
			retn->opcode = OP_JUMPBACK;

		}
		else
		{

			retn->data = ("jumpback " + std::to_string(decode_D(curr)));
			retn->size = 1u;
			retn->opcode = OP_JUMPBACK;

		}

		retn->byte_code = info->pcode_bytes[offset];
		retn->basic = basic_info::branch;
		retn->has_cmp_source = false;
		break;
	}
	case OP_JUMPIFNOT: {

		if (detail_info)
			retn->data = ("jumpifnot r" + std::to_string(decode_A(curr)) + " jumpifnot_" + std::to_string(decode_D(curr)));

		else
			retn->data = ("jumpifnot r" + std::to_string(decode_A(curr)) + " " + std::to_string(decode_D(curr)));

		retn->size = 1u;
		retn->opcode = OP_JUMPIFNOT;
		retn->byte_code = info->pcode_bytes[offset];
		retn->basic = basic_info::branch;
		retn->cmp_source_reg = decode_A(curr);
		retn->is_comparative = true;
		break;
	}
	case OP_JUMPIF: {

		if (detail_info)
			retn->data = ("jumpif r" + std::to_string(decode_A(curr)) + " jumpif_" + std::to_string(decode_D(curr)));

		else
			retn->data = ("jumpif r" + std::to_string(decode_A(curr)) + " " + std::to_string(decode_D(curr)));

		retn->size = 1u;
		retn->opcode = OP_JUMPIF;
		retn->byte_code = info->pcode_bytes[offset];
		retn->basic = basic_info::branch;
		retn->cmp_source_reg = decode_A(curr);
		retn->is_comparative = true;
		break;
	}
	case OP_CALL: {
		retn->data = ("call r" + std::to_string(decode_A(curr)) + " " + std::to_string(decode_B(std::int32_t(curr)) - 1) + " " + std::to_string(decode_C(std::int32_t(curr)) - 1));
		retn->size = 1u;
		retn->opcode = OP_CALL;
		retn->basic = basic_info::call;
		retn->byte_code = info->pcode_bytes[offset];
		retn->dest_reg = decode_A(curr);
		break;
	}
	case OP_GETIMPORT: {

		if (detail_info)
		{

			std::string compiled = "";

			const auto source = (bytecode[offset + 1]);

			const std::int32_t brco = source >> 30;

			const auto id1 = (brco > 0) ? std::int32_t(source >> 20) & 1023 : -1;
			const auto id2 = (brco > 1) ? std::int32_t(source >> 10) & 1023 : -1;
			const auto id3 = (brco > 2) ? std::int32_t(source) & 1023 : -1;

			if (id1 >= 0)
				compiled += info->proto.k[id1];

			if (id2 >= 0)
				compiled += '.' + info->proto.k[id2];

			if (id3 >= 0)
				compiled += '.' + info->proto.k[id3];

			compiled.erase(std::remove(compiled.begin(), compiled.end(), '\"'), compiled.end());


			retn->data = ("getimport r" + std::to_string(decode_A(curr)) + " " + compiled);
			retn->size = 2u;
			retn->opcode = OP_GETIMPORT;

		}
		else
		{

			retn->data = ("getimport r" + std::to_string(decode_A(curr)) + " k" + std::to_string(decode_D(curr)));
			retn->size = 2u;
			retn->opcode = OP_GETIMPORT;

		}

		retn->byte_code = info->pcode_bytes[offset] + info->pcode_bytes[offset + 1u];
		retn->basic = basic_info::fetch;
		retn->dest_reg = decode_A(curr);

		break;
	}
	case OP_GETVARARGS: {


		retn->data = ("getvarargs r" + std::to_string(decode_A(curr)) + " " + std::to_string(decode_B(std::int32_t(curr)) - 1));
		retn->size = 1u;
		retn->opcode = opcode;
		retn->basic = basic_info::fetch;
		retn->byte_code = info->pcode_bytes[offset];
		retn->dest_reg = decode_A(curr);

		break;
	}
	/* Load. */
	case OP_LOADNIL: {
		retn->data = ("loadnil r" + std::to_string(decode_A(curr)));
		retn->size = 1u;
		retn->opcode = OP_LOADNIL;
		retn->basic = basic_info::load;
		retn->byte_code = info->pcode_bytes[offset];
		retn->dest_reg = decode_A(curr);
		break;
	}
	case OP_LOADN: {
		retn->data = ("loadn r" + std::to_string(decode_A(curr)) + ' ' + std::to_string(decode_D(curr)));
		retn->size = 1u;
		retn->opcode = OP_LOADN;
		retn->basic = basic_info::load;
		retn->byte_code = info->pcode_bytes[offset];
		retn->dest_reg = decode_A(curr);
		break;
	}
	case OP_LOADK: {


		if (detail_info)
			retn->data = ("loadk r" + std::to_string(decode_A(curr)) + " " + info->proto.k[decode_D(curr)]);
		else
			retn->data = ("loadk r" + std::to_string(decode_A(curr)) + " k" + std::to_string(decode_D(curr)));


		retn->byte_code = info->pcode_bytes[offset];
		retn->basic = basic_info::load;
		retn->dest_reg = decode_A(curr);
		retn->size = 1u;
		retn->opcode = OP_LOADK;

		break;
	}
	case OP_LOADB: {

		if (decode_C(curr))
			retn->data = ("loadb r" + std::to_string(decode_A(curr)) + ' ' + std::to_string(decode_B(curr)) + " loadb_" + std::to_string(decode_C(curr)));

		else
			retn->data = ("loadb r" + std::to_string(decode_A(curr)) + ' ' + std::to_string(decode_B(curr)));

		retn->size = 1u;
		retn->opcode = OP_LOADB;
		retn->basic = basic_info::load;
		retn->byte_code = info->pcode_bytes[offset];
		retn->dest_reg = decode_A(curr);

		break;
	}
	case OP_LOADKX: {


		if (detail_info)
			retn->data = ("loadkx r" + std::to_string(decode_A(curr)) + " " + info->proto.k[info->proto.code[offset + 1u]]);
		else
			retn->data = ("loadkx r" + std::to_string(decode_A(curr)) + " k" + std::to_string(info->proto.code[offset + 1u]));


		retn->byte_code = (info->pcode_bytes[offset] + info->pcode_bytes[offset + 1u]);
		retn->basic = basic_info::load;
		retn->dest_reg = decode_A(curr);
		retn->size = 2u;
		retn->opcode = OP_LOADKX;

		break;
	}
	case OP_MOVE: {
		retn->data = ("move r" + std::to_string(decode_A(curr)) + " r" + std::to_string(decode_B(curr)));
		retn->size = 1u;
		retn->opcode = OP_MOVE;
		retn->basic = basic_info::load;
		retn->byte_code = info->pcode_bytes[offset];
		retn->dest_reg = decode_A(curr);
		retn->source_reg = decode_B(curr);
		retn->has_source = true;
		break;
	}
	case OP_NEWTABLE: {
		retn->data = ("newtable r" + std::to_string(decode_A(curr)) + " " + std::to_string((!(decode_B(curr)) ? 0 : (1 << (decode_B(curr) - 1)))) + ' ' + std::to_string(info->proto.code[offset + 1u]));
		retn->size = 2u;
		retn->opcode = OP_NEWTABLE;
		retn->basic = basic_info::set;
		retn->byte_code = (info->pcode_bytes[offset] + info->pcode_bytes[offset + 1u]);;
		retn->dest_reg = decode_A(curr);
		break;
	}
	case OP_SETLIST: {
		retn->data = ("setlist r" + std::to_string(decode_A(curr)) + " r" + std::to_string(decode_B(curr)) + ' ' + std::to_string(decode_C(std::int32_t(curr)) - 1) + ' ' + std::to_string(info->proto.code[offset + 1u]));
		retn->size = 2u;
		retn->opcode = OP_SETLIST;
		retn->basic = basic_info::set;
		retn->byte_code = (info->pcode_bytes[offset] + info->pcode_bytes[offset + 1u]);
		retn->dest_reg = decode_A(curr);
		break;
	}
	case OP_DUPTABLE: {

		if (detail_info)
			retn->data = ("duptable r" + std::to_string(decode_A(curr)) + ' ' + info->proto.k[decode_D(curr)]);
		else
			retn->data = ("duptable r" + std::to_string(decode_A(curr)) + " k" + std::to_string(decode_D(curr)));

		retn->size = 1u;
		retn->opcode = OP_DUPTABLE;
		retn->basic = basic_info::set;
		retn->byte_code = info->pcode_bytes[offset];
		retn->dest_reg = decode_A(curr);
		break;
	}
	case OP_CLOSEUPVALS: {
		retn->data = ("closeupvalues r" + std::to_string(decode_A(curr)));
		retn->size = 1u;
		retn->opcode = OP_CLOSEUPVALS;
		retn->basic = basic_info::set;
		retn->byte_code = info->pcode_bytes[offset];
		retn->dest_reg = decode_A(curr);
		break;
	}
	case OP_NEWCLOSURE: {

		if (detail_info)
			retn->data = ("newclosure r" + std::to_string(decode_A(curr)) + " proto_" + std::to_string(decode_D(curr)));
		else
			retn->data = ("newclosure r" + std::to_string(decode_A(curr)) + ' ' + std::to_string(decode_D(curr)));

		retn->size = 1u;
		retn->opcode = OP_NEWCLOSURE;
		retn->basic = basic_info::set;
		retn->byte_code = info->pcode_bytes[offset];
		retn->dest_reg = decode_A(curr);
		break;
	}
	case OP_DUPCLOSURE: {

		if (detail_info)
			retn->data = ("dupclosure r" + std::to_string(decode_A(curr)) + " " + info->proto.k[decode_D(curr)]);

		else
			retn->data = ("dupclosure r" + std::to_string(decode_A(curr)) + " k" + std::to_string(decode_D(curr)));

		retn->size = 1u;
		retn->opcode = OP_DUPCLOSURE;
		retn->basic = basic_info::closure;
		retn->dest_reg = decode_A(curr);

		break;
	}
	case OP_CAPTURE: {

		const auto tt = decode_A(curr);

		if (tt == 2u)
			retn->data = ("capture " + std::to_string(tt) + " upvalue_" + std::to_string(decode_B(curr)));
		else
		{
			retn->data = ("capture " + std::to_string(tt) + " r" + std::to_string(decode_B(curr)));
			retn->source_reg = decode_B(curr);
			retn->has_source = true;
		}

		retn->size = 1u;
		retn->opcode = OP_CAPTURE;
		retn->basic = basic_info::set;
		retn->tt = tt;

		break;
	}
	case OP_GETUPVAL: {

		retn->data = ("getupvalue r" + std::to_string(decode_A(curr)) + ' ' + std::to_string(decode_B(curr)));
		retn->size = 1u;
		retn->opcode = OP_GETUPVAL;
		retn->basic = basic_info::fetch;
		retn->dest_reg = decode_A(curr);

		break;
	}
	case OP_SETUPVAL: {

		retn->data = ("setupvalue r" + std::to_string(decode_A(curr)) + ' ' + std::to_string(decode_B(curr)));
		retn->size = 1u;
		retn->opcode = OP_SETUPVAL;
		retn->basic = basic_info::set;
		retn->dest_reg = decode_A(curr);

		break;
	}
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

		const auto operation = do_arith_instruction(opcode);
		const auto constant = is_arith_constant_dis(opcode);

		std::string last = "";

		/* Do constant. */
		if (constant)
			last = ((!detail_info) ? (" r" + std::to_string(decode_C(curr))) : (" " + info->proto.k[decode_C(curr)]));
		else
			last = " r" + std::to_string(decode_C(curr));


		retn->data = (std::string(operation) + 'r' + std::to_string(decode_A(curr)) + " r" + std::to_string(decode_B(curr)) + last);
		retn->size = 1u;
		retn->opcode = opcode;
		retn->basic = basic_info::math;
		retn->byte_code = info->pcode_bytes[offset];
		retn->dest_reg = decode_A(curr);
		retn->source_reg = decode_B(curr);
		retn->has_source = true;

		if (!constant)
		{
			retn->value_reg = decode_C(curr);
			retn->has_value = true;
		}

		break;
	}
	case OP_NOT: {


		retn->data = ("not r" + std::to_string(decode_A(curr)) + " r" + std::to_string(decode_B(curr)));
		retn->size = 1u;
		retn->opcode = opcode;
		retn->basic = basic_info::math;
		retn->byte_code = info->pcode_bytes[offset];
		retn->dest_reg = decode_A(curr);
		retn->source_reg = decode_B(curr);
		retn->has_source = true;

		break;
	}
	case OP_MINUS: {


		retn->data = ("minus r" + std::to_string(decode_A(curr)) + " r" + std::to_string(decode_B(curr)));
		retn->size = 1u;
		retn->opcode = opcode;
		retn->basic = basic_info::math;
		retn->byte_code = info->pcode_bytes[offset];
		retn->dest_reg = decode_A(curr);
		retn->source_reg = decode_B(curr);
		retn->has_source = true;

		break;
	}
	/* for loops */
	case OP_FORNLOOP: {

		if (detail_info)
			retn->data = ("fornloop r" + std::to_string(decode_A(curr)) + " fornloop_" + std::to_string(decode_D(curr)));

		else
			retn->data = ("fornloop r" + std::to_string(decode_A(curr)) + " " + std::to_string(decode_D(curr)));


		retn->size = 1u;
		retn->opcode = OP_FORNLOOP;
		retn->basic = basic_info::for_;
		retn->byte_code = info->pcode_bytes[offset];

		break;
	}
	case OP_FORGLOOP: {

		if (detail_info)
			retn->data = ("forgloop r" + std::to_string(decode_A(curr)) + " forgloop_" + std::to_string(decode_D(curr)) + ' ' + std::to_string(info->proto.code[offset + 1u]));

		else
			retn->data = ("forgloop r" + std::to_string(decode_A(curr)) + ' ' + std::to_string(decode_D(curr)) + ' ' + std::to_string(info->proto.code[offset + 1u]));


		retn->size = 2u;
		retn->opcode = OP_FORGLOOP;
		retn->basic = basic_info::for_;
		retn->byte_code = (info->pcode_bytes[offset] + info->pcode_bytes[offset + 1u]);

		break;
	}
	case OP_FORGLOOP_NEXT: {

		if (detail_info)
			retn->data = ("forgloop_next r" + std::to_string(decode_A(curr)) + " forgloop_next_" + std::to_string(decode_D(curr)));

		else
			retn->data = ("forgloop_next r" + std::to_string(decode_A(curr)) + " " + std::to_string(decode_D(curr)));


		retn->size = 1u;
		retn->opcode = OP_FORGLOOP_NEXT;
		retn->basic = basic_info::for_;
		retn->byte_code = info->pcode_bytes[offset];

		break;
	}
	case OP_FORGLOOP_INEXT: {

		if (detail_info)
			retn->data = ("forgloop_inext r" + std::to_string(decode_A(curr)) + " forgloop_inext_" + std::to_string(decode_D(curr)));

		else
			retn->data = ("forgloop_inext r" + std::to_string(decode_A(curr)) + " " + std::to_string(decode_D(curr)));


		retn->size = 1u;
		retn->opcode = OP_FORGLOOP_INEXT;
		retn->basic = basic_info::for_;
		retn->byte_code = info->pcode_bytes[offset];

		break;
	}

						  /* Prep */
	case OP_FORNPREP: {

		if (detail_info)
			retn->data = ("fornprep r" + std::to_string(decode_A(curr)) + " fornnprep_" + std::to_string(decode_D(curr)));

		else
			retn->data = ("fornprep r" + std::to_string(decode_A(curr)) + " " + std::to_string(decode_D(curr)));


		retn->size = 1u;
		retn->opcode = OP_FORNPREP;
		retn->basic = basic_info::for_;
		retn->byte_code = info->pcode_bytes[offset];

		break;
	}
	case OP_FORGPREP: {

		if (detail_info)
			retn->data = ("fornprep r" + std::to_string(decode_A(curr)) + " fornnprep_" + std::to_string(decode_D(curr)));

		else
			retn->data = ("fornprep r" + std::to_string(decode_A(curr)) + " " + std::to_string(decode_D(curr)));


		retn->size = 1u;
		retn->opcode = OP_FORGPREP;
		retn->basic = basic_info::for_;
		retn->byte_code = info->pcode_bytes[offset];

		break;
	}
	case OP_FORGPREP_NEXT: {

		if (detail_info)
			retn->data = ("forgprep_next r" + std::to_string(decode_A(curr)) + " forgprep_next_" + std::to_string(decode_D(curr)));

		else
			retn->data = ("forgprep_next r" + std::to_string(decode_A(curr)) + " " + std::to_string(decode_D(curr)));


		retn->size = 1u;
		retn->opcode = OP_FORGPREP_NEXT;
		retn->basic = basic_info::for_;
		retn->byte_code = info->pcode_bytes[offset];

		break;
	}
	case OP_FORGPREP_INEXT: {

		if (detail_info)
			retn->data = ("forgprep_inext r" + std::to_string(decode_A(curr)) + " forgprep_inext_" + std::to_string(decode_D(curr)));

		else
			retn->data = ("forgprep_inext r" + std::to_string(decode_A(curr)) + " " + std::to_string(decode_D(curr)));


		retn->size = 1u;
		retn->opcode = OP_FORGPREP_INEXT;
		retn->basic = basic_info::for_;
		retn->byte_code = info->pcode_bytes[offset];

		break;
	}
	case OP_FASTCALL: {
		retn->data = ("fastcall " + std::to_string(decode_A(curr)) + ' ' + std::to_string(decode_C(std::int32_t(curr))));
		retn->size = 1u;
		retn->opcode = OP_FASTCALL;
		retn->basic = basic_info::fast_call;
		retn->byte_code = info->pcode_bytes[offset];
		break;
	}
	case OP_FASTCALL1: {
		retn->data = ("fastcall1 " + std::to_string(decode_A(curr)) + " r" + std::to_string(decode_B(curr)) + ' ' + std::to_string(decode_C(std::int32_t(curr))));
		retn->size = 1u;
		retn->opcode = OP_FASTCALL1;
		retn->basic = basic_info::fast_call;
		retn->byte_code = info->pcode_bytes[offset];
		break;
	}
	case OP_FASTCALL2: {
		retn->data = ("fastcall2 " + std::to_string(decode_A(curr)) + " r" + std::to_string(decode_B(curr)) + " r" + std::to_string(info->proto.code[offset + 1u]) + ' ' + std::to_string(decode_C(std::int32_t(curr))));
		retn->size = 2u;
		retn->opcode = OP_FASTCALL2;
		retn->basic = basic_info::fast_call;
		retn->byte_code = (info->pcode_bytes[offset] + info->pcode_bytes[offset + 1u]);
		break;
	}
	case OP_FASTCALL2K: {
		retn->data = ("fastcall2k " + std::to_string(decode_A(curr)) + " r" + std::to_string(decode_B(curr)) + " k" + std::to_string(info->proto.code[offset + 1u]) + ' ' + std::to_string(decode_C(std::int32_t(curr))));
		retn->size = 2u;
		retn->opcode = OP_FASTCALL2K;
		retn->basic = basic_info::fast_call;
		retn->byte_code = (info->pcode_bytes[offset] + info->pcode_bytes[offset + 1u]);
		break;
	}
	case OP_CONCAT: {


		retn->data = ("concat r" + std::to_string(decode_A(curr)) + " r" + std::to_string(decode_B(curr)) + " r" + std::to_string(decode_C(curr)));
		retn->size = 1u;
		retn->opcode = opcode;
		retn->basic = basic_info::load;
		retn->byte_code = info->pcode_bytes[offset];
		retn->dest_reg = decode_A(curr);
		retn->source_reg = decode_B(curr);
		retn->value_reg = decode_C(curr);
		retn->has_value = true;
		retn->has_source = true;

		break;
	}
	case OP_NAMECALL: {
		/* Remove string qoutes. */
		auto& str = info->proto.k[info->proto.code[offset + 1u]];
		str.erase(std::remove(str.begin(), str.end(), '\"'), str.end());

		if (detail_info)
			retn->data = ("namecall r" + std::to_string(decode_A(curr)) + " r" + std::to_string(decode_B(curr)) + ' ' + str);

		else
			retn->data = ("namecall r" + std::to_string(decode_A(curr)) + " r" + std::to_string(decode_B(curr)) + " k" + std::to_string(info->proto.code[offset + 1u]));


		retn->size = 2u;
		retn->opcode = OP_NAMECALL;
		retn->basic = basic_info::call;
		retn->byte_code = (info->pcode_bytes[offset] + info->pcode_bytes[offset + 1u]);
		retn->dest_reg = decode_A(curr);
		retn->source_reg = decode_B(curr);
		retn->has_source = true;
		break;
	}
	/* Globals. */
	case OP_SETGLOBAL: {

		/* Remove string qoutes. */
		auto& str = info->proto.k[info->proto.code[offset + 1u]];
		str.erase(std::remove(str.begin(), str.end(), '\"'), str.end());

		if (detail_info)
			retn->data = ("setglobal r" + std::to_string(decode_A(curr)) + ' ' + str);

		else
			retn->data = ("setglobal r" + std::to_string(decode_A(curr)) + " k" + std::to_string(info->proto.code[offset + 1u]));


		retn->size = 2u;
		retn->opcode = OP_SETGLOBAL;
		retn->basic = basic_info::set;
		retn->byte_code = (info->pcode_bytes[offset] + info->pcode_bytes[offset + 1u]);
		retn->dest_reg = decode_A(curr);

		break;
	}
	case OP_GETGLOBAL: {

		/* Remove string qoutes. */
		auto& str = info->proto.k[info->proto.code[offset + 1u]];
		str.erase(std::remove(str.begin(), str.end(), '\"'), str.end());

		if (detail_info)
			retn->data = ("getglobal r" + std::to_string(decode_A(curr)) + ' ' + str);

		else
			retn->data = ("getglobal r" + std::to_string(decode_A(curr)) + " k" + std::to_string(info->proto.code[offset + 1u]));


		retn->size = 2u;
		retn->opcode = OP_GETGLOBAL;
		retn->basic = basic_info::fetch;
		retn->byte_code = (info->pcode_bytes[offset] + info->pcode_bytes[offset + 1u]);
		retn->dest_reg = decode_A(curr);

		break;
	}
	 /* Table. */
	case OP_LENGTH: {
		retn->data = ("length r" + std::to_string(decode_A(curr)) + " r" + std::to_string(decode_B(curr)));
		retn->size = 1u;
		retn->opcode = OP_LENGTH;
		retn->basic = basic_info::fetch;
		retn->byte_code = info->pcode_bytes[offset];
		retn->dest_reg = decode_A(curr);
		retn->source_reg = decode_B(curr);
		retn->has_source = true;
		break;
	}
	case OP_GETTABLEKS: {

		auto& str = info->proto.k[info->proto.code[offset + 1u]];
		str.erase(std::remove(str.begin(), str.end(), '\"'), str.end());

		if (detail_info)
			retn->data = ("gettableks r" + std::to_string(decode_A(curr)) + " r" + std::to_string(decode_B(curr)) + ' ' + str);
		else
			retn->data = ("gettableks r" + std::to_string(decode_A(curr)) + " r" + std::to_string(decode_B(curr)) + " k" + std::to_string(info->proto.code[offset + 1]));

		retn->size = 2u;
		retn->opcode = OP_GETTABLEKS;
		retn->basic = basic_info::load;
		retn->byte_code = (info->pcode_bytes[offset] + info->pcode_bytes[offset + 1]);
		retn->dest_reg = decode_A(curr);
		retn->source_reg = decode_B(curr);
		retn->has_source = true;
		break;
	}
	case OP_GETTABLEN: {

		retn->data = ("gettablen r" + std::to_string(decode_A(curr)) + " r" + std::to_string(decode_B(curr)) + ' ' + std::to_string(decode_C(curr) + 1u));

		retn->size = 1u;
		retn->opcode = OP_GETTABLEN;
		retn->basic = basic_info::load;
		retn->byte_code = (info->pcode_bytes[offset]);
		retn->dest_reg = decode_A(curr);
		retn->source_reg = decode_B(curr);
		retn->has_source = true;

		break;
	}
	case OP_GETTABLE: {

		retn->data = ("gettable r" + std::to_string(decode_A(curr)) + " r" + std::to_string(decode_B(curr)) + " r" + std::to_string(decode_C(curr)));

		retn->size = 1u;
		retn->opcode = OP_GETTABLE;
		retn->basic = basic_info::load;
		retn->byte_code = (info->pcode_bytes[offset]);
		retn->dest_reg = decode_A(curr);
		retn->source_reg = decode_B(curr);
		retn->has_source = true;

		break;
	}
	case OP_SETTABLEKS: {

		auto& str = info->proto.k[info->proto.code[offset + 1u]];
		str.erase(std::remove(str.begin(), str.end(), '\"'), str.end());

		if (detail_info)
			retn->data = ("settableks r" + std::to_string(decode_A(curr)) + " r" + std::to_string(decode_B(curr)) + ' ' + str);
		else
			retn->data = ("settableks r" + std::to_string(decode_A(curr)) + " r" + std::to_string(decode_B(curr)) + " k" + std::to_string(info->proto.code[offset + 1]));

		retn->size = 2u;
		retn->opcode = OP_SETTABLEKS;
		retn->basic = basic_info::load;
		retn->byte_code = (info->pcode_bytes[offset] + info->pcode_bytes[offset + 1]);
		retn->dest_reg = decode_A(curr);
		retn->ancestor = ancestor_info::settable;
		break;
	}
	case OP_SETTABLEN: {

		retn->data = ("settablen r" + std::to_string(decode_A(curr)) + " r" + std::to_string(decode_B(curr)) + ' ' + std::to_string(decode_C(curr) + 1u));

		retn->size = 1u;
		retn->opcode = OP_SETTABLEN;
		retn->basic = basic_info::load;
		retn->byte_code = (info->pcode_bytes[offset]);
		retn->dest_reg = decode_A(curr);
		retn->ancestor = ancestor_info::settable;

		break;
	}
	case OP_SETTABLE: {

		retn->data = ("settable r" + std::to_string(decode_A(curr)) + " r" + std::to_string(decode_B(curr)) + " r" + std::to_string(decode_C(curr)));

		retn->size = 1u;
		retn->opcode = OP_SETTABLE;
		retn->basic = basic_info::load;
		retn->byte_code = (info->pcode_bytes[offset]);
		retn->dest_reg = decode_A(curr);
		retn->ancestor = ancestor_info::settable;

		break;
	}
	case OP_BREAK: {

		retn->data = "break ";
		retn->size = 1u;
		retn->opcode = OP_BREAK;
		retn->basic = basic_info::none;
		retn->byte_code = (info->pcode_bytes[offset]);

		break;
	}
	case OP_COVERAGE: {

		retn->data = "coverage ";
		retn->size = 1u;
		retn->opcode = OP_COVERAGE;
		retn->basic = basic_info::none;
		retn->byte_code = (info->pcode_bytes[offset]);

		break;
	}
	default: {
		retn->data = "nop ";
		retn->size = 1u;
		retn->byte_code = info->pcode_bytes[offset];
		break;
	}
	}

	return retn;
}

std::vector <std::shared_ptr<dissassembler_data>> dissassemble_whole(std::shared_ptr<decompile_info>& info, const bool detail_info) {

	std::vector <std::shared_ptr<dissassembler_data>> retn;  

	auto offset = 0u;
	const auto goal = info->proto.size_code;
	
	while (offset != goal) 
	{
		const auto disasm = dissassemble(info, offset, detail_info);
		retn.emplace_back(disasm);
		offset += disasm->size;
	}

	return retn;
}

std::vector <std::shared_ptr<dissassembler_data>> dissassemble_range(std::shared_ptr<decompile_info>& info, const std::uint32_t start, const std::uint32_t end, const bool detail_info) {

	const auto max = info->proto.size_code; 
	std::vector <std::shared_ptr<dissassembler_data>> retn;
	auto offset = start;

	while (offset <= end) 
	{
		/* Safe check*/
		if (offset == max)
			break;

		const auto disasm = dissassemble(info, offset, detail_info);
		retn.emplace_back(disasm);
		offset += disasm->size;
	}

	return retn;
}
