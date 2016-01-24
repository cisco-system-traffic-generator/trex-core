import termios
import sys
import os
import time
from common.text_opts import *
from common import trex_stats
from client_utils import text_tables
from collections import OrderedDict
import datetime
from cStringIO import StringIO
from client.trex_stateless_client import STLError

class SimpleBar(object):
    def __init__ (self, desc, pattern):
        self.desc = desc
        self.pattern = pattern
        self.pattern_len = len(pattern)
        self.index = 0

    def show (self):
        if self.desc:
            print format_text("{0} {1}".format(self.desc, self.pattern[self.index]), 'bold')
        else:
            print format_text("{0}".format(self.pattern[self.index]), 'bold')

        self.index = (self.index + 1) % self.pattern_len


# base type of a panel
class TrexTUIPanel(object):
    def __init__ (self, mng, name):

        self.mng = mng
        self.name = name
        self.stateless_client = mng.stateless_client

    def show (self):
        raise NotImplementedError("must implement this")

    def get_key_actions (self):
        raise NotImplementedError("must implement this")

    def get_name (self):
        return self.name


# dashboard panel
class TrexTUIDashBoard(TrexTUIPanel):
    def __init__ (self, mng):
        super(TrexTUIDashBoard, self).__init__(mng, "dashboard")

        self.key_actions = OrderedDict()

        self.key_actions['c'] = {'action': self.action_clear,  'legend': 'clear', 'show': True}
        self.key_actions['p'] = {'action': self.action_pause,  'legend': 'pause', 'show': True}
        self.key_actions['r'] = {'action': self.action_resume, 'legend': 'resume', 'show': True}
        self.key_actions['+'] = {'action': self.action_raise,  'legend': 'up 5%', 'show': True}
        self.key_actions['-'] = {'action': self.action_lower,  'legend': 'low 5%', 'show': True}

        self.ports = self.stateless_client.get_all_ports()


    def show (self):
        stats = self.stateless_client._get_formatted_stats(self.ports, trex_stats.COMPACT)
        # print stats to screen
        for stat_type, stat_data in stats.iteritems():
            text_tables.print_table_with_header(stat_data.text_table, stat_type)


    def get_key_actions (self):
        allowed = {}

        allowed['c'] = self.key_actions['c']

        if self.stateless_client.is_all_ports_acquired():
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


    def show (self):
        stats = self.stateless_client._get_formatted_stats([self.port_id], trex_stats.COMPACT)
        # print stats to screen
        for stat_type, stat_data in stats.iteritems():
            text_tables.print_table_with_header(stat_data.text_table, stat_type)

    def get_key_actions (self):

        allowed = {}

        allowed['c'] = self.key_actions['c']

        if self.stateless_client.is_all_ports_acquired():
            return allowed

        if self.port.state == self.port.STATE_TX:
            allowed['p'] = self.key_actions['p']
            allowed['+'] = self.key_actions['+']
            allowed['-'] = self.key_actions['-']

        elif self.port.state == self.port.STATE_PAUSE:
            allowed['r'] = self.key_actions['r']


        return allowed

    # actions
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

        print format_text("\nLog:", 'bold', 'underline')

        for msg in self.log[cut:]:
            print msg


# Panels manager (contains server panels)
class TrexTUIPanelManager():
    def __init__ (self, tui):
        self.tui = tui
        self.stateless_client = tui.stateless_client
        self.ports = self.stateless_client.get_all_ports()
        

        self.panels = {}
        self.panels['dashboard'] = TrexTUIDashBoard(self)

        self.key_actions = OrderedDict()
        self.key_actions['q'] = {'action': self.action_quit, 'legend': 'quit', 'show': True}
        self.key_actions['g'] = {'action': self.action_show_dash, 'legend': 'dashboard', 'show': True}

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

        for k, v in self.key_actions.iteritems():
            if v['show']:
                x = "'{0}' - {1}, ".format(k, v['legend'])
                self.legend += "{:}".format(x)

        self.legend += "'0-{0}' - port display".format(len(self.ports) - 1)


        self.legend += "\n{:<12}".format(self.main_panel.get_name() + ":")
        for k, v in self.main_panel.get_key_actions().iteritems():
            if v['show']:
                x = "'{0}' - {1}, ".format(k, v['legend'])
                self.legend += "{:}".format(x)


    def print_connection_status (self):
        if self.tui.get_state() == self.tui.STATE_ACTIVE:
            self.conn_bar.show()
        else:
            self.dis_bar.show()

    def print_legend (self):
        print format_text(self.legend, 'bold')


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



# shows a textual top style window
class TrexTUI():

    STATE_ACTIVE     = 0
    STATE_LOST_CONT  = 1
    STATE_RECONNECT  = 2

    def __init__ (self, stateless_client):
        self.stateless_client = stateless_client

        self.pm = TrexTUIPanelManager(self)



    def handle_key_input (self):
        # try to read a single key
        ch = os.read(sys.stdin.fileno(), 1)
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
                        self.stateless_client.connect("RO")
                        self.state = self.STATE_ACTIVE
                    except STLError:
                        self.state = self.STATE_LOST_CONT


        finally:
            # restore
            termios.tcsetattr(sys.stdin, termios.TCSADRAIN, old_settings)

        print ""


    # draw once
    def draw_screen (self, force_draw = False):

        if (self.draw_policer >= 5) or (force_draw):

            # capture stdout to a string
            old_stdout = sys.stdout
            sys.stdout = mystdout = StringIO()
            self.pm.show()
            sys.stdout = old_stdout

            self.clear_screen()
            print mystdout.getvalue()
            sys.stdout.flush()

            self.draw_policer = 0
        else:
            self.draw_policer += 1 

    def get_state (self):
        return self.state

