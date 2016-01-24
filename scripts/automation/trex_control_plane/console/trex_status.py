#from time import sleep
#
#import os
#
#import curses 
#from curses import panel
#import random
#import collections
#import operator
#import datetime
#
#g_curses_active = False
#
#################### utils #################
#
## simple percetange show
#def percentage (a, total):
#    x = int ((float(a) / total) * 100)
#    return str(x) + "%"
#
#################### panels #################
#
## panel object
#class TrexStatusPanel(object):
#    def __init__ (self, h, l, y, x, headline, status_obj):
#
#        self.status_obj = status_obj
#
#        self.log = status_obj.log
#        self.stateless_client = status_obj.stateless_client
#
#        self.stats         = status_obj.stats
#        self.general_stats = status_obj.general_stats
#
#        self.h = h
#        self.l = l
#        self.y = y
#        self.x = x
#        self.headline = headline
#
#        self.win = curses.newwin(h, l, y, x)
#        self.win.erase()
#        self.win.box()
#
#        self.win.addstr(1, 2, headline, curses.A_UNDERLINE)
#        self.win.refresh()
#
#        panel.new_panel(self.win)
#        self.panel = panel.new_panel(self.win)
#        self.panel.top()
#
#    def clear (self):
#        self.win.erase()
#        self.win.box()
#        self.win.addstr(1, 2, self.headline, curses.A_UNDERLINE)
#
#    def getwin (self):
#        return self.win
#
#
## various kinds of panels
#
## Server Info Panel
#class ServerInfoPanel(TrexStatusPanel):
#    def __init__ (self, h, l, y, x, status_obj):
#
#        super(ServerInfoPanel, self).__init__(h, l, y ,x ,"Server Info:", status_obj)
#
#    def draw (self):
#
#        if not self.status_obj.server_version :
#            return
#
#        if not self.status_obj.server_sys_info:
#            return
#
#
#        self.clear()
#
#        self.getwin().addstr(3, 2, "{:<30} {:30}".format("Server:",self.status_obj.server_sys_info["hostname"] + ":" + str(self.stateless_client.get_connection_port())))
#        self.getwin().addstr(4, 2, "{:<30} {:30}".format("Version:", self.status_obj.server_version["version"]))
#        self.getwin().addstr(5, 2, "{:<30} {:30}".format("Build:", 
#                                                                    self.status_obj.server_version["build_date"] + " @ " + 
#                                                                    self.status_obj.server_version["build_time"] + " by " + 
#                                                                    self.status_obj.server_version["built_by"]))
#
#        self.getwin().addstr(6, 2, "{:<30} {:30}".format("Server Uptime:", self.status_obj.server_sys_info["uptime"]))
#        self.getwin().addstr(7, 2, "{:<30} {:<3} / {:<30}".format("DP Cores:", str(self.status_obj.server_sys_info["dp_core_count"]) + 
#                                                                  " cores", self.status_obj.server_sys_info["core_type"]))
#
#        self.getwin().addstr(9, 2, "{:<30} {:<30}".format("Ports Count:", self.status_obj.server_sys_info["port_count"]))
#
#        ports_owned = " ".join(str(x) for x in self.status_obj.owned_ports_list)
#
#        if not ports_owned:
#            ports_owned = "None"
#
#        self.getwin().addstr(10, 2, "{:<30} {:<30}".format("Ports Owned:", ports_owned))
#
## general info panel
#class GeneralInfoPanel(TrexStatusPanel):
#    def __init__ (self, h, l, y, x, status_obj):
#
#        super(GeneralInfoPanel, self).__init__(h, l, y ,x ,"General Info:", status_obj)
#
#    def draw (self):
#        self.clear()
#
#        if not self.general_stats.is_online():
#            self.getwin().addstr(3, 2, "No Published Data From TRex Server")
#            return
#
#        self.getwin().addstr(3, 2, "{:<30} {:0.2f} %".format("CPU util.:", self.general_stats.get("m_cpu_util")))
#
#        self.getwin().addstr(6, 2, "{:<30} {:} / {:}".format("Total Tx. rate:",
#                                                               self.general_stats.get("m_tx_bps", format = True, suffix = "bps"),
#                                                               self.general_stats.get("m_tx_pps", format = True, suffix = "pps")))
#
#      
#        self.getwin().addstr(8, 2, "{:<30} {:} / {:}".format("Total Tx:",
#                                                             self.general_stats.get_rel("m_total_tx_bytes", format = True, suffix = "B"),
#                                                             self.general_stats.get_rel("m_total_tx_pkts", format = True, suffix = "pkts")))
#
#        self.getwin().addstr(11, 2, "{:<30} {:} / {:}".format("Total Rx. rate:",
#                                                               self.general_stats.get("m_rx_bps", format = True, suffix = "bps"),
#                                                               self.general_stats.get("m_rx_pps", format = True, suffix = "pps")))
#
#      
#        self.getwin().addstr(13, 2, "{:<30} {:} / {:}".format("Total Rx:",
#                                                             self.general_stats.get_rel("m_total_rx_bytes", format = True, suffix = "B"),
#                                                             self.general_stats.get_rel("m_total_rx_pkts", format = True, suffix = "pkts")))
#
## all ports stats
#class PortsStatsPanel(TrexStatusPanel):
#    def __init__ (self, h, l, y, x, status_obj):
#
#        super(PortsStatsPanel, self).__init__(h, l, y ,x ,"Trex Ports:", status_obj)
#
#
#    def draw (self):
#
#        self.clear()
#
#        owned_ports = self.status_obj.owned_ports_list
#        if not owned_ports:
#            self.getwin().addstr(3, 2, "No Owned Ports - Please Acquire One Or More Ports")
#            return
#
#        # table header 
#        self.getwin().addstr(3, 2, "{:^15} {:^30} {:^30} {:^30}".format(
#            "Port ID", "Tx Rate [bps/pps]", "Rx Rate [bps/pps]", "Total Bytes [tx/rx]"))
#
#
#
#        for i, port_index in enumerate(owned_ports):
#
#            port_stats = self.status_obj.stats.get_port_stats(port_index)
#
#            if port_stats:
#                self.getwin().addstr(5 + (i * 4), 2, "{:^15} {:^30} {:^30} {:^30}".format(
#                    "{0} ({1})".format(str(port_index), self.status_obj.server_sys_info["ports"][port_index]["speed"]),
#                    "{0} / {1}".format(port_stats.get("m_total_tx_bps", format = True, suffix = "bps"),
#                                       port_stats.get("m_total_tx_pps", format = True, suffix = "pps")),
#                                       
#                    "{0} / {1}".format(port_stats.get("m_total_rx_bps", format = True, suffix = "bps"),
#                                       port_stats.get("m_total_rx_pps", format = True, suffix = "pps")),
#                    "{0} / {1}".format(port_stats.get_rel("obytes", format = True, suffix = "B"),
#                                       port_stats.get_rel("ibytes", format = True, suffix = "B"))))
#        
#            else:
#
#                self.getwin().addstr(5 + (i * 4), 2, "{:^15} {:^30} {:^30} {:^30}".format(
#                    "{0} ({1})".format(str(port_index), self.status_obj.server_sys_info["ports"][port_index]["speed"]),
#                    "N/A",
#                    "N/A",
#                    "N/A",
#                    "N/A"))
#
#
#                # old format
##            if port_stats:
##                self.getwin().addstr(5 + (i * 4), 2, "{:^15} {:^15} {:^15} {:^15} {:^15} {:^15} {:^15}".format(
##                    "{0} ({1})".format(str(port_index), self.status_obj.server_sys_info["ports"][port_index]["speed"]),
##                    port_stats.get("m_total_tx_pps", format = True, suffix = "pps"),
##                    port_stats.get("m_total_tx_bps", format = True, suffix = "bps"),
##                    port_stats.get_rel("obytes", format = True, suffix = "B"),
##                    port_stats.get("m_total_rx_pps", format = True, suffix = "pps"),
##                    port_stats.get("m_total_rx_bps", format = True, suffix = "bps"),
##                    port_stats.get_rel("ibytes", format = True, suffix = "B")))
##        
##            else:
##                self.getwin().addstr(5 + (i * 4), 2, "{:^15} {:^15} {:^15} {:^15} {:^15} {:^15} {:^15}".format(
##                    "{0} ({1})".format(str(port_index), self.status_obj.server_sys_info["ports"][port_index]["speed"]),
##                    "N/A",
##                    "N/A",
##                    "N/A",
##                    "N/A",
##                    "N/A",
##                    "N/A"))
#
## control panel
#class ControlPanel(TrexStatusPanel):
#    def __init__ (self, h, l, y, x, status_obj):
#
#        super(ControlPanel, self).__init__(h, l, y, x, "", status_obj)
#
#
#    def draw (self):
#        self.clear()
#
#        self.getwin().addstr(1, 2, "'g' - general, '0-{0}' - specific port, 'f' - freeze, 'c' - clear stats, 'p' - ping server, 'q' - quit"
#                             .format(self.status_obj.stateless_client.get_port_count() - 1))
#
#        self.log.draw(self.getwin(), 2, 3)
#
## specific ports panels
#class SinglePortPanel(TrexStatusPanel):
#    def __init__ (self, h, l, y, x, status_obj, port_id):
#
#        super(SinglePortPanel, self).__init__(h, l, y, x, "Port {0}".format(port_id), status_obj)
#
#        self.port_id = port_id
#
#    def draw (self):
#        y = 3
#
#        self.clear()
#
#        if not self.port_id in self.status_obj.owned_ports_list:
#             self.getwin().addstr(y, 2, "Port {0} is not owned by you, please acquire the port for more info".format(self.port_id))
#             return
#
#        # streams
#        self.getwin().addstr(y, 2, "Streams:", curses.A_UNDERLINE)
#        y += 2
#
#        # stream table header 
#        self.getwin().addstr(y, 2, "{:^15} {:^15} {:^15} {:^15} {:^15} {:^15} {:^15}".format(
#            "Stream ID", "Enabled", "Type", "Self Start", "ISG", "Next Stream", "VM"))
#        y += 2
#
#        # streams
#        
#        if 'streams' in self.status_obj.owned_ports[str(self.port_id)]:
#            stream_info = self.status_obj.owned_ports[str(self.port_id)]['streams']
#
#            for stream_id, stream in sorted(stream_info.iteritems(), key=operator.itemgetter(0)):
#                self.getwin().addstr(y, 2, "{:^15} {:^15} {:^15} {:^15} {:^15} {:^15} {:^15}".format(
#                    stream_id,
#                    ("True" if stream['enabled'] else "False"),
#                    stream['mode']['type'],
#                    ("True" if stream['self_start'] else "False"),
#                    stream['isg'],
#                    (stream['next_stream_id'] if stream['next_stream_id'] != -1 else "None"),
#                    ("{0} instr.".format(len(stream['vm'])) if stream['vm'] else "None")))
#
#                y += 1
#
#        # new section - traffic
#        y += 2
#
#        self.getwin().addstr(y, 2, "Traffic:", curses.A_UNDERLINE)
#        y += 2
#
#
#
#         # table header 
#        self.getwin().addstr(y, 2, "{:^15} {:^30} {:^30} {:^30}".format(
#            "Port ID", "Tx Rate [bps/pps]", "Rx Rate [bps/pps]", "Total Bytes [tx/rx]"))
#
#
#        y += 2
#
#        port_stats = self.status_obj.stats.get_port_stats(self.port_id)
#
#        if port_stats:
#            self.getwin().addstr(y, 2, "{:^15} {:^30} {:^30} {:^30}".format(
#                "{0} ({1})".format(str(self.port_id), self.status_obj.server_sys_info["ports"][self.port_id]["speed"]),
#                "{0} / {1}".format(port_stats.get("m_total_tx_bps", format = True, suffix = "bps"),
#                                   port_stats.get("m_total_tx_pps", format = True, suffix = "pps")),
#                                        
#                "{0} / {1}".format(port_stats.get("m_total_rx_bps", format = True, suffix = "bps"),
#                                   port_stats.get("m_total_rx_pps", format = True, suffix = "pps")),
#                "{0} / {1}".format(port_stats.get_rel("obytes", format = True, suffix = "B"),
#                                   port_stats.get_rel("ibytes", format = True, suffix = "B"))))
#        
#        else:
#            self.getwin().addstr(y, 2, "{:^15} {:^30} {:^30} {:^30}".format(
#                "{0} ({1})".format(str(self.port_id), self.status_obj.server_sys_info["ports"][self.port_id]["speed"]),
#                "N/A",
#                "N/A",
#                "N/A",
#                "N/A"))
#
#
#################### main objects #################
#
## status log
#class TrexStatusLog():
#    def __init__ (self):
#        self.log = []
#
#    def add_event (self, msg):
#        self.log.append("[{0}] {1}".format(str(datetime.datetime.now().time()), msg))
#
#    def draw (self, window, x, y, max_lines = 4):
#        index = y
#
#        cut = len(self.log) - max_lines
#        if cut < 0:
#            cut = 0
#
#        for msg in self.log[cut:]:
#            window.addstr(index, x, msg)
#            index += 1
#
## status commands
#class TrexStatusCommands():
#    def __init__ (self, status_object):
#
#        self.status_object = status_object
#
#        self.stateless_client = status_object.stateless_client
#        self.log = self.status_object.log
#
#        self.actions = {}
#        self.actions[ord('q')] = self._quit
#        self.actions[ord('p')] = self._ping
#        self.actions[ord('f')] = self._freeze 
#
#        self.actions[ord('g')] = self._show_ports_stats
#
#        # register all the available ports shortcuts
#        for port_id in xrange(0, self.stateless_client.get_port_count()):
#            self.actions[ord('0') + port_id] = self._show_port_generator(port_id)
#
#
#    # handle a key pressed
#    def handle (self, ch):
#        if ch in self.actions:
#            return self.actions[ch]()
#        else:
#            self.log.add_event("Unknown key pressed, please see legend")
#            return True
#
#    # show all ports
#    def _show_ports_stats (self):
#        self.log.add_event("Switching to all ports view")
#        self.status_object.stats_panel = self.status_object.ports_stats_panel
#
#        return True
#
#
#     # function generator for different ports requests
#    def _show_port_generator (self, port_id):
#        def _show_port():
#            self.log.add_event("Switching panel to port {0}".format(port_id))
#            self.status_object.stats_panel = self.status_object.ports_panels[port_id]
#
#            return True
#
#        return _show_port
#
#    def _freeze (self):
#        self.status_object.update_active = not self.status_object.update_active
#        self.log.add_event("Update continued" if self.status_object.update_active else "Update stopped")
#
#        return True
#
#    def _quit(self):
#        return False
#
#    def _ping (self):
#        self.log.add_event("Pinging RPC server")
#
#        rc, msg = self.stateless_client.ping()
#        if rc:
#            self.log.add_event("Server replied: '{0}'".format(msg))
#        else:
#            self.log.add_event("Failed to get reply")
#
#        return True
#
## status object
## 
##
##
#class CTRexStatus():
#    def __init__ (self, stdscr, stateless_client):
#        self.stdscr = stdscr
#
#        self.stateless_client = stateless_client
#
#        self.log  = TrexStatusLog()
#        self.cmds = TrexStatusCommands(self)
#
#        self.stats         = stateless_client.get_stats_async()
#        self.general_stats = stateless_client.get_stats_async().get_general_stats()
#
#        # fetch server info
#        self.server_sys_info = self.stateless_client.get_system_info()
#
#        self.server_version = self.stateless_client.get_server_version()
#
#        # list of owned ports
#        self.owned_ports_list = self.stateless_client.get_acquired_ports()
#     
#        # data per port
#        self.owned_ports = {}
#
#        for port_id in self.owned_ports_list:
#            self.owned_ports[str(port_id)] = {}
#            self.owned_ports[str(port_id)]['streams'] = {}
#
#            stream_list = self.stateless_client.get_all_streams(port_id)
#
#            self.owned_ports[str(port_id)] = stream_list
#        
#        
#        try:
#            curses.curs_set(0)
#        except:
#            pass
#
#        curses.use_default_colors()        
#        self.stdscr.nodelay(1)
#        curses.nonl()
#        curses.noecho()
#     
#        self.generate_layout()
#
#  
#    def generate_layout (self):
#        self.max_y = self.stdscr.getmaxyx()[0]
#        self.max_x = self.stdscr.getmaxyx()[1]
#
#        self.server_info_panel    = ServerInfoPanel(int(self.max_y * 0.3), self.max_x / 2, int(self.max_y * 0.5), self.max_x /2, self)
#        self.general_info_panel   = GeneralInfoPanel(int(self.max_y * 0.5), self.max_x / 2, 0, self.max_x /2, self)
#        self.control_panel        = ControlPanel(int(self.max_y * 0.2), self.max_x , int(self.max_y * 0.8), 0, self)
#
#        # those can be switched on the same place 
#        self.ports_stats_panel    = PortsStatsPanel(int(self.max_y * 0.8), self.max_x / 2, 0, 0, self)
#
#        self.ports_panels = {}
#        for i in xrange(0, self.stateless_client.get_port_count()):
#            self.ports_panels[i] = SinglePortPanel(int(self.max_y * 0.8), self.max_x / 2, 0, 0, self, i)
#
#        # at start time we point to the main one 
#        self.stats_panel = self.ports_stats_panel
#        self.stats_panel.panel.top()
#
#        panel.update_panels(); self.stdscr.refresh()
#        return 
#
#
#    def wait_for_key_input (self):
#        ch = self.stdscr.getch()
#
#        # no key , continue
#        if ch == curses.ERR:
#            return True
#    
#        return self.cmds.handle(ch)
#
#    # main run entry point
#    def run (self):
#
#        # list of owned ports
#        self.owned_ports_list = self.stateless_client.get_acquired_ports()
#     
#        # data per port
#        self.owned_ports = {}
#
#        for port_id in self.owned_ports_list:
#            self.owned_ports[str(port_id)] = {}
#            self.owned_ports[str(port_id)]['streams'] = {}
#
#            stream_list = self.stateless_client.get_all_streams(port_id)
#
#            self.owned_ports[str(port_id)] = stream_list
#
#        self.update_active = True
#        while (True):
#
#            rc = self.wait_for_key_input()
#            if not rc:
#                break
#
#            self.server_info_panel.draw()
#            self.general_info_panel.draw()
#            self.control_panel.draw()
#
#            # can be different kinds of panels
#            self.stats_panel.panel.top()
#            self.stats_panel.draw()
#
#            panel.update_panels()
#            self.stdscr.refresh()
#            sleep(0.01)
#
#
## global container
#trex_status = None
#
#def show_trex_status_internal (stdscr, stateless_client):
#    global trex_status
#
#    if trex_status == None:
#        trex_status = CTRexStatus(stdscr, stateless_client)
#
#    trex_status.run()
#
#def show_trex_status (stateless_client):
#
#    try:
#        curses.wrapper(show_trex_status_internal, stateless_client)
#    except KeyboardInterrupt:
#        curses.endwin()
#
#def cleanup ():
#    try:
#        curses.endwin()
#    except:
#        pass
#
