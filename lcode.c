/*
** $Id: $
** Code generator for Lua
** See Copyright Notice in lua.h
*/


#include "lcode.h"
#include "llex.h"
#include "lmem.h"
#include "lobject.h"
#include "lopcodes.h"
#include "lparser.h"


static Instruction *last_i (FuncState *fs) {
  static Instruction dummy = SET_OPCODE(0, ENDCODE);
  if (fs->last_pc < 0)
    return &dummy;
  else
    return &fs->f->code[fs->last_pc];
}


int luaK_primitivecode (LexState *ls, Instruction i) {
  FuncState *fs = ls->fs;
  luaM_growvector(ls->L, fs->f->code, fs->pc, 1, Instruction, codeEM, MAXARG_S);
  fs->f->code[fs->pc] = i;
  return fs->pc++;
}



int luaK_code (LexState *ls, Instruction i) {
  FuncState *fs = ls->fs;
  Instruction *last = last_i(fs);
  switch (GET_OPCODE(i)) {

    case MINUSOP:
      switch(GET_OPCODE(*last)) {
        case PUSHINT: *last = SETARG_S(*last, -GETARG_S(*last)); break;
        case PUSHNUM: *last = SET_OPCODE(*last, PUSHNEGNUM); break;
        case PUSHNEGNUM: *last = SET_OPCODE(*last, PUSHNUM); break;
        default: fs->last_pc = luaK_primitivecode(ls, i);
      }
      break;

    case GETTABLE:
      switch(GET_OPCODE(*last)) {
        case PUSHSTRING: *last = SET_OPCODE(*last, GETDOTTED); break;
        default: fs->last_pc = luaK_primitivecode(ls, i);
      }
      break;

    case RETCODE:
      switch(GET_OPCODE(*last)) {
        case CALL:
          *last = SET_OPCODE(*last, TAILCALL);
          *last = SETARG_B(*last, GETARG_U(i));
          break;
        default: fs->last_pc = luaK_primitivecode(ls, i);
      }
      break;

    case ADDOP:
      switch(GET_OPCODE(*last)) {
        case PUSHINT: *last = SET_OPCODE(*last, ADDI); break;
        default: fs->last_pc = luaK_primitivecode(ls, i);
      }
      break;

    case SUBOP:
      switch(GET_OPCODE(*last)) {
        case PUSHINT:
          *last = SET_OPCODE(*last, ADDI);
          *last = SETARG_S(*last, -GETARG_S(*last));
          break;
        default: fs->last_pc = luaK_primitivecode(ls, i);
      }
      break;

    default: fs->last_pc = luaK_primitivecode(ls, i);
  }
  return fs->last_pc;
}


void luaK_fixjump (LexState *ls, int pc, int dest) {
  FuncState *fs = ls->fs;
  Instruction *jmp = &fs->f->code[pc];
  /* jump is relative to position following jump instruction */
  *jmp = SETARG_S(*jmp, dest-(pc+1));
  fs->last_pc = pc;
}


