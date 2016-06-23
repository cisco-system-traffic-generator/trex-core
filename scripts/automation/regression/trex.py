#!/router/bin/python

import os
import sys
import subprocess
import misc_methods
import re
import signal
import time
from CProgressDisp import TimedProgressBar
from stateful_tests.tests_exceptions import TRexInUseError
import datetime

class CTRexScenario:
    modes            = set() # list of modes of this setup: loopback, virtual etc.
    server_logs      = False
    is_test_list     = False
    is_init          = False
    is_stl_init      = False
    trex_crashed     = False
    configuration    = None
    trex             = None
    stl_trex         = None
    stl_ports_map    = None
    stl_init_error   = None
    router           = None
    router_cfg       = None
    daemon_log_lines = 0
    setup_name       = None
    setup_dir        = None
    router_image     = None
    trex_version     = None
    scripts_path     = None
    benchmark        = None
    report_dir       = 'reports'
    # logger         = None
    test_types       = {'functional_tests': [], 'stateful_tests': [], 'stateless_tests': []}
    is_copied        = False
    GAManager        = None
    no_daemon        = False
    router_image     = None
    debug_image      = False

class CTRexRunner:
    """This is an instance for generating a CTRexRunner"""

    def __init__ (self, config_dict, yaml):
        self.trex_config = config_dict#misc_methods.load_config_file(config_file)
        self.yaml = yaml


    def get_config (self):
        """ get_config() -> dict

        Returns the stored configuration of the TRex server of the CTRexRunner instance as a dictionary
        """
        return self.trex_config

    def set_yaml_file (self, yaml_path):
        """ update_yaml_file (self, yaml_path) -> None

        Defines the yaml file to be used by the TRex.
        """
        self.yaml = yaml_path


    def generate_run_cmd (self, multiplier, cores, duration, nc = True, export_path="/tmp/trex.txt", **kwargs):
        """ generate_run_cmd(self, multiplier, duration, export_path) -> str

        Generates a custom running command for the kick-off of the TRex traffic generator.
        Returns a command (string) to be issued on the trex server

        Parameters
        ----------
        multiplier : float
            Defines the TRex multiplier factor (platform dependant)
        duration : int
            Defines the duration of the test
        export_path : str
            a full system path to which the results of the trex-run will be logged.

        """
        fileName, fileExtension = os.path.splitext(self.yaml)
        if self.yaml == None:
            raise ValueError('TRex yaml file is not defined')
        elif fileExtension != '.yaml':
            raise TypeError('yaml path is not referencing a .yaml file')

        if 'results_file_path' in kwargs:
            export_path = kwargs['results_file_path']

        trex_cmd_str = './t-rex-64 -c %d -m %f -d %d -f %s '

        if nc:
            trex_cmd_str = trex_cmd_str + ' --nc '

        trex_cmd = trex_cmd_str % (cores,
            multiplier,
            duration,
            self.yaml)
            # self.trex_config['trex_latency'])

        for key, value in kwargs.items():
            tmp_key = key.replace('_','-')
            dash = ' -' if (len(key)==1) else ' --'
            if value == True:
                trex_cmd += (dash + tmp_key)
            else:
                trex_cmd += (dash + '{k} {val}'.format( k = tmp_key, val =  value ))

        print("\nTRex COMMAND: ", trex_cmd)

        cmd = 'sshpass.exp %s %s root "cd %s; %s > %s"' % (self.trex_config['trex_password'],
            self.trex_config['trex_name'],
            self.trex_config['trex_version_path'],
            trex_cmd,
            export_path)

        return cmd;

    def generate_fetch_cmd (self, result_file_full_path="/tmp/trex.txt"):
        """ generate_fetch_cmd(self, result_file_full_path) -> str

        Generates a custom command for which will enable to fetch the resutls of the TRex run.
        Returns a command (string) to be issued on the trex server.

        Example use: fetch_trex_results()                                   -   command that will fetch the content from the default log file- /tmp/trex.txt
                     fetch_trex_results("/tmp/trex_secondary_file.txt")     -   command that will fetch the content from a custom log file- /tmp/trex_secondary_file.txt
        """
        #dir = os.path.dirname(os.path.abspath(inspect.getfile(inspect.currentframe())))
        script_running_dir = os.path.dirname(os.path.realpath(__file__))    # get the current script working directory so that the sshpass could be accessed.
        cmd = script_running_dir + '/sshpass.exp %s %s root "cat %s"' % (self.trex_config['trex_password'],
            self.trex_config['trex_name'],
            result_file_full_path);
        return cmd;



    def run (self, multiplier, cores, duration, **kwargs):
        """ run(self, multiplier, duration, results_file_path) -> CTRexResults

        Running the TRex server based on the config file.
        Returns a CTRexResults object containing the results of the run.

        Parameters
        ----------
        multiplier : float
            Defines the TRex multiplier factor (platform dependant)
        duration : int
            Defines the duration of the test
        results_file_path : str
            a full system path to which the results of the trex-run will be logged and fetched from.

        """
        tmp_path = None
        # print kwargs
        if 'export_path' in kwargs:
            tmp_path = kwargs['export_path']
            del kwargs['export_path']
            cmd = self.generate_run_cmd(multiplier, cores, duration, tmp_path, **kwargs)
        else:
            cmd = self.generate_run_cmd(multiplier, cores, duration, **kwargs)

#       print 'TRex complete command to be used:'
#       print cmd
        # print kwargs

        progress_thread = TimedProgressBar(duration)
        progress_thread.start()
        interrupted = False
        try:
            start_time = time.time()
            start = datetime.datetime.now()
            results = subprocess.call(cmd, shell = True, stdout = open(os.devnull, 'wb'))
            end_time = time.time()
            fin = datetime.datetime.now()
            # print "Time difference : ", fin-start
            runtime_deviation =  abs(( (end_time - start_time)/ (duration+15) ) - 1)
            print("runtime_deviation: %2.0f %%" % ( runtime_deviation*100.0))
            if (   runtime_deviation  > 0.6 )  :
                # If the run stopped immediately - classify as Trex in use or reachability issue
                interrupted = True
                if ((end_time - start_time) < 2):
                    raise TRexInUseError ('TRex run failed since TRex is used by another process, or due to reachability issues')
                else:
                    CTRexScenario.trex_crashed = True
            # results = subprocess.Popen(cmd, stdout = open(os.devnull, 'wb'),
            #            shell=True, preexec_fn=os.setsid)
        except KeyboardInterrupt:
            print("\nTRex test interrupted by user during traffic generation!!")
            results.killpg(results.pid, signal.SIGTERM)  # Send the kill signal to all the process groups
            interrupted = True
            raise RuntimeError
        finally:
            progress_thread.join(isPlannedStop = (not interrupted) )

        if results!=0:
            sys.stderr.write("TRex run failed. Please Contact trex-dev mailer for further details")
            sys.stderr.flush()
            return None
        elif interrupted:
            sys.stderr.write("TRex run failed due user-interruption.")
            sys.stderr.flush()
            return None
        else:

            if tmp_path:
                cmd = self.generate_fetch_cmd( tmp_path )#**kwargs)#results_file_path)
            else:
                cmd = self.generate_fetch_cmd()

            try:
                run_log = subprocess.check_output(cmd, shell = True)
                trex_result = CTRexResult(None, run_log)
                trex_result.load_file_lines()
                trex_result.parse()

                return trex_result

            except subprocess.CalledProcessError:
                sys.stderr.write("TRex result fetching failed. Please Contact trex-dev mailer for further details")
                sys.stderr.flush()
                return None

class CTRexResult():
    """This is an instance for generating a CTRexResult"""
    def __init__ (self, file, buffer = None):
        self.file = file
        self.buffer = buffer
        self.result = {}


    def load_file_lines (self):
        """ load_file_lines(self) -> None

        Loads into the self.lines the content of self.file
        """
        if self.buffer:
            self.lines = self.buffer.split("\n")
        else:
            f = open(self.file,'r')
            self.lines = f.readlines()
            f.close()


    def dump (self):
        """ dump(self) -> None

        Prints nicely the content of self.result dictionary into the screen
        """
        for key, value in self.result.items():
            print("{0:20} : \t{1}".format(key, float(value)))

    def update (self, key, val, _str):
        """ update (self, key, val, _str) -> None

        Updates the self.result[key] with a possibly new value representation of val
        Example: 15K might be updated into 15000.0

        Parameters
        ----------
        key :
            Key of the self.result dictionary of the TRexResult instance
        val : float
            Key of the self.result dictionary of the TRexResult instance
        _str : str
            a represntation of the BW (.

        """

        s = _str.strip()

        if s[0]=="G":
            val = val*1E9
        elif s[0]=="M":
            val = val*1E6
        elif s[0]=="K":
            val = val*1E3

        if key in self.result:
            if self.result[key] > 0:
                if (val/self.result[key] > 0.97 ):
                    self.result[key]= val
            else:
                self.result[key] = val
        else:
            self.result[key] = val



    def parse (self):
        """ parse(self) -> None

        Parse the content of the result file from the TRex test and upload the data into
        """
        stop_read = False
        d = {
            'total-tx'      : 0,
            'total-rx'      : 0,
            'total-pps'     : 0,
            'total-cps'     : 0,

            'expected-pps'  : 0,
            'expected-cps'  : 0,
            'expected-bps'  : 0,
            'active-flows'  : 0,
            'open-flows'    : 0
        }

        self.error = ""

        # Parse the output of the test, line by line (each line matches another RegEx and as such
        # different rules apply
        for line in self.lines:
            match = re.match(".*/var/run/.rte_config.*", line)
            if match:
                stop_read = True
                continue

            #Total-Tx        :     462.42 Mbps   Nat_time_out    :        0   ==> we try to parse the next decimal in this case Nat_time_out
#           match = re.match("\W*(\w(\w|[-])+)\W*([:]|[=])\W*(\d+[.]\d+)\W*\w+\W+(\w+)\W*([:]|[=])\W*(\d+)(.*)", line);
#           if match:
#               key = misc_methods.mix_string(match.group(5))
#               val = float(match.group(7))
#               # continue to parse !! we try the second
#               self.result[key] = val #update latest

            # check if we need to stop reading
            match = re.match(".*latency daemon has stopped.*", line)
            if match:
                stop_read = True
                continue

            match = re.match("\W*(\w(\w|[-])+)\W*([:]|[=])\W*(\d+[.]\d+)(.*ps)\s+(\w+)\W*([:]|[=])\W*(\d+)", line)
            if match:
                key = misc_methods.mix_string(match.group(1))
                val = float(match.group(4))
                if key in d:
                   if stop_read == False:
                       self.update (key, val, match.group(5))
                else:
                    self.result[key] = val # update latest
                key2 = misc_methods.mix_string(match.group(6))
                val2 = int(match.group(8))
                self.result[key2] = val2 # always take latest


            match = re.match("\W*(\w(\w|[-])+)\W*([:]|[=])\W*(\d+[.]\d+)(.*)", line)
            if match:
               key = misc_methods.mix_string(match.group(1))
               val = float(match.group(4))
               if key in d:
                   if stop_read == False:
                       self.update (key, val, match.group(5))
               else:
                    self.result[key] = val # update latest
               continue

            match = re.match("\W*(\w(\w|[-])+)\W*([:]|[=])\W*(\d+)(.*)", line)
            if match:
                key = misc_methods.mix_string(match.group(1))
                val = float(match.group(4))
                self.result[key] = val #update latest
                continue

            match = re.match("\W*(\w(\w|[-])+)\W*([:]|[=])\W*(OK)(.*)", line)
            if match:
                key = misc_methods.mix_string(match.group(1))
                val = 0 # valid
                self.result[key] = val #update latest
                continue

            match = re.match("\W*(Cpu Utilization)\W*([:]|[=])\W*(\d+[.]\d+)  %(.*)", line)
            if match:
                key = misc_methods.mix_string(match.group(1))
                val = float(match.group(3))
                if key in self.result:
                    if (self.result[key] < val): # update only if larger than previous value
                        self.result[key] = val
                else:
                    self.result[key] = val
                continue

            match = re.match(".*(rx_check\s.*)\s+:\s+(\w+)", line)
            if match:
                key = misc_methods.mix_string(match.group(1))
                try:
                    val = int(match.group(2))
                except ValueError: # corresponds with rx_check validation case
                    val = match.group(2)
                finally:
                    self.result[key] = val
                continue


    def get_status (self, drop_expected = False):
        if (self.error != ""):
            print(self.error)
            return (self.STATUS_ERR_FATAL)

        d = self.result

        # test for latency
        latency_limit = 5000
        if ( d['maximum-latency'] > latency_limit ):
            self.reason="Abnormal latency measured (higher than %s" % latency_limit
            return self.STATUS_ERR_LATENCY

        # test for drops
        if drop_expected == False:
            if ( d['total-pkt-drop'] > 0 ):
                self.reason=" At least one packet dropped "
                return self.STATUS_ERR_DROP

        # test for rx/tx distance
        rcv_vs_tx = d['total-tx']/d['total-rx']
        if ( (rcv_vs_tx >1.2) or (rcv_vs_tx <0.9) ):
            self.reason="rx and tx should be close"
            return self.STATUS_ERR_RX_TX_DISTANCE

        # expected measurement
        expect_vs_measued=d['total-tx']/d['expected-bps']
        if ( (expect_vs_measued >1.1) or (expect_vs_measued < 0.9) ) :
            print(expect_vs_measued)
            print(d['total-tx'])
            print(d['expected-bps'])
            self.reason="measure is not as expected"
            return self.STATUS_ERR_BAD_EXPECTED_MEASUREMENT

        if ( d['latency-any-error'] !=0 ):
            self.reason=" latency-any-error has error"
            return self.STATUS_ERR_LATENCY_ANY_ERROR

        return self.STATUS_OK

        # return types
        STATUS_OK = 0
        STATUS_ERR_FATAL = 1
        STATUS_ERR_LATENCY = 2
        STATUS_ERR_DROP = 3
        STATUS_ERR_RX_TX_DISTANCE = 4
        STATUS_ERR_BAD_EXPECTED_MEASUREMENT = 5,
        STATUS_ERR_LATENCY_ANY_ERROR = 6

def test_TRex_result_parser():
    t=CTRexResult('trex.txt');
    t.load_file_lines()
    t.parse()
    print(t.result)




if __name__ == "__main__":
    #test_TRex_result_parser();
    pass
