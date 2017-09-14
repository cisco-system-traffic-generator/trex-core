# hhaim 2015
import sys 
import os
import argparse;
import uuid
import urllib2



H_COV_VER = "0.0.1"

class cov_driver(object):
     args=None;


BUILD_NUM_FILE  = "../VERSION" 

COV_FILE_OUT = 'trex-64.bz2'

#'http://www.python.org/'
def check_url_is_valid (url):
    try:
      f = urllib2.urlopen(url)
      f.read()
      return 0
    except :
       return -1


def get_build_num ():
 s='';
 if os.path.isfile(BUILD_NUM_FILE):
     f=open(BUILD_NUM_FILE,'r');
     s+=f.readline().rstrip();
     f.close();
 return s;

def get_build_num_dis ():
     return get_build_num ()+ "-"+str(uuid.uuid1())


def process_options ():
    parser = argparse.ArgumentParser(usage=""" 
    cov -b # build  sa
    cov -u  #upload sa
    """,
   description="sa utility ",
    epilog=" written by hhaim");


    parser.add_argument('-b', action='store_true',
                        help='build ')
    parser.add_argument('-u', action='store_true',
                        help='upload  ')
    parser.add_argument('-nc', action='store_true',
                        help='build without clean  ')

    parser.add_argument('--version', action='version',
                        version=H_COV_VER )

    cov_driver.args = parser.parse_args();




def run_cmd (cmd,is_exception=True):
    print ("run cmd '%s'" % (cmd))
    res=os.system(cmd);
    if is_exception and (res !=0):
        s= "ERORR cmd return error !";
        raise Exception(s);
    else:
        print("OK")


def run_build (is_clean):
    clean_str = ""
    if is_clean :
        clean_str = "clean"
    cov_build_cmd = cov_driver.tool_path+"cov-analysis-linux64-2017.07/bin/cov-build --dir cov-int ./b %s build --target=_t-rex-64" % (clean_str);
    run_cmd(cov_build_cmd);
    if os.path.isfile(COV_FILE_OUT) :
        run_cmd(('rm %s' % COV_FILE_OUT));
    run_cmd("tar czvf %s cov-int" % COV_FILE_OUT);

def upload ():
    if not os.path.isfile(COV_FILE_OUT) :
        s="ERROR file %s does not exit try to build it " % (COV_FILE_OUT);
        raise Exception (s)
    if check_url_is_valid ('http://www.google.com/')<0:
        s="ERROR, You are under firewall, try from another build server";
        raise Exception (s)

    ver=get_build_num_dis ()
    cmd='curl --form token=fRIZZCAGD9TnkSiuxXiEAQ --form email='+cov_driver.user_name+'@cisco.com --form file=@./'+COV_FILE_OUT+'  --form version="'+ver+'" --form description="'+ver+'" https://scan.coverity.com/builds?project=cisco-system-traffic-generator%2Ftrex-core'
    run_cmd(cmd);
    print("You should get an email with the results")
    print("or visit http://scan.coverity.com/projects/cisco-system-traffic-generator-trex-core?tab=overview")

def check_env (env,err):
    if  os.environ.has_key(env) == False :
        s= "ERROR you should define %s, %s" % (env,err)
        raise Exception(s);


def main_cov ():
    args=cov_driver.args

    # default nothing was given 
    if args.b == False and args.u == False :
        run_build (True)
        upload ()

    if args.b :
        is_clean = not args.nc 
        run_build (is_clean)

    if args.u:
        upload ()


def main ():
    try:
        check_env ('NBAR_ENV',"should defined to the tools path")
        check_env ('USER',"should be defined as your user name")

        cov_driver.tool_path=os.environ['NBAR_ENV']
        cov_driver.user_name=os.environ['USER']
        process_options ()
        main_cov ()
        exit(0);
    except Exception as e:
        print(str(e))
        exit(-1);



if __name__ == "__main__":
    main();

    
