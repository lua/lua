This is lua, a sample Lua interpreter.
It can be used as a batch interpreter and also interactively.
There are man pages for it in both nroff and html in ../../doc.

Usage: ./lua [options] [script [args]].  Available options are:
  -        execute stdin as a file
  -e stat  execute string `stat'
  -i       enter interactive mode after executing `script'
  -l name  load and run library `name'
  -v       show version information
  --       stop handling options

This interpreter is suitable for using Lua as a standalone language; it loads
all standard libraries. For a minimal interpreter, see ../../etc/min.c.

If your application simply exports new functions to Lua (which is common),
then you can use this interpreter (almost) unmodified, as follows:

* First, define a function 
	 void myinit (lua_State *L)
  in your own code. In this function, you should do whatever initializations
  are needed by your application, typically exporting your functions to Lua.
  (Of course, you can use any name instead of "myinit".)

* Then, #define lua_userinit(L) to be "openstdlibs(L)+myinit(L)".
  Here, openstdlibs is a function in lua.c that opens all standard libraries.
  If you don't need them, just don't call openstdlibs and open any standard
  libraries that you do need in myinit.

* Finally, remember to link your C code when building lua.

For other customizations, see ../../etc/config.c.
