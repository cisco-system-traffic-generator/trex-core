# -*- coding: utf-8 -*-

"""
Copyright (c) 2017-2017 Cisco Systems, Inc.
Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at
    http://www.apache.org/licenses/LICENSE-2.0
Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
"""
import argparse
import os
import sys
import subprocess
from pprint import pprint
import shlex
from subprocess import Popen, PIPE
import json
import pprint  
from .trex_astf_exceptions import ASTFError

class map_driver(object):
    opts=None;


def run_cmd (cmd):
    process = Popen(shlex.split(cmd), stdout=PIPE)
    (output, err) = process.communicate()
    output=output.decode("utf-8") 
    exit_code = process.wait()
    return (exit_code,output, err)


class TsharkFileProcess(object):
    def __init__(self, file_name,is_tcp):
        self.file_name = file_name
        self.is_tcp    = is_tcp

    def is_valid_tcp (self):
        res=(self.is_tcp_one_flow() and self.is_tcp_file_is_clean());
        return (res);

    def is_tcp_file_is_clean (self):
        file = self.file_name;
        cmd='tshark -r %s -R "tcp.analysis.out_of_order or tcp.analysis.duplicate_ack or tcp.analysis.retransmission or tcp.analysis.fast_retransmission or tcp.analysis.ack_lost_segment or tcp.analysis.keep_alive"  ' % (file);
        (exit_code,output, err)=run_cmd (cmd)
        if exit_code==0:
             if len(output)==0:
                 return True;
        return False

    def is_tcp_one_flow (self):
        file = self.file_name;
        cmd='tshark -r %s -T fields -e tcp.stream ' % (file);
        (exit_code,output, err)=run_cmd (cmd)
        if exit_code==0:
             return self._proces_output_is_one_tcp_flow(output)
        return False

    def _proces_output_is_one_tcp_flow (self,output):
        found=False;
        cnt=0;
        l=output.split("\n")
        for line in l:
            last=False;
            if line.isdigit() :
                if int(line)==0:
                    found=True;
                else:
                    found=False
                    break;
            else:
                cnt=cnt+1
                last=True;
    
        return ((cnt==0) or (cnt==1) and (last==True)) and found;



#def main(args=None):
#    for root, subdirs, files in os.walk(PCAP_DIR):
#        for filename in files:
#            file=os.path.join(root, filename);
#            if process_file_name (file):
#                print file;

profile="""
from trex_astf_lib.api import *


class Prof1():
    def __init__(self):
        pass

    def get_profile(self):

        # ip generator
        ip_gen_c = ASTFIPGenDist(ip_range=["16.0.0.0", "16.0.0.255"], distribution="seq")
        ip_gen_s = ASTFIPGenDist(ip_range=["48.0.0.0", "48.0.255.255"], distribution="seq")
        ip_gen = ASTFIPGen(glob=ASTFIPGenGlobal(ip_offset="1.0.0.0"),
                           dist_client=ip_gen_c,
                           dist_server=ip_gen_s)

        return ASTFProfile(default_ip_gen=ip_gen,
                            cap_list=[ASTFCapInfo(file="%s",
                            cps=2.776)])


def register():
    return Prof1()
""";



def tshark_stream_process (output):
    a=output.split("\n")[6:]
    s="\n".join(a)
    return convert_file_l7 (s)


def get_payload_data (file):
    cmd='tshark  -nr %s -q -z follow,tcp,raw,0 ' % (file);
    (exit_code,output, err)=run_cmd (cmd)
    if exit_code==0:
         return tshark_stream_process (output)
    raise ASTFError("can't get payload ")

def compare_l7_data (file_a,file_b):
    (s1,c1)=get_payload_data (file_a)
    (s2,c2)=get_payload_data (file_b)
    if s1==s2:
        if c1==c2:
            return (True,c1)
        else:
            print("ERROR counters are not the same !\n");
            print(c1)
            print(c2)
            return (False,[0,0]);
    else:
        print(c1)
        print(c2)
        print("FILES not the same !\n");
        print(" FILE ## 1 - f1.txt")
        f=open("f1.txt","w+");
        f.write(s1);
        f.close();
        print(" FILE ## 2 - f2.txt")
        f=open("f2.txt","w+");
        f.write(s2);
        f.close();
        return (False,[0,0]);


def process_sim_to_json (output):
    lines=output.split("\n");
    next=False;
    for l in lines:
        if next==True:
            return(json.loads(l));
        if l.strip()=="json-start":
            next=True;
    raise ASTFError("can't find json output ")



class SimCounter(object):
    def __init__(self, json):
        self.m_json =json;

    def is_any_error (self):
        j=self.m_json;
        sides=['client','server']
        for s in sides:
            if 'err' in j['data'][s]:
                return(True)
        return(False)

    def get_error_str (self):
        res="";
        j=self.m_json;
        sides=['client','server']
        for s in sides:
            if 'err' in j['data'][s]:
                res+=pprint.pformat( j['data'][s]['err']);
        return(res)


    def dump(self):
        pprint.pprint(self.m_json);

    def compare_counter (self,c,name,val):
        read_val=c.get(name,0);
        if (read_val != val):
            raise ASTFError("counter {0} read {1} expect {2}".format(name,read_val,val));

    def _match (self,_str,val1,val2):
        if (val1 != val2):
            raise ASTFError("counter-name{0} read {1} expect {2}".format(_str,val1,val2));


    def get_bytes (self,side):
        j=self.m_json;
        c=j['data'][side]['all'];
        self.compare_counter(c,'tcps_connects',1)
        self.compare_counter(c,'tcps_closed',1)
        if side=='client':
            self.compare_counter(c,'tcps_connattempt',1)
        else:
            self.compare_counter(c,'tcps_accepts',1)

        return (c.get('tcps_sndbyte',0),c.get('tcps_rcvbyte',0))


    def compare (self,tx_bytes,rx_bytes):
        (ctx,crx)=self.get_bytes ('client')
        (stx,srx)=self.get_bytes ('server')
        print("client [%d,%d]" %(ctx,crx));
        print("server [%d,%d]" %(stx,srx));
        print("expect [%d,%d]" %(tx_bytes,rx_bytes));

        self._match("client.tx==server.rx",ctx,srx);
        self._match("client.rx==server.tx",crx,stx);
        self._match("client.tx==pcap.rx",ctx,tx_bytes);
        self._match("client.rx==pcap.rx",crx,rx_bytes);


def compare_counters (output,tx_bytes,
                             rx_bytes,skip_errors):
    json=process_sim_to_json(output)
    simc = SimCounter(json);
    
    if skip_errors==False:
        if map_driver.opts.skip_counter_err==False and  simc.is_any_error():
            raise ASTFError("ERROR counters has an error {0} ".format(simc.get_error_str ()));
    simc.compare (tx_bytes,rx_bytes)


def  _run_sim(file):
    ifile=file.rstrip();
    p =profile% (ifile);
    f=open("n.py",'w')
    lines=f.write(p);
    f.close();
    ofile="generated/"+os.path.split(ifile)[1]

    print("process: ", ifile)
    cmd=""
    if map_driver.opts.cmd:
        cmd=","+map_driver.opts.cmd

    cmd='./astf-sim --python3 -f n.py -o {0} -v -c="--sim-json{1}"'.format(ofile,cmd);
    print(cmd);
    (exit_code,output, err)=run_cmd (cmd)
    if map_driver.opts.verbose:
        print(output);

    if exit_code==100:
        print("SKIP3-TRex reason");
        return;

    if exit_code==0:
        file1 = ifile;
        file2 = ofile+"_c.pcap"
        file3 = ofile+"_s.pcap"
        (p,c)=compare_l7_data(file1,file2);
        if p!=True:
            raise ASTFError("ERROR {0} {1} are not the same".format(file1,file2));
        (p,c)=compare_l7_data(file2,file3)

        if p!=True:
            raise ASTFError("ERROR {0} {1} are not the same".format(file2,file3));

        compare_counters(output,c[0],c[1],False)
        print ("OK",c)
    else:
        raise ASTFError("ERROR running TRex {0} are not the same".format(output));

def  run_sim(file):
    skip=map_driver.opts.skip

    if not skip:
        _run_sim(file)
    else:
        try:
            _run_sim(file)
        except Exception as e:
            print(e);



def file_is_ok(f):
    a= TsharkFileProcess(f,True)
    return (a.is_valid_tcp());


def run_one_file (one_pcap_file):
    print("run-"+one_pcap_file);
    if file_is_ok (one_pcap_file):
        run_sim(one_pcap_file)
    else:
        print("SKIP1 "+one_pcap_file);

def load_files (files):
    f=open(files,'r')
    lines=f.readlines();
    f.close();
    for obj in lines:
        if (len(obj)>0) and (obj[0]=="#"):
            print("SKIP "+obj);
            continue;
        run_one_file (obj)


def tshark_trunc_line (line,dir):
    if dir==1:
        return(line[1:])
    else:
        return(line)


def tshark_add_line (line,dir):
    if dir==1:
        return("        "+line);
    else:
        return(line)
    
def convert_file_l7 (s):
    bytes=[0,0]
    l=s.split("\n");
    res=""
    last_dir=-1;
    cur="";
    for line in l:
        # set dir
        if len(line)==0:
            break;
        if line[0]=="=":
            break;
        if line[0]=='\x09':
            dir=1
        else:
            dir=0
        if (dir!=last_dir) and (last_dir!=-1):
            res+=(tshark_add_line(cur,dir^1)+"\n")
            cur="";

        lp=tshark_trunc_line(line,dir)

        bytes[dir]+=len(lp)/2;
        cur+=lp;
        last_dir=dir
    if len(cur)>0:
        res+=(tshark_add_line(cur,dir^1)+"\n")
    return(res,bytes);


            
def test_convert_l7 ():
    f=open("f1.txt","r");
    s=f.read()
    f.close();
    return (convert_file_l7 (s))


def run_sfr ():
    l=[
    "avl/delay_10_http_browsing_0.pcap",
    "avl/delay_10_http_get_0.pcap",
    "avl/delay_10_http_post_0.pcap",

    "avl/delay_10_https_0.pcap",
    "avl/delay_10_exchange_0.pcap",
    "avl/delay_10_mail_pop_0.pcap",
    "avl/delay_10_oracle_0.pcap", 
    "avl/delay_10_smtp_0.pcap", 
    "avl/delay_10_citrix_0.pcap"]
    for obj in l:
        run_sim(obj)



def setParserOptions():
    parser = argparse.ArgumentParser(prog="sim_utl.py",
    usage="""

    Examples:
    ---------
    ./astf-sim-utl --sfr  # run SFR profile 
    ./astf-sim-utl -f pcap-file.pcap [sim-option]  # run one pcap file 
    ./astf-sim-utl -f pcap-list.txt  [sim-option]  # run a list of pcap files 
    ./astf-sim-utl -f pcap-list.txt --cmd="--sim-mode=28,--sim-arg=0.1" --skip-counter-err  --dev
    
    
        """)

    parser.add_argument('-p', '--path',
                        help="BP sim path",
                        dest='bp_sim_path',
                        default=None,
                        type=str)


    parser.add_argument("-k", "--skip",
                        help="skip in case of an error",
                        action="store_true",
                        default=False)

    parser.add_argument('-v', '--verbose',
                        action="store_true",
                        help="Print output to screen")

    parser.add_argument('-d', '--dev',
                        action="store_true",
                        help="Print output to screen")


    parser.add_argument( "--skip-counter-err",
                        help="skip in case of an error",
                        action="store_true",
                        default=False)


    parser.add_argument("-c", "--cmd",
                        help="command to the simulator",
                        dest='cmd',
                        default=None,
                        type=str)
    group = parser.add_mutually_exclusive_group()

    group.add_argument("-f",
                        dest="input_file",
                        help="list of pcap files or one pcap file",
                        )

    group.add_argument("--sfr",
                        dest="sfr",
                        action="store_true",
                        default=False,
                        help="run sfr profile test ",
                        )
    return parser

def test1 ():
    run_sfr();

def run ():
    opts =map_driver.opts
    if opts.sfr:
        run_sfr ()
        return;

    f=opts.input_file;
    if not os.path.isfile(f) :
        raise ASTFError("ERROR {0} is not a file".format(f));
    extension = os.path.splitext(f)[1]
    if extension=='.txt':
        load_files (f)
        return;
    if extension in ['.pcap','.cap'] :
        run_one_file (f)
        return;
    raise ASTFError("ERROR {0} is not a file in the right format".format(f));


def main(args=None):

    parser = setParserOptions()
    if args is None:
        opts = parser.parse_args()
    else:
        opts = parser.parse_args(args)
    map_driver.opts = opts;

    if opts.dev:
        run();
    else:
      try:
        run();
      except Exception as e:
        print(e)
        sys.exit(1)

if __name__ == '__main__':
    main()






    




