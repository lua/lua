R= readme
T= $R.txt
H= $R.html

$T:	$H
	lynx -dump $? >$@

clean:
	-rm -f $T
