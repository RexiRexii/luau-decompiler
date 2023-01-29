#pragma once
#include <string>

#define format_lenght 30  /* Amount of spacings between instruction and operands. */

/* Dissassembles bytecode and visualizes it into string*/
void dissassemble_textualize(std::shared_ptr<decompile_info>& decom_info, std::string& write);