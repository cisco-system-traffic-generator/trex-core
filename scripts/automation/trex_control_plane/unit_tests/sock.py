import os
import dpkt
import struct
import socket
import sys
import argparse;


H_SCRIPT_VER = "0.1"

class sock_driver(object):
     args=None;


def nl (buf):
    return ( struct.unpack('>I', buf)[0]);

def dump_tuple (t):
    for obj in t:
        print hex(obj),",",

class CFlowRec:
    def __init__ (self):
        self.is_init_dir=False;
        self.bytes=0;
        self.data=None;

    def __str__ (self):
        if self.is_init_dir :
            s=" client "
        else:
            s=" server "
        s+= " %d " %(self.bytes)
        return (s);



class CPcapFileReader:
    def __init__ (self,file_name):
        self.file_name=file_name;
        self.tuple=None;
        self.swap=False;
        self.info=[];

    def dump_info (self):
        for obj in self.info:
            print obj
            #print "'",obj.data,"'"

    def is_client_side (self,swap):
        if self.swap ==swap:
            return (True);
        else:
            return (False);

    def add_server(self,server,data):
        r=CFlowRec();
        r.is_init_dir =False;
        r.bytes = server
        r.data=data
        self.info.append(r);

    def add_client(self,client,data):
        r=CFlowRec();
        r.is_init_dir =True;
        r.bytes = client
        r.data=data
        self.info.append(r);

    def check_tcp_flow (self):
        f = open(self.file_name)
        pcap = dpkt.pcap.Reader(f)
        for ts, buf in pcap:
            eth = dpkt.ethernet.Ethernet(buf)
            ip = eth.data
            tcp = ip.data
            if ip.p != 6 :
                raise Exception("not a TCP flow ..");
            if tcp.flags  != dpkt.tcp.TH_SYN :
                raise Exception("first packet should be with SYN");
            break;
        f.close();

    def check_one_flow (self):
        cnt=1
        client=0;
        server=0;
        client_data=''
        server_data=''
        is_c=False # the direction
        is_s=False
        f = open(self.file_name)
        pcap = dpkt.pcap.Reader(f)
        for ts, buf in pcap:
            eth = dpkt.ethernet.Ethernet(buf)
            ip = eth.data
            tcp = ip.data
            pld = tcp.data;
            
            pkt_swap=False
            if nl(ip.src) > nl(ip.dst):
                pkt_swap=True
                tuple= (nl(ip.dst),nl(ip.src), tcp.dport ,tcp.sport,ip.p );
            else:
                tuple= (nl(ip.src),nl(ip.dst) ,tcp.sport,tcp.dport,ip.p );

            if self.tuple == None:
                self.swap=pkt_swap
                self.tuple=tuple
            else:
                if self.tuple != tuple:
                    raise Exception("More than one flow - can't process this flow");

            
            print " %5d," % (cnt),
            if self.is_client_side (pkt_swap):
                print "client",
                if len(pld) >0 :
                    if is_c==False:
                        is_c=True
                    if is_s:
                        self.add_server(server,server_data);
                        server=0;
                        server_data=''
                        is_s=False;
    
                    client+=len(pld);
                    client_data=client_data+pld;
            else:
                if len(pld) >0 :
                    if is_s==False:
                        is_s=True
                    if is_c:
                        self.add_client(client,client_data);
                        client=0;
                        client_data=''
                        is_c=False;
    
                    server+=len(pld)
                    server_data=server_data+pld;

                print "server",
            print " %5d" % (len(pld)),
            dump_tuple (tuple)
            print 

            cnt=cnt+1

        if is_c:
            self.add_client(client,client_data);
        if is_s:
            self.add_server(server,server_data);

        f.close();


class CClientServerCommon(object):

    def __init__ (self):
        pass;

    def send_info (self,data):
         print "server send %d bytes" % (len(data))
         self.connection.sendall(data)

    def rcv_info (self,msg_size):
        print "server wait for %d bytes" % (msg_size)

        bytes_recd = 0
        while bytes_recd < msg_size:
            chunk = self.connection.recv(min(msg_size - bytes_recd, 2048))
            if chunk == '':
                raise RuntimeError("socket connection broken")
            bytes_recd = bytes_recd + len(chunk)


    def process (self,is_server):
         pcapinfo=self.pcapr.info
         for obj in pcapinfo:
             if is_server:
                 if obj.is_init_dir:
                     self.rcv_info (obj.bytes);
                 else:
                     self.send_info (obj.data);
             else:
                 if obj.is_init_dir:
                     self.send_info (obj.data);
                 else:
                     self.rcv_info (obj.bytes);

         self.connection.close();
         self.connection = None


class CServer(CClientServerCommon) :
    def __init__ (self,pcapr,port):
        super(CServer, self).__init__()
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)

        server_address = ('', port)
        print 'starting up on %s port %s' % server_address
        sock.bind(server_address)
        sock.listen(1)

        self.pcapr=pcapr; # save the info 

        while True:
            # Wait for a connection
            print 'waiting for a connection'
            connection, client_address = sock.accept()

            try:
               print 'connection from', client_address
               self.connection = connection;

               self.process(True);
            finally:
                if self.connection :
                   self.connection.close()
                   self.connection = None


class CClient(CClientServerCommon):
    def __init__ (self,pcapr,ip,port):
        super(CClient, self).__init__()
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        #sock.setsockopt(socket.SOL_SOCKET,socket.TCP_MAXSEG,300)
        server_address = (ip, port)
        print 'connecting to %s port %s' % server_address

        sock.connect(server_address)
        self.connection=sock;
        self.pcapr=pcapr; # save the info 

        try:

            self.process(False);
        finally:
            if self.connection :
               self.connection.close()
               self.connection = None


def test_file_load ():
    pcapr= CPcapFileReader("delay_10_http_browsing_0.pcap")
    pcapr.check_tcp_flow ()
    pcapr.check_one_flow ()
    pcapr.dump_info();


def process_options ():
    parser = argparse.ArgumentParser(usage=""" 
    sock [-s|-c] -f file_name 

    """,
                                    description="offline process a pcap file",
                                    epilog=" written by hhaim");

    parser.add_argument("-f",  dest="file_name",
                        help=""" the file name to process """,
                        required=True)

    parser.add_argument('-c', action='store_true',
                        help='client side')

    parser.add_argument('-s', action='store_true',
                        help='server side  ')

    parser.add_argument('--fix-time', action='store_true',
                        help='fix_time  ')

    parser.add_argument('--port', type=int,  default=1000,
                        help='server_port  ')

    parser.add_argument('--ip',   default='127.0.0.1',
                        help='socket ip  ')

    parser.add_argument('--debug', action='store_true',
                        help='debug mode')

    parser.add_argument('--version', action='version',
                        version=H_SCRIPT_VER )



    sock_driver.args = parser.parse_args();

    if sock_driver.args.fix_time :
        return ;
    if (sock_driver.args.c ^ sock_driver.args.s) ==0:
        raise Exception ("you must set either client or server mode");

def load_pcap_file ():
    pcapr= CPcapFileReader(sock_driver.args.file_name)
    pcapr.check_tcp_flow ()
    pcapr.check_one_flow ()
    pcapr.dump_info();
    return pcapr

def run_client_side ():
    pcapr=load_pcap_file ()
    socket_client = CClient(pcapr,sock_driver.args.ip,sock_driver.args.port);


def run_server_side ():
    pcapr=load_pcap_file ()
    socket_server = CServer(pcapr,sock_driver.args.port);


class CPktWithTime:
    def __init__ (self,pkt,ts):
        self.pkt=pkt;
        self.ts=ts
    def __cmp__ (self,other):
        return cmp(self.ts,other.ts);

    def __repr__ (self):
        s=" %x:%d" %(self.pkt,self.ts)
        return s;


class CPcapFixTime:
    def __init__ (self,in_file_name,
                       out_file_name):
        self.in_file_name  = in_file_name;
        self.out_file_name = out_file_name;
        self.tuple=None;
        self.swap=False;
        self.rtt =0;
        self.rtt_syn_ack_ack =0; # ack on the syn ack 
        self.pkts=[] 

    def calc_rtt (self):
        f = open(self.in_file_name)
        pcap = dpkt.pcap.Reader(f)
        cnt=0;
        first_time_set=False;
        first_time=0;
        last_syn_time=0;
        rtt=0;
        rtt_syn_ack_ack=0;

        for ts, buf in pcap:
            eth = dpkt.ethernet.Ethernet(buf)
            ip = eth.data
            tcp = ip.data

            if first_time_set ==False:
                first_time=ts;
                first_time_set=True;
            else:
                rtt=ts-first_time;

            if ip.p != 6 :
                raise Exception("not a TCP flow ..");

            if cnt==0 or cnt==1:
                if (tcp.flags & dpkt.tcp.TH_SYN)  != dpkt.tcp.TH_SYN :
                    raise Exception("first packet should be with SYN");

            if cnt==1:
                last_syn_time=ts;

            if cnt==2:
                rtt_syn_ack_ack=ts-last_syn_time;

            if cnt > 1 :
                break;
            cnt = cnt +1;

        f.close();
        self.rtt_syn_ack_ack = rtt_syn_ack_ack;
        return (rtt);

    def is_client_side (self,swap):
        if self.swap ==swap:
            return (True);
        else:
            return (False);

    def calc_timing (self):
        self.rtt=self.calc_rtt ();

    def fix_timing (self):

        rtt=self.calc_rtt ();
        print "RTT is %f msec" % (rtt*1000)

        if (rtt/2)*1000<5:
            raise Exception ("RTT is less than 5msec, you should replay it");

        time_to_center=rtt/4; 

        f = open(self.in_file_name)
        fo = open(self.out_file_name,"wb")
        pcap = dpkt.pcap.Reader(f)
        pcap_out = dpkt.pcap.Writer(fo)

        for ts, buf in pcap:
            eth = dpkt.ethernet.Ethernet(buf)
            ip = eth.data
            tcp = ip.data
            pld = tcp.data;

            pkt_swap=False
            if nl(ip.src) > nl(ip.dst):
                pkt_swap=True
                tuple= (nl(ip.dst),nl(ip.src), tcp.dport ,tcp.sport,ip.p );
            else:
                tuple= (nl(ip.src),nl(ip.dst) ,tcp.sport,tcp.dport,ip.p );

            if self.tuple == None:
                self.swap=pkt_swap
                self.tuple=tuple
            else:
                if self.tuple != tuple:
                    raise Exception("More than one flow - can't process this flow");

            if self.is_client_side (pkt_swap):
                self.pkts.append(CPktWithTime( buf,ts+time_to_center));
            else:
                self.pkts.append(CPktWithTime( buf,ts-time_to_center));

        self.pkts.sort();
        for pkt in self.pkts:
            pcap_out.writepkt(pkt.pkt, pkt.ts)

        f.close()
        fo.close();

    

def main ():
    process_options ()

    if sock_driver.args.fix_time:
        pcap = CPcapFixTime(sock_driver.args.file_name ,sock_driver.args.file_name+".fix.pcap")
        pcap.fix_timing ()

    if sock_driver.args.c:
        run_client_side ();

    if sock_driver.args.s:
        run_server_side ();


files_to_convert=[
'citrix_0',
'exchange_0',
'http_browsing_0',
'http_get_0',
'http_post_0',
'https_0',
'mail_pop_0',
'mail_pop_1',
'mail_pop_2',
'oracle_0',
'rtsp_0',
'smtp_0',
'smtp_1',
'smtp_2'
];


#files_to_convert=[
#'http_browsing_0',
#];

def test_pcap_file ():
    for file in files_to_convert:
        fn='tun_'+file+'.pcap';
        fno='_tun_'+file+'_fixed.pcap';
        print "convert ",fn
        pcap = CPcapFixTime(fn,fno)
        pcap.fix_timing ()




class CPcapFileState:
    def __init__ (self,file_name):
        self.file_name = file_name
        self.is_one_tcp_flow = False;
        self.is_rtt_valid = False;
        self.rtt=0;
        self.rtt_ack=0;

    def calc_stats (self):
        file = CPcapFileReader(self.file_name);
        try:
            file.check_tcp_flow()
            file.check_one_flow ()
            self.is_one_tcp_flow = True;
        except Exception :
            self.is_one_tcp_flow = False;

        print self.is_one_tcp_flow
        if self.is_one_tcp_flow :
            pcap= CPcapFixTime(self.file_name,"");
            try:
                pcap.calc_timing ()
                print "rtt : %d %d \n" % (pcap.rtt*1000,pcap.rtt_syn_ack_ack*1000);
                if (pcap.rtt*1000) > 10 and (pcap.rtt_syn_ack_ack*1000) >0.0 and   (pcap.rtt_syn_ack_ack*1000) <2.0 :
                    self.is_rtt_valid = True
                    self.rtt = pcap.rtt*1000;
                    self.rtt_ack =pcap.rtt_syn_ack_ack*1000;
            except Exception :
                pass;


def test_pcap_file (file_name):
    p= CPcapFileState(file_name)
    p.calc_stats(); 
    if p.is_rtt_valid:
        return True
    else:
        return False

def iterate_tree_files (dirwalk,path_to):
    fl=open("res.csv","w+");
    cnt=0;
    cnt_valid=0
    for root, _, files in os.walk(dirwalk):
        for f in files:
            fullpath = os.path.join(root, f)
            p= CPcapFileState(fullpath)
            p.calc_stats(); 

            valid=test_pcap_file (fullpath)
            s='%s,%d,%d,%d \n' %(fullpath,p.is_rtt_valid,p.rtt,p.rtt_ack)
            cnt = cnt +1 ;
            if p.is_rtt_valid:
                cnt_valid =  cnt_valid +1;
                diro=path_to+"/"+root;
                fo = os.path.join(diro, f)
                os.system("mkdir -p "+ diro);
                pcap = CPcapFixTime(fullpath,fo)
                pcap.fix_timing ()

            print s
            fl.write(s);
    print " %d %% %d valids \n" % (100*cnt_valid/cnt,cnt);
    fl.close();

path_code="/scratch/tftp/pFidelity/pcap_repository"

iterate_tree_files (path_code,"output")
#test_pcap_file ()
#test_pcap_file ()
#main();

