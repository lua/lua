# [![Lua](logo.gif)](http://www.lua.org/)Welcome to Lua 5.3

<div class="menubar">[about](#about) · [installation](#install) · [changes](#changes) · [license](#license) · [reference manual](contents.md)</div>

## <a name="about">About Lua</a>

Lua is a powerful, fast, lightweight, embeddable scripting language developed by a [team](http://www.lua.org/authors.html) at [PUC-Rio](http://www.puc-rio.br/), the Pontifical Catholic University of Rio de Janeiro in Brazil. Lua is [free software](#license) used in many products and projects around the world.

Lua's [official web site](http://www.lua.org/) provides complete information about Lua, including an [executive summary](http://www.lua.org/about.html) and updated [documentation](http://www.lua.org/docs.html), especially the [reference manual](http://www.lua.org/manual/5.3/), which may differ slightly from the [local copy](contents.md) distributed in this package.

## <a name="install">Installing Lua</a>

Lua is distributed in [source](http://www.lua.org/ftp/) form. You need to build it before using it. Building Lua should be straightforward because Lua is implemented in pure ANSI C and compiles unmodified in all known platforms that have an ANSI C compiler. Lua also compiles unmodified as C++. The instructions given below for building Lua are for Unix-like platforms. See also [instructions for other systems](#other) and [customization options](#customization).

If you don't have the time or the inclination to compile Lua yourself, get a binary from [LuaBinaries](http://lua-users.org/wiki/LuaBinaries). Try also [LuaDist](http://luadist.org/), a multi-platform distribution of Lua that includes batteries.

### Building Lua

In most Unix-like platforms, simply do <kbd>make</kbd> with a suitable target. Here are the details.

1.  Open a terminal window and move to the top-level directory, which is named <tt>lua-5.3.x</tt>. The <tt>Makefile</tt> there controls both the build process and the installation process.
2.  Do <kbd>make</kbd> and see if your platform is listed. The platforms currently supported are:

    aix bsd c89 freebsd generic linux macosx mingw posix solaris

    If your platform is listed, just do <kbd>make xxx</kbd>, where xxx is your platform name.

    If your platform is not listed, try the closest one or posix, generic, c89, in this order.

3.  The compilation takes only a few moments and produces three files in the <tt>src</tt> directory: lua (the interpreter), luac (the compiler), and liblua.a (the library).
4.  To check that Lua has been built correctly, do <kbd>make test</kbd> after building Lua. This will run the interpreter and print its version.

If you're running Linux and get compilation errors, make sure you have installed the <tt>readline</tt> development package (which is probably named <tt>libreadline-dev</tt> or <tt>readline-devel</tt>). If you get link errors after that, then try <kbd>make linux MYLIBS=-ltermcap</kbd>.

### Installing Lua

Once you have built Lua, you may want to install it in an official place in your system. In this case, do <kbd>make install</kbd>. The official place and the way to install files are defined in the <tt>Makefile</tt>. You'll probably need the right permissions to install files.

To build and install Lua in one step, do <kbd>make xxx install</kbd>, where xxx is your platform name.

To install Lua locally, do <kbd>make local</kbd>. This will create a directory <tt>install</tt> with subdirectories <tt>bin</tt>, <tt>include</tt>, <tt>lib</tt>, <tt>man</tt>, <tt>share</tt>, and install Lua as listed below. To install Lua locally, but in some other directory, do <kbd>make install INSTALL_TOP=xxx</kbd>, where xxx is your chosen directory. The installation starts in the <tt>src</tt> and <tt>doc</tt> directories, so take care if <tt>INSTALL_TOP</tt> is not an absolute path.

<dl class="display">

<dt>

<dd>

<dt>

<dd>

<dt>

<dd>

<dt>

<dd>

</dl>

These are the only directories you need for development. If you only want to run Lua programs, you only need the files in <tt>bin</tt> and <tt>man</tt>. The files in <tt>include</tt> and <tt>lib</tt> are needed for embedding Lua in C or C++ programs.

### <a name="customization">Customization</a>

Three kinds of things can be customized by editing a file:

*   Where and how to install Lua — edit <tt>Makefile</tt>.
*   How to build Lua — edit <tt>src/Makefile</tt>.
*   Lua features — edit <tt>src/luaconf.h</tt>.

You don't actually need to edit the Makefiles because you may set the relevant variables in the command line when invoking make. Nevertheless, it's probably best to edit and save the Makefiles to record the changes you've made.

On the other hand, if you need to customize some Lua features, you'll need to edit <tt>src/luaconf.h</tt> before building and installing Lua. The edited file will be the one installed, and it will be used by any Lua clients that you build, to ensure consistency. Further customization is available to experts by editing the Lua sources.

### <a name="other">Building Lua on other systems</a>

If you're not using the usual Unix tools, then the instructions for building Lua depend on the compiler you use. You'll need to create projects (or whatever your compiler uses) for building the library, the interpreter, and the compiler, as follows:

<dl class="display">

<dt>

<dd>

<dt>

<dd>

<dt>

<dd>

</dl>

To use Lua as a library in your own programs you'll need to know how to create and use libraries with your compiler. Moreover, to dynamically load C libraries for Lua you'll need to know how to create dynamic libraries and you'll need to make sure that the Lua API functions are accessible to those dynamic libraries — but _don't_ link the Lua library into each dynamic library. For Unix, we recommend that the Lua library be linked statically into the host program and its symbols exported for dynamic linking; <tt>src/Makefile</tt> does this for the Lua interpreter. For Windows, we recommend that the Lua library be a DLL. In all cases, the compiler luac should be linked statically.

As mentioned above, you may edit <tt>src/luaconf.h</tt> to customize some features before building Lua.

## <a name="changes">Changes since Lua 5.2</a>

Here are the main changes introduced in Lua 5.3. The [reference manual](contents.md) lists the [incompatibilities](manual.md#8) that had to be introduced.

### Main changes

*   integers (64-bit by default)
*   official support for 32-bit numbers
*   bitwise operators
*   basic utf-8 support
*   functions for packing and unpacking values

Here are the other changes introduced in Lua 5.3:

### Language

*   userdata can have any Lua value as uservalue
*   integer division
*   more flexible rules for some metamethods

### Libraries

*   `ipairs` and the table library respect metamethods
*   strip option in `string.dump`
*   table library respects metamethods
*   new function `table.move`
*   new function `string.pack`
*   new function `string.unpack`
*   new function `string.packsize`

### C API

*   simpler API for continuation functions in C
*   `lua_gettable` and similar functions return type of resulted value
*   strip option in `lua_dump`
*   new function: `lua_geti`
*   new function: `lua_seti`
*   new function: `lua_isyieldable`
*   new function: `lua_numbertointeger`
*   new function: `lua_rotate`
*   new function: `lua_stringtonumber`

### Lua standalone interpreter

*   can be used as calculator; no need to prefix with '='
*   `arg` table available to all code

## <a name="license">License</a>

[![[osi certified]](osi-certified-72x60.png)](http://www.opensource.org/docs/definition.php)Lua is free software distributed under the terms of the [MIT license](http://www.opensource.org/licenses/mit-license.html) reproduced below; it may be used for any purpose, including commercial purposes, at absolutely no cost without having to ask us. The only requirement is that if you do use Lua, then you should give us credit by including the appropriate copyright notice somewhere in your product or its documentation. For details, see [this](http://www.lua.org/license.html).

> Copyright © 1994–2015 Lua.org, PUC-Rio.
> 
> Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
> 
> The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
> 
> THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Last update: Mon Jun 1 21:48:24 BRT 2015
