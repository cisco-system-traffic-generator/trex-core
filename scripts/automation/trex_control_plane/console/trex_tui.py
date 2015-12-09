import termios
import sys
import os
import time
from common.text_opts import *
from common import trex_stats
from client_utils import text_tables
from collections import OrderedDict
import datetime

# base type of a panel
class TrexTUIPanel(object):
    def __init__ (self, mng, name):

        self.mng = mng
        self.name = name
        self.stateless_client = mng.stateless_client


    def show (self):
        raise Exception("must implement this")

    def get_key_actions (self):
        raise Exception("must implement this")

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
        stats = self.stateless_client.cmd_stats(self.ports, trex_stats.COMPACT)
        # print stats to screen
        for stat_type, stat_data in stats.iteritems():
            text_tables.print_table_with_header(stat_data.text_table, stat_type)


    def get_key_actions (self):
        allowed = {}

        allowed['c'] = self.key_actions['c']

        # thats it for read only
        if self.stateless_client.is_read_only():
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
        rc = self.stateless_client.pause_traffic(self.mng.ports)

        ports_succeeded = []
        for rc_single, port_id in zip(rc.rc_list, self.mng.ports):
            if rc_single.rc:
                ports_succeeded.append(port_id)

        if len(ports_succeeded) > 0:
            return "paused traffic on port(s): {0}".format(ports_succeeded)
        else:
            return ""


    def action_resume (self):
        rc = self.stateless_client.resume_traffic(self.mng.ports)

        ports_succeeded = []
        for rc_single, port_id in zip(rc.rc_list, self.mng.ports):
            if rc_single.rc:
                ports_succeeded.append(port_id)

        if len(ports_succeeded) > 0:
            return "resumed traffic on port(s): {0}".format(ports_succeeded)
        else:
            return ""


    def action_raise (self):
        mul = {'type': 'percentage', 'value': 5, 'op': 'add'}
        rc = self.stateless_client.update_traffic(mul, self.mng.ports)

        ports_succeeded = []
        for rc_single, port_id in zip(rc.rc_list, self.mng.ports):
            if rc_single.rc:
                ports_succeeded.append(port_id)

        if len(ports_succeeded) > 0:
            return "raised B/W by %5 on port(s): {0}".format(ports_succeeded)
        else:
            return ""

    def action_lower (self):
        mul = {'type': 'percentage', 'value': 5, 'op': 'sub'}
        rc = self.stateless_client.update_traffic(mul, self.mng.ports)

        ports_succeeded = []
        for rc_single, port_id in zip(rc.rc_list, self.mng.ports):
            if rc_single.rc:
                ports_succeeded.append(port_id)

        if len(ports_succeeded) > 0:
            return "lowered B/W by %5 on port(s): {0}".format(ports_succeeded)
        else:
            return ""


    def action_clear (self):
        self.stateless_client.cmd_clear(self.mng.ports)
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
        stats = self.stateless_client.cmd_stats([self.port_id], trex_stats.COMPACT)
        # print stats to screen
        for stat_type, stat_data in stats.iteritems():
            text_tables.print_table_with_header(stat_data.text_table, stat_type)

    def get_key_actions (self):

        allowed = {}

        allowed['c'] = self.key_actions['c']

        # thats it for read only
        if self.stateless_client.is_read_only():
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
        rc = self.stateless_client.pause_traffic([self.port_id])
        if rc.good():
            return "port {0}: paused traffic".format(self.port_id)
        else:
            return ""

    def action_resume (self):
        rc = self.stateless_client.resume_traffic([self.port_id])
        if rc.good():
            return "port {0}: resumed traffic".format(self.port_id)
        else:
            return ""

    def action_raise (self):
        mul = {'type': 'percentage', 'value': 5, 'op': 'add'}
        rc = self.stateless_client.update_traffic(mul, [self.port_id])

        if rc.good():
            return "port {0}: raised B/W by 5%".format(self.port_id)
        else:
            return ""

    def action_lower (self):
        mul = {'type': 'percentage', 'value': 5, 'op': 'sub'}
        rc = self.stateless_client.update_traffic(mul, [self.port_id])

        if rc.good():
            return "port {0}: lowered B/W by 5%".format(self.port_id)
        else:
            return ""

    def action_clear (self):
        self.stateless_client.cmd_clear([self.port_id])
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


    def print_legend (self):
        print format_text(self.legend, 'bold')


    # on window switch or turn on / off of the TUI we call this
    def init (self):
        self.generate_legend()

    def show (self):
        print self.ports
        self.main_panel.show()
        self.print_legend()
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
        self.init()
        return ""

    def action_show_port (self, port_id):
        def action_show_port_x ():
            self.main_panel = self.panels['port {0}'.format(port_id)]
            self.init()
            return ""

        return action_show_port_x



# shows a textual top style window
class TrexTUI():
    def __init__ (self, stateless_client):
        self.stateless_client = stateless_client

        self.pm = TrexTUIPanelManager(self)



    def handle_key_input (self):
        # try to read a single key
        ch = os.read(sys.stdin.fileno(), 1)
        if ch != None and len(ch) > 0:
            return self.pm.handle_key(ch)

        else:
            return True
            

    def clear_screen (self):
        os.system('clear')



    def show (self):
        # init termios
        old_settings = termios.tcgetattr(sys.stdin)
        new_settings = termios.tcgetattr(sys.stdin)
        new_settings[3] = new_settings[3] & ~(termios.ECHO | termios.ICANON) # lflags
        new_settings[6][termios.VMIN] = 0  # cc
        new_settings[6][termios.VTIME] = 0 # cc
        termios.tcsetattr(sys.stdin, termios.TCSADRAIN, new_settings)

        self.pm.init()

        try:
            while True:
                self.clear_screen()

                cont = self.handle_key_input()
                self.pm.show()

                if not cont:
                    break

                time.sleep(0.1)

        finally:
            # restore
            termios.tcsetattr(sys.stdin, termios.TCSADRAIN, old_settings)

        print ""

    # key actions
    def action_quit (self):
        return False

