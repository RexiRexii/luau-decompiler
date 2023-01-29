#pragma once
#include <string>
#include "config.hpp"

#if CONFIG_TESTING_BUILD == false

extern __declspec(dllexport) std::string luaU_decompile(const std::string& bytecode);

extern __declspec(dllexport) std::shared_ptr<decompile_info> build_decompiler_info(const std::string& bytecode, const std::size_t len);

#endif


