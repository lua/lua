readfrom("template.html")
TEMPLATE=read("*a")
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
 for i,v in t do
  GLOBAL[i]=v
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
