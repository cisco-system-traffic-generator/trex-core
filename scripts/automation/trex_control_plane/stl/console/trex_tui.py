import termios
import sys
import os
import time
from collections import OrderedDict, deque
import datetime

if sys.version_info > (3,0):
    from io import StringIO
else:
    from cStringIO import StringIO

from trex_stl_lib.utils.text_opts import *
from trex_stl_lib.utils import text_tables
from trex_stl_lib import trex_stl_stats

# for STL exceptions
from trex_stl_lib.api import *

class SimpleBar(object):
    def __init__ (self, desc, pattern):
        self.desc = desc
        self.pattern = pattern
        self.pattern_len = len(pattern)
        self.index = 0

    def show (self):
        if self.desc:
            print(format_text("{0} {1}".format(self.desc, self.pattern[self.index]), 'bold'))
        else:
            print(format_text("{0}".format(self.pattern[self.index]), 'bold'))

        self.index = (self.index + 1) % self.pattern_len


# base type of a panel
class TrexTUIPanel(object):
    def __init__ (self, mng, name):

        self.mng = mng
        self.name = name
        self.stateless_client = mng.stateless_client
        self.is_graph = False

    def show (self):
        raise NotImplementedError("must implement this")

    def get_key_actions (self):
        raise NotImplementedError("must implement this")


    def get_name (self):
        return self.name


# dashboard panel
class TrexTUIDashBoard(TrexTUIPanel):

    FILTER_ACQUIRED = 1
    FILTER_ALL      = 2

    def __init__ (self, mng):
        super(TrexTUIDashBoard, self).__init__(mng, "dashboard")

        self.key_actions = OrderedDict()

        self.key_actions['c'] = {'action': self.action_clear,  'legend': 'clear', 'show': True}
        self.key_actions['p'] = {'action': self.action_pause,  'legend': 'pause', 'show': True}
        self.key_actions['r'] = {'action': self.action_resume, 'legend': 'resume', 'show': True}
        self.key_actions['+'] = {'action': self.action_raise,  'legend': 'up 5%', 'show': True}
        self.key_actions['-'] = {'action': self.action_lower,  'legend': 'low 5%', 'show': True}

        self.key_actions['o'] = {'action': self.action_show_owned,  'legend': 'owned ports', 'show': True}
        self.key_actions['a'] = {'action': self.action_show_all,  'legend': 'all ports', 'show': True}

        if self.stateless_client.get_acquired_ports():
            self.ports_filter = self.FILTER_ACQUIRED
        else:
            self.ports_filter = self.FILTER_ALL


    def get_ports (self):
        if self.ports_filter == self.FILTER_ACQUIRED:
            return self.stateless_client.get_acquired_ports()

        elif self.ports_filter == self.FILTER_ALL:
            return self.stateless_client.get_all_ports()

        assert(0)

    def show (self):
        stats = self.stateless_client._get_formatted_stats(self.get_ports())
        # print stats to screen
        for stat_type, stat_data in stats.items():
            text_tables.print_table_with_header(stat_data.text_table, stat_type)


    def get_key_actions (self):
        allowed = OrderedDict()

        allowed['c'] = self.key_actions['c']
        allowed['o'] = self.key_actions['o']
        allowed['a'] = self.key_actions['a']

        if self.ports_filter == self.FILTER_ALL and self.stateless_client.get_acquired_ports() != self.stateless_client.get_all_ports():
            return allowed

        if len(self.stateless_client.get_transmitting_ports()) > 0:
            allowed['p'] = self.key_actions['p']
            allowed['+'] = self.key_actions['+']
            allowed['-'] = self.key_actions['-']


        if len(self.stateless_client.get_paused_ports()) > 0:
            allowed['r'] = self.key_actions['r']

        return allowed


    ######### actions
    def action_pause (self):
        try:
            rc = self.stateless_client.pause(ports = self.mng.ports)
        except STLError:
            pass

        return ""



    def action_resume (self):
        try:
            self.stateless_client.resume(ports = self.mng.ports)
        except STLError:
            pass

        return ""


    def action_raise (self):
        try:
            self.stateless_client.update(mult = "5%+", ports = self.mng.ports)
        except STLError:
            pass

        return ""


    def action_lower (self):
        try:
            self.stateless_client.update(mult = "5%-", ports = self.mng.ports)
        except STLError:
            pass

        return ""


    def action_show_owned (self):
        self.ports_filter = self.FILTER_ACQUIRED
        return ""

    def action_show_all (self):
        self.ports_filter = self.FILTER_ALL
        return ""

    def action_clear (self):
        self.stateless_client.clear_stats(self.mng.ports)
        return "cleared all stats"


# port panel
class TrexTUIPort(TrexTUIPanel):
    def __init__ (self, mng, port_id):
        super(TrexTUIPort, self).__init__(mng, "port {0}".format(port_id))

        self.port_id = port_id
        self.port = self.mng.stateless_client.get_port(port_id)

        self.key_actions = OrderedDict()

        self.key_actions['c'] = {'action': self.action_clear,  'legend': 'clear', 'show': True}
        self.key_actions['p'] = {'action': self.action_pause, 'legend': 'pause', 'show': True}
        self.key_actions['r'] = {'action': self.action_resume, 'legend': 'resume', 'show': True}
        self.key_actions['+'] = {'action': self.action_raise, 'legend': 'up 5%', 'show': True}
        self.key_actions['-'] = {'action': self.action_lower, 'legend': 'low 5%', 'show': True}
        self.key_actions['t'] = {'action': self.action_toggle_graph, 'legend': 'toggle graph', 'show': True}


    def show (self):
        if self.mng.tui.is_graph is False:
            stats = self.stateless_client._get_formatted_stats([self.port_id])
            # print stats to screen
            for stat_type, stat_data in stats.items():
                text_tables.print_table_with_header(stat_data.text_table, stat_type)
        else:
            stats = self.stateless_client._get_formatted_stats([self.port_id], stats_mask = trex_stl_stats.GRAPH_PORT_COMPACT)
            for stat_type, stat_data in stats.items():
                text_tables.print_table_with_header(stat_data.text_table, stat_type)

    def get_key_actions (self):

        allowed = OrderedDict()

        allowed['c'] = self.key_actions['c']
        allowed['t'] = self.key_actions['t']

        if self.port_id not in self.stateless_client.get_acquired_ports():
            return allowed

        if self.port.state == self.port.STATE_TX:
            allowed['p'] = self.key_actions['p']
            allowed['+'] = self.key_actions['+']
            allowed['-'] = self.key_actions['-']

        elif self.port.state == self.port.STATE_PAUSE:
            allowed['r'] = self.key_actions['r']


        return allowed

    def action_toggle_graph(self):
        try:
            self.mng.tui.is_graph = not self.mng.tui.is_graph
        except Exception:
            pass

        return ""

    def action_pause (self):
        try:
            self.stateless_client.pause(ports = [self.port_id])
        except STLError:
            pass

        return ""

    def action_resume (self):
        try:
            self.stateless_client.resume(ports = [self.port_id])
        except STLError:
            pass

        return ""


    def action_raise (self):
        mult = {'type': 'percentage', 'value': 5, 'op': 'add'}

        try:
            self.stateless_client.update(mult = mult, ports = [self.port_id])
        except STLError:
            pass

        return ""

    def action_lower (self):
        mult = {'type': 'percentage', 'value': 5, 'op': 'sub'}

        try:
            self.stateless_client.update(mult = mult, ports = [self.port_id])
        except STLError:
            pass

        return ""

    def action_clear (self):
        self.stateless_client.clear_stats([self.port_id])
        return "port {0}: cleared stats".format(self.port_id)



# streams stats
class TrexTUIStreamsStats(TrexTUIPanel):
    def __init__ (self, mng):
        super(TrexTUIStreamsStats, self).__init__(mng, "sstats")

        self.key_actions = OrderedDict()

        self.key_actions['c'] = {'action': self.action_clear,  'legend': 'clear', 'show': True}


    def show (self):
        stats = self.stateless_client._get_formatted_stats(port_id_list = None, stats_mask = trex_stl_stats.SS_COMPAT)
        # print stats to screen
        for stat_type, stat_data in stats.items():
            text_tables.print_table_with_header(stat_data.text_table, stat_type)
        pass


    def get_key_actions (self):
        return self.key_actions 

    def action_clear (self):
         self.stateless_client.flow_stats.clear_stats()

         return ""


# log
class TrexTUILog():
    def __init__ (self):
        self.log = []

    def add_event (self, msg):
        self.log.append("[{0}] {1}".format(str(datetime.datetime.now().time()), msg))

    def show (self, max_lines = 4):

        cut = len(self.log) - max_lines
        if cut < 0:
            cut = 0

        print(format_text("\nLog:", 'bold', 'underline'))

        for msg in self.log[cut:]:
            print(msg)


# Panels manager (contains server panels)
class TrexTUIPanelManager():
    def __init__ (self, tui):
        self.tui = tui
        self.stateless_client = tui.stateless_client
        self.ports = self.stateless_client.get_all_ports()
        

        self.panels = {}
        self.panels['dashboard'] = TrexTUIDashBoard(self)
        self.panels['sstats']    = TrexTUIStreamsStats(self)

        self.key_actions = OrderedDict()
        self.key_actions['q'] = {'action': self.action_quit, 'legend': 'quit', 'show': True}
        self.key_actions['g'] = {'action': self.action_show_dash, 'legend': 'dashboard', 'show': True}
        self.key_actions['s'] = {'action': self.action_show_sstats, 'legend': 'streams stats', 'show': True}

        for port_id in self.ports:
            self.key_actions[str(port_id)] = {'action': self.action_show_port(port_id), 'legend': 'port {0}'.format(port_id), 'show': False}
            self.panels['port {0}'.format(port_id)] = TrexTUIPort(self, port_id)

        # start with dashboard
        self.main_panel = self.panels['dashboard']

        # log object
        self.log = TrexTUILog()

        self.generate_legend()

        self.conn_bar = SimpleBar('status: ', ['|','/','-','\\'])
        self.dis_bar =  SimpleBar('status: ', ['X', ' '])
        self.show_log = False


    def generate_legend (self):
        self.legend = "\n{:<12}".format("browse:")

        for k, v in self.key_actions.items():
            if v['show']:
                x = "'{0}' - {1}, ".format(k, v['legend'])
                self.legend += "{:}".format(x)

        self.legend += "'0-{0}' - port display".format(len(self.ports) - 1)


        self.legend += "\n{:<12}".format(self.main_panel.get_name() + ":")
        for k, v in self.main_panel.get_key_actions().items():
            if v['show']:
                x = "'{0}' - {1}, ".format(k, v['legend'])
                self.legend += "{:}".format(x)


    def print_connection_status (self):
        if self.tui.get_state() == self.tui.STATE_ACTIVE:
            self.conn_bar.show()
        else:
            self.dis_bar.show()

    def print_legend (self):
        print(format_text(self.legend, 'bold'))


    # on window switch or turn on / off of the TUI we call this
    def init (self, show_log = False):
        self.show_log = show_log
        self.generate_legend()

    def show (self):
        self.main_panel.show()
        self.print_connection_status()
        self.print_legend()

        if self.show_log:
            self.log.show()
        

    def handle_key (self, ch):
        # check for the manager registered actions
        if ch in self.key_actions:
            msg = self.key_actions[ch]['action']()

        # check for main panel actions
        elif ch in self.main_panel.get_key_actions():
            msg = self.main_panel.get_key_actions()[ch]['action']()

        else:
            msg = ""

        self.generate_legend()

        if msg == None:
            return False
        else:
            if msg:
                self.log.add_event(msg)
            return True
            

    # actions

    def action_quit (self):
        return None

    def action_show_dash (self):
        self.main_panel = self.panels['dashboard']
        self.init(self.show_log)
        return ""

    def action_show_port (self, port_id):
        def action_show_port_x ():
            self.main_panel = self.panels['port {0}'.format(port_id)]
            self.init()
            return ""

        return action_show_port_x


    def action_show_sstats (self):
        self.main_panel = self.panels['sstats']
        self.init(self.show_log)
        return ""

# shows a textual top style window
class TrexTUI():

    STATE_ACTIVE     = 0
    STATE_LOST_CONT  = 1
    STATE_RECONNECT  = 2
    is_graph = False

    def __init__ (self, stateless_client):
        self.stateless_client = stateless_client

        self.pm = TrexTUIPanelManager(self)



    def handle_key_input (self):
        # try to read a single key
        ch = os.read(sys.stdin.fileno(), 1).decode()
        if ch != None and len(ch) > 0:
            return (self.pm.handle_key(ch), True)

        else:
            return (True, False)
            

    def clear_screen (self):
        #os.system('clear')
        # maybe this is faster ?
        sys.stdout.write("\x1b[2J\x1b[H")



    def show (self, show_log = False):
        # init termios
        old_settings = termios.tcgetattr(sys.stdin)
        new_settings = termios.tcgetattr(sys.stdin)
        new_settings[3] = new_settings[3] & ~(termios.ECHO | termios.ICANON) # lflags
        new_settings[6][termios.VMIN] = 0  # cc
        new_settings[6][termios.VTIME] = 0 # cc
        termios.tcsetattr(sys.stdin, termios.TCSADRAIN, new_settings)

        self.pm.init(show_log)

        self.state = self.STATE_ACTIVE
        self.draw_policer = 0

        try:
            while True:
                # draw and handle user input
                cont, force_draw = self.handle_key_input()
                self.draw_screen(force_draw)
                if not cont:
                    break
                time.sleep(0.1)

                # regular state
                if self.state == self.STATE_ACTIVE:
                    # if no connectivity - move to lost connecitivty
                    if not self.stateless_client.async_client.is_alive():
                        self.stateless_client._invalidate_stats(self.pm.ports)
                        self.state = self.STATE_LOST_CONT


                # lost connectivity
                elif self.state == self.STATE_LOST_CONT:
                    # got it back
                    if self.stateless_client.async_client.is_alive():
                        # move to state reconnect
                        self.state = self.STATE_RECONNECT


                # restored connectivity - try to reconnect
                elif self.state == self.STATE_RECONNECT:

                    try:
                        self.stateless_client.connect()
                        self.state = self.STATE_ACTIVE
                    except STLError:
                        self.state = self.STATE_LOST_CONT


        finally:
            # restore
            termios.tcsetattr(sys.stdin, termios.TCSADRAIN, old_settings)

        print("")


    # draw once
    def draw_screen (self, force_draw = False):

        if (self.draw_policer >= 5) or (force_draw):

            # capture stdout to a string
            old_stdout = sys.stdout
            sys.stdout = mystdout = StringIO()
            self.pm.show()
            sys.stdout = old_stdout

            self.clear_screen()

            print(mystdout.getvalue())

            sys.stdout.flush()

            self.draw_policer = 0
        else:
            self.draw_policer += 1 

    def get_state (self):
        return self.state

