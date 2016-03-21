## This file is part of Scapy
## See http://www.secdev.org/projects/scapy for more informations
## Copyright (C) Philippe Biondi <phil@secdev.org>
## This program is published under a GPLv2 license

"""
PacketList: holds several packets and allows to do operations on them.
"""


import os,subprocess
from .config import conf
from .base_classes import BasePacket,BasePacketList
from collections import defaultdict

from .utils import do_graph,hexdump,make_table,make_lined_table,make_tex_table,get_temp_file

from scapy.arch import NETWORKX
if NETWORKX:
    import networkx as nx


#############
## Results ##
#############

class PacketList(BasePacketList):
    res = []
    def __init__(self, res=None, name="PacketList", stats=None, vector_index = None):
        """create a packet list from a list of packets
           res: the list of packets
           stats: a list of classes that will appear in the stats (defaults to [TCP,UDP,ICMP])"""
        if stats is None:
            stats = conf.stats_classic_protocols
        self.stats = stats
        if res is None:
            res = []
        if isinstance(res, PacketList):
            res = res.res
        self.res = res
        self.listname = name
        self.vector_index = vector_index
    def __len__(self):
        return len(self.res)
    def _elt2pkt(self, elt):
        if self.vector_index == None:
            return elt
        else:
            return elt[self.vector_index]
    def _elt2sum(self, elt):
        if self.vector_index == None:
            return elt.summary()
        else:
            return "%s ==> %s" % (elt[0].summary(),elt[1].summary()) 

    def _elt2show(self, elt):
        return self._elt2sum(elt)
    def __repr__(self):
        stats=dict.fromkeys(self.stats,0)
        other = 0
        for r in self.res:
            f = 0
            for p in stats:
                if self._elt2pkt(r).haslayer(p):
                    stats[p] += 1
                    f = 1
                    break
            if not f:
                other += 1
        s = ""
        ct = conf.color_theme
        for p in self.stats:
            s += " %s%s%s" % (ct.packetlist_proto(p.name),
                              ct.punct(":"),
                              ct.packetlist_value(stats[p]))
        s += " %s%s%s" % (ct.packetlist_proto("Other"),
                          ct.punct(":"),
                          ct.packetlist_value(other))
        return "%s%s%s%s%s" % (ct.punct("<"),
                               ct.packetlist_name(self.listname),
                               ct.punct(":"),
                               s,
                               ct.punct(">"))
    def __getattr__(self, attr):
        return getattr(self.res, attr)
    def __getitem__(self, item):
        if isinstance(item,type) and issubclass(item,BasePacket):
            #return self.__class__(filter(lambda x: item in self._elt2pkt(x),self.res),
            return self.__class__([ x for x in self.res if item in self._elt2pkt(x) ],
                                  name="%s from %s"%(item.__name__,self.listname))
        if type(item) is slice:
            return self.__class__(self.res.__getitem__(item),
                                  name = "mod %s" % self.listname)
        return self.res.__getitem__(item)
    def __getslice__(self, *args, **kargs):
        return self.__class__(self.res.__getslice__(*args, **kargs),
                              name="mod %s"%self.listname)
    def __add__(self, other):
        return self.__class__(self.res+other.res,
                              name="%s+%s"%(self.listname,other.listname))
    def summary(self, prn=None, lfilter=None):
        """prints a summary of each packet
prn:     function to apply to each packet instead of lambda x:x.summary()
lfilter: truth function to apply to each packet to decide whether it will be displayed"""
        for r in self.res:
            if lfilter is not None:
                if not lfilter(r):
                    continue
            if prn is None:
                print(self._elt2sum(r))
            else:
                print(prn(r))
    def nsummary(self,prn=None, lfilter=None):
        """prints a summary of each packet with the packet's number
prn:     function to apply to each packet instead of lambda x:x.summary()
lfilter: truth function to apply to each packet to decide whether it will be displayed"""
        for i, p in enumerate(self.res):
            if lfilter is not None:
                if not lfilter(p):
                    continue
            print(conf.color_theme.id(i,fmt="%04i"), end = " ")
            if prn is None:
                print(self._elt2sum(p))
            else:
                print(prn(p))
    def display(self): # Deprecated. Use show()
        """deprecated. is show()"""
        self.show()
    def show(self, *args, **kargs):
        """Best way to display the packet list. Defaults to nsummary() method"""
        return self.nsummary(*args, **kargs)
    
    def filter(self, func):
        """Returns a packet list filtered by a truth function"""
        return self.__class__(list(filter(func,self.res)),
                              name="filtered %s"%self.listname)

    def plot(self, f, lfilter=None,**kargs):
        """Applies a function to each packet to get a value that will be plotted with matplotlib. A matplotlib object is returned
        lfilter: a truth function that decides whether a packet must be ploted"""

        return plt.plot([ f(i) for i in self.res if not lfilter or lfilter(i) ], **kargs)

    def diffplot(self, f, delay=1, lfilter=None, **kargs):
        """diffplot(f, delay=1, lfilter=None)
        Applies a function to couples (l[i],l[i+delay])"""

        return plt.plot([ f(i, j) for i in self.res[:-delay] for j in self.res[delay:] if not lfilter or (lfilter(i) and lfilter(j))], 
            **kargs)

    def multiplot(self, f, lfilter=None, **kargs):
        """Uses a function that returns a label and a value for this label, then plots all the values label by label"""

        d = defaultdict(list)
        for i in self.res:
            if lfilter and not lfilter(i):
                continue 
            k, v = f(i)
            d[k].append(v)

        figure = plt.figure()
        ax = figure.add_axes(plt.axes())
        for i in d:
            ax.plot(d[i], **kargs)
        return figure
        

    def rawhexdump(self):
        """Prints an hexadecimal dump of each packet in the list"""
        for p in self:
            hexdump(self._elt2pkt(p))

    def hexraw(self, lfilter=None):
        """Same as nsummary(), except that if a packet has a Raw layer, it will be hexdumped
        lfilter: a truth function that decides whether a packet must be displayed"""
        for i,p in enumerate(self.res):
            p1 = self._elt2pkt(p)
            if lfilter is not None and not lfilter(p1):
                continue
            print("%s %s %s" % (conf.color_theme.id(i,fmt="%04i"),
                                p1.sprintf("%.time%"),
                                self._elt2sum(p)))
            if p1.haslayer(conf.raw_layer):
                hexdump(p1.getlayer(conf.raw_layer).load)

    def hexdump(self, lfilter=None):
        """Same as nsummary(), except that packets are also hexdumped
        lfilter: a truth function that decides whether a packet must be displayed"""
        for i,p in enumerate(self.res):
            p1 = self._elt2pkt(p)
            if lfilter is not None and not lfilter(p1):
                continue
            print("%s %s %s" % (conf.color_theme.id(i,fmt="%04i"),
                                p1.sprintf("%.time%"),
                                self._elt2sum(p)))
            hexdump(p1)

    def padding(self, lfilter=None):
        """Same as hexraw(), for Padding layer"""
        for i,p in enumerate(self.res):
            p1 = self._elt2pkt(p)
            if p1.haslayer(conf.padding_layer):
                if lfilter is None or lfilter(p1):
                    print("%s %s %s" % (conf.color_theme.id(i,fmt="%04i"),
                                        p1.sprintf("%.time%"),
                                        self._elt2sum(p)))
                    hexdump(p1.getlayer(conf.padding_layer).load)

    def nzpadding(self, lfilter=None):
        """Same as padding() but only non null padding"""
        for i,p in enumerate(self.res):
            p1 = self._elt2pkt(p)
            if p1.haslayer(conf.padding_layer):
                pad = p1.getlayer(conf.padding_layer).load
                if pad == pad[0]*len(pad):
                    continue
                if lfilter is None or lfilter(p1):
                    print("%s %s %s" % (conf.color_theme.id(i,fmt="%04i"),
                                        p1.sprintf("%.time%"),
                                        self._elt2sum(p)))
                    hexdump(p1.getlayer(conf.padding_layer).load)
        

    def conversations(self, getsrcdst=None, draw = True, **kargs):
        """Graphes a conversations between sources and destinations and display it
        (using graphviz)
        getsrcdst: a function that takes an element of the list and return the source and dest
                   by defaults, return source and destination IP
        if networkx library is available returns a DiGraph, or draws it if draw = True otherwise graphviz is used
        format: output type (svg, ps, gif, jpg, etc.), passed to dot's "-T" option
        target: output filename. If None, matplotlib is used to display
        prog: which graphviz program to use"""
        if getsrcdst is None:
            getsrcdst = lambda x:(x['IP'].src, x['IP'].dst)
        conv = {}
        for p in self.res:
            p = self._elt2pkt(p)
            try:
                c = getsrcdst(p)
            except:
                #XXX warning()
                continue
            conv[c] = conv.get(c,0)+1

        if NETWORKX: # networkx is available
            gr = nx.DiGraph()
            for s,d in conv:
                if s not in gr:
                    gr.add_node(s)
                if d not in gr:
                    gr.add_node(d)
                gr.add_edge(s, d)
            if draw:
                return do_graph(gr, **kargs)
            else:
                return gr
        else:
            gr = 'digraph "conv" {\n'
            for s,d in conv:
                gr += '\t "%s" -> "%s"\n' % (s,d)
            gr += "}\n"        
            return do_graph(gr, **kargs)

    def afterglow(self, src=None, event=None, dst=None, **kargs):
        """Experimental clone attempt of http://sourceforge.net/projects/afterglow
        each datum is reduced as src -> event -> dst and the data are graphed.
        by default we have IP.src -> IP.dport -> IP.dst"""
        if src is None:
            src = lambda x: x['IP'].src
        if event is None:
            event = lambda x: x['IP'].dport
        if dst is None:
            dst = lambda x: x['IP'].dst
        sl = {}
        el = {}
        dl = {}
        for i in self.res:
            try:
                s,e,d = src(i),event(i),dst(i)
                if s in sl:
                    n,l = sl[s]
                    n += 1
                    if e not in l:
                        l.append(e)
                    sl[s] = (n,l)
                else:
                    sl[s] = (1,[e])
                if e in el:
                    n,l = el[e]
                    n+=1
                    if d not in l:
                        l.append(d)
                    el[e] = (n,l)
                else:
                    el[e] = (1,[d])
                dl[d] = dl.get(d,0)+1
            except:
                continue

        import math
        def normalize(n):
            return 2+math.log(n)/4.0

        def minmax(x):
            m,M = min(x),max(x)
            if m == M:
                m = 0
            if M == 0:
                M = 1
            return m,M

        #mins,maxs = minmax(map(lambda (x,y): x, sl.values()))
        mins,maxs = minmax([ a[0] for a in sl.values()])
        #mine,maxe = minmax(map(lambda (x,y): x, el.values()))
        mine,maxe = minmax([ a[0] for a in el.values()])
        mind,maxd = minmax(dl.values())
    
        gr = 'digraph "afterglow" {\n\tedge [len=2.5];\n'

        gr += "# src nodes\n"
        for s in sl:
            n,l = sl[s]; n = 1+(n-mins)/(maxs-mins)
            gr += '"src.%s" [label = "%s", shape=box, fillcolor="#FF0000", style=filled, fixedsize=1, height=%.2f,width=%.2f];\n' % (repr(s),repr(s),n,n)
        gr += "# event nodes\n"
        for e in el:
            n,l = el[e]; n = n = 1+(n-mine)/(maxe-mine)
            gr += '"evt.%s" [label = "%s", shape=circle, fillcolor="#00FFFF", style=filled, fixedsize=1, height=%.2f, width=%.2f];\n' % (repr(e),repr(e),n,n)
        for d in dl:
            n = dl[d]; n = n = 1+(n-mind)/(maxd-mind)
            gr += '"dst.%s" [label = "%s", shape=triangle, fillcolor="#0000ff", style=filled, fixedsize=1, height=%.2f, width=%.2f];\n' % (repr(d),repr(d),n,n)

        gr += "###\n"
        for s in sl:
            n,l = sl[s]
            for e in l:
                gr += ' "src.%s" -> "evt.%s";\n' % (repr(s),repr(e)) 
        for e in el:
            n,l = el[e]
            for d in l:
                gr += ' "evt.%s" -> "dst.%s";\n' % (repr(e),repr(d)) 
            
        gr += "}"
        return do_graph(gr, **kargs)


    def _dump_document(self, **kargs):
        import pyx
        d = pyx.document.document()
        l = len(self.res)
        for i in range(len(self.res)):
            elt = self.res[i]
            c = self._elt2pkt(elt).canvas_dump(**kargs)
            cbb = c.bbox()
            c.text(cbb.left(),cbb.top()+1,r"\font\cmssfont=cmss12\cmssfont{Frame %i/%i}" % (i,l),[pyx.text.size.LARGE])
            if conf.verb >= 2:
                os.write(1,b".")
            d.append(pyx.document.page(c, paperformat=pyx.document.paperformat.A4,
                                       margin=1*pyx.unit.t_cm,
                                       fittosize=1))
        return d
                     
                 

    def psdump(self, filename = None, **kargs):
        """Creates a multipage poscript file with a psdump of every packet
        filename: name of the file to write to. If empty, a temporary file is used and
                  conf.prog.psreader is called"""
        d = self._dump_document(**kargs)
        if filename is None:
            filename = get_temp_file(autoext=".ps")
            d.writePSfile(filename)
            subprocess.Popen([conf.prog.psreader, filename+".ps"])
        else:
            d.writePSfile(filename)
        print
        
    def pdfdump(self, filename = None, **kargs):
        """Creates a PDF file with a psdump of every packet
        filename: name of the file to write to. If empty, a temporary file is used and
                  conf.prog.pdfreader is called"""
        d = self._dump_document(**kargs)
        if filename is None:
            filename = get_temp_file(autoext=".pdf")
            d.writePDFfile(filename)
            subprocess.Popen([conf.prog.pdfreader, filename+".pdf"])
        else:
            d.writePDFfile(filename)
        print

    def sr(self,multi=0):
        """sr([multi=1]) -> (SndRcvList, PacketList)
        Matches packets in the list and return ( (matched couples), (unmatched packets) )"""
        remain = self.res[:]
        sr = []
        i = 0
        while i < len(remain):
            s = remain[i]
            j = i
            while j < len(remain)-1:
                j += 1
                r = remain[j]
                if r.answers(s):
                    sr.append((s,r))
                    if multi:
                        remain[i]._answered=1
                        remain[j]._answered=2
                        continue
                    del(remain[j])
                    del(remain[i])
                    i -= 1
                    break
            i += 1
        if multi:
            remain = filter(lambda x:not hasattr(x,"_answered"), remain)
        return SndRcvList(sr),PacketList(remain)

    def sessions(self, session_extractor=None):
        if session_extractor is None:
            def session_extractor(p):
                sess = "Other"
                if 'Ether' in p:
                    if 'IP' in p:
                        if 'TCP' in p:
                            sess = p.sprintf("TCP %IP.src%:%r,TCP.sport% > %IP.dst%:%r,TCP.dport%")
                        elif 'UDP' in p:
                            sess = p.sprintf("UDP %IP.src%:%r,UDP.sport% > %IP.dst%:%r,UDP.dport%")
                        elif 'ICMP' in p:
                            sess = p.sprintf("ICMP %IP.src% > %IP.dst% type=%r,ICMP.type% code=%r,ICMP.code% id=%ICMP.id%")
                        else:
                            sess = p.sprintf("IP %IP.src% > %IP.dst% proto=%IP.proto%")
                    elif 'ARP' in p:
                        sess = p.sprintf("ARP %ARP.psrc% > %ARP.pdst%")
                    else:
                        sess = p.sprintf("Ethernet type=%04xr,Ether.type%")
                return sess
        sessions = defaultdict(self.__class__)
        for p in self.res:
            sess = session_extractor(self._elt2pkt(p))
            sessions[sess].append(p)
        return dict(sessions)
    
    def replace(self, *args, **kargs):
        """
        lst.replace(<field>,[<oldvalue>,]<newvalue>)
        lst.replace( (fld,[ov],nv),(fld,[ov,]nv),...)
          if ov is None, all values are replaced
        ex:
          lst.replace( IP.src, "192.168.1.1", "10.0.0.1" )
          lst.replace( IP.ttl, 64 )
          lst.replace( (IP.ttl, 64), (TCP.sport, 666, 777), )
        """
        delete_checksums = kargs.get("delete_checksums",False)
        x=PacketList(name="Replaced %s" % self.listname)
        if type(args[0]) is not tuple:
            args = (args,)
        for p in self.res:
            p = self._elt2pkt(p)
            copied = False
            for scheme in args:
                fld = scheme[0]
                old = scheme[1] # not used if len(scheme) == 2
                new = scheme[-1]
                for o in fld.owners:
                    if o in p:
                        if len(scheme) == 2 or p[o].getfieldval(fld.name) == old:
                            if not copied:
                                p = p.copy()
                                if delete_checksums:
                                    p.delete_checksums()
                                copied = True
                            setattr(p[o], fld.name, new)
            x.append(p)
        return x
                
            
class SndRcvList(PacketList):
    def __init__(self, res=None, name="Results", stats=None):
        PacketList.__init__(self, res, name, stats, vector_index = 1)
    def summary(self, prn=None, lfilter=None):
        """prints a summary of each SndRcv packet pair
prn:     function to apply to each packet pair instead of lambda s, r: "%s ==> %s" % (s.summary(),r.summary())
lfilter: truth function to apply to each packet pair to decide whether it will be displayed"""
        for s, r in self.res:
            if lfilter is not None:
                if not lfilter(s, r):
                    continue
            if prn is None:
                print(self._elt2sum((s, r)))
            else:
                print(prn(s, r))
    def nsummary(self,prn=None, lfilter=None):
        """prints a summary of each SndRcv packet pair with the pair's number
prn:     function to apply to each packet pair instead of lambda s, r: "%s ==> %s" % (s.summary(),r.summary()) 
lfilter: truth function to apply to each packet pair to decide whether it will be displayed"""
        for i, (s, r) in enumerate(self.res):
            if lfilter is not None:
                if not lfilter(s, r):
                    continue
            print(conf.color_theme.id(i,fmt="%04i"), end = " ")
            if prn is None:
                print(self._elt2sum((s, r)))
            else:
                print(prn(s, r))
    def filter(self, func):
        """Returns a SndRcv list filtered by a truth function"""
        return self.__class__( [ i for i in self.res if func(*i) ], name='filtered %s'%self.listname)

    def make_table(self, *args, **kargs):
        """Prints a table using a function that returs for each packet its head column value, head row value and displayed value
        ex: p.make_table(lambda s, r:(s[IP].dst, r[TCP].sport, s[TCP].sprintf("%flags%")) """
        return make_table(self.res, *args, **kargs)
    def make_lined_table(self, *args, **kargs):
        """Same as make_table, but print a table with lines"""
        return make_lined_table(self.res, *args, **kargs)
    def make_tex_table(self, *args, **kargs):
        """Same as make_table, but print a table with LaTeX syntax"""
        return make_tex_table(self.res, *args, **kargs)
