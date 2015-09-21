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


# total stats (ports + global)
class Stats():
    def __init__ (self, rpc_client, port_list, interval = 100):

        self.rpc_client = rpc_client

        self.port_list = port_list
        self.port_stats = {}

        self.interval = interval
        self.delay_count = 0

    def get_port_stats (self, port_id):
        if self.port_stats.get(port_id):
            return self.port_stats[port_id]
        else:
            return None

    def query_sync (self):
        self.delay_count += 1
        if self.delay_count < self.interval:
            return

        self.delay_count = 0

        # query global stats

        # query port stats

        rc, resp_list = self.rpc_client.get_port_stats(self.port_list)
        if not rc:
            return

        for i, rc in enumerate(resp_list):
            if rc[0]:
                self.port_stats[self.port_list[i]] = rc[1]



# status object
class TrexStatus():
    def __init__ (self, stdscr, rpc_client):
        self.stdscr = stdscr
        self.log = []
        self.rpc_client = rpc_client

        self.get_server_info()

        self.stats = Stats(rpc_client, self.rpc_client.get_owned_ports())

        self.actions = {}
        self.actions[ord('q')] = self.action_quit
        self.actions[ord('p')] = self.action_ping

    def action_quit(self):
        return False

    def action_ping (self):
        self.add_log_event("Pinging RPC server")
        rc, msg = self.rpc_client.ping_rpc_server()
        if rc:
            self.add_log_event("Server replied: '{0}'".format(msg))
        else:
            self.add_log_event("Failed to get reply")
        return True

    def get_server_info (self):
        rc, msg = self.rpc_client.get_rpc_server_version()

        if rc:
            self.server_version = msg
        else:
            self.server_version = None

        rc, msg = self.rpc_client.get_system_info()

        if rc:
            self.server_sys_info = msg
        else:
            self.server_sys_info = None


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
        if self.server_version == None:
            return

        self.info_panel.clear()

        connection_details = self.rpc_client.get_connection_details()

        self.info_panel.getwin().addstr(3, 2, "{:<30} {:30}".format("Server:",self.server_sys_info["hostname"] + ":" + str(connection_details['port'])))
        self.info_panel.getwin().addstr(4, 2, "{:<30} {:30}".format("Version:", self.server_version["version"]))
        self.info_panel.getwin().addstr(5, 2, "{:<30} {:30}".format("Build:", 
                                                                    self.server_version["build_date"] + " @ " + self.server_version["build_time"] + " by " + self.server_version["built_by"]))

        self.info_panel.getwin().addstr(6, 2, "{:<30} {:30}".format("Server Uptime:", self.server_sys_info["uptime"]))
        self.info_panel.getwin().addstr(7, 2, "{:<30} {:<3} / {:<30}".format("DP Cores:", str(self.server_sys_info["dp_core_count"]) + " cores", self.server_sys_info["core_type"]))
        self.info_panel.getwin().addstr(9, 2, "{:<30} {:<30}".format("Ports Count:", self.server_sys_info["port_count"]))

        ports_owned = " ".join(str(x) for x in self.rpc_client.get_owned_ports())
        if not ports_owned:
            ports_owned = "None"
        self.info_panel.getwin().addstr(10, 2, "{:<30} {:<30}".format("Ports Owned:", ports_owned))


    # general stats
    def update_ports_stats (self):

        self.ports_stats_panel.clear()

        owned_ports = self.rpc_client.get_owned_ports()
        if not owned_ports:
            self.ports_stats_panel.getwin().addstr(3, 2, "No Owned Ports - Please Acquire One Or More Ports")
            return

        # table header 
        self.ports_stats_panel.getwin().addstr(3, 2, "{:^15} {:^15} {:^15} {:^15} {:^15} {:^15} {:^15}".format(
            "Port ID", "Tx [pps]", "Tx [bps]", "Tx [bytes]", "Rx [pps]", "Rx [bps]", "Rx [bytes]"))

        # port loop
        self.stats.query_sync()

        for i, port_index in enumerate(owned_ports):

            port_stats = self.stats.get_port_stats(port_index)

            if port_stats:
                self.ports_stats_panel.getwin().addstr(5 + (i * 4), 2, "{:^15} {:^15,} {:^15,} {:^15,} {:^15,} {:^15,} {:^15,}".format(
                    "{0} ({1})".format(str(port_index), self.server_sys_info["ports"][port_index]["speed"]),
                    port_stats["tx_pps"],
                    port_stats["tx_bps"],
                    port_stats["total_tx_bytes"],
                    port_stats["rx_pps"],
                    port_stats["rx_bps"],
                    port_stats["total_rx_bytes"]))

            else:
                self.ports_stats_panel.getwin().addstr(5 + (i * 4), 2, "{:^15} {:^15} {:^15} {:^15} {:^15} {:^15} {:^15}".format(
                    "{0} ({1})".format(str(port_index), self.server_sys_info["ports"][port_index]["speed"]),
                    "N/A",
                    "N/A",
                    "N/A",
                    "N/A",
                    "N/A",
                    "N/A"))

    # control panel
    def update_control (self):
        self.control_panel.clear()

        self.control_panel.getwin().addstr(1, 2, "'g' - general, '0-{0}' - specific port, 'f' - freeze, 'c' - clear stats, 'p' - ping server, 'q' - quit"
                                           .format(self.rpc_client.get_port_count() - 1))

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
        self.ports_stats_panel = TrexStatusPanel(int(self.max_y * 0.8), self.max_x / 2, 0,0, "Trex Ports:")

        self.general_panel = TrexStatusPanel(int(self.max_y * 0.5), self.max_x / 2, 0, self.max_x /2, "General Statistics:")

        self.info_panel    = TrexStatusPanel(int(self.max_y * 0.3), self.max_x / 2, int(self.max_y * 0.5), self.max_x /2, "Server Info:")

        self.control_panel = TrexStatusPanel(int(self.max_y * 0.2), self.max_x , int(self.max_y * 0.8), 0, "")

        panel.update_panels(); self.stdscr.refresh()

    def wait_for_key_input (self):
        ch = self.stdscr.getch()

        # no key , continue
        if ch == curses.ERR:
            return True
        
        # check for registered function        
        if ch in self.actions:
            return self.actions[ch]()
        else:
            self.add_log_event("Unknown key pressed, please see legend")

        return True

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
                self.add_log_event("Unknown key pressed, please see legend")

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
            self.update_ports_stats()

            panel.update_panels(); 
            self.stdscr.refresh()
            sleep(0.01)


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

