/*
** $Id: print.h,v 1.1 2000/11/06 20:03:12 lhf Exp $
** extracted automatically from lopcodes.h by mkprint.lua -- DO NOT EDIT
** See Copyright Notice in lua.h
*/

  case OP_END: P_OP("END"); P_NONE; break;
  case OP_RETURN: P_OP("RETURN"); P_U; break;
  case OP_CALL: P_OP("CALL"); P_AB; break;
  case OP_TAILCALL: P_OP("TAILCALL"); P_AB; break;
  case OP_PUSHNIL: P_OP("PUSHNIL"); P_U; break;
  case OP_POP: P_OP("POP"); P_U; break;
  case OP_PUSHINT: P_OP("PUSHINT"); P_S; break;
  case OP_PUSHSTRING: P_OP("PUSHSTRING"); P_Q; break;
  case OP_PUSHNUM: P_OP("PUSHNUM"); P_N; break;
  case OP_PUSHNEGNUM: P_OP("PUSHNEGNUM"); P_N; break;
  case OP_PUSHUPVALUE: P_OP("PUSHUPVALUE"); P_U; break;
  case OP_GETLOCAL: P_OP("GETLOCAL"); P_L; break;
  case OP_GETGLOBAL: P_OP("GETGLOBAL"); P_K; break;
  case OP_GETTABLE: P_OP("GETTABLE"); P_NONE; break;
  case OP_GETDOTTED: P_OP("GETDOTTED"); P_K; break;
  case OP_GETINDEXED: P_OP("GETINDEXED"); P_L; break;
  case OP_PUSHSELF: P_OP("PUSHSELF"); P_K; break;
  case OP_CREATETABLE: P_OP("CREATETABLE"); P_U; break;
  case OP_SETLOCAL: P_OP("SETLOCAL"); P_L; break;
  case OP_SETGLOBAL: P_OP("SETGLOBAL"); P_K; break;
  case OP_SETTABLE: P_OP("SETTABLE"); P_AB; break;
  case OP_SETLIST: P_OP("SETLIST"); P_AB; break;
  case OP_SETMAP: P_OP("SETMAP"); P_U; break;
  case OP_ADD: P_OP("ADD"); P_NONE; break;
  case OP_ADDI: P_OP("ADDI"); P_S; break;
  case OP_SUB: P_OP("SUB"); P_NONE; break;
  case OP_MULT: P_OP("MULT"); P_NONE; break;
  case OP_DIV: P_OP("DIV"); P_NONE; break;
  case OP_POW: P_OP("POW"); P_NONE; break;
  case OP_CONCAT: P_OP("CONCAT"); P_U; break;
  case OP_MINUS: P_OP("MINUS"); P_NONE; break;
  case OP_NOT: P_OP("NOT"); P_NONE; break;
  case OP_JMPNE: P_OP("JMPNE"); P_J; break;
  case OP_JMPEQ: P_OP("JMPEQ"); P_J; break;
  case OP_JMPLT: P_OP("JMPLT"); P_J; break;
  case OP_JMPLE: P_OP("JMPLE"); P_J; break;
  case OP_JMPGT: P_OP("JMPGT"); P_J; break;
  case OP_JMPGE: P_OP("JMPGE"); P_J; break;
  case OP_JMPT: P_OP("JMPT"); P_J; break;
  case OP_JMPF: P_OP("JMPF"); P_J; break;
  case OP_JMPONT: P_OP("JMPONT"); P_J; break;
  case OP_JMPONF: P_OP("JMPONF"); P_J; break;
  case OP_JMP: P_OP("JMP"); P_J; break;
  case OP_PUSHNILJMP: P_OP("PUSHNILJMP"); P_NONE; break;
  case OP_FORPREP: P_OP("FORPREP"); P_J; break;
  case OP_FORLOOP: P_OP("FORLOOP"); P_J; break;
  case OP_LFORPREP: P_OP("LFORPREP"); P_J; break;
  case OP_LFORLOOP: P_OP("LFORLOOP"); P_J; break;
  case OP_CLOSURE: P_OP("CLOSURE"); P_F; break;
