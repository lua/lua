# makefile for lua distribution

all:
	latex manual.tex
	latex manual.tex
	dvips -o manual.ps manual.dvi
	rm -f manual.tex manual.aux manual.log

clean:
	rm -f manual.tex

co:
	co -f -M manual.tex

klean:	clean
	rm -f manual.tex manual.ps manual.dvi
