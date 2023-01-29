#pragma once
#define CONFIG_DEBUG_ENABLE true /* Enables debuging during decompilation */
#define CONFIG_DECRYPT false /* Decrypt certain values in code or somewhere else if there encypted. */
#define CONFIG_TESTING_BUILD true

#define CONFIG_DEBUG_LOAD_PROTO_INFO_PRINT false /* Prints basic proto info when loaded. */
#define CONFIG_DISABLE_DEBUG true /* Disables debug info from proto. */

#if CONFIG_DEBUG_ENABLE == true
#define CONFIG_DEBUG_ENABLE_REGISTERS false
#define CONFIG_DEBUG_ENABLE_BUFFER false
#define CONFIG_DEBUG_ENABLE_PRINT_WRITE_PRE_BUETIFY false
#define CONFIG_DEBUG_ENABLE_INDIVISUAL_REGISTER false /* Modify config for it in decompile_parse.cpp. */
#define CONFIG_DEBUG_ENABLE_REGISTER_ADDRESS false
#endif

#if CONFIG_DECRYPT == true
#define CONFIG_DECRYPT_OPCODE true /* Modify this in decode op macro to what you need. */
#endif


/*

	 LUA-VM CALL STACK:
	 {
		args,
		local_vars,
		general_porpose_vars
	 } (Each one is garunteed to be seperate and non mutable unles stated or compiler optimizes then can be).

	 Compiler info:
	 Call routines - All dest regs are mutable by the compiled if it doesnt occure but incs it's locvar same goes for concat.
	 These exist as for how luaU structures OP_CALL and OP_CONCAT thought optimization can be done so this routine can be mitgated.
	 Though it is a lot of work and maybe worth it can be.

*/