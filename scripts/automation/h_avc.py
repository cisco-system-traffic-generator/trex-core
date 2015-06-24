#!/router/bin/python-2.4.3
import time,os, sys, string
from os.path import exists
from os import system, remove, chdir
import re
import time
import random
import copy
import telnetlib
import datetime
import collections
from trex_perf import TrexRunException


import random
import time

class RouterIOException(Exception):
    def __init__ (self, reason):
        # generate the error message
        #self.message = "\nSummary of error:\n\n %s\n" % (reason)
        self.message = reason

    def __str__(self):
        return self.message

# basic router class
class Router:
    def __init__ (self, host, port, password, str_wait = "#"):
        self.host = host
        self.port = port;
        self.pr = str_wait;
        self.password = password
        self.to = 60
        self.cpu_util_histo = []

    # private function - command send
    def _command (self, command, match = None, timeout = None):
        to = timeout if (timeout != None) else self.to
        m = match if (match != None) else [self.pr]

        if not isinstance(m, list):
            m = [m]

        total_to = 0
        while True:
            self.tn.write(command + "\n")
            ret = self.tn.expect(m, timeout = 2)
            total_to += 2

            if ret[0] != -1:
                result = {}
                result['match_index'] = ret[0]
                result['output'] = ret[2]
                return (result)

            if total_to >= self.to:
                raise RouterIOException("Failed to process command to router %s" % command)

    # connect to router by telnet
    def connect (self):
        # create telnet session
        self.tn = telnetlib.Telnet ()

        try:
            self.tn.open(self.host, self.port)
        except IOError:
            raise RouterIOException("Failed To Connect To Router interface at '%s' : '%s'" % (self.host, self.port))

        # get a ready console and decides if you need password
        ret = self._command("", ["Password", ">", "#"])
        if ret['match_index'] == 0:
            self._command(self.password, [">", "#"])

        # can't hurt to call enable even if on enable
        ret = self._command("enable 15", ["Password", "#"])
        if (ret['match_index'] == 0):
            self._command(self.password, "#")

        self._command("terminal length 0")

    def close (self):
        self.tn.close ()
        self.tn = None

    # implemented through derived classes
    def sample_cpu (self):
        raise Exception("abstract method called")

    def get_last_cpu_util (self):
        if not self.cpu_util_histo:
            return (0)
        else:
            return self.cpu_util_histo[len(self.cpu_util_histo) - 1]

    def get_cpu_util_histo (self):
        return self.cpu_util_histo

    def get_filtered_cpu_util_histo (self):
        trim_start = int(0.15 * len(self.cpu_util_histo))

        filtered = self.cpu_util_histo[trim_start:]
        if not filtered:
            return [0]

        m = collections.Counter(filtered).most_common(n = 1)[0][0]
        #m = max(self.cpu_util_histo)
        filtered = [x for x in filtered if (x > (0.9*m))]
        return filtered

    def clear_sampling_stats (self):
        self.cpu_util_histo = []


    # add a sample to the database
    def sample_stats (self):
        # sample CPU util
        cpu_util = self.sample_cpu()
        self.cpu_util_histo.append(cpu_util)

    def get_stats (self):
        stats = {}

        filtered_cpu_util = self.get_filtered_cpu_util_histo()

        if not filtered_cpu_util:
            stats['cpu_util'] = 0
        else:
            stats['cpu_util'] = sum(filtered_cpu_util)/len(filtered_cpu_util)

        stats['cpu_histo'] = self.get_cpu_util_histo()

        return stats


class ASR1k(Router):
    def __init__ (self, host, password, port, str_wait = "#"):
        Router.__init__(self, host, password, port, str_wait)

    def sample_cpu (self):
        cpu_show_cmd = "show platform hardware qfp active datapath utilization | inc Load"
        output = self._command(cpu_show_cmd)['output']
        lines = output.split('\n');

        cpu_util = -1.0
        # search for the line 
        for l in lines:
            m = re.match("\W*Processing: Load\D*(\d+)\D*(\d+)\D*(\d+)\D*(\d+)\D*", l)
            if m:
                cpu_util = float(m.group(1))

        if (cpu_util == -1.0):
            raise Exception("cannot determine CPU util. for asr1k")

        return cpu_util


class ISR(Router):
    def __init__ (self, host, password, port, str_wait = "#"):
        Router.__init__(self, host, password, port, str_wait)

    def sample_cpu (self):
        cpu_show_cmd = "show processes cpu sorted | inc CPU utilization"
        output = self._command(cpu_show_cmd)['output']
        lines = output.split('\n');

        cpu_util = -1.0

        # search for the line 
        for l in lines:
            m = re.match("\W*CPU utilization for five seconds: (\d+)%/(\d+)%", l)
            if m:
                max_cpu_util = float(m.group(1))
                min_cpu_util = float(m.group(2))
                cpu_util = (min_cpu_util + max_cpu_util)/2

        if (cpu_util == -1.0):
            raise Exception("cannot determine CPU util. for ISR")

        return cpu_util



if __name__ == "__main__":
    #router = ASR1k("pqemb19ts", "cisco", port=2052)
    router = ISR("10.56.198.7", "lab")
    router.connect()
    for i in range(1, 10):
        router.sample_stats()
        time.sleep(1)





