import sys,os
import re
import argparse


class doc_driver(object):
     args=None;

def  str_hex_to_ip(ip_hex):
    bytes = ["".join(x) for x in zip(*[iter(ip_hex)]*2)]
    bytes = [int(x, 16) for x in bytes]
    s=".".join(str(x) for x in bytes)
    return s

class CTemplateFile:
    def __init__ (self):
        self.l=[];

    def dump (self):
        print "pkt,time sec,template,fid,flow-pkt-id,client_ip,client_port,server_ip ,desc"
        for obj in self.l:
            s='';
            if obj[7]=='1':
                s ='->'
            else:
                s ='<-'

            print obj[0],",",obj[1],",",obj[6],",", obj[2],",",obj[4],",",str_hex_to_ip(obj[11]),",",obj[13],",",str_hex_to_ip(obj[12]),",",s

    def dump_sj (self):
        flows=0;
        print "["
        for obj in self.l:
            print "[",
            print obj[1],",",int("0x"+obj[2],16),",",obj[6],",",obj[4],
            print "],"

        print "]"



    def load_file (self,file):
        f=open(file,'r');
        x=f.readlines()
        f.close();
        found=False;
        for line in x:
            if found :
               line=line.rstrip('\n');
               info=line.split(',');
               if len(info)>5:
                   self.l+=[info];
               else:
                   break;
            else:
                if 'pkt_id' in line:
                    found=True;




def do_main ():
    temp = CTemplateFile();
    temp.load_file(doc_driver.args.file)
    temp.dump();
    temp.dump_sj();


def process_options ():
    parser = argparse.ArgumentParser(usage=""" 
    doc_process -f <debug_output> 
    """,
    description="process bp-sim output for docomentation",
    epilog=" written by hhaim");

    parser.add_argument("-f", "--file", dest="file",
                        metavar="file",
                        required=True)

    parser.add_argument('--version', action='version',
                        version="0.1" )

    doc_driver.args = parser.parse_args();



def main ():
    try:
        process_options ();
        do_main ()
        exit(0);
    except Exception, e:
        print str(e);
        exit(-1);



if __name__ == "__main__":
    main()










