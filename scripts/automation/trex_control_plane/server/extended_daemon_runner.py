#!/usr/bin/python

import outer_packages
import lockfile
from daemon import runner,daemon
from daemon.runner import *
import os, sys
from argparse import ArgumentParser
from trex_server import trex_parser
try:
    from termstyle import termstyle
except ImportError:
    import termstyle


def daemonize_parser(parser_obj, action_funcs, help_menu):
    """Update the regular process parser to deal with daemon process options"""
    parser_obj.description += " (as a daemon process)"
    parser_obj.usage = None
    parser_obj.add_argument("action", choices=action_funcs,
                            action="store", help=help_menu)
    return


class ExtendedDaemonRunner(runner.DaemonRunner):
    """ Controller for a callable running in a separate background process.

    The first command-line argument is the action to take:

    * 'start': Become a daemon and call `app.run()`.
    * 'stop': Exit the daemon process specified in the PID file.
    * 'restart': Stop, then start.

        """

    help_menu = """Specify action command to be applied on server.
    (*) start      : start the application in as a daemon process.
    (*) show       : prompt an updated status of daemon process (running/ not running).
    (*) stop       : exit the daemon process.
    (*) restart    : stop, then start again the application as daemon process
    (*) start-live : start the application in live mode (no daemon process).
    """

    def __init__(self, app, parser_obj):
        """ Set up the parameters of a new runner.
            THIS METHOD INTENTIONALLY DO NOT INVOKE SUPER __init__() METHOD

            :param app: The application instance; see below.
            :return: ``None``.

            The `app` argument must have the following attributes:

            * `stdin_path`, `stdout_path`, `stderr_path`: Filesystem paths
              to open and replace the existing `sys.stdin`, `sys.stdout`,
              `sys.stderr`.

            * `pidfile_path`: Absolute filesystem path to a file that will
              be used as the PID file for the daemon. If ``None``, no PID
              file will be used.

            * `pidfile_timeout`: Used as the default acquisition timeout
              value supplied to the runner's PID lock file.

            * `run`: Callable that will be invoked when the daemon is
              started.

            """
        super(runner.DaemonRunner, self).__init__()
        # update action_funcs to support more operations
        self.update_action_funcs()

        daemonize_parser(parser_obj, self.action_funcs, ExtendedDaemonRunner.help_menu)
        args = parser_obj.parse_args()
        self.action = unicode(args.action)

        self.app = app
        self.daemon_context = daemon.DaemonContext()
        self.daemon_context.stdin = open(app.stdin_path, 'rt')
        self.daemon_context.stdout = open(app.stdout_path, 'w+t')
        self.daemon_context.stderr = open(app.stderr_path,
                                          'a+t', buffering=0)

        self.pidfile = None
        if app.pidfile_path is not None:
            self.pidfile = make_pidlockfile(app.pidfile_path, app.pidfile_timeout)
        self.daemon_context.pidfile = self.pidfile

        # mask out all arguments that aren't relevant to main app script

    def update_action_funcs(self):
        self.action_funcs.update({u'start-live': self._start_live, u'show': self._show})     # add key (=action), value (=desired func)

    @staticmethod
    def _start_live(self):
        self.app.run()

    @staticmethod
    def _show(self):
        if self.pidfile.is_locked():
            print termstyle.red("T-Rex server daemon is running")
        else:
            print termstyle.red("T-Rex server daemon is NOT running")

    def do_action(self):
        self.__prevent_duplicate_runs()
        self.__prompt_init_msg()
        try:
            super(ExtendedDaemonRunner, self).do_action()
            if self.action == 'stop':
                self.__verify_termination()
        except runner.DaemonRunnerStopFailureError:
            if self.action == 'restart':
                # error means server wasn't running in the first place- so start it!
                self.action = 'start'
                self.do_action()

    
    def __prevent_duplicate_runs(self):
        if self.action == 'start' and self.pidfile.is_locked():
            print termstyle.green("Server daemon is already running")
            exit(1)
        elif self.action == 'stop' and not self.pidfile.is_locked():
            print termstyle.green("Server daemon is not running")
            exit(1)

    def __prompt_init_msg(self):
        if self.action == 'start':
            print termstyle.green("Starting daemon server...")
        elif self.action == 'stop':
            print termstyle.green("Stopping daemon server...")

    def __verify_termination(self):
        pass
#       import time
#       while self.pidfile.is_locked():
#           time.sleep(2)
#           self._stop()
#


if __name__ == "__main__":
    pass
