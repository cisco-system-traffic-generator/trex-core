import sys
import re


p1 = re.compile(r'[^.]*([.][.][/][.][.][/]src[^:]+[:]).*')
for line in sys.stdin:
    n = p1.match(line)
    if n != None:
        old = n.groups()[0]
        new = old[6:]
        s1=line.replace(old,new)
        sys.stdout.write(s1)
    else:
        sys.stdout.write(line)


