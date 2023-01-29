#include <algorithm>
#include <cmath>
#include <math.h>
#include <iostream>
#include <tuple>
#include <unordered_map>
#include "ast.hpp"
#include "config.hpp"
#include "decode.hpp"
#include "decompile_parse.hpp"
#include "dissassemble.hpp"
#include "finalize.hpp"
#include "opcodes.hpp"
#include "optimization.hpp"

#pragma region core_data

/* Legal characters for table index. */
static constexpr const char  legal_chars[] = {

    'a',
    'b',
    'c',
    'd',
    'e',
    'f',
    'g',
    'h',
    'i',
    'j',
    'k',
    'l',
    'm',
    'n',
    'o',
    'p',
    'q',
    'r',
    's',
    't',
    'u',
    'v',
    'w',
    'x',
    'y',
    'z',


    'A',
    'B',
    'C',
    'D',
    'E',
    'F',
    'G',
    'H',
    'I',
    'J',
    'K',
    'L',
    'M',
    'N',
    'O',
    'P',
    'Q',
    'R',
    'S',
    'T',
    'U',
    'V',
    'W',
    'X',
    'Y',
    'Z',

    '1',
    '2',
    '3',
    '4',
    '5',
    '6',
    '7',
    '8',
    '9',
    '0'

};


#define add_missing(registers, dest)  if (registers.find(dest) == registers.end()) registers.insert(std::make_pair(dest, std::make_shared <lua_register>())); else if (registers[dest] == nullptr)  registers[dest] = std::make_shared <lua_register>();

#define form_var(variable_prefix, variable_suffix) "local " + variable_prefix + std::to_string (variable_suffix) + " = "
#define var_name(variable_prefix, variable_suffix)  variable_prefix + std::to_string (variable_suffix)


enum class __type : std::uint8_t
{
    var,
    arg,
    express
};

enum class definition : std::uint8_t
{
    none,
    table
};


struct lua_register
{

    std::string container;
    std::uint32_t expression_size = 0u;
    __type type = __type::express;
    definition def = definition::none;
    bool is_global = false;
    bool is_upvalue = false;
    bool marked_iter = false; /* Marked to move for iteration. */

    std::shared_ptr<lua_register> duplicate()
    {
        auto ptr = std::make_shared <lua_register>();
        ptr->type = this->type;
        ptr->expression_size = this->expression_size;
        ptr->container = std::string(this->container);
        ptr->is_global = this->is_global;
        ptr->is_upvalue = this->is_upvalue;
        ptr->def = this->def;
        return ptr;
    };

    void clear()
    {
        container = "";
        expression_size = 0u;
        type = __type::express;
        is_global = false;
        is_upvalue = false;
        def = definition::none;
        return;
    }
};


struct branch_info
{
    /* Branch endings. */
    std::vector <std::int32_t> ip_branch;

};



#pragma endregion

/* Replicates another copy of registers. */
static std::unordered_map <std::uint8_t, std::shared_ptr <lua_register>> replicate_register(const  std::unordered_map <std::uint8_t, std::shared_ptr <lua_register>>& info)
{

    std::unordered_map <std::uint8_t, std::shared_ptr <lua_register>> retn;

    for (const auto& i : info)
        retn.insert(std::make_pair(i.first, i.second->duplicate()));


    return retn;
}

namespace helper_functions
{
    /* Forms branch based on ast data. */
    static void do_branch(std::string& write, std::string& expression_buffer, const std::shared_ptr<ast_data>& ast, std::uint32_t ip, std::uint32_t& inside, std::uint32_t& inside_loop, const bool is_break)
    {

        /* You can't have 2 elseif, else, if at the same address. */
        switch (ast->branches[ip].front())
        {

        case type_branch::if_: {
            ++inside;
            write += "if ( " + expression_buffer + " ) then\n";
            break;
        }

        case type_branch::elseif_: {
            write += "elseif ( " + expression_buffer + " ) then\n";
            break;
        }

        case type_branch::else_: {
            write += "else\n";
            break;
        }

        default: {
            return;
        }
        }

        /* Parse as break. */
        if (is_break && inside_loop)
        {
            write += "break;\nend\n";
            --inside;
            expression_buffer.clear();
        }

        return;
    }

    /* Returns assignment operator or normal operator for given opcode. Also supports and && or operations. */
    template<bool assignment>
    static const char* const do_arith(const std::uint16_t op)
    {

        switch (op)
        {
        case OP_ADDK:
        case OP_ADD: {
            return ((assignment) ? " += " : " + ");
        }

        case OP_SUBK:
        case OP_SUB: {
            return ((assignment) ? " -= " : " - ");
        }

        case OP_DIVK:
        case OP_DIV: {
            return ((assignment) ? " /= " : " / ");
        }

        case OP_MULK:
        case OP_MUL: {
            return ((assignment) ? " *= " : " * ");
        }

        case OP_POWK:
        case OP_POW: {
            return ((assignment) ? " ^= " : " ^ ");
        }

        case OP_MODK:
        case OP_MOD: {
            return ((assignment) ? " %= " : " % ");
        }

        case OP_ANDK:
        case OP_AND: {
            return " and ";
        }

        case OP_ORK:
        case OP_OR: {
            return " or ";
        }
        }


        return " + ";
    }

    /* Checks if arith opcodes garunteeds a k index. */
    static bool is_arith_constant(const std::uint16_t op)
    {

        switch (op)
        {
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

    /* Turns K arith opcode to normal. */
    static std::uint16_t arithk_normal(const std::uint16_t op)
    {

        switch (op)
        {

        case OP_ADDK: {
            return OP_ADD;
        }

        case OP_SUBK: {
            return OP_SUB;
        }

        case OP_DIVK: {
            return OP_DIV;
        }

        case OP_MULK: {
            return OP_MUL;
        }

        case OP_POWK: {
            return OP_POW;
        }

        case OP_MODK: {
            return OP_MOD;
        }

        case OP_ANDK: {
            return OP_AND;
        }

        case OP_ORK: {
            return OP_OR;
        }

        default: {
            return OP_ADD;
        }

        }

    }


    /* Returns wether a string index is illgal and should be treat as a string. *Ex. first char cant be digit and all chars have to be UTF-8. */
    static bool legal_index(const std::string& str)
    {

        /*Check if first char is digit ex eliminate. 1test = 100; */
        if (std::isdigit(str.front()))
            return false;

        for (const auto i : str)
        {
            auto retn = false;

            /* Loop for legals. */
            for (const auto o : legal_chars)
                if (o == i)
                {
                    retn = true;
                    break;
                }

            if (!retn)
                return false;
        }

        return true;
    }


    /* Gets compare type from opcode. */
    template<bool opposite>
    static const char* const get_compare(const std::uint16_t op_code)
    {
        switch (op_code)
        {

        case OP_JUMPIFLT: {
            return (!opposite) ? " > " : " < ";
        }

        case OP_JUMPIFNOT: {
            return (!opposite) ? "not " : " ";
        }

        case OP_JUMPIF: {
            return (!opposite) ? " " : "not ";
        }

        case OP_JUMPIFLE: {
            return (!opposite) ? " < " : " > ";
        }

        case OP_JUMPIFEQ:
        case OP_JUMPIFEQK: {
            return (!opposite) ? " == " : " ~= ";
        }


        case OP_JUMPIFNOTEQ:
        case OP_JUMPIFNOTEQK: {
            return (!opposite) ? " ~= " : " == ";
        }

        }

        return " == ";
    }

}

void decompiler_parse(const std::shared_ptr<decompile_config>& config, std::shared_ptr<decompile_info>& decom_info, std::string& write)
{
    /* No code to decompile. */
    if (!decom_info->proto.size_code)
        return;

    /* Ast */
    auto ast = create_ast(decom_info, config);

    /* Closure. */
    std::unordered_map<std::uint32_t, std::string> closure_functions; /* { proto_id, data } */

    std::sort(ast->else_routine_begin.begin(), ast->else_routine_begin.end());

    /* Reduces shared location calls in asm. */
    auto& ast_branches = ast->branches;
    auto& ast_ends = ast->ends;
    auto& ast_logical_operations = ast->logical_operations;
    auto& ast_tables = ast->forming_tables;
    auto& ast_dedicated_loops = ast->dedicated_loops;
    auto& ast_analyzed_loops = ast->loop_map;
    auto& ast_branch_pos_scope = ast->branch_pos_scope;
    auto& ast_else_routine_begin = ast->else_routine_begin;
    auto& while_end = ast->while_ends;

    const auto ast_branch_pos_scope_begin = ast_branch_pos_scope.begin();
    const auto ast_branch_pos_scope_end = ast_branch_pos_scope.end();

    std::string loop_append = ""; /* Appending loop info. */

    /* Dissassembly */
    const auto dissassembly = dissassemble_whole(decom_info);
    auto dissassembly_location = 0u; /* Relative to IP. */
    const auto can_view_bytecode = config->include_bytecode;
    const auto can_view_dissassemble = config->include_dissassembly;

    /* Var/Arg naming && decom append. */
    auto variable_suffix = 0u;
    auto argument_suffix = 0u;
    auto iterator_suffix = 0u;
    auto global_suffix = 0u;
    auto upvalue_suffix = 0u;
    auto function_suffix = 0u;
    auto loop_variable_suffix = 0u;

    /* Config setup. */
    const auto& argument_prefix = config->argument_prefix;
    const auto& variable_prefix = config->variable_prefix;
    const auto& iterator_prefix = config->iterator_prefix;
    const auto& function_prefix = config->function_prefix;
    const auto& upvalue_prefix = config->upvalue_prefix;
    const auto& loop_variable_prefix = config->loop_variable_prefix;
    const auto& loop_variable_prefix_2 = config->loop_variable_prefix_2;

    /* Bytecode info. */
    const auto bytecode = decom_info->proto.code;
    const auto bytecode_size = decom_info->proto.size_code;

    /* K-Values */
    auto& kval = decom_info->proto.k;

    /* Register info. */
    std::unordered_map <std::uint8_t, std::shared_ptr <lua_register>> registers;      /* { Reg, Register_data } */
    std::unordered_map <std::uint32_t, std::unordered_map <std::uint8_t, std::shared_ptr <lua_register>>> branch_regs;      /* { Branch Instruction Location, { Reg, Register_data } } */

    auto branch_regx = 0u; /* Any branch index. */

    /* Branch stuff. */
    auto final_else = false;
    auto inside_elseif = false;
    auto inside_loop = 0u;
    auto inside_branch = 0u;

    /* Logical operations. */
    auto logical_operation_end = -1;
    auto logical_operation_reg = 0u;
    auto logical_operation_type = __type::express;
    std::string logical_operation_data = ""; /* Reserved for args and vars. */
    std::shared_ptr<loc_var> logical_operation_locvar = nullptr;

    /* Genric expression buffer */
    std::string expression_buffer = ""; /* Can be used for normal expressions or logical expressions. */
    auto is_break = false; /* See if current branch routine is break. */

    /* Cached original iteration. */
    auto original_iter = 0u;
    auto prev_iter = 0u;

    /* Call concat. */
    auto last_dest = 0u; /* Previous dest reg (Used for multret call). */

    /* Tables */
    auto end_table = 0u; /* Parent table. *Acts as a indecator that were in a table could check size of table vector but this is faster and better. */
    auto table_count = 0u; /* This is only here is multiple tables end at one opcode. *Could resolve this in AST but thats too much work for something simple. */
    std::vector<std::pair<std::uint32_t, std::uint32_t>> tables;  /* If there is any sub tables they will be put here { {end, reg} }*/
    std::unordered_map<std::uint32_t, std::uint32_t> end_table_counter; /* End of table counter for ends when settable hits. */

    /* While loops. */
    auto inside_while = false;
    std::vector<std::uint32_t> while_final_branch; /* Positions of while loops final loops. *Indications for while loops. */

    /* Variable */
    auto loc_var_pos = 0u; /* Next loc variable position. */
    auto loc_next_idx = 0u; /* Cache for current idx. */
    std::shared_ptr<loc_var> loc_var; /* Loc var data. */

    /* Generic while, repeat loops. */
    auto generic_loop_end = 0u;

    /* Place next locvar */
    loc_var = ((!decom_info->proto.locvars.size()) ? nullptr : decom_info->proto.locvars[loc_next_idx]);
    if (loc_var != nullptr)
    {
        ++loc_next_idx;
        loc_var_pos = loc_var->end;
    }

    /* Setup for closure. */
    if (decom_info->proto.closure.is_child_proto)
    {

        for (auto o = 0u; o < decom_info->proto.closure.arg_count; ++o)
        {
            auto r_ptr = std::make_shared<lua_register>();
            r_ptr->type = __type::arg;
            r_ptr->container = std::string(argument_prefix + std::to_string(o));
            registers.insert(std::make_pair(o, r_ptr));
        }

    }

    /* Core */
    for (auto iter = 0u; iter < bytecode_size; ++iter)
    {
        original_iter = iter;

        const auto instruction_data = bytecode[iter];
        const auto curr_opcode = decode_opcode(instruction_data);

#if CONFIG_DEBUG_ENABLE == true

        std::cout << "on " << iter << " : " << dissassemble(decom_info, iter, true)->data << std::endl;

#if CONFIG_DEBUG_ENABLE_BUFFER == true
        std::cout << "write " << write << std::endl;
#endif

#if CONFIG_DEBUG_ENABLE_REGISTERS == true
        for (const auto& ii : registers)
            std::cout << "register " << std::to_string(ii.first) << " : " << ii.second->container << std::endl;
#endif

#if CONFIG_DEBUG_ENABLE_INDIVISUAL_REGISTER == true
        /* Modify these to what you want. */
        if (iter > 0u)
            std::cout << registers[0] << " : " << &registers[0]->container << std::endl;
#endif      

#if CONFIG_DEBUG_ENABLE_REGISTER_ADDRESS == true
        std::cout << "Register address " << &registers << std::endl;
#endif

#endif


#pragma region pre_routine 
        /* Ends. */
        if (std::binary_search(ast_ends.begin(), ast_ends.end(), iter))
        {
            auto begin = ast_ends.begin();

            while ((begin = std::find_if(begin, ast_ends.end(), [=](const auto it) { return it == iter; })) != ast_ends.end())
            {
                write += "end\n";
                ++begin;
                registers = branch_regs[branch_regx--];
                branch_regs.erase(branch_regx + 1u);
            }

            if (inside_branch)
                --inside_branch;
        }


        /* Branches. *Only inc for branch info we will write registers at the end. */
        if (ast_branches.find(iter) != ast_branches.end())
        {
            const auto& branch_on = ast_branches[iter];
            auto while_count = 1u;
            const auto o_while_count = std::count(branch_on.begin(), branch_on.end(), type_branch::while_);
            for (const auto tb : branch_on)
            {
                switch (tb)
                {
                    /* Repeat loop. */
                case type_branch::repeat:
                {
                    ++inside_loop;
                    /* Set gernic type when where there. */
                    write += "repeat\n";
                    branch_regs.insert(std::make_pair(++branch_regx, replicate_register(registers)));
                    break;
                }

                /* While loop. */
                case type_branch::while_:
                {
                    ++inside_loop;
                    inside_while = true; /* While loop will be further compiled during later decompilation. */

                    const auto& on_loop = ast_analyzed_loops[iter];

                    /* Check for while true/ repeat until true loops. Back is always the value to search for for not while true loops. */
                    if (while_count == o_while_count && !on_loop.back().second.second)
                    {

                        while_final_branch.emplace_back(on_loop.back().first);
                        branch_regs.insert(std::make_pair(++branch_regx, replicate_register(registers)));

                    }
                    else
                    {
                        /* While true/repeat until true loops get check in AST. Which has indication of its start being its branching end. */
                        write += "while ( true ) do\n";
                        branch_regs.insert(std::make_pair(++branch_regx, replicate_register(registers)));
                    }

                    ++while_count;

                    break;
                }

                /* Forgloop */
                case type_branch::for_: {

                    const auto curr = ast_dedicated_loops[iter].front(); /* Can't have multiple loops at the same address. */
                    const auto for_op = decode_opcode(decom_info->proto.code[curr]);

                    ++inside_loop;

                    switch (for_op)
                    {
                    case OP_FORNLOOP: {
                        /* Place scope registers. */
                        branch_regs.insert(std::make_pair(++branch_regx, replicate_register(registers)));

                        const auto a = decode_A(decom_info->proto.code[curr]);

                        const auto end = a;
                        const auto inc = (a + 1u);
                        const auto start = (a + 2u);

                        /* Write start end etc and finalize. */
                        const auto iterate = iterator_prefix + std::to_string(iterator_suffix++);
                        write += "for " + iterate + " = " + registers[start]->container + ", " + registers[end]->container;


                        /* Include incrementor garunteed if arg or var but check if 1 number. */
                        const auto incrementor = registers[inc]->container;
                        if (registers[inc]->type == __type::arg || registers[inc]->type == __type::var || incrementor != "1")
                            write += ", " + incrementor;

                        write += " do \n";

                        /* Set iteration variable. */
                        add_missing(registers, start);

                        const auto& dest_reg = registers[start];
                        dest_reg->container = iterate;
                        dest_reg->expression_size = 0u;
                        dest_reg->type = __type::var;
                        dest_reg->marked_iter = true;

                        break;
                    }

                    case OP_FORGLOOP_NEXT:
                    case OP_FORGLOOP_INEXT: {

                        /* Place scope registers. */
                        branch_regs.insert(std::make_pair(++branch_regx, replicate_register(registers)));

                        const auto a = decode_A(decom_info->proto.code[curr]);

                        std::string compiled = "for ";

                        /* K. */
                        const auto name_K = loop_variable_prefix + std::to_string(loop_variable_suffix++);

                        add_missing(registers, (a + 3u));

                        const auto& dest_K = registers[a + 3u];
                        dest_K->expression_size = 0u;
                        dest_K->type = __type::var;
                        dest_K->container = name_K;

                        compiled += name_K;

                        /* V */
                        const auto name_V = loop_variable_prefix_2 + std::to_string(loop_variable_suffix++);

                        add_missing(registers, (a + 4u));

                        const auto& dest_V = registers[a + 4u];
                        dest_V->expression_size = 0u;
                        dest_V->type = __type::var;
                        dest_V->container = name_V;

                        compiled += ", " + name_V; /* defo has k can use split. */

                        /* Prep for target. */
                        compiled += " in ";

                        for (auto i = 0u; i < 3u; ++i)
                        {

                            const auto& dest = registers[a + i];

                            /* invalid */
                            if (dest->container == "nil")
                                break;

                            compiled += dest->container;

                            /* Next not found so end. */
                            if (registers.find(a + i + 1u) == registers.end() || registers[a + i + 1u]->container == "nil")
                                break;
                            else /* add split */
                                compiled += ", ";

                        }

                        write += compiled + " do\n";
                        break;
                    }


                    case OP_FORGLOOP: {

                        /* Place scope registers. */
                        branch_regs.insert(std::make_pair(++branch_regx, replicate_register(registers)));

                        const auto begin = decode_A(decom_info->proto.code[curr]); /* +1, +2 are used. */
                        const auto count = decom_info->proto.code[curr + 1u];

                        /* Compile variables in loop statement. */
                        std::string compiled_vars = "";
                        for (auto i = 0u; i < count; ++i)
                        {

                            /* Make iterating variable name and compile it. */
                            const auto name = loop_variable_prefix + std::to_string(loop_variable_suffix++);
                            compiled_vars += (name + ((i == count) ? " " : ", "));


                            /* Write register if it doesn't exists. */
                            const auto on = (i + begin + 3u);
                            add_missing(registers, on);


                            /* Write info. */
                            const auto& reg = registers[on];
                            reg->expression_size = 1u;
                            reg->type = __type::var;
                            reg->container = name;

                        }

                        /* Compile iterators. */
                        std::string compiled_iterator = "";
                        for (auto i = begin; i < (begin + 3u); ++i)
                        {

                            /* Make iterator and compile it. */
                            if (registers.find(i) != registers.end())
                            {

                                /* Register exists so compile it if its not nil. */
                                const auto& reg = registers[i];

                                /* Add divider. */
                                if (!compiled_iterator.empty() && reg->container != "nil")
                                    compiled_iterator += ", ";

                                if (reg->container != "nil")
                                    compiled_iterator += reg->container;
                            }
                            else
                                break;
                        }

                        /* Remove ", " *This is here just incase it hangs. */
                        if (compiled_iterator[compiled_iterator.length() - 2u] == ',')
                            compiled_iterator.erase(compiled_iterator.length() - 2u);

                        write += ("for " + compiled_vars + "in " + compiled_iterator + " do\n");
                        break;
                    }
                    }

                    /* Skip jump. */
                    if (curr_opcode == OP_JUMP)
                        goto end_parse;

                    break;
                }
                default: {
                    ++inside_branch;
                    break;
                }
                }
            }
        }

        /* Useful for repeat so log begin and erase from cache. Also make sure its a repeat. */
        if (ast_analyzed_loops.find(iter) != ast_analyzed_loops.end() && ast_analyzed_loops[iter].front().second.first)
        {
            /* Get biggest value. */
            /* The value we want is the compare end of our loop which thi vector is only here for recurring values. */
            auto p = 0u;
            for (const auto& i : ast_analyzed_loops[iter])
                if (p < i.first)
                    p = i.first;

            generic_loop_end = p;
            ast_analyzed_loops.erase(iter);
        }

        /* Comment data. */
        if (can_view_bytecode || can_view_dissassemble)
        {

            /* Write dissassembly data. */
            if (can_view_dissassemble)
            {
                write += "--   " + dissassembly[dissassembly_location]->data;
                if (!can_view_bytecode)
                    write += '\n';
            }

            /* Write bytecode data. */
            if (can_view_bytecode)
            {
                std::string fill;
                fill.resize((25u - dissassembly[dissassembly_location]->data.length()), ' ');
                write += ((can_view_dissassemble) ? fill : "--   ") + dissassembly[dissassembly_location]->byte_code + '\n';
            }


        }


        /* Logical operation begin. */
        if (ast_logical_operations.find(iter) != ast_logical_operations.end())
        {

            const auto& logical_op = ast_logical_operations[iter];
            logical_operation_end = logical_op->end;
            logical_operation_reg = logical_op->reg;

            add_missing(registers, logical_operation_reg);

            /* Cache type as to not mess stuff up. */
            logical_operation_type = registers[logical_operation_reg]->type;
            registers[logical_operation_reg]->type = __type::express;

            /* Cache arg or var. */
            if (logical_operation_type == __type::arg || logical_operation_type == __type::var)
                logical_operation_data = registers[logical_operation_reg]->container;
        }


        /* To form end of logical operation we need to cache current locvar and set current to nullptr we will set everything later. */
        if (loc_var != nullptr && signed(loc_var->end) == logical_operation_end)
        {
            logical_operation_locvar = loc_var;
            loc_var = nullptr;
        }


#pragma endregion 




        switch (curr_opcode)
        {

        case OP_NOT: {

            const auto dest = decode_A(instruction_data);
            add_missing(registers, dest);
            const auto source = decode_B(instruction_data);


            auto& dest_reg = registers[dest];
            const auto& source_reg = registers[source];


            /* Variable */
            if (loc_var != nullptr && loc_var_pos == original_iter && !loc_var->in_table)
            {

                const auto is_emp = loc_var->name.empty();
                const auto name = ((is_emp) ? (variable_prefix + std::to_string(variable_suffix)) : loc_var->name);

                dest_reg->container = name;
                dest_reg->expression_size = 0u;
                dest_reg->type = __type::var;

                write += ((is_emp) ? form_var(variable_prefix, variable_suffix) : "local " + name + " = ") + "not " + source_reg->container + ";\n";

                if (is_emp)
                    ++variable_suffix;

            }
            else
            {

                /* Assignment */
                if (dest_reg->type == __type::var || dest_reg->type == __type::arg)
                    write += dest_reg->container + " = not " + source_reg->container + ";\n";

                else
                {

                    /* Expression */
                    dest_reg->container = "not " + source_reg->container;
                    dest_reg->type = __type::express;
                    ++dest_reg->expression_size;

                }

            }

            break;
        }


        case OP_GETVARARGS: {

            auto counter = (decode_B(std::int32_t(instruction_data)) - 1);
            if (counter == -1)
                counter = last_dest;

            for (auto on = decode_A(instruction_data); on < (decode_A(instruction_data) + counter); ++on)
            {

                const auto dest = on;
                add_missing(registers, dest);

                auto& dest_reg = registers[dest];

                /* Variable */
                if (loc_var != nullptr && loc_var_pos == original_iter && !loc_var->in_table)
                {

                    const auto is_emp = loc_var->name.empty();
                    const auto name = ((is_emp) ? (variable_prefix + std::to_string(variable_suffix)) : loc_var->name);

                    dest_reg->container = name;
                    dest_reg->expression_size = 0u;
                    dest_reg->type = __type::var;

                    write += ((is_emp) ? form_var(variable_prefix, variable_suffix) : "local " + name + " = ") + "...;\n";

                    if (is_emp)
                        ++variable_suffix;

                }
                else
                {

                    /* Assignment */
                    if (dest_reg->type == __type::var || dest_reg->type == __type::arg)
                        write += dest_reg->container + " = ...;\n";

                    else
                    {

                        /* Expression */
                        dest_reg->container = "...";
                        dest_reg->type = __type::express;
                        ++dest_reg->expression_size;

                    }

                }

            }

            break;
        }


        case OP_MINUS: {

            const auto dest = decode_A(instruction_data);
            const auto source = decode_B(instruction_data);

            add_missing(registers, dest);

            auto& dest_reg = registers[dest];
            const auto& source_reg = registers[source];

            /* Variable */
            if (loc_var != nullptr && loc_var_pos == original_iter && !loc_var->in_table)
            {

                const auto is_emp = loc_var->name.empty();
                const auto name = ((is_emp) ? (variable_prefix + std::to_string(variable_suffix)) : loc_var->name);

                dest_reg->container = name;
                dest_reg->expression_size = 0u;
                dest_reg->type = __type::var;

                write += ((is_emp) ? form_var(variable_prefix, variable_suffix) : "local " + name + " = -") + source_reg->container + ";\n";

                if (is_emp)
                    ++variable_suffix;

            }
            else
            {

                /* Assignment */
                if (dest_reg->type == __type::var || dest_reg->type == __type::arg)
                    write += dest_reg->container + " = -" + source_reg->container + ";\n";

                else
                {

                    /* Expression */
                    dest_reg->container = "-" + source_reg->container;
                    dest_reg->type = __type::express;
                    ++dest_reg->expression_size;

                }

            }

            break;
        }

        case OP_RETURN: {


            const auto dest = decode_A(instruction_data);
            auto returns = (decode_B(std::int32_t(instruction_data)) - 1);

            /* If its not main proto then write a return. */
            if (decom_info->proto.closure.is_child_proto || (iter + 1u) != decom_info->proto.size_code)
            { /* Skip op */

/* Data. */
                auto first = true;
                std::string compiled = "";


                /* Compile return. */
                if (returns == -1)
                    returns = last_dest;


                /* Check to make sure if last dest reg is current dest reg and was multret if so just one return format. */
                if ((decode_B(std::int32_t(instruction_data)) - 1) == -1 && dest == last_dest)
                    compiled = ' ' + registers[dest]->container;
                else
                {

                    for (auto i = 0u; i < unsigned(returns); ++i)
                    {

                        if (first)
                        {
                            compiled += ' ';
                            first = false;
                        }

                        compiled += registers[dest + i]->container;

                        if ((i + 1u) != unsigned(returns))
                            compiled += ", ";

                    }

                }


                /* Write return.*/
                write += "return" + compiled + ";\n";

            }


            break;
        }

        case OP_GETIMPORT: {

            /* Form import */
            std::string compiled = "";


            const auto dest = decode_A(instruction_data);
            add_missing(registers, dest);


            const auto& dest_reg = registers[dest];

            const auto source = (bytecode[++iter]);

            const std::int32_t brco = source >> 30;

            const auto id1 = (brco > 0) ? std::int32_t(source >> 20) & 1023 : -1;
            const auto id2 = (brco > 1) ? std::int32_t(source >> 10) & 1023 : -1;
            const auto id3 = (brco > 2) ? std::int32_t(source) & 1023 : -1;

            if (id1 >= 0)
                compiled += kval[id1];

            if (id2 >= 0)
                compiled += '.' + kval[id2];

            if (id3 >= 0)
                compiled += '.' + kval[id3];

            compiled.erase(std::remove(compiled.begin(), compiled.end(), '\"'), compiled.end());




            /* Variable */
            if (loc_var != nullptr && (loc_var_pos == (iter - 1u) || loc_var_pos == original_iter) && !loc_var->in_table)
            {

                const auto is_emp = loc_var->name.empty();
                const auto name = ((is_emp) ? (variable_prefix + std::to_string(variable_suffix)) : loc_var->name);

                dest_reg->container = name;
                dest_reg->expression_size = 0u;
                dest_reg->type = __type::var;

                write += ((is_emp) ? form_var(variable_prefix, variable_suffix) : "local " + name + " = ") + compiled + ";\n";

                if (is_emp)
                    ++variable_suffix;

            }
            else
            {

                /* Assignment */
                if (dest_reg->type == __type::var || dest_reg->type == __type::arg)
                    write += dest_reg->container + " = " + compiled + "; \n";

                else
                {

                    /* Expression */
                    dest_reg->container = compiled;
                    dest_reg->type = __type::express;
                    dest_reg->expression_size = 0u;

                }

            }

            break;
        }



                         /* Loads. */
        case OP_LOADNIL: {

            const auto dest = decode_A(instruction_data);

            add_missing(registers, dest);
            const auto& dest_reg = registers[dest];

            /* Variable */
            if (loc_var != nullptr && loc_var_pos == original_iter && !loc_var->in_table)
            {

                const auto is_emp = loc_var->name.empty();
                const auto name = ((is_emp) ? (variable_prefix + std::to_string(variable_suffix)) : loc_var->name);

                dest_reg->container = name;
                dest_reg->expression_size = 0u;
                dest_reg->type = __type::var;

                write += ((is_emp) ? form_var(variable_prefix, variable_suffix) : "local " + name + " = ") + " nil;\n";

                if (is_emp)
                    ++variable_suffix;

            }
            else
            {

                /* Assignment */
                if (dest_reg->type == __type::var || dest_reg->type == __type::arg)
                    write += dest_reg->container + " = nil;\n";

                else
                {

                    /* Expression */
                    dest_reg->container = "nil";
                    dest_reg->type = __type::express;
                    dest_reg->expression_size = 0u;

                }

            }

            break;
        }
        case OP_LOADK: {

            const auto dest = decode_A(instruction_data);
            const auto source = decode_D(instruction_data);

            add_missing(registers, dest);
            const auto& dest_reg = registers[dest];

            /* Variable */
            if (loc_var != nullptr && loc_var_pos == original_iter && !loc_var->in_table)
            {

                const auto is_emp = loc_var->name.empty();
                const auto name = ((is_emp) ? (variable_prefix + std::to_string(variable_suffix)) : loc_var->name);

                dest_reg->container = name;
                dest_reg->expression_size = 0u;
                dest_reg->type = __type::var;

                write += ((is_emp) ? form_var(variable_prefix, variable_suffix) : "local " + name + " = ") + kval[source] + ";\n";

                if (is_emp)
                    ++variable_suffix;

            }
            else
            {

                /* Assignment */
                if (dest_reg->type == __type::var || dest_reg->type == __type::arg)
                    write += dest_reg->container + " = " + kval[source] + "; \n";

                else
                {

                    /* Expression */
                    dest_reg->container = kval[source];
                    dest_reg->type = __type::express;
                    dest_reg->expression_size = 0u;

                }

            }


            break;
        }
        case OP_LOADKX: {

            const auto dest = decode_A(instruction_data);
            const auto source = decom_info->proto.code[iter + 1u];

            add_missing(registers, dest);
            const auto& dest_reg = registers[dest];

            /* Variable */
            if (loc_var != nullptr && loc_var_pos == original_iter && !loc_var->in_table)
            {

                const auto is_emp = loc_var->name.empty();
                const auto name = ((is_emp) ? (variable_prefix + std::to_string(variable_suffix)) : loc_var->name);

                dest_reg->container = name;
                dest_reg->expression_size = 0u;
                dest_reg->type = __type::var;

                write += ((is_emp) ? form_var(variable_prefix, variable_suffix) : "local " + name + " = ") + kval[source] + ";\n";

                if (is_emp)
                    ++variable_suffix;

            }
            else
            {

                /* Assignment */
                if (dest_reg->type == __type::var || dest_reg->type == __type::arg)
                    write += dest_reg->container + " = " + kval[source] + "; \n";

                else
                {

                    /* Expression */
                    dest_reg->container = kval[source];
                    dest_reg->type = __type::express;
                    dest_reg->expression_size = 0u;

                }

            }


            break;
        }
        case OP_LOADB: {


            const auto dest = decode_A(instruction_data);
            const auto source = decode_B(instruction_data);

            const auto str = ((source) ? "true" : "false");

            add_missing(registers, dest);
            const auto& dest_reg = registers[dest];

            /* Has jump so take jump and set to variable. */
            const auto jump = decode_C(instruction_data);
            if (jump)
            { /* Data will get at end. */
                iter += jump;
                ++dissassembly_location;
                break;
            }


            /* Variable */
            if (loc_var != nullptr && loc_var_pos == original_iter && !loc_var->in_table)
            {

                const auto is_emp = loc_var->name.empty();
                const auto name = ((is_emp) ? (variable_prefix + std::to_string(variable_suffix)) : loc_var->name);

                dest_reg->container = name;
                dest_reg->expression_size = 0u;
                dest_reg->type = __type::var;

                write += ((is_emp) ? form_var(variable_prefix, variable_suffix) : "local " + name + " = ") + str + ";\n";

                if (is_emp)
                    ++variable_suffix;

            }
            else
            {

                /* Assignment */
                if (dest_reg->type == __type::var || dest_reg->type == __type::arg)
                    write += dest_reg->container + " = " + str + "; \n";

                else
                {

                    /* Expression */
                    dest_reg->container = str;
                    dest_reg->type = __type::express;
                    dest_reg->expression_size = 0u;

                }

            }

            break;
        }
        case OP_LOADN: {

            const auto dest = decode_A(instruction_data);
            const auto source = (decode_D(std::int32_t(instruction_data)));

            add_missing(registers, dest);
            const auto& dest_reg = registers[dest];


            /* Variable */
            if (loc_var != nullptr && loc_var_pos == original_iter && !loc_var->in_table)
            {

                const auto is_emp = loc_var->name.empty();
                const auto name = ((is_emp) ? (variable_prefix + std::to_string(variable_suffix)) : loc_var->name);

                dest_reg->container = name;
                dest_reg->expression_size = 0u;
                dest_reg->type = __type::var;

                write += ((is_emp) ? form_var(variable_prefix, variable_suffix) : "local " + name + " = ") + std::to_string(source) + ";\n";

                if (is_emp)
                    ++variable_suffix;

            }
            else
            {

                /* Assignment */
                if (dest_reg->type == __type::var || dest_reg->type == __type::arg)
                    write += dest_reg->container + " = " + std::to_string(source) + "; \n";

                else
                {

                    /* Expression */
                    dest_reg->container = std::to_string(source);
                    dest_reg->type = __type::express;
                    dest_reg->expression_size = 0u;

                }

            }

            break;
        }




                     /* Compare. */
        case OP_JUMPIFEQK:
        case OP_JUMPIFNOTEQK:
        case OP_JUMPIFEQ:
        case OP_JUMPIFLE:
        case OP_JUMPIFLT:
        case OP_JUMPIFNOTEQ:
        case OP_JUMPIFNOTLE:
        case OP_JUMPIFNOTLT: {


            const auto& reg = registers[decode_A(instruction_data)];


            const auto next = bytecode[++iter]; /* Next bytecode data *Contains source data */
            const auto len = decode_D(instruction_data); /* Jump lenght */


            /* Make sure if its a constant compare or not. */
            const auto& reg1 = ((curr_opcode == OP_JUMPIFNOTEQK || curr_opcode == OP_JUMPIFEQK) ? kval[next] : registers[next]->container);

            /* Check for break. */
            if (!is_break)
                for (const auto& i : ast_dedicated_loops)
                    if (std::find(i.second.begin(), i.second.end(), (iter + len)) != i.second.end()) /* Don't include inc to hit jumpback. */
                        is_break = true;


            /* Set ands and ors for our branch. */
            if (ast->branch_comparisons.find(original_iter) != ast->branch_comparisons.end())
            {
                printf("chazkay\n");
                auto& ptr = ast->branch_comparisons[original_iter];

                /* Always follow with break. */
                if (is_break)
                    ptr->compare = helper_functions::get_compare<false>(curr_opcode);


                switch (ptr->i)
                {

                case branch_data_e::or_close:
                case branch_data_e::close:
                case branch_data_e::and_close: {

                    expression_buffer += (reg->container + ' ' + ptr->compare + ' ' + reg1 + " )");

                    break;
                }

                case branch_data_e::and_: {

                    expression_buffer += (reg->container + ' ' + ptr->compare + ' ' + reg1 + " and ");

                    break;
                }

                case branch_data_e::and_open: {

                    expression_buffer += ("( " + reg->container + ' ' + ptr->compare + ' ' + reg1 + " and ");

                    break;
                }

                case branch_data_e::or_: {

                    expression_buffer += (reg->container + ' ' + ptr->compare + ' ' + reg1 + " or ");

                    break;
                }

                case branch_data_e::or_open: {

                    expression_buffer += ("( " + reg->container + ' ' + ptr->compare + ' ' + reg1 + " or ");

                    break;
                }

                case branch_data_e::none: {

                    expression_buffer += (reg->container + ' ' + ptr->compare + ' ' + reg1);

                    break;
                }

                default: {
                    break;
                }

                }

            }
            else
            {

                /* Remove break. */
                if (is_break && ast->extra_branches.find(original_iter) != ast->extra_branches.end())
                    ast->extra_branches.erase(original_iter);

                /* Write to buffer. */
                expression_buffer += (reg->container + ((ast->extra_branches.find(original_iter) != ast->extra_branches.end()) ? (' ' + std::string(ast->extra_branches[original_iter]) + ' ') : ((is_break) ? helper_functions::get_compare<false>(curr_opcode) : helper_functions::get_compare<true>(curr_opcode))) + reg1);


                /* Logical operation will get parsed later. Also cant be loop compare end. And cant be forming any while loops.  */
                /* Indication of end of branch routine. */
                if (logical_operation_end == -1 && !generic_loop_end && !while_final_branch.size())
                {

                    /* If there is a break we need to remove need to remove end from cache. */
                    if (is_break)
                    {

                        auto counter = 0u;
                        auto on = (original_iter + dissassemble(decom_info, original_iter)->size);
                        const auto dis = dissassemble_range(decom_info, on, bytecode_size);

                        for (const auto& i : dis)
                        {

                            /* Check if its an if. */
                            if (i->is_comparative && i->basic == basic_info::branch && ast_branches.find(on) != ast_branches.end() && ast_branches[on].front() == type_branch::if_) /* Multiple ifs can't be at the same address. */
                                ++counter;

                            /* Found. */
                            if (std::find(ast_ends.begin(), ast_ends.end(), on) != ast_ends.end())
                                if (counter)
                                    --counter;
                                else
                                    ast_ends.erase(std::remove(ast_ends.begin(), ast_ends.end(), on), ast_ends.end());

                            on += i->size;
                        }

                    }

                    /* If there is no logical end and next is load b with jump and doesnt exceed bytecode. *This should be resolved by ast but we do it again 2 make code simpler. */
                    if (logical_operation_end == -1 && (iter + 1u) < bytecode_size && decode_opcode(decom_info->proto.code[(iter + 1u)]) == OP_LOADB && decode_C(decom_info->proto.code[(iter + 1u)]))
                    {

                        /* Place holder for now will be used for something in the future. */
                        const auto next_reg = decode_A(decom_info->proto.code[(iter + 1u)]);
                        add_missing(registers, next_reg);

                        const auto& dest_reg = registers[next_reg];

                        /* Format buffer. */
                        if (expression_buffer.front() != '(')
                            expression_buffer = "( " + expression_buffer + " )";

                        dest_reg->container = expression_buffer;
                        dest_reg->type = __type::express;
                        dest_reg->expression_size = 0u;


                    }
                    else
                    {
                        /* Do opposite on curr because we go through current branch. Makes it easier to structure it this way. */
                        helper_functions::do_branch(write, expression_buffer, ast, original_iter, inside_branch, inside_loop, is_break);
                    }

                    expression_buffer.clear();

                }

                /* Just say it's end even thought it could be any end of branch routine. */
                /* Will get reset back if needed. */
                inside_elseif = false;

                /* Reset. */
                is_break = false;
            }


            /* Check for reapeat until routine. */
            /* Find if next instruction loops back also we do false for template compare because how this decompiler is structered. */
            if (inside_loop && decode_opcode(decom_info->proto.code[iter + 1u]) == OP_JUMPBACK)
            {
                loop_append = expression_buffer;
                expression_buffer.clear();
            }

            /* While loop end so write. */
            if (std::find(while_final_branch.begin(), while_final_branch.end(), original_iter) != while_final_branch.end())
            {
                write += "while ( " + expression_buffer + " ) do\n";
                while_final_branch.erase(std::remove(while_final_branch.begin(), while_final_branch.end(), original_iter), while_final_branch.end());
                expression_buffer.clear();
            }

            break;
        }
        case OP_JUMPIFNOT:
        case OP_JUMPIF: {

            auto cached_buffer = std::string(expression_buffer);
            const auto& reg = registers[decode_A(instruction_data)];
            const auto len = decode_D(instruction_data); /* Jump lenght */

            /* Must but not and no logical operation for it or while end to be not. */
            auto data = (curr_opcode == OP_JUMPIFNOT && logical_operation_end == -1 && std::find(while_final_branch.begin(), while_final_branch.end(), original_iter) == while_final_branch.end()) ? "not " + reg->container : reg->container;

            /* Check for break. */
            if (!is_break)
                for (const auto& i : ast_dedicated_loops)
                    if (std::find(i.second.begin(), i.second.end(), (iter + len + 1u)) != i.second.end()) /* Don't include inc to hit jumpback. */
                        is_break = true;


            /* Set ands and ors for our branch. */
            if (ast->branch_comparisons.find(iter) != ast->branch_comparisons.end())
            {

                auto cmpp = (curr_opcode == OP_JUMPIFNOT) ? std::string("not ") : std::string("");
                auto cmpp_opo = (curr_opcode != OP_JUMPIFNOT) ? std::string("not ") : std::string("");

                /* Always follow with break. */
                if (is_break)
                {
                    cmpp = helper_functions::get_compare<false>(curr_opcode);
                    cmpp_opo = cmpp;
                }

                const auto ptr = ast->branch_comparisons[iter];

                /* Format on runtime. */

                switch (ptr->i)
                {

                case branch_data_e::or_close:
                case branch_data_e::close:
                case branch_data_e::and_close: {

                    expression_buffer += (cmpp + reg->container + " )");

                    break;
                }

                case branch_data_e::and_: {

                    expression_buffer += (cmpp_opo + reg->container + " and ");

                    break;
                }

                case branch_data_e::and_open: {

                    expression_buffer += ("( " + cmpp_opo + reg->container + " and ");

                    break;
                }

                case branch_data_e::or_: {

                    expression_buffer += (cmpp + reg->container + " or ");

                    break;
                }

                case branch_data_e::or_open: {

                    expression_buffer += ("( " + cmpp + reg->container + " or ");

                    break;
                }

                case branch_data_e::none: {

                    expression_buffer += cmpp + reg->container;

                    break;
                }

                default: {
                    break;
                }

                }

            }
            else
            {

                if (is_break)
                {
                    expression_buffer += (helper_functions::get_compare<false>(curr_opcode) + reg->container);
                }
                else
                {
                    /* Write to buffer. */
                    expression_buffer += ((ast->extra_branches.find(original_iter) != ast->extra_branches.end()) ? ((std::strlen(ast->extra_branches[original_iter]) ? std::string(ast->extra_branches[original_iter]) + ' ' : std::string("")) + reg->container) : data);
                }

                /* Logical operation will get parsed later. Also cant be loop compare end. And cant be forming any while loops.  */
                /* Indication of end of branch routine. */
                if (logical_operation_end == -1 && !generic_loop_end && !while_final_branch.size())
                {

                    /* If there is a break we need to remove need to remove end from cache. */
                    if (is_break)
                    {

                        auto counter = 0u;
                        auto on = (original_iter + dissassemble(decom_info, original_iter)->size);
                        const auto dis = dissassemble_range(decom_info, on, bytecode_size);

                        for (const auto& i : dis)
                        {

                            /* Check if its an if. */
                            if (i->is_comparative && i->basic == basic_info::branch && ast_branches.find(on) != ast_branches.end() && ast_branches[on].front() == type_branch::if_) /* No multiple ifs at the same address. */
                                ++counter;

                            /* Found. */
                            if (std::find(ast_ends.begin(), ast_ends.end(), on) != ast_ends.end())
                                if (counter)
                                    --counter;
                                else
                                    ast_ends.erase(std::remove(ast_ends.begin(), ast_ends.end(), on), ast_ends.end());

                            on += i->size;

                        }

                    }

                    /* Do opposite on curr because we go through current branch. Makes it easier to structure it this way. */
                    helper_functions::do_branch(write, expression_buffer, ast, original_iter, inside_branch, inside_loop, is_break);
                    expression_buffer.clear();

                }

                /* Just say it's end even thought it could be any end of branch routine. */
                /* Will get reset back if needed. */
                inside_elseif = false;

                /* Reset. */
                is_break = false;
            }

            /* Check for reapeat until routine. */
            /* Find if next instruction loops back also we do false for template compare because how this decompiler is structered. */
            if (inside_loop && decode_opcode(decom_info->proto.code[iter + 1u]) == OP_JUMPBACK)
            {
                loop_append = expression_buffer;
                expression_buffer.clear();
            }

            /* While loop end so write. */
            if (std::find(while_final_branch.begin(), while_final_branch.end(), original_iter) != while_final_branch.end())
            {
                /* Do opposite for last compare. */
                write += "while ( " + cached_buffer + ((curr_opcode != OP_JUMPIFNOT) ? "not " : "") + reg->container + " ) do\n";
                while_final_branch.erase(std::remove(while_final_branch.begin(), while_final_branch.end(), original_iter), while_final_branch.end());
                expression_buffer.clear();
            }

            break;
        }
        case OP_JUMPBACK: {

            if (std::find(while_end.begin(), while_end.end(), original_iter) != while_end.end())
            {

                /* Write end cuz its while loop. */
                write += "end\n";

            }
            else
            {

                /* Has ending to loop.*/
                const auto& prev_inst = dissassembly[dissassembly_location - 1u];


                if (prev_inst->basic == basic_info::branch && (prev_inst->is_comparative || prev_inst->opcode == OP_JUMP))
                {

                    write += "until ( " + loop_append + " );\n";

                }
                else
                {

                    /* Never ending loop. */
                    write += "end\n";

                }

            }

            expression_buffer.clear();
            --inside_loop;

            /* Place parent scope registers. */
            registers = branch_regs[branch_regx--];
            branch_regs.erase(branch_regx + 1u);

            break;
        }
        case OP_JUMPX:
        case OP_JUMP: {

            const auto len = decode_D(instruction_data); /* Jump lenght */

            if (ast_branches.find(original_iter) != ast_branches.end() && ast_branches[original_iter].front() == type_branch::else_)
                write += "else\n";


            /* Check for break. */
            if (!is_break)
                for (const auto& i : ast_dedicated_loops)
                    if (std::find(i.second.begin(), i.second.end(), (iter + len)) != i.second.end())
                        write += "break;\n";

            break;
        }


        case OP_DUPTABLE: {

            const auto reg = decode_A(instruction_data);
            add_missing(registers, reg);

            const auto& dest_reg = registers[reg];
            const auto table_end = table::end(decom_info, iter, bytecode_size);

            /* No end of table so make new one. */
            if (!end_table)
            {
                end_table = ast_tables[iter];
                dest_reg->container = "{ ";
            }
            else
            {

                dest_reg->container += " { ";

                if (end_table_counter.find(table_end) == end_table_counter.end())
                    end_table_counter.insert(std::make_pair(table_end, 1u)); /* Wil only invoke if value is greater than 1. */
                else
                    ++end_table_counter[table_end];

            }

            dest_reg->def = definition::table;

            /* Get table size manually because tables analysis for ast only analyze parent not sub tables those should be resolved on decompilation time to improve accuracy. */
            tables.emplace_back(std::make_pair(table_end, reg));

            ++table_count;
            break;
        }



        case OP_MOVE: {

            const auto dest = decode_A(instruction_data);
            const auto source = decode_B(instruction_data);

            add_missing(registers, dest);

            const auto& dest_reg = registers[dest];
            const auto& source_reg = registers[source];


            /* Is marked. */
            if (source_reg->marked_iter)
            {
                dest_reg->container = source_reg->container;
                dest_reg->expression_size = 0u;
                dest_reg->type = __type::var;
                break;
            }

            /* Variable */
            if (loc_var != nullptr && loc_var_pos == original_iter && !loc_var->in_table)
            {

                const auto is_emp = loc_var->name.empty();
                const auto name = ((is_emp) ? (variable_prefix + std::to_string(variable_suffix)) : loc_var->name);

                dest_reg->container = name;
                dest_reg->expression_size = 0u;
                dest_reg->type = __type::var;

                write += ((is_emp) ? form_var(variable_prefix, variable_suffix) : "local " + name + " = ") + source_reg->container + ";\n";

                if (is_emp)
                    ++variable_suffix;

            }
            else
            {

                /* Assignment */
                if (dest_reg->type == __type::var || dest_reg->type == __type::arg)
                    write += dest_reg->container + " = " + source_reg->container + "; \n";

                else
                {

                    /* Expression */
                    dest_reg->container = source_reg->container;
                    dest_reg->type = __type::express;
                    dest_reg->expression_size = 0u;

                }

            }


            break;
        }

        case OP_CONCAT: {

            const auto dest = decode_A(instruction_data);
            const auto source = decode_B(instruction_data);
            const auto val = decode_C(instruction_data);

            add_missing(registers, dest);

            const auto& dest_reg = registers[dest];


            std::string data = "";

            /* Form data. */
            for (auto i = source; i <= val; ++i)
                data += registers[i]->container + ((i != val) ? " .. " : "");


            /* Variable */
            if (loc_var != nullptr && loc_var_pos == original_iter && !loc_var->in_table)
            {

                const auto is_emp = loc_var->name.empty();
                const auto name = ((is_emp) ? (variable_prefix + std::to_string(variable_suffix)) : loc_var->name);

                dest_reg->container = name;
                dest_reg->expression_size = 0u;
                dest_reg->type = __type::var;

                write += ((is_emp) ? form_var(variable_prefix, variable_suffix) : "local " + name + " = ") + data + ";\n";

                if (is_emp)
                    ++variable_suffix;

            }
            else
            {

                /* Assignment */
                if (dest_reg->type == __type::var || dest_reg->type == __type::arg)
                    write += dest_reg->container + " = " + data + "; \n";

                else
                {

                    /* Expression */
                    dest_reg->container = data;
                    dest_reg->type = __type::express;
                    dest_reg->expression_size = 0u;

                }

            }


            break;
        }

                      /* Call. */
        case OP_CALL: {



            std::string compiled_decom = "";

            const auto calling = decode_A(instruction_data);
            const auto calling_reg = registers[calling]->container;
            const auto& dest_reg = registers[calling];


            auto args = (decode_B(signed(instruction_data)) - 1);
            const auto retns = (std::int32_t(decode_C(instruction_data)) - 1);


            /* Fix args if its multi. */
            if (args == -1)
                args = (last_dest - calling);



            /* Format. */

            compiled_decom += calling_reg + "( ";

            std::uint32_t primitive_arg_count = 0u;

            /* Has args so parse them. */
            if (args)
                for (auto i = 0; i < args; ++i)
                {

                    /* Skip this with args. */
                    if (!i && decode_opcode(decom_info->proto.code[prev_iter]) == OP_NAMECALL)
                        continue;
                    else
                    {


                        ++primitive_arg_count;

                        const auto idx = i + 1u + calling;
                        const auto& str = registers[idx]->container;

                        compiled_decom += str;

                        if ((i + 1) != args)
                            compiled_decom += ", ";

                    }

                }

            /* Has return. */
            if (retns)
            {

                /* Variable */
                if (loc_var != nullptr && loc_var_pos == original_iter && !loc_var->in_table)
                {

                    const auto is_emp = loc_var->name.empty();
                    const auto name = ((is_emp) ? (variable_prefix + std::to_string(variable_suffix)) : loc_var->name);

                    dest_reg->container = name;
                    dest_reg->expression_size = 0u;
                    dest_reg->type = __type::var;

                    write += ((is_emp) ? form_var(variable_prefix, variable_suffix) : "local " + name + " = ") + compiled_decom + " );\n";

                    if (is_emp)
                        ++variable_suffix;

                }
                else
                {

                    /* Assignment */
                    if (dest_reg->type == __type::var || dest_reg->type == __type::arg)
                        write += dest_reg->container + " = " + compiled_decom + " );\n";

                    else
                    {

                        /* Expression */
                        dest_reg->container = compiled_decom + " )";
                        dest_reg->type = __type::express;
                        dest_reg->expression_size = 0u;

                    }

                }


                /* Fill */
                if (retns > 0)
                    for (auto i = 0; i < (retns - 1); ++i)
                    {  /* Fill with nil. */

/* Make sure target reg exists. */
                        const auto target = (i + calling + 1u);
                        if (registers.find(target) != registers.end())
                        {
                            const auto& dest_reg = registers[target];
                            dest_reg->container = "nil";
                            dest_reg->expression_size = 0u;
                            dest_reg->type = __type::express;
                        }
                        else
                            break;

                    }


            }
            else /* No return write output. */
                write += compiled_decom + " );\n";



            break;
        }
        case OP_NAMECALL: {


            const auto dest = decode_A(instruction_data);
            const auto source = decode_B(std::int32_t(instruction_data));
            add_missing(registers, dest);


            const auto& dest_reg = registers[dest];
            const auto& source_reg = registers[source];
            const auto& source_k_val = kval[bytecode[++iter]];

            const auto data = (source_reg->container + ':' + source_k_val);



            /* Variable */
            if (loc_var != nullptr && loc_var_pos == original_iter && !loc_var->in_table)
            {

                const auto is_emp = loc_var->name.empty();
                const auto name = ((is_emp) ? (variable_prefix + std::to_string(variable_suffix)) : loc_var->name);

                dest_reg->container = name;
                dest_reg->expression_size = 0u;
                dest_reg->type = __type::var;

                write += ((is_emp) ? form_var(variable_prefix, variable_suffix) : "local " + name + " = ") + data + ";\n";

                if (is_emp)
                    ++variable_suffix;

            }
            else
            {

                /* Assignment */
                if (dest_reg->type == __type::var || dest_reg->type == __type::arg)
                    write += dest_reg->container + " = " + data + ";\n";

                else
                {

                    /* Expression */
                    dest_reg->container = data;
                    dest_reg->type = __type::express;
                    ++dest_reg->expression_size;

                }

            }


            break;
        }






                        /* Arith operations and, "and", "or". */
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

            /* Opcode is constant so we can look at constant table. */
            const auto constant = helper_functions::is_arith_constant(std::uint16_t(curr_opcode));

            const auto dest = decode_A(instruction_data);
            const auto source_1 = decode_B(instruction_data);
            const auto source_2 = decode_C(instruction_data);

            const auto operation = helper_functions::do_arith<false>(std::uint16_t(curr_opcode));


            add_missing(registers, dest);
            const auto& dest_reg = registers[dest];
            auto source_1_reg = registers[source_1]->container;
            auto source_2_reg = ((constant) ? kval[source_2] : registers[source_2]->container);


            std::string concat = "";




            /* Arith concatation. */

            const auto last_opcode = dissassembly[dissassembly_location + 1]->opcode;

            /* Turn  curr arith and last arith to normal. */
            const auto next_arithn = ((helper_functions::is_arith_constant(curr_opcode)) ? helper_functions::arithk_normal(curr_opcode) : curr_opcode);
            const auto last_arithn = ((helper_functions::is_arith_constant(last_opcode)) ? helper_functions::arithk_normal(last_opcode) : last_opcode);


            /* New arith operation so put it parenthesis. */
            if (next_arithn != last_arithn && dissassembly[dissassembly_location + 1]->basic == basic_info::math)
            {
                source_1_reg.insert(source_1_reg.begin(), '(');
                source_2_reg.insert(source_2_reg.end(), ')');
            }


            /* Full operation *Doesn't include assignment we determine that in later code. */
            concat = (source_1_reg + operation + source_2_reg);


            /* Variable */
            if (loc_var != nullptr && loc_var_pos == original_iter && !loc_var->in_table)
            {

                const auto is_emp = loc_var->name.empty();
                const auto name = ((is_emp) ? (variable_prefix + std::to_string(variable_suffix)) : loc_var->name);

                dest_reg->container = name;
                dest_reg->expression_size = 0u;
                dest_reg->type = __type::var;

                write += ((is_emp) ? form_var(variable_prefix, variable_suffix) : "local " + name + " = ") + '(' + concat + ");\n";

                if (is_emp)
                    ++variable_suffix;

            }
            else
            {

                /* Assignment */
                if (dest_reg->type == __type::var || dest_reg->type == __type::arg)
                {

                    /* Do assignment if dest and source regs are the same. Can't be or, and. */
                    if (dest == source_1 && (curr_opcode != OP_AND && curr_opcode != OP_ANDK && curr_opcode != OP_OR && curr_opcode != OP_ORK))
                        write += dest_reg->container + helper_functions::do_arith<true>(curr_opcode) + source_2_reg + "; \n";
                    else
                        write += dest_reg->container + " = (" + concat + "); \n";

                }
                else
                {

                    /* Expression */
                    dest_reg->container = concat;
                    dest_reg->type = __type::express;
                    dest_reg->expression_size = 0u;

                }

            }


            break;
        }



                  /* Loops prep. */
        case OP_FORGPREP:
        case OP_FORNPREP:
        case OP_FORGPREP_NEXT:
        case OP_FORGPREP_INEXT: {


            /* These are determined by their loops not there preps as preps arent garunteed. */


            break;
        }

                              /* Loops end. */
        case OP_FORGLOOP_INEXT:
        case OP_FORGLOOP_NEXT:
        case OP_FORGLOOP:
        case OP_FORNLOOP: {

            write += "end\n";

            if (curr_opcode == OP_FORGLOOP) /* Skip var encoding. */
                ++iter;


            registers = branch_regs[branch_regx--];
            branch_regs.erase(branch_regx + 1u);

            break;
        }



                        /* Globals. */
        case OP_SETGLOBAL: {


            const auto& value = registers[decode_A(instruction_data)]->container;
            const auto& key = decom_info->proto.code[++iter];


            write += kval[key] + " = " + value + ";\n";

            break;
        }
        case OP_GETGLOBAL: {

            const auto value = decode_A(instruction_data);
            add_missing(registers, value);

            const auto& key = decom_info->proto.code[++iter];

            const auto& dest_reg = registers[value];


            /* Variable */
            if (loc_var_pos == original_iter)
            {

                const auto is_emp = loc_var->name.empty();
                const auto name = ((is_emp) ? (variable_prefix + std::to_string(variable_suffix)) : loc_var->name);

                dest_reg->container = name;
                dest_reg->expression_size = 0u;
                dest_reg->type = __type::var;

                write += ((is_emp) ? form_var(variable_prefix, variable_suffix) : "local " + name + " = ") + kval[key] + ";\n";

                if (is_emp)
                    ++variable_suffix;

            }
            else
            {

                /* Assignment */
                if (dest_reg->type == __type::var || dest_reg->type == __type::arg)
                {

                    /* Same equal don't write. */
                    if (dest_reg->container == kval[key])
                    {
                        dest_reg->type = __type::express;
                        break;
                    }

                    write += dest_reg->container + " = " + kval[key] + ";\n";
                }
                else
                {

                    /* Expression */
                    dest_reg->container = kval[key];
                    dest_reg->type = __type::express;
                    ++dest_reg->expression_size;

                }

            }

            break;
        }



                         /* Table get/set. */
        case OP_GETTABLE: {

            const auto dest = decode_A(instruction_data);
            add_missing(registers, dest);

            const auto& dest_reg = registers[dest];

            const auto& source_1 = registers[decode_B(std::int32_t(instruction_data))]->container;
            const auto& source_2 = registers[decode_C(std::int32_t(instruction_data))]->container;
            const auto data = source_1 + '[' + source_2 + ']';


            /* Variable */
            if (loc_var != nullptr && loc_var_pos == original_iter && !loc_var->in_table)
            {

                const auto is_emp = loc_var->name.empty();
                const auto name = ((is_emp) ? (variable_prefix + std::to_string(variable_suffix)) : loc_var->name);

                dest_reg->container = name;
                dest_reg->expression_size = 0u;
                dest_reg->type = __type::var;

                write += ((is_emp) ? form_var(variable_prefix, variable_suffix) : "local " + name + " = ") + data + ";\n";

                if (is_emp)
                    ++variable_suffix;

            }
            else
            {

                /* Assignment */
                if (dest_reg->type == __type::var || dest_reg->type == __type::arg)
                    write += dest_reg->container + " = " + data + ";\n";

                else
                {

                    /* Expression */
                    dest_reg->container = data;
                    dest_reg->type = __type::express;
                    ++dest_reg->expression_size;

                }

            }


            break;
        }
        case OP_GETTABLEN: {

            const auto dest = decode_A(instruction_data);
            add_missing(registers, dest);

            const auto& dest_reg = registers[dest];

            const auto& source_1 = registers[decode_B(std::int32_t(instruction_data))]->container;
            const auto source_2 = (decode_C(std::int32_t(instruction_data)) + 1u);
            const auto data = source_1 + '[' + std::to_string(source_2) + ']';


            /* Variable */
            if (loc_var != nullptr && loc_var_pos == original_iter && !loc_var->in_table)
            {

                const auto is_emp = loc_var->name.empty();
                const auto name = ((is_emp) ? (variable_prefix + std::to_string(variable_suffix)) : loc_var->name);

                dest_reg->container = name;
                dest_reg->expression_size = 0u;
                dest_reg->type = __type::var;

                write += ((is_emp) ? form_var(variable_prefix, variable_suffix) : "local " + name + " = ") + data + ";\n";

                if (is_emp)
                    ++variable_suffix;

            }
            else
            {

                /* Assignment */
                if (dest_reg->type == __type::var || dest_reg->type == __type::arg)
                    write += dest_reg->container + " = " + data + ";\n";

                else
                {

                    /* Expression */
                    dest_reg->container = data;
                    dest_reg->type = __type::express;
                    ++dest_reg->expression_size;

                }

            }


            break;
        }
        case OP_GETTABLEKS: {

            const auto dest = decode_A(instruction_data);
            add_missing(registers, dest);

            const auto& dest_reg = registers[dest];

            const auto& source_1 = registers[decode_B(std::int32_t(instruction_data))]->container;
            const auto source_2 = bytecode[++iter];

            /* Index is legal *index = int */
            const auto legal = helper_functions::legal_index(kval[source_2]);
            const auto data = ((legal) ? (source_1 + '.' + kval[source_2]) : (source_1 + "[\"" + kval[source_2] + "\"]"));


            /* Variable */
            if (loc_var != nullptr && loc_var_pos == original_iter && !loc_var->in_table)
            {

                const auto is_emp = loc_var->name.empty();
                const auto name = ((is_emp) ? (variable_prefix + std::to_string(variable_suffix)) : loc_var->name);

                dest_reg->container = name;
                dest_reg->expression_size = 0u;
                dest_reg->type = __type::var;

                write += ((is_emp) ? form_var(variable_prefix, variable_suffix) : "local " + name + " = ") + data + ";\n";

                if (is_emp)
                    ++variable_suffix;

            }
            else
            {

                /* Assignment */
                if (dest_reg->type == __type::var || dest_reg->type == __type::arg)
                    write += dest_reg->container + " = " + data + ";\n";

                else
                {

                    /* Expression */
                    dest_reg->container = data;
                    dest_reg->type = __type::express;
                    ++dest_reg->expression_size;

                }

            }

            break;
        }
        case OP_SETTABLEKS: {


            const auto dest = decode_A(instruction_data);
            add_missing(registers, dest);

            const auto& dest_reg = registers[dest];
            const auto& table_register = registers[decode_B(std::int32_t(instruction_data))];

            auto& source_1 = table_register->container;
            const auto& source_2 = bytecode[++iter];


            /* Index is int? */
            const auto legal = helper_functions::legal_index(kval[source_2]);

            if (end_table)
            {

                /* Format and add. */
                source_1 += kval[source_2] + " = " + dest_reg->container;

                /* Check for end if so do standard locvar routine.*/
                if (end_table == original_iter)
                {

                    source_1 += " }";

                    const auto& table_dest_reg = registers[decode_B(std::int32_t(instruction_data))];

                    /* See if variable. */
                    if (loc_var != nullptr && loc_var_pos == original_iter)
                    {

                        const auto is_emp = loc_var->name.empty();
                        const auto name = ((is_emp) ? (variable_prefix + std::to_string(variable_suffix)) : loc_var->name);

                        write += ((is_emp) ? form_var(variable_prefix, variable_suffix) : "local " + name + " = ") + source_1 + ";\n";

                        table_dest_reg->container = name;
                        table_dest_reg->expression_size = 0u;
                        table_dest_reg->type = __type::var;

                        if (is_emp)
                            ++variable_suffix;

                    }

                    end_table = 0u;
                }
                else
                {
                    /* Check for ends. */
                    auto t_end = false;
                    for (const auto& i : tables)
                        if (i.first == original_iter)
                            t_end = true;

                    /* Place end if end. */
                    if (t_end)
                        source_1 += " }";
                    else
                        source_1 += ", ";
                }

            }
            else
            {
                /* Isnt expression and aint a table so place. */
                if (table_register->def != definition::table || table_register->type != __type::express)
                    write += ((legal) ? (source_1 + '.' + kval[source_2]) : (source_1 + "[\"" + kval[source_2] + "\"]")) + " = " + registers[dest]->container + ";\n";
                else
                {
                    source_1 += kval[source_2] + " = " + dest_reg->container;

                    /* Place in reg and end. */
                    auto t_end = false;
                    for (const auto& i : tables)
                        if (i.first == original_iter)
                            t_end = true;

                    /* Place end if end. */
                    if (t_end)
                        source_1 += " }";
                    else
                        source_1 += ", ";
                }
            }
            break;
        }
        case OP_SETTABLEN: {

            const auto dest = decode_A(instruction_data);
            add_missing(registers, dest);

            const auto& dest_reg = registers[dest];
            const auto& table_register = registers[decode_B(std::int32_t(instruction_data))];

            auto& source_1 = table_register->container;
            const auto source_2 = (decode_C(std::int32_t(instruction_data)) + 1u);

            if (end_table)
            {

                /* Format and add. */
                source_1 += " [" + std::to_string(source_2) + "] = " + dest_reg->container;

                /* Check for end if so do standard locvar routine.*/
                if (end_table == original_iter)
                {

                    source_1 += " }";
                    const auto& table_dest_reg = registers[decode_B(std::int32_t(instruction_data))];

                    /* See if variable. */
                    if (loc_var != nullptr && loc_var_pos == original_iter)
                    {

                        const auto is_emp = loc_var->name.empty();
                        const auto name = ((is_emp) ? (variable_prefix + std::to_string(variable_suffix)) : loc_var->name);

                        write += ((is_emp) ? form_var(variable_prefix, variable_suffix) : "local " + name + " = ") + source_1 + ";\n";

                        table_dest_reg->container = name;
                        table_dest_reg->expression_size = 0u;
                        table_dest_reg->type = __type::var;

                        if (is_emp)
                            ++variable_suffix;

                    }

                    end_table = 0u;
                }
                else
                {

                    /* Check for ends. */
                    auto t_end = false;
                    for (const auto& i : tables)
                        if (i.first == original_iter)
                            t_end = true;

                    /* Place end if end. */
                    if (t_end)
                        source_1 += " }";
                    else
                    {
                        if (end_table_counter.find(original_iter) != end_table_counter.end())
                        {
                            const auto counter = end_table_counter[original_iter];
                            const auto counter_sub = counter - 1u;

                            for (auto c = 0u; c < counter_sub; ++c)
                                source_1 += " }";

                            source_1 += " }, ";
                        }
                        else
                            source_1 += ", ";

                    }

                }

                dest_reg->container.clear(); /* Clear for other stuff. */
            }
            else
            {

                /* Isnt expression and aint a table so place. */
                if (table_register->def != definition::table || table_register->type != __type::express)
                    write += source_1 + '[' + std::to_string(source_2) + "]  = " + dest_reg->container + ";\n";
                else
                {

                    source_1 += source_1 + '[' + std::to_string(source_2) + "]  = " + dest_reg->container;

                    /* Place in reg and end. */
                    auto t_end = false;
                    for (const auto& i : tables)
                        if (i.first == original_iter)
                            t_end = true;

                    /* Place end if end. */
                    if (t_end)
                        source_1 += " }";
                    else
                        source_1 += ", ";
                }

            }

            break;
        }
        case OP_SETTABLE: {
            const auto dest = decode_A(instruction_data);
            add_missing(registers, dest);

            const auto& dest_reg = registers[dest];
            const auto& table_register = registers[decode_B(std::int32_t(instruction_data))];

            auto& source_1 = table_register->container;
            const auto& source_2 = registers[decode_C(std::int32_t(instruction_data))]->container;

            /* Table index or variable if number = [100] = ... else abc = ... */
            if (end_table)
            {
                /* Format and add. */
                source_1 += '[' + source_2 + ']' + " = " + registers[dest]->container;

                /* Check for end if so do standard locvar routine.*/
                if (end_table == original_iter)
                {
                    source_1 += " }";
                    const auto& table_dest_reg = registers[decode_B(std::int32_t(instruction_data))];

                    /* See if variable. */
                    if (loc_var != nullptr && loc_var_pos == original_iter)
                    {

                        const auto is_emp = loc_var->name.empty();
                        const auto name = ((is_emp) ? (variable_prefix + std::to_string(variable_suffix)) : loc_var->name);

                        write += ((is_emp) ? form_var(variable_prefix, variable_suffix) : "local " + name + " = ") + source_1 + ";\n";

                        table_dest_reg->container = name;
                        table_dest_reg->expression_size = 0u;
                        table_dest_reg->type = __type::var;

                        if (is_emp)
                            ++variable_suffix;

                    }

                    end_table = 0u;
                }
                else
                {
                    /* Check for ends. */
                    auto t_end = false;
                    for (const auto& i : tables)
                        if (i.first == original_iter)
                            t_end = true;

                    /* Place end if end. */
                    if (t_end)
                        source_1 += " }";
                    else
                        source_1 += ", ";

                }
            }
            else
            {
                /* Isnt expression and aint a table so place. */
                if (table_register->def != definition::table || table_register->type != __type::express)
                    write += source_1 + '[' + source_2 + "] = " + registers[dest]->container + ";\n";
                else
                {
                    source_1 += source_1 + '[' + source_2 + "] = " + registers[dest]->container;

                    /* Place in reg and end. */
                    auto t_end = false;
                    for (const auto& i : tables)
                        if (i.first == original_iter)
                            t_end = true;

                    /* Place end if end. */
                    if (t_end)
                        source_1 += " }";
                    else
                        source_1 += ", ";

                }
            }

            break;
        }

        /* Table misc. */
        case OP_LENGTH: {
            const auto dest = decode_A(instruction_data);
            add_missing(registers, dest);

            const auto source = decode_B(std::int32_t(instruction_data));

            const auto& dest_reg = registers[dest];
            const auto data = '#' + registers[source]->container;

            /* Variable */
            if (loc_var != nullptr && loc_var_pos == original_iter && !loc_var->in_table)
            {
                const auto is_emp = loc_var->name.empty();
                const auto name = ((is_emp) ? (variable_prefix + std::to_string(variable_suffix)) : loc_var->name);

                dest_reg->container = name;
                dest_reg->expression_size = 0u;
                dest_reg->type = __type::var;

                write += ((is_emp) ? form_var(variable_prefix, variable_suffix) : "local " + name + " = ") + data + ";\n";

                if (is_emp)
                    ++variable_suffix;
            }
            else
            {
                /* Assignment */
                if (dest_reg->type == __type::var || dest_reg->type == __type::arg)
                    write += dest_reg->container + " = " + data + ";\n";
                else
                {

                    /* Expression */
                    dest_reg->container = data;
                    dest_reg->type = __type::express;
                    ++dest_reg->expression_size;

                }
            }

            break;
        }
        case OP_NEWTABLE: {
            /* No end of table so make new one. */
            const auto reg = decode_A(instruction_data);
            add_missing(registers, reg);

            const auto& dest_reg = registers[reg];
            const auto table_end = table::end(decom_info, original_iter, bytecode_size);

            dest_reg->def = definition::table;

            /* Empty table or has items but emblaces at end. */
            if (table_end == original_iter || (loc_var != nullptr && loc_var_pos == original_iter && !loc_var->in_table))
            {
                const auto data = "{ }";

                /* Variable */
                if (loc_var != nullptr && loc_var_pos == original_iter && !loc_var->in_table)
                {
                    const auto is_emp = loc_var->name.empty();
                    const auto name = ((is_emp) ? (variable_prefix + std::to_string(variable_suffix)) : loc_var->name);

                    dest_reg->container = name;
                    dest_reg->expression_size = 0u;
                    dest_reg->type = __type::var;

                    write += ((is_emp) ? form_var(variable_prefix, variable_suffix) : "local " + name + " = ") + data + ";\n";

                    if (is_emp)
                        ++variable_suffix;
                }
                else
                {
                    /* Assignment */
                    if (dest_reg->type == __type::var || dest_reg->type == __type::arg)
                        write += dest_reg->container + " = " + data + ";\n";
                    else
                    {

                        /* Expression */
                        dest_reg->container = data;
                        dest_reg->type = __type::express;
                        ++dest_reg->expression_size;

                    }
                }
            }
            else
            {
                if (!end_table)
                {
                    end_table = ast_tables[iter];
                    dest_reg->container = "{ ";
                }
                else
                {
                    dest_reg->container = "{ ";

                    if (end_table_counter.find(table_end) == end_table_counter.end())
                        end_table_counter.insert(std::make_pair(table_end, 1u)); /* Wil only invoke if value is greater than 1. */
                    else
                        ++end_table_counter[table_end];
                }

                /* Get table size manually because tables analysis for ast only analyze parent not sub tables those should be resolved on decompilation time to improve accuracy. */
                tables.emplace_back(std::make_pair(table_end, reg));

                ++table_count;
            }
            ++iter;

            break;
        }
        case OP_SETLIST: {
            const auto dest = decode_A(instruction_data);
            const auto start = decode_B(instruction_data);
            add_missing(registers, dest);

            const auto& dest_reg = registers[dest];
            const auto aux = decom_info->proto.code[++iter];

            /* Amount we need to iterate starting from dest to concat table. */
            auto amt = (decode_C(std::int32_t(instruction_data)) - 1);
            if (amt == -1)
                amt = last_dest;

            --table_count;

            auto& data = dest_reg->container;

            for (auto i = 0u; i < unsigned(amt); ++i)
                data += registers[start + i]->container + (((i + 1u) == amt) ? " }" : ", ");

            /* End of table routine. */
            if (end_table == original_iter)
            {
                /* Variable */
                if (loc_var != nullptr && loc_var_pos == original_iter && !loc_var->in_table)
                {
                    const auto is_emp = loc_var->name.empty();
                    const auto name = ((is_emp) ? (variable_prefix + std::to_string(variable_suffix)) : loc_var->name);

                    write += ((is_emp) ? form_var(variable_prefix, variable_suffix) : "local " + name + " = ") + data + ";\n";

                    dest_reg->container = name;
                    dest_reg->expression_size = 0u;
                    dest_reg->type = __type::var;

                    if (is_emp)
                        ++variable_suffix;
                }
                else
                {
                    /* Assignment */
                    if (dest_reg->type == __type::var || dest_reg->type == __type::arg)
                        write += dest_reg->container + " = " + data + ";\n";

                    else
                    {

                        /* Expression */
                        dest_reg->container = data;
                        dest_reg->type = __type::express;
                        ++dest_reg->expression_size;

                    }
                }
                end_table = 0u;
            }

            break;
        }

        /* Fastcall */
        case OP_FASTCALL:
        case OP_FASTCALL1: {
            /* Fast call resolves itself so no need todo anything. */
            break;
        }
        case OP_FASTCALL2:
        case OP_FASTCALL2K: {
            /* Fast call resolves itself so no need todo anything. */
            ++iter;
            break;
        }

        /* Upvalues/Closure */
        case OP_CLOSEUPVALS: {

            const auto dest = decode_A(instruction_data);
            const auto size = registers.size();

            /* Clear */
            for (auto i = dest; i < size; ++i)
                if (registers.find(i) != registers.end() && registers[i]->is_upvalue)
                    registers[i]->clear();

            break;
        }
        case OP_DUPCLOSURE:
        case OP_NEWCLOSURE: {
            const auto dest = decode_A(instruction_data);
            add_missing(registers, dest);
            const auto& dest_reg = registers[dest];

            std::shared_ptr<decompile_info> proto_ptr;

            const auto proto_id = decode_D(instruction_data);

            /* Dupclosure or newclosure. */
            if (curr_opcode == OP_NEWCLOSURE)
                proto_ptr = decom_info->proto.p[proto_id];
            else
            {
                auto str = std::string(decom_info->proto.k[proto_id]);
                str.erase(str.find("func_"), (sizeof("func_") - 1u));
                const auto p_id = unsigned(std::stoi(str.c_str()) - 0);
                proto_ptr = ((decom_info->proto.p.size() <= p_id) ? (decom_info->proto.p.back()) : decom_info->proto.p[p_id]);
            }

            const auto& data = proto_ptr->proto.closure.data;

            /* Skip captures. */
            auto o = (iter + 1u);
            while (dissassemble(decom_info, o)->opcode == OP_CAPTURE)
                ++o;

            /* Set local etc. */
            dest_reg->expression_size = 0u;
            dest_reg->type = __type::var;

            if (proto_ptr->proto.closure.tt == closure_type::global)
                iter = (o + 1u); /* Skip setglobal. */

            /* Parse args. */
            std::string routine_args = "";
            for (auto o = 0u; o < proto_ptr->proto.closure.arg_count; ++o)
                routine_args += (((o + 1u) == proto_ptr->proto.closure.arg_count) ? (argument_prefix + std::to_string(o)) : (argument_prefix + std::to_string(o) + ", "));

            /* Function contains varargs. */
            if (proto_ptr->proto.closure.varargs)
                routine_args += ((proto_ptr->proto.closure.arg_count) ? ", ..." : "...");

            /* Always expression. */
            if (end_table)
            {
                dest_reg->container = "\n(function ( " + routine_args + " )\n" + data + "end)";
                dest_reg->type = __type::express;
            }
            else
            {
                /* Format function then write to decompilation. */
                switch (proto_ptr->proto.closure.tt)
                {

                case closure_type::local: {
                    /* Name is set pre decompilation in ast for closure. */
                    dest_reg->container = ((loc_var != nullptr && !loc_var->name.empty()) ? loc_var->name : (function_prefix + std::to_string(function_suffix)));
                    write += "\nlocal function " + dest_reg->container + " ( " + routine_args + " )\n" + data + "end\n\n";
                    ++function_suffix;
                    break;
                }

                case closure_type::global: {
                    dest_reg->type = __type::express;
                    dest_reg->container = proto_ptr->proto.closure.name;
                    write += "\nfunction " + dest_reg->container + " ( " + routine_args + " )\n" + data + "end\n\n";
                    break;
                }

                case closure_type::newclosure: {
                    dest_reg->container = "\n(function ( " + routine_args + " )\n" + data + "end)";
                    dest_reg->type = __type::express;
                    break;
                }

                default: {
                    throw std::runtime_error("Ast didn't capture a closure please check bytecode or code\n");
                }
                }

            }

            break;
        }
        case OP_CAPTURE: {

            /* Really nothing needed to add here. We already decided upvalues in earlier code. */
            break;
        }
        case OP_SETUPVAL: {
            const auto dest = decode_A(instruction_data);
            const auto source = decode_B(instruction_data);

            const auto& dest_reg = registers[dest];
            const auto& source_reg = registers[source];

            write += dest_reg->container + " = " + source_reg->container + ";\n";

            break;
        }
        case OP_GETUPVAL: {
            const auto dest = decode_A(instruction_data);
            add_missing(registers, dest);

            const auto& dest_reg = registers[dest];
            const auto data = decom_info->proto.upvalues[(decode_B(instruction_data))];

            /* Variable */
            if (loc_var != nullptr && loc_var_pos == original_iter && !loc_var->in_table)
            {
                const auto is_emp = loc_var->name.empty();
                const auto name = ((is_emp) ? (variable_prefix + std::to_string(variable_suffix)) : loc_var->name);

                dest_reg->container = name;
                dest_reg->expression_size = 0u;
                dest_reg->type = __type::var;

                write += ((is_emp) ? form_var(variable_prefix, variable_suffix) : "local " + name + " = ") + data + ";\n";

                if (is_emp)
                    ++variable_suffix;
            }
            else
            {
                /* Assignment */
                if (dest_reg->type == __type::var || dest_reg->type == __type::arg)
                    write += dest_reg->container + " = " + data + ";\n";

                else
                {
                    /* Expression */
                    dest_reg->container = data;
                    dest_reg->type = __type::express;
                    ++dest_reg->expression_size;
                }
            }

            break;
        }

        case OP_NOP: {
            break;
        }
        default: {
            break;
        }
        }

    end_parse:
#pragma region post_decompilation

        /* Cache previous. */
        prev_iter = original_iter;

        /* Set previous dest reg. */
        if (curr_opcode == OP_CAPTURE) /* Capture has int for dest. */
            last_dest = 0u;

        else if (iter != bytecode_size && (iter + 1u) != bytecode_size && dissassemble(decom_info, original_iter)->basic != basic_info::fast_call)
        { /* Ignore namecall followed by call. */
            const auto next_dism = dissassemble(decom_info, (iter + 1u));

            if (curr_opcode == OP_NAMECALL && dissassemble(decom_info, (iter + 1u))->opcode != OP_CALL) /* No namecall call */
                last_dest = decode_A(instruction_data);

            else if (next_dism->opcode != OP_CALL || decode_A(instruction_data) != next_dism->dest_reg) /* No getimp r1, k1; call r1, 1, 2 */
                last_dest = decode_A(instruction_data);
        }

        /* Set logical operation info. *Will also do if loadb and has jump. */
        if ((logical_operation_end == signed(original_iter) || (curr_opcode == OP_LOADB && decode_C(decom_info->proto.code[original_iter]))) && expression_buffer.size())
        {
            /* Set for next logical operation. */
            logical_operation_end = -1;

            const auto& dest_reg = registers[logical_operation_reg];
            auto data = "( " + expression_buffer + dest_reg->container + " )";

            /* Fix for loadb data. */
            if (curr_opcode == OP_LOADB && decode_C(decom_info->proto.code[original_iter]))
                data = "( " + expression_buffer + " )";

            dest_reg->type = logical_operation_type;

            /* Set arg or var back. */
            if (dest_reg->type == __type::var || dest_reg->type == __type::arg)
                dest_reg->container = logical_operation_data;

            if (logical_operation_locvar != nullptr && !logical_operation_locvar->in_table)
            {
                const auto is_emp = logical_operation_locvar->name.empty();
                const auto name = ((is_emp) ? (variable_prefix + std::to_string(variable_suffix)) : logical_operation_locvar->name);

                dest_reg->container = name;
                dest_reg->expression_size = 0u;
                dest_reg->type = __type::var;

                write += ((is_emp) ? form_var(variable_prefix, variable_suffix) : "local " + name + " = ") + data + ";\n";

                if (is_emp)
                    ++variable_suffix;

                loc_var = logical_operation_locvar;
            }
            else
            {
                /* Assignment */
                if (dest_reg->type == __type::var || dest_reg->type == __type::arg)
                    write += dest_reg->container + " = " + data + ";\n";

                else
                {
                    /* Expression */
                    dest_reg->container = data;
                    dest_reg->type = __type::express;
                    ++dest_reg->expression_size;
                }
            }

            expression_buffer.clear();
            logical_operation_locvar = nullptr;
            logical_operation_reg = 0u;
        }

        /* Next locvar. */
        if (iter >= loc_var_pos && loc_var != nullptr)
        {
            const auto nextloc = ((decom_info->proto.locvars.size() == loc_next_idx) ? nullptr : decom_info->proto.locvars[loc_next_idx]);
            loc_var = nextloc;
            loc_var_pos = ((nextloc != nullptr) ? nextloc->end : 0u);
            ++loc_next_idx;
        }

        /* Clear repeat, while if end. */
        if (original_iter == generic_loop_end)
            generic_loop_end = 0u;

        /* Reset scope for else if, else routine. */
        if (std::binary_search(ast_else_routine_begin.begin(), ast_else_routine_begin.end(), original_iter))
        {
            registers = branch_regs[branch_regx--];
            branch_regs.erase(branch_regx + 1u);
            branch_regs.insert(std::make_pair(++branch_regx, replicate_register(registers)));
        }

        /* Write registers. *Have to use original as iter could get mutated.  */
        if (ast_branches.find(original_iter) != ast_branches.end())
        {
            const auto& on = ast_branches[original_iter];

            /* For gets handled on runtime. *Theres a special retn for if, else, elseif. */
            if (std::find(on.begin(), on.end(), type_branch::elseif_) == on.end() &&
                std::find(on.begin(), on.end(), type_branch::else_) == on.end() &&
                std::find(on.begin(), on.end(), type_branch::for_) == on.end() &&
                std::find(on.begin(), on.end(), type_branch::if_) == on.end())
            {
                branch_regs.insert(std::make_pair(++branch_regx, replicate_register(registers)));
            }
        }

        /* Scopes for else, elseif, if. */
        if (std::binary_search(ast_branch_pos_scope_begin, ast_branch_pos_scope_end, original_iter))
            branch_regs.insert(std::make_pair(++branch_regx, replicate_register(registers)));

#pragma endregion
        ++dissassembly_location;
    }

#if CONFIG_DEBUG_ENABLE_PRINT_WRITE_PRE_BUETIFY
    std::cout << write << std::endl;
#endif

    /* Post-Optization */
    if (config->optimize)
        post_optization(write, config);

    /* Beautify. */
    finalize_decompilation(write);
    return;
}