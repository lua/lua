
typedef union  
{
 int   vInt;
 long  vLong;
 float vFloat;
 char *pChar;
 Word  vWord;
 Byte *pByte;
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
# define NUMBER 270
# define FUNCTION 271
# define STRING 272
# define NAME 273
# define DEBUG 274
# define AND 275
# define OR 276
# define NE 277
# define LE 278
# define GE 279
# define CONC 280
# define UNARY 281
# define NOT 282
