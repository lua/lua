$debug

readfrom("template.html")
TEMPLATE=read(".*")
readfrom()

PAT="|(%a%a*)|"

GLOBAL={
	DATE=date("%d/%m/%Y %T"),
}

function get(i)
     if  LOCAL[i] then return  LOCAL[i]
 elseif GLOBAL[i] then return GLOBAL[i]
 else return "<BLINK>?"..i.."?</BLINK>" end
end

function global(t)
 local i,v=next(t,nil)
 while i do
  GLOBAL[i]=v
  i,v=next(t,i)
 end
end

function staff(t)
 LOCAL=t
 if t.AREAS then t.AREAS=gsub(t.AREAS,"[;,] *","\n<LI>") end
 local p,n=TEMPLATE
 if t.WWW=="" then p=gsub(p,'<A HREF="|WWW|">|NAME|</A>',"|NAME|") end
 repeat p,n=gsub(p,PAT,get) until n==0
 write(t.LOGIN,"\n")
 writeto(t.LOGIN..".html")
 write(p)
 writeto()
end
