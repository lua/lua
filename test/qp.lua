-- decode quoted-printable text

T=read"*a"
T=gsub(T,"=\n","")
T=gsub(T,"=(%x%x)",function (x) return strchar(tonumber(x,16)) end)
write(T)
