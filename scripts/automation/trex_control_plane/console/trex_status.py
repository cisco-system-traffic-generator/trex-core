from time import sleep

import os

import curses 
from curses import panel
import random
import collections
import operator
import datetime

g_curses_active = False

#
def percentage (a, total):
    x = int ((float(a) / total) * 100)
    return str(x) + "%"

# panel object
class TrexStatusPanel():
    def __init__ (self, h, l, y, x, headline):
        self.h = h
        self.l = l
        self.y = y
        self.x = x
        self.headline = headline

        self.win = curses.newwin(h, l, y, x)
        self.win.erase()
        self.win.box()

        self.win.addstr(1, 2, headline, curses.A_UNDERLINE)
        self.win.refresh()

        panel.new_panel(self.win)
        self.panel = panel.new_panel(self.win)
        self.panel.top()

    def clear (self):
        self.win.erase()
        self.win.box()
        self.win.addstr(1, 2, self.headline, curses.A_UNDERLINE)

    def getwin (self):
        return self.win

def float_to_human_readable (size, suffix = "bps"):
    for unit in ['','K','M','G']:
        if abs(size) < 1024.0:
            return "%3.1f %s%s" % (size, unit, suffix)
        size /= 1024.0
    return "NaN"

# status object
class TrexStatus():
    def __init__ (self, stdscr, rpc_client):
        self.stdscr = stdscr
        self.log = []
        self.rpc_client = rpc_client

        self.get_server_info()

    def get_server_info (self):
        rc, msg = self.rpc_client.get_rpc_server_status()

        if rc:
            self.server_status = msg
        else:
            self.server_status = None

    def add_log_event (self, msg):
        self.log.append("[{0}] {1}".format(str(datetime.datetime.now().time()), msg))

    def add_panel (self, h, l, y, x, headline):
         win = curses.newwin(h, l, y, x)
         win.erase()
         win.box()

         win.addstr(1, 2, headline)
         win.refresh()

         panel.new_panel(win)
         panel1 = panel.new_panel(win)
         panel1.top()

         return win, panel1

    # static info panel
    def update_info (self):
        if self.server_status == None:
            return

        self.info_panel.clear()

        connection_details = self.rpc_client.get_connection_details()

        self.info_panel.getwin().addstr(3, 2, "{:<30} {:30}".format("Server:", connection_details['server'] + ":" + str(connection_details['port'])))
        self.info_panel.getwin().addstr(4, 2, "{:<30} {:30}".format("Version:", self.server_status["general"]["version"]))
        self.info_panel.getwin().addstr(5, 2, "{:<30} {:30}".format("Build:", 
                                                                    self.server_status["general"]["build_date"] + " @ " + self.server_status["general"]["build_time"] + " by " + self.server_status["general"]["version_user"]))

        self.info_panel.getwin().addstr(6, 2, "{:<30} {:30}".format("Server Uptime:", self.server_status["general"]["uptime"]))

    # general stats
    def update_general (self, gen_stats):
        pass

    # control panel
    def update_control (self):
        self.control_panel.clear()

        self.control_panel.getwin().addstr(1, 2, "'f' - freeze, 'c' - clear stats, 'p' - ping server, 'q' - quit")

        index = 3

        cut = len(self.log) - 4
        if cut < 0:
            cut = 0

        for l in self.log[cut:]:
            self.control_panel.getwin().addstr(index, 2, l)
            index += 1

    def generate_layout (self):
        self.max_y = self.stdscr.getmaxyx()[0]
        self.max_x = self.stdscr.getmaxyx()[1]

        # create cls panel
        self.main_panel = TrexStatusPanel(int(self.max_y * 0.8), self.max_x / 2, 0,0, "Trex Activity:")

        self.general_panel = TrexStatusPanel(int(self.max_y * 0.6), self.max_x / 2, 0, self.max_x /2, "General Statistics:")

        self.info_panel    = TrexStatusPanel(int(self.max_y * 0.2), self.max_x / 2, int(self.max_y * 0.6), self.max_x /2, "Server Info:")

        self.control_panel = TrexStatusPanel(int(self.max_y * 0.2), self.max_x , int(self.max_y * 0.8), 0, "")

        panel.update_panels(); self.stdscr.refresh()

    def wait_for_key_input (self):
        ch = self.stdscr.getch()

        if (ch != curses.ERR):
            # stop/start status
            if (ch == ord('f')):
                self.update_active = not self.update_active
                self.add_log_event("Update continued" if self.update_active else "Update stopped")

            elif (ch == ord('p')):
                self.add_log_event("Pinging RPC server")
                rc, msg = self.rpc_client.ping_rpc_server()
                if rc:
                    self.add_log_event("Server replied: '{0}'".format(msg))
                else:
                    self.add_log_event("Failed to get reply")

            # c - clear stats
            elif (ch == ord('c')):
                self.add_log_event("Statistics cleared")

            elif (ch == ord('q')):
                return False
            else:
                self.add_log_event("Unknown key pressed {0}".format("'" + chr(ch) + "'" if chr(ch).isalpha() else ""))

        return True

    # main run entry point
    def run (self):
        try:
            curses.curs_set(0)
        except:
            pass

        curses.use_default_colors()        
        self.stdscr.nodelay(1)
        curses.nonl()
        curses.noecho()
     
        self.generate_layout()

        self.update_active = True
        while (True):

            rc = self.wait_for_key_input()
            if not rc:
                break

            self.update_control()
            self.update_info()

            panel.update_panels(); 
            self.stdscr.refresh()
            sleep(0.1)


def show_trex_status_internal (stdscr, rpc_client):
    trex_status = TrexStatus(stdscr, rpc_client)
    trex_status.run()

def show_trex_status (rpc_client):

    try:
        curses.wrapper(show_trex_status_internal, rpc_client)
    except KeyboardInterrupt:
        curses.endwin()

def cleanup ():
    try:
        curses.endwin()
    except:
        pass

