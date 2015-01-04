dofile("staff.lua")

global{
  ROOT='http://www.tecgraf.puc-rio.br',
  EMAIL="|LOGIN|@tecgraf.puc-rio.br",
  WWW="|ROOT|/~|LOGIN|",
  webmast='|ROOT|/webmaster.html',
  TECGRAF='<A HREF="http://www.tecgraf.puc-rio.br">TeCGraf</A>',
  CS='<A HREF="http://www.inf.puc-rio.br">Computer Science Department</A>',
  PUCRIO='<A HREF="http://www.puc-rio.br">PUC-Rio</A>',
}

staff{
DI="inf.puc-rio.br",
  LOGIN="roberto",
  NAME="Roberto Ierusalimschy",
  TITLE="D.Sc., |PUCRIO|, 1990",
  POSITION="Associate Professor, |CS|, |PUCRIO|",
  AREAS="Programming Languages, Object Oriented Programming",
  EMAIL="|LOGIN|@|DI|",
  WWW="http://www.|DI|/~|LOGIN|",
}

staff{
PCG="http://www.graphics.cornell.edu",
  LOGIN="celes",
  NAME="Waldemar Celes",
  TITLE="D.Sc., |PUCRIO|, 1995",
  POSITION="Postdoctoral Associate at "..
'<A HREF="|PCG|">Program of Computer Graphics</A>, '..
'<A HREF="http://www.cornell.edu">Cornell University</A>',
  WWW="|PCG|/~celes/",
  AREAS="Image segmentation and 3D reconstruction; "
.."Physical simulation and educational software;"
.."Geometric modeling and topological data structures;"
.."Extension languages and customizable applications"
}

staff{
  LOGIN="lhf",
  NAME="Luiz Henrique de Figueiredo",
  TITLE='D.Sc., <A HREF="http://www.impa.br">IMPA</A>, 1992',
  POSITION='Associate Researcher at <A HREF="http://www.lncc.br">LNCC</A>; Consultant at |TECGRAF|',
  AREAS="Geometric modeling; Software tools",
  WWW="http://www2.lncc.br/~lhf/",
}
