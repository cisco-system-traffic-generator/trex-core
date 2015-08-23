#!/router/bin/python-2.7.4

import trex_root_path
from client.trex_client import *
from common.trex_exceptions import *
import cmd
from python_lib.termstyle import termstyle
import os
from argparse import ArgumentParser
from pprint import pprint
import json
import time
import socket
import errno

class InteractiveTRexClient(cmd.Cmd):

    intro = termstyle.green("\nInteractive shell to play with Cisco's T-Rex API.\nType help to view available pre-defined scenarios\n(c) All rights reserved.\n")
    prompt = '> '

    def __init__(self, trex_host, max_history_size = 100, trex_port = 8090, verbose_mode = False ):
        cmd.Cmd.__init__(self)
        self.verbose = verbose_mode
        self.trex = CTRexClient(trex_host, max_history_size, trex_daemon_port = trex_port, verbose = verbose_mode)
        self.DEFAULT_RUN_PARAMS = dict( m = 1.5,
                                        nc = True,
                                        p  = True,
                                        d = 100,   
                                        f = 'avl/sfr_delay_10_1g.yaml',
                                        l = 1000)
        self.run_params = dict(self.DEFAULT_RUN_PARAMS)
        self.decoder = json.JSONDecoder()


    def do_push_files (self, filepaths):
        """Pushes a custom file to be stored locally on T-Rex server.\nPush multiple files by spefiying their path separated by ' ' (space)."""
        try:
            filepaths = filepaths.split(' ')
            print termstyle.green("*** Starting pushing files ({trex_files}) to T-Rex. ***".format (trex_files = ', '.join(filepaths)) )
            ret_val = self.trex.push_files(filepaths)
            if ret_val:
                print termstyle.green("*** End of T-Rex push_files method (success) ***")
            else:
                print termstyle.magenta("*** End of T-Rex push_files method (failed) ***")

        except IOError as inst:
            print termstyle.magenta(inst)

    def do_show_default_run_params(self,line):
        """Outputs the default T-Rex running parameters"""
        pprint(self.DEFAULT_RUN_PARAMS)
        print termstyle.green("*** End of default T-Rex running parameters ***")

    def do_show_run_params(self,line):
        """Outputs the currently configured T-Rex running parameters"""
        pprint(self.run_params)
        print termstyle.green("*** End of T-Rex running parameters ***")

    def do_update_run_params(self, json_str):
        """Updates provided parameters on T-Rex running configuration. Provide using JSON string"""
        if json_str:
            try:
                upd_params = self.decoder.decode(json_str)
                self.run_params.update(upd_params)
                print termstyle.green("*** End of T-Rex parameters update ***")
            except ValueError as inst:
                print termstyle.magenta("Provided illegal JSON string. Please try again.\n[", inst,"]")
        else:
            print termstyle.magenta("JSON configuration string is missing. Please try again.")
    
    def do_show_status (self, line):
        """Prompts T-Rex current status"""
        print self.trex.get_running_status()
        print termstyle.green("*** End of T-Rex status prompt ***")

    def do_show_trex_files_path (self, line):
        """Prompts the local path in which files are stored when pushed to t-rex server from client"""
        print self.trex.get_trex_files_path()
        print termstyle.green("*** End of trex_files_path prompt ***")

    def do_show_reservation_status (self, line):
        """Prompts if T-Rex is currently reserved or not"""
        if self.trex.is_reserved():
            print "T-Rex is reserved"
        else:
            print "T-Rex is NOT reserved"
        print termstyle.green("*** End of reservation status prompt ***")

    def do_reserve_trex (self, user):
        """Reserves the usage of T-Rex to a certain user"""
        try:
            if not user:
                ret = self.trex.reserve_trex()
            else:
                ret = self.trex.reserve_trex(user.split(' ')[0])
            print termstyle.green("*** T-Rex reserved successfully ***")
        except TRexException as inst:
            print termstyle.red(inst)

    def do_cancel_reservation (self, user):
        """Cancels a current reservation of T-Rex to a certain user"""
        try:
            if not user:
                ret = self.trex.cancel_reservation()
            else:
                ret = self.trex.cancel_reservation(user.split(' ')[0])
            print termstyle.green("*** T-Rex reservation canceled successfully ***")
        except TRexException as inst:
            print termstyle.red(inst)

    def do_restore_run_default (self, line):
        """Restores original T-Rex running configuration"""
        self.run_params = dict(self.DEFAULT_RUN_PARAMS)
        print termstyle.green("*** End of restoring default run parameters ***")

    def do_run_until_finish (self, sample_rate):
        """Starts T-Rex and sample server until run is done."""
        print termstyle.green("*** Starting T-Rex run_until_finish scenario ***")

        if not sample_rate: # use default sample rate if not passed
            sample_rate = 5
        try:
            sample_rate = int(sample_rate)
            ret = self.trex.start_trex(**self.run_params)
            self.trex.sample_to_run_finish(sample_rate)
            print termstyle.green("*** End of T-Rex run ***")
        except ValueError as inst:
            print termstyle.magenta("Provided illegal sample rate value. Please try again.\n[", inst,"]")
        except TRexException as inst:
            print termstyle.red(inst)

    def do_run_and_poll (self, sample_rate):
        """Starts T-Rex and sample server manually until run is done."""
        print termstyle.green("*** Starting T-Rex run and manually poll scenario ***")
        if not sample_rate: # use default sample rate if not passed
            sample_rate = 5
        try:
            sample_rate = int(sample_rate)
            ret = self.trex.start_trex(**self.run_params)
            last_res = dict()
            while self.trex.is_running(dump_out = last_res):
                obj = self.trex.get_result_obj()
                if (self.verbose):
                    print obj
                # do WHATEVER here
                time.sleep(sample_rate)
                
            print termstyle.green("*** End of T-Rex run ***")
        except ValueError as inst:
            print termstyle.magenta("Provided illegal sample rate value. Please try again.\n[", inst,"]")
        except TRexException as inst:
            print termstyle.red(inst)


    def do_run_until_condition (self, sample_rate):
        """Starts T-Rex and sample server until condition is satisfied."""
        print termstyle.green("*** Starting T-Rex run until condition is satisfied scenario ***")

        def condition (result_obj):
                return result_obj.get_current_tx_rate()['m_tx_pps'] > 200000

        if not sample_rate: # use default sample rate if not passed
            sample_rate = 5
        try:
            sample_rate = int(sample_rate)
            ret = self.trex.start_trex(**self.run_params)
            ret_val = self.trex.sample_until_condition(condition, sample_rate)
            print ret_val
            print termstyle.green("*** End of T-Rex run ***")
        except ValueError as inst:
            print termstyle.magenta("Provided illegal sample rate value. Please try again.\n[", inst,"]")
        except TRexException as inst:
            print termstyle.red(inst)

    def do_start_and_return (self, line):
        """Start T-Rex run and once in 'Running' mode, return to cmd prompt"""
        print termstyle.green("*** Starting T-Rex run, wait until in 'Running' state ***")
        try:
            ret = self.trex.start_trex(**self.run_params)
            print termstyle.green("*** End of scenario (T-Rex is probably still running!) ***")
        except TRexException as inst:
            print termstyle.red(inst)

    def do_poll_once (self, line):
        """Performs a single poll of T-Rex current data dump (if T-Rex is running) and prompts and short version of latest result_obj"""
        print termstyle.green("*** Trying T-Rex single poll ***")
        try:
            last_res = dict()
            if self.trex.is_running(dump_out = last_res):
                obj = self.trex.get_result_obj()
                print obj
            else:
                print termstyle.magenta("T-Rex isn't currently running.")
            print termstyle.green("*** End of scenario (T-Rex is posssibly still running!) ***")
        except TRexException as inst:
            print termstyle.red(inst)


    def do_stop_trex (self, line):
        """Try to stop T-Rex run (if T-Rex is currently running)"""
        print termstyle.green("*** Starting T-Rex termination ***")
        try:
            ret = self.trex.stop_trex()
            print termstyle.green("*** End of scenario (T-Rex is not running now) ***")
        except TRexException as inst:
            print termstyle.red(inst)

    def do_kill_indiscriminately (self, line):
        """Force killing of running T-Rex process (if exists) on the server."""
        print termstyle.green("*** Starting T-Rex termination ***")
        ret = self.trex.force_kill()
        if ret:
            print termstyle.green("*** End of scenario (T-Rex is not running now) ***")
        elif ret is None:
            print termstyle.magenta("*** End of scenario (T-Rex termination aborted) ***")
        else:
            print termstyle.red("*** End of scenario (T-Rex termination failed) ***")

    def do_exit(self, arg):
        """Quits the application"""
        print termstyle.cyan('Bye Bye!')
        return True


if __name__ == "__main__":
    parser = ArgumentParser(description = termstyle.cyan('Run T-Rex client API demos and scenarios.'),
        usage = """client_interactive_example [options]""" )

    parser.add_argument('-v', '--version', action='version', version='%(prog)s 1.0 \t (C) Cisco Systems Inc.\n')

    parser.add_argument("-t", "--trex-host", required = True, dest="trex_host",
        action="store", help="Specify the hostname or ip to connect with T-Rex server.", 
        metavar="HOST" )
    parser.add_argument("-p", "--trex-port", type=int, default = 8090, metavar="PORT", dest="trex_port", 
        help="Select port on which the T-Rex server listens. Default port is 8090.", action="store")
    parser.add_argument("-m", "--maxhist", type=int, default = 100, metavar="SIZE", dest="hist_size", 
        help="Specify maximum history size saved at client side. Default size is 100.", action="store")
    parser.add_argument("--verbose", dest="verbose",
        action="store_true", help="Switch ON verbose option at T-Rex client. Default is: OFF.", 
        default = False )
    args = parser.parse_args()
    
    try:
        InteractiveTRexClient(args.trex_host, args.hist_size, args.trex_port, args.verbose).cmdloop()

    except KeyboardInterrupt:
        print termstyle.cyan('Bye Bye!')
        exit(-1)
    except socket.error, e:
        if e.errno == errno.ECONNREFUSED:
            raise socket.error(errno.ECONNREFUSED, "Connection from T-Rex server was terminated. Please make sure the server is up.")



