typedef union  
{
 int   vInt;
 float vFloat;
 char *pChar;
 Word  vWord;
 Long  vLong;
 TFunc *pFunc;
 TreeNode *pNode;
} YYSTYPE;
extern YYSTYPE yylval;
# define WRONGTOKEN 257
# define NIL 258
# define IF 259
# define THEN 260
# define ELSE 261
# define ELSEIF 262
# define WHILE 263
# define DO 264
# define REPEAT 265
# define UNTIL 266
# define END 267
# define RETURN 268
# define LOCAL 269
# define FUNCTION 270
# define NUMBER 271
# define STRING 272
# define NAME 273
# define DEBUG 274
# define AND 275
# define OR 276
# define EQ 277
# define NE 278
# define LE 279
# define GE 280
# define CONC 281
# define UNARY 282
# define NOT 283
