/*
* bin2c.c
* convert binary files to byte arrays
* Luiz Henrique de Figueiredo (lhf@tecgraf.puc-rio.br)
* 25 Jun 98 10:55:12
*/

#include <ctype.h>
#include <stdio.h>

void dump(FILE* f, int n)
{
 printf("static unsigned char B%d[]={\n");
 for (n=1;;n++)
 {
  int c=getc(f); 
  if (c==EOF) break;
#if 0
  printf("0x%02x,",c);
#else
  printf("%3u,",c);
#endif
  if (n==20) { putchar('\n'); n=0; }
 }
 printf("\n};\n\n");
}

void fdump(char* fn, int n)
{
 FILE* f= (fn==NULL) ? stdin : fopen(fn,"rb");	/* must open in binary mode */
 if (f==NULL)
 {
  fprintf(stderr,"bin2c: cannot open ");
  perror(fn);
  exit(1);
 }
 else
 {
  if (fn!=NULL) printf("/* %s */\n",fn);
  dump(f,n);
  fclose(f);
 }
}

void emit(char* fn, int n)
{
 printf(" lua_dobuffer(B%d,sizeof(B%d),\"%s\");\n",n,n,fn);
}

int main(int argc, char* argv[])
{
 printf("{\n");
 if (argc<2)
 {
  dump(stdin,0);
  emit("(stdin)",0);
 }
 else
 {
  int i;
  for (i=1; i<argc; i++) fdump(argv[i],i);
  for (i=1; i<argc; i++) emit(argv[i],i);
 }
 printf("}\n");
 return 0;
}
