from time import sleep

import os

import curses 
from curses import panel
import random
import collections
import operator
import datetime

g_curses_active = False

# simple percetange show
def percentage (a, total):
    x = int ((float(a) / total) * 100)
    return str(x) + "%"

# simple float to human readable
def float_to_human_readable (size, suffix = "bps"):
    for unit in ['','K','M','G']:
        if abs(size) < 1024.0:
            return "%3.1f %s%s" % (size, unit, suffix)
        size /= 1024.0
    return "NaN"

# panel object
class TrexStatusPanel(object):
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


# various kinds of panels

# Server Info Panel
class ServerInfoPanel(TrexStatusPanel):
    def __init__ (self, h, l, y, x, status_obj):

        super(ServerInfoPanel, self).__init__(h, l, y ,x ,"Server Info:")

        self.status_obj = status_obj

    def draw (self):

        if self.status_obj.server_version == None:
            return

        self.clear()

        connection_details = self.status_obj.rpc_client.get_connection_details()

        self.getwin().addstr(3, 2, "{:<30} {:30}".format("Server:",self.status_obj.server_sys_info["hostname"] + ":" + str(connection_details['port'])))
        self.getwin().addstr(4, 2, "{:<30} {:30}".format("Version:", self.status_obj.server_version["version"]))
        self.getwin().addstr(5, 2, "{:<30} {:30}".format("Build:", 
                                                                    self.status_obj.server_version["build_date"] + " @ " + 
                                                                    self.status_obj.server_version["build_time"] + " by " + 
                                                                    self.status_obj.server_version["built_by"]))

        self.getwin().addstr(6, 2, "{:<30} {:30}".format("Server Uptime:", self.status_obj.server_sys_info["uptime"]))
        self.getwin().addstr(7, 2, "{:<30} {:<3} / {:<30}".format("DP Cores:", str(self.status_obj.server_sys_info["dp_core_count"]) + 
                                                                  " cores", self.status_obj.server_sys_info["core_type"]))

        self.getwin().addstr(9, 2, "{:<30} {:<30}".format("Ports Count:", self.status_obj.server_sys_info["port_count"]))

        ports_owned = " ".join(str(x) for x in self.status_obj.rpc_client.get_owned_ports())

        if not ports_owned:
            ports_owned = "None"

        self.getwin().addstr(10, 2, "{:<30} {:<30}".format("Ports Owned:", ports_owned))

# general info panel
class GeneralInfoPanel(TrexStatusPanel):
    def __init__ (self, h, l, y, x, status_obj):

        super(GeneralInfoPanel, self).__init__(h, l, y ,x ,"General Info:")

        self.status_obj = status_obj

    def draw (self):
        pass

# all ports stats
class PortsStatsPanel(TrexStatusPanel):
    def __init__ (self, h, l, y, x, status_obj):

        super(PortsStatsPanel, self).__init__(h, l, y ,x ,"Trex Ports:")

        self.status_obj = status_obj

    def draw (self):

        self.clear()

        owned_ports = self.status_obj.rpc_client.get_owned_ports()
        if not owned_ports:
            self.getwin().addstr(3, 2, "No Owned Ports - Please Acquire One Or More Ports")
            return

        # table header 
        self.getwin().addstr(3, 2, "{:^15} {:^15} {:^15} {:^15} {:^15} {:^15} {:^15}".format(
            "Port ID", "Tx [pps]", "Tx [bps]", "Tx [bytes]", "Rx [pps]", "Rx [bps]", "Rx [bytes]"))

        # port loop
        self.status_obj.stats.query_sync()

        for i, port_index in enumerate(owned_ports):

            port_stats = self.status_obj.stats.get_port_stats(port_index)

            if port_stats:
                self.getwin().addstr(5 + (i * 4), 2, "{:^15} {:^15,.2f} {:^15,.2f} {:^15,} {:^15,.2f} {:^15,.2f} {:^15,}".format(
                    "{0} ({1})".format(str(port_index), self.status_obj.server_sys_info["ports"][port_index]["speed"]),
                    port_stats["tx_pps"],
                    port_stats["tx_bps"],
                    port_stats["total_tx_bytes"],
                    port_stats["rx_pps"],
                    port_stats["rx_bps"],
                    port_stats["total_rx_bytes"]))

            else:
                self.getwin().addstr(5 + (i * 4), 2, "{:^15} {:^15} {:^15} {:^15} {:^15} {:^15} {:^15}".format(
                    "{0} ({1})".format(str(port_index), self.status_obj.server_sys_info["ports"][port_index]["speed"]),
                    "N/A",
                    "N/A",
                    "N/A",
                    "N/A",
                    "N/A",
                    "N/A"))

# control panel
class ControlPanel(TrexStatusPanel):
    def __init__ (self, h, l, y, x, status_obj):

        super(ControlPanel, self).__init__(h, l, y, x, "")

        self.status_obj = status_obj

    def draw (self):
        self.clear()

        self.getwin().addstr(1, 2, "'g' - general, '0-{0}' - specific port, 'f' - freeze, 'c' - clear stats, 'p' - ping server, 'q' - quit"
                             .format(self.status_obj.rpc_client.get_port_count() - 1))

        index = 3

        cut = len(self.status_obj.log) - 4
        if cut < 0:
            cut = 0

        for l in self.status_obj.log[cut:]:
            self.getwin().addstr(index, 2, l)
            index += 1

# specific ports panels
class SinglePortPanel(TrexStatusPanel):
    def __init__ (self, h, l, y, x, status_obj, port_id):

        super(SinglePortPanel, self).__init__(h, l, y, x, "Port {0}".format(port_id))

        self.status_obj = status_obj
        self.port_id = port_id

    def draw (self):
        y = 3

        self.clear()

        if not self.port_id in self.status_obj.rpc_client.get_owned_ports():
             self.getwin().addstr(y, 2, "Port {0} is not owned by you, please acquire the port for more info".format(self.port_id))
             return

        # streams
        self.getwin().addstr(y, 2, "Streams:", curses.A_UNDERLINE)
        y += 2

        # stream table header 
        self.getwin().addstr(y, 2, "{:^15} {:^15} {:^15} {:^15} {:^15} {:^15} {:^15}".format(
            "Stream ID", "Enabled", "Type", "Self Start", "ISG", "Next Stream", "VM"))
        y += 2

        # streams
        if 'streams' in self.status_obj.snapshot[self.port_id]:
            for stream_id, stream in self.status_obj.snapshot[self.port_id]['streams'].iteritems():
                self.getwin().addstr(y, 2, "{:^15} {:^15} {:^15} {:^15} {:^15} {:^15} {:^15}".format(
                    stream_id,
                    ("True" if stream['stream']['enabled'] else "False"),
                    stream['stream']['mode']['type'],
                    ("True" if stream['stream']['self_start'] else "False"),
                    stream['stream']['isg'],
                    (stream['stream']['next_stream_id'] if stream['stream']['next_stream_id'] != -1 else "None"),
                    ("{0} instr.".format(len(stream['stream']['vm'])) if stream['stream']['vm'] else "None")))

                y += 1

        # new section - traffic
        y += 2

        self.getwin().addstr(y, 2, "Traffic:", curses.A_UNDERLINE)
        y += 2

        self.status_obj.stats.query_sync()
        port_stats = self.status_obj.stats.get_port_stats(self.port_id)


        # table header 
        self.getwin().addstr(y, 2, "{:^15} {:^15} {:^15} {:^15} {:^15} {:^15} {:^15}".format(
            "Port ID", "Tx [pps]", "Tx [bps]", "Tx [bytes]", "Rx [pps]", "Rx [bps]", "Rx [bytes]"))

        y += 2

        if port_stats:
                self.getwin().addstr(y, 2, "{:^15} {:^15,} {:^15,} {:^15,} {:^15,} {:^15,} {:^15,}".format(
                    "{0} ({1})".format(str(self.port_id), self.status_obj.server_sys_info["ports"][self.port_id]["speed"]),
                    port_stats["tx_pps"],
                    port_stats["tx_bps"],
                    port_stats["total_tx_bytes"],
                    port_stats["rx_pps"],
                    port_stats["rx_bps"],
                    port_stats["total_rx_bytes"]))

        else:
            self.getwin().addstr(y, 2, "{:^15} {:^15} {:^15} {:^15} {:^15} {:^15} {:^15}".format(
                "{0} ({1})".format(str(self.port_id), self.status_obj.server_sys_info["ports"][self.port_id]["speed"]),
                "N/A",
                "N/A",
                "N/A",
                "N/A",
                "N/A",
                "N/A"))

        y += 2

# status object
class TrexStatus():
    def __init__ (self, stdscr, rpc_client):
        self.stdscr = stdscr
        self.log = []
        self.rpc_client = rpc_client

        self.snapshot = self.rpc_client.snapshot()

        # fetch server info
        self.get_server_info()

        # create stats objects
        self.stats = Stats(rpc_client, self.rpc_client.get_owned_ports())

        # register actions
        self.actions = {}
        self.actions[ord('q')] = self.action_quit
        self.actions[ord('p')] = self.action_ping
        self.actions[ord('f')] = self.action_freeze 

        self.actions[ord('g')] = self.action_show_ports_stats

        for port_id in xrange(0, self.rpc_client.get_port_count()):
            self.actions[ord('0') + port_id] = self.action_show_port_generator(port_id)

        
    # all ports stats
    def action_show_ports_stats (self):
        self.add_log_event("Switching to all ports view")
        self.stats_panel = self.ports_stats_panel
        
        return True

    # function generator for different ports requests
    def action_show_port_generator (self, port_id):
        def action_show_port():
            self.add_log_event("Switching panel to port {0}".format(port_id))
            self.stats_panel = self.ports_panels[port_id]

            return True

        return action_show_port

    def action_freeze (self):
        self.update_active = not self.update_active
        self.add_log_event("Update continued" if self.update_active else "Update stopped")

        return True

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

        self.server_version = self.rpc_client.get_rpc_server_version()
        self.server_sys_info = self.rpc_client.get_system_info()


    def add_log_event (self, msg):
        self.log.append("[{0}] {1}".format(str(datetime.datetime.now().time()), msg))

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

        self.server_info_panel    = ServerInfoPanel(int(self.max_y * 0.3), self.max_x / 2, int(self.max_y * 0.5), self.max_x /2, self)
        self.general_info_panel   = GeneralInfoPanel(int(self.max_y * 0.5), self.max_x / 2, 0, self.max_x /2, self)
        self.control_panel        = ControlPanel(int(self.max_y * 0.2), self.max_x , int(self.max_y * 0.8), 0, self)

        # those can be switched on the same place 
        self.ports_stats_panel    = PortsStatsPanel(int(self.max_y * 0.8), self.max_x / 2, 0, 0, self)

        self.ports_panels = {}
        for i in xrange(0, self.rpc_client.get_port_count()):
            self.ports_panels[i] = SinglePortPanel(int(self.max_y * 0.8), self.max_x / 2, 0, 0, self, i)

        # at start time we point to the main one 
        self.stats_panel = self.ports_stats_panel
        self.stats_panel.panel.top()

        panel.update_panels(); self.stdscr.refresh()
        return 


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

            self.server_info_panel.draw()
            self.general_info_panel.draw()
            self.control_panel.draw()

            # can be different kinds of panels
            self.stats_panel.panel.top()
            self.stats_panel.draw()

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

