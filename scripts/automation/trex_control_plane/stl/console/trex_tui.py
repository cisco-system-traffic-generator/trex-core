from __future__ import print_function

import termios
import sys
import os
import time
import threading

from collections import OrderedDict, deque
from texttable import ansi_len


import datetime
import readline


if sys.version_info > (3,0):
    from io import StringIO
else:
    from cStringIO import StringIO

from trex_stl_lib.utils.text_opts import *
from trex_stl_lib.utils import text_tables
from trex_stl_lib import trex_stl_stats
from trex_stl_lib.utils.filters import ToggleFilter

class TUIQuit(Exception):
    pass


# for STL exceptions
from trex_stl_lib.api import *

def ascii_split (s):
    output = []

    lines = s.split('\n')
    for elem in lines:
        if ansi_len(elem) > 0:
            output.append(elem)

    return output

class SimpleBar(object):
    def __init__ (self, desc, pattern):
        self.desc = desc
        self.pattern = pattern
        self.pattern_len = len(pattern)
        self.index = 0

    def show (self, buffer):
        if self.desc:
            print(format_text("{0} {1}".format(self.desc, self.pattern[self.index]), 'bold'), file = buffer)
        else:
            print(format_text("{0}".format(self.pattern[self.index]), 'bold'), file = buffer)

        self.index = (self.index + 1) % self.pattern_len


# base type of a panel
class TrexTUIPanel(object):
    def __init__ (self, mng, name):

        self.mng = mng
        self.name = name
        self.stateless_client = mng.stateless_client
        self.is_graph = False

    def show (self, buffer):
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

        self.ports = self.stateless_client.get_all_ports()

        self.key_actions = OrderedDict()

        self.key_actions['c'] = {'action': self.action_clear,  'legend': 'clear', 'show': True}
        self.key_actions['p'] = {'action': self.action_pause,  'legend': 'pause', 'show': True, 'color': 'red'}
        self.key_actions['r'] = {'action': self.action_resume, 'legend': 'resume', 'show': True, 'color': 'blue'}

        self.key_actions['o'] = {'action': self.action_show_owned,  'legend': 'owned ports', 'show': True}
        self.key_actions['n'] = {'action': self.action_reset_view,  'legend': 'reset view', 'show': True}
        self.key_actions['a'] = {'action': self.action_show_all,  'legend': 'all ports', 'show': True}

        # register all the ports to the toggle action
        for port_id in self.ports:
            self.key_actions[str(port_id)] = {'action': self.action_toggle_port(port_id), 'legend': 'port {0}'.format(port_id), 'show': False}


        self.toggle_filter = ToggleFilter(self.ports)

        if self.stateless_client.get_acquired_ports():
            self.action_show_owned()
        else:
            self.action_show_all()


    def get_showed_ports (self):
        return self.toggle_filter.filter_items()


    def show (self, buffer):
        stats = self.stateless_client._get_formatted_stats(self.get_showed_ports())
        # print stats to screen
        for stat_type, stat_data in stats.items():
            text_tables.print_table_with_header(stat_data.text_table, stat_type, buffer = buffer)


    def get_key_actions (self):
        allowed = OrderedDict()


        allowed['n'] = self.key_actions['n']
        allowed['o'] = self.key_actions['o']
        allowed['a'] = self.key_actions['a']
        for i in self.ports:
            allowed[str(i)] = self.key_actions[str(i)]


        if self.get_showed_ports():
            allowed['c'] = self.key_actions['c']

        # if not all ports are acquired - no operations
        if not (set(self.get_showed_ports()) <= set(self.stateless_client.get_acquired_ports())):
            return allowed

        # if any/some ports can be resumed
        if set(self.get_showed_ports()) & set(self.stateless_client.get_paused_ports()):
            allowed['r'] = self.key_actions['r']

        # if any/some ports are transmitting - support those actions
        if set(self.get_showed_ports()) & set(self.stateless_client.get_transmitting_ports()):
            allowed['p'] = self.key_actions['p']


        return allowed


    ######### actions
    def action_pause (self):
        try:
            rc = self.stateless_client.pause(ports = self.get_showed_ports())
        except STLError:
            pass

        return ""



    def action_resume (self):
        try:
            self.stateless_client.resume(ports = self.get_showed_ports())
        except STLError:
            pass

        return ""


    def action_reset_view (self):
        self.toggle_filter.reset()
        return ""

    def action_show_owned (self):
        self.toggle_filter.reset()
        self.toggle_filter.toggle_items(*self.stateless_client.get_acquired_ports())
        return ""

    def action_show_all (self):
        self.toggle_filter.reset()
        self.toggle_filter.toggle_items(*self.stateless_client.get_all_ports())
        return ""

    def action_clear (self):
        self.stateless_client.clear_stats(self.toggle_filter.filter_items())
        return "cleared all stats"


    def action_toggle_port(self, port_id):
        def action_toggle_port_x():
            self.toggle_filter.toggle_item(port_id)
            return ""

        return action_toggle_port_x



# streams stats
class TrexTUIStreamsStats(TrexTUIPanel):
    def __init__ (self, mng):
        super(TrexTUIStreamsStats, self).__init__(mng, "sstats")

        self.key_actions = OrderedDict()

        self.key_actions['c'] = {'action': self.action_clear,  'legend': 'clear', 'show': True}


    def show (self, buffer):
        stats = self.stateless_client._get_formatted_stats(port_id_list = None, stats_mask = trex_stl_stats.SS_COMPAT)
        # print stats to screen
        for stat_type, stat_data in stats.items():
            text_tables.print_table_with_header(stat_data.text_table, stat_type, buffer = buffer)
        pass


    def get_key_actions (self):
        return self.key_actions 

    def action_clear (self):
         self.stateless_client.flow_stats.clear_stats()

         return ""


# latency stats
class TrexTUILatencyStats(TrexTUIPanel):
    def __init__ (self, mng):
        super(TrexTUILatencyStats, self).__init__(mng, "lstats")
        self.key_actions = OrderedDict()
        self.key_actions['c'] = {'action': self.action_clear,  'legend': 'clear', 'show': True}
        self.key_actions['h'] = {'action': self.action_toggle_histogram,  'legend': 'histogram toggle', 'show': True}
        self.is_histogram = False


    def show (self, buffer):
        if self.is_histogram:
            stats = self.stateless_client._get_formatted_stats(port_id_list = None, stats_mask = trex_stl_stats.LH_COMPAT)
        else:
            stats = self.stateless_client._get_formatted_stats(port_id_list = None, stats_mask = trex_stl_stats.LS_COMPAT)
        # print stats to screen
        for stat_type, stat_data in stats.items():
            if stat_type == 'latency_statistics':
                untouched_header = ' (usec)'
            else:
                untouched_header = ''
            text_tables.print_table_with_header(stat_data.text_table, stat_type, untouched_header = untouched_header, buffer = buffer)

    def get_key_actions (self):
        return self.key_actions 

    def action_toggle_histogram (self):
        self.is_histogram = not self.is_histogram
        return ""

    def action_clear (self):
         self.stateless_client.latency_stats.clear_stats()
         return ""


# utilization stats
class TrexTUIUtilizationStats(TrexTUIPanel):
    def __init__ (self, mng):
        super(TrexTUIUtilizationStats, self).__init__(mng, "ustats")
        self.key_actions = {}

    def show (self, buffer):
        stats = self.stateless_client._get_formatted_stats(port_id_list = None, stats_mask = trex_stl_stats.UT_COMPAT)
        # print stats to screen
        for stat_type, stat_data in stats.items():
            text_tables.print_table_with_header(stat_data.text_table, stat_type, buffer = buffer)

    def get_key_actions (self):
        return self.key_actions 


# log
class TrexTUILog():
    def __init__ (self):
        self.log = []

    def add_event (self, msg):
        self.log.append("[{0}] {1}".format(str(datetime.datetime.now().time()), msg))

    def show (self, buffer, max_lines = 4):

        cut = len(self.log) - max_lines
        if cut < 0:
            cut = 0

        print(format_text("\nLog:", 'bold', 'underline'), file = buffer)

        for msg in self.log[cut:]:
            print(msg, file = buffer)


# a predicate to wrap function as a bool
class Predicate(object):
    def __init__ (self, func):
        self.func = func

    def __nonzero__ (self):
        return True if self.func() else False
    def __bool__ (self):
        return True if self.func() else False


# Panels manager (contains server panels)
class TrexTUIPanelManager():
    def __init__ (self, tui):
        self.tui = tui
        self.stateless_client = tui.stateless_client
        self.ports = self.stateless_client.get_all_ports()
        self.locked = False

        self.panels = {}
        self.panels['dashboard'] = TrexTUIDashBoard(self)
        self.panels['sstats']    = TrexTUIStreamsStats(self)
        self.panels['lstats']    = TrexTUILatencyStats(self)
        self.panels['ustats']    = TrexTUIUtilizationStats(self)

        self.key_actions = OrderedDict()

        # we allow console only when ports are acquired
        self.key_actions['ESC'] = {'action': self.action_none, 'legend': 'console', 'show': Predicate(lambda : not self.locked)}

        self.key_actions['q'] = {'action': self.action_none, 'legend': 'quit', 'show': True}
        self.key_actions['d'] = {'action': self.action_show_dash, 'legend': 'dashboard', 'show': True}
        self.key_actions['s'] = {'action': self.action_show_sstats, 'legend': 'streams', 'show': True}
        self.key_actions['l'] = {'action': self.action_show_lstats, 'legend': 'latency', 'show': True}
        self.key_actions['u'] = {'action': self.action_show_ustats, 'legend': 'util', 'show': True}
        

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
                if v.get('color'):
                    self.legend += "{:}".format(format_text(x, v.get('color')))
                else:
                    self.legend += "{:}".format(x)


        self.legend += "\n{:<12}".format(self.main_panel.get_name() + ":")

        for k, v in self.main_panel.get_key_actions().items():
            if v['show']:
                x = "'{0}' - {1}, ".format(k, v['legend'])

                if v.get('color'):
                    self.legend += "{:}".format(format_text(x, v.get('color')))
                else:
                    self.legend += "{:}".format(x)


    def print_connection_status (self, buffer):
        if self.tui.get_state() == self.tui.STATE_ACTIVE:
            self.conn_bar.show(buffer = buffer)
        else:
            self.dis_bar.show(buffer = buffer)

    def print_legend (self, buffer):
        print(format_text(self.legend, 'bold'), file = buffer)


    # on window switch or turn on / off of the TUI we call this
    def init (self, show_log = False, locked = False):
        self.show_log = show_log
        self.locked = locked
        self.generate_legend()

    def show (self, show_legend, buffer):
        self.main_panel.show(buffer)
        self.print_connection_status(buffer)

        if show_legend:
            self.generate_legend()
            self.print_legend(buffer)

        if self.show_log:
            self.log.show(buffer)
        

    def handle_key (self, ch):
        # check for the manager registered actions
        if ch in self.key_actions:
            msg = self.key_actions[ch]['action']()

        # check for main panel actions
        elif ch in self.main_panel.get_key_actions():
            msg = self.main_panel.get_key_actions()[ch]['action']()

        else:
            return False

        self.generate_legend()
        return True

        #if msg == None:
        #    return False
        #else:
        #    if msg:
        #        self.log.add_event(msg)
        #    return True
            

    # actions

    def action_none (self):
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


    def action_show_lstats (self):
        self.main_panel = self.panels['lstats']
        self.init(self.show_log)
        return ""

    def action_show_ustats(self):
        self.main_panel = self.panels['ustats']
        self.init(self.show_log)
        return ""



# ScreenBuffer is a class designed to
# avoid inline delays when reprinting the screen
class ScreenBuffer():
    def __init__ (self, redraw_cb):
        self.snapshot = ''
        self.lock = threading.Lock()

        self.redraw_cb = redraw_cb
        self.update_flag = False


    def start (self):
        self.active = True
        self.t = threading.Thread(target = self.__handler)
        self.t.setDaemon(True)
        self.t.start()

    def stop (self):
        self.active = False
        self.t.join()


    # request an update
    def update (self):
        self.update_flag = True

    # fetch the screen, return None if no new screen exists yet
    def get (self):

        if not self.snapshot:
            return None

        # we have a snapshot - fetch it
        with self.lock:
            x = self.snapshot
            self.snapshot = None
            return x


    def __handler (self):

        while self.active:
            if self.update_flag:
                self.__redraw()

            time.sleep(0.01)

    # redraw the next screen
    def __redraw (self):
        buffer = StringIO()

        self.redraw_cb(buffer)

        with self.lock:
            self.snapshot = buffer
            self.update_flag = False

# a policer class to make sure no too-fast redraws
# occurs - it filters fast bursts of redraws
class RedrawPolicer():
    def __init__ (self, rate):
        self.ts = 0
        self.marked = False
        self.rate = rate
        self.force = False

    def mark_for_redraw (self, force = False):
        self.marked = True
        if force:
            self.force = True

    def should_redraw (self):
        dt = time.time() - self.ts
        return self.force or (self.marked and (dt > self.rate))

    def reset (self, restart = False):
        self.ts = time.time()
        self.marked = restart
        self.force = False


# shows a textual top style window
class TrexTUI():

    STATE_ACTIVE     = 0
    STATE_LOST_CONT  = 1
    STATE_RECONNECT  = 2
    is_graph = False

    MIN_ROWS = 50
    MIN_COLS = 111


    class ScreenSizeException(Exception):
        def __init__ (self, cols, rows):
            msg = "TUI requires console screen size of at least {0}x{1}, current is {2}x{3}".format(TrexTUI.MIN_COLS,
                                                                                                    TrexTUI.MIN_ROWS,
                                                                                                    cols,
                                                                                                    rows)
            super(TrexTUI.ScreenSizeException, self).__init__(msg)


    def __init__ (self, stateless_client):
        self.stateless_client = stateless_client

        self.tui_global_lock = threading.Lock()
        self.pm = TrexTUIPanelManager(self)
        self.sb = ScreenBuffer(self.redraw_handler)

    def redraw_handler (self, buffer):
        # this is executed by the screen buffer - should be protected against TUI commands
        with self.tui_global_lock:
            self.pm.show(show_legend = self.async_keys.is_legend_mode(), buffer = buffer)

    def clear_screen (self, lines = 50):
        # reposition the cursor
        sys.stdout.write("\x1b[0;0H")

        # clear all lines
        for i in range(lines):
            sys.stdout.write("\x1b[0K")
            if i < (lines - 1):
                sys.stdout.write("\n")

        # reposition the cursor
        sys.stdout.write("\x1b[0;0H")



    def show (self, client, save_console_history, show_log = False, locked = False):
        
        rows, cols = os.popen('stty size', 'r').read().split()
        if (int(rows) < TrexTUI.MIN_ROWS) or (int(cols) < TrexTUI.MIN_COLS):
            raise self.ScreenSizeException(rows = rows, cols = cols)

        with AsyncKeys(client, save_console_history, self.tui_global_lock, locked) as async_keys:
            sys.stdout.write("\x1bc")
            self.async_keys = async_keys
            self.show_internal(show_log, locked)



    def show_internal (self, show_log, locked):

        self.pm.init(show_log, locked)

        self.state = self.STATE_ACTIVE

        # create print policers
        self.full_redraw = RedrawPolicer(0.5)
        self.keys_redraw = RedrawPolicer(0.05)
        self.full_redraw.mark_for_redraw()


        try:
            self.sb.start()

            while True:
                # draw and handle user input
                status = self.async_keys.tick(self.pm)

                # prepare the next frame
                self.prepare(status)
                time.sleep(0.01)
                self.draw_screen()

                with self.tui_global_lock:
                    self.handle_state_machine()

        except TUIQuit:
            print("\nExiting TUI...")

        finally:
            self.sb.stop()

        print("")

        

    # handle state machine
    def handle_state_machine (self):
       # regular state
        if self.state == self.STATE_ACTIVE:
            # if no connectivity - move to lost connecitivty
            if not self.stateless_client.async_client.is_active():
                self.stateless_client._invalidate_stats(self.pm.ports)
                self.state = self.STATE_LOST_CONT


        # lost connectivity
        elif self.state == self.STATE_LOST_CONT:
            # if the async is alive (might be zomibe, but alive) try to reconnect
            if self.stateless_client.async_client.is_alive():
                # move to state reconnect
                self.state = self.STATE_RECONNECT


        # restored connectivity - try to reconnect
        elif self.state == self.STATE_RECONNECT:

            try:
                self.stateless_client.connect()
                self.stateless_client.acquire()
                self.state = self.STATE_ACTIVE
            except STLError:
                self.state = self.STATE_LOST_CONT


    # logic before printing
    def prepare (self, status):
        if status == AsyncKeys.STATUS_REDRAW_ALL:
            self.full_redraw.mark_for_redraw(force = True)

        elif status == AsyncKeys.STATUS_REDRAW_KEYS:
            self.keys_redraw.mark_for_redraw()

        if self.full_redraw.should_redraw():
            self.sb.update()
            self.full_redraw.reset(restart = True)

        return


    # draw once
    def draw_screen (self):

        # check for screen buffer's new screen
        x = self.sb.get()

        # we have a new screen to draw
        if x:
            self.clear_screen()
            
            self.async_keys.draw(x)
            sys.stdout.write(x.getvalue())
            sys.stdout.flush()

        # maybe we need to redraw the keys
        elif self.keys_redraw.should_redraw():
            sys.stdout.write("\x1b[4A")
            self.async_keys.draw(sys.stdout)
            sys.stdout.flush()

            # reset the policer for next time
            self.keys_redraw.reset()


     

    def get_state (self):
        return self.state


class TokenParser(object):
    def __init__ (self, seq):
        self.buffer = list(seq)

    def pop (self):
        return self.buffer.pop(0)
        

    def peek (self):
        if not self.buffer:
            return None
        return self.buffer[0]

    def next_token (self):
        if not self.peek():
            return None

        token = self.pop()

        # special chars
        if token == '\x1b' and self.peek() == '[':
            token += self.pop()
            if self.peek():
                token += self.pop()

        return token

    def parse (self):
        tokens = []

        while True:
            token = self.next_token()
            if token == None:
                break
            tokens.append(token)

        return tokens


# handles async IO
class AsyncKeys:

    MODE_LEGEND  = 1
    MODE_CONSOLE = 2

    STATUS_NONE        = 0
    STATUS_REDRAW_KEYS = 1
    STATUS_REDRAW_ALL  = 2

    def __init__ (self, client, save_console_history, tui_global_lock, locked = False):
        self.tui_global_lock = tui_global_lock

        self.engine_console = AsyncKeysEngineConsole(self, client, save_console_history)
        self.engine_legend  = AsyncKeysEngineLegend(self)
        self.locked = locked

        if locked:
            self.engine = self.engine_legend
            self.locked = True
        else:
            self.engine = self.engine_console
            self.locked = False

    def __enter__ (self):
        # init termios
        self.old_settings = termios.tcgetattr(sys.stdin)
        new_settings = termios.tcgetattr(sys.stdin)
        new_settings[3] = new_settings[3] & ~(termios.ECHO | termios.ICANON) # lflags
        new_settings[6][termios.VMIN] = 0  # cc
        new_settings[6][termios.VTIME] = 0 # cc
        termios.tcsetattr(sys.stdin, termios.TCSADRAIN, new_settings)

        # huge buffer - no print without flush
        sys.stdout = open('/dev/stdout', 'w', TrexTUI.MIN_COLS * TrexTUI.MIN_COLS * 2)
        return self

    def __exit__ (self, type, value, traceback):
        termios.tcsetattr(sys.stdin, termios.TCSADRAIN, self.old_settings)

        # restore sys.stdout
        sys.stdout.close()
        sys.stdout = sys.__stdout__


    def is_legend_mode (self):
        return self.engine.get_type() == AsyncKeys.MODE_LEGEND

    def is_console_mode (self):
        return self.engine.get_type == AsyncKeys.MODE_CONSOLE

    def switch (self):
        if self.is_legend_mode():
            self.engine = self.engine_console
        else:
            self.engine = self.engine_legend


    # parse the buffer to manageble tokens
    def parse_tokens (self, seq):

        tokens = []
        chars = list(seq)

        while chars:
            token = chars.pop(0)

            # special chars
            if token == '\x1b' and chars[0] == '[':
                token += chars.pop(0)
                token += chars.pop(0)

            tokens.append(token)

        return tokens

    def handle_token (self, token, pm):
        # ESC for switch
        if token == '\x1b':
            if not self.locked:
                self.switch()
            return self.STATUS_REDRAW_ALL

        # EOF (ctrl + D)
        if token == '\x04':
            raise TUIQuit()

        # pass tick to engine
        return self.engine.tick(token, pm)


    def tick (self, pm):
        rc = self.STATUS_NONE

        # fetch the stdin buffer
        seq = os.read(sys.stdin.fileno(), 1024).decode()
        if not seq:
            return self.STATUS_NONE

        # parse all the tokens from the buffer
        tokens = TokenParser(seq).parse()

        # process them
        for token in tokens:
            token_rc = self.handle_token(token, pm)
            rc = max(rc, token_rc)

    
        return rc


    def draw (self, buffer):
        self.engine.draw(buffer)
 

    
# Legend engine
class AsyncKeysEngineLegend:
    def __init__ (self, async):
        self.async = async

    def get_type (self):
        return self.async.MODE_LEGEND

    def tick (self, seq, pm):

        if seq == 'q':
            raise TUIQuit()

        # ignore escapes
        if len(seq) > 1:
            return AsyncKeys.STATUS_NONE

        rc = pm.handle_key(seq)
        return AsyncKeys.STATUS_REDRAW_ALL if rc else AsyncKeys.STATUS_NONE

    def draw (self, buffer):
        pass


# console engine
class AsyncKeysEngineConsole:
    def __init__ (self, async, client, save_console_history):
        self.async = async
        self.lines = deque(maxlen = 100)

        self.generate_prompt       = client.generate_prompt
        self.save_console_history  = save_console_history

        self.ac = {'start'        : client.start_line,
                   'stop'         : client.stop_line,
                   'pause'        : client.pause_line,
                   'clear'        : client.clear_stats_line,
                   'push'         : client.push_line,
                   'resume'       : client.resume_line,
                   'reset'        : client.reset_line,
                   'update'       : client.update_line,
                   'connect'      : client.connect_line,
                   'disconnect'   : client.disconnect_line,
                   'acquire'      : client.acquire_line,
                   'release'      : client.release_line,
                   'quit'         : self.action_quit,
                   'q'            : self.action_quit,
                   'exit'         : self.action_quit,
                   'help'         : self.action_help,
                   '?'            : self.action_help}

        # fetch readline history and add relevants
        for i in range(0, readline.get_current_history_length()):
            cmd = readline.get_history_item(i)
            if cmd and cmd.split()[0] in self.ac:
                self.lines.appendleft(CmdLine(cmd))

        # new line
        self.lines.appendleft(CmdLine(''))
        self.line_index = 0
        self.last_status = ''

    def action_quit (self, _):
        raise TUIQuit()

    def action_help (self, _):
        return ' '.join([format_text(cmd, 'bold') for cmd in self.ac.keys()])

    def get_type (self):
        return self.async.MODE_CONSOLE


    def handle_escape_char (self, seq):
        # up
        if seq == '\x1b[A':
            self.line_index = min(self.line_index + 1, len(self.lines) - 1)

        # down
        elif seq == '\x1b[B':
            self.line_index = max(self.line_index - 1, 0)

        # left
        elif seq == '\x1b[D':
            self.lines[self.line_index].go_left()

        # right
        elif seq == '\x1b[C':
            self.lines[self.line_index].go_right()

        # del
        elif seq == '\x1b[3~':
            self.lines[self.line_index].del_key()

        # home
        elif seq == '\x1b[H':
            self.lines[self.line_index].home_key()

        # end
        elif seq == '\x1b[F':
            self.lines[self.line_index].end_key()
            return True

        # unknown key
        else:
            return AsyncKeys.STATUS_NONE

        return AsyncKeys.STATUS_REDRAW_KEYS


    def tick (self, seq, _):
    
        # handle escape chars
        if len(seq) > 1:
            return self.handle_escape_char(seq)

        # handle each char
        for ch in seq:
            return self.handle_single_key(ch)


    
    def handle_single_key (self, ch):
        # newline
        if ch == '\n':
            self.handle_cmd()

        # backspace
        elif ch == '\x7f':
            self.lines[self.line_index].backspace()

        # TAB
        elif ch == '\t':
            tokens = self.lines[self.line_index].get().split()
            if not tokens:
                return

            if len(tokens) == 1:
                self.handle_tab_names(tokens[0])
            else:
                self.handle_tab_files(tokens)


        # simple char
        else:
            self.lines[self.line_index] += ch

        return AsyncKeys.STATUS_REDRAW_KEYS


    # handle TAB key for completing function names
    def handle_tab_names (self, cur):
        matching_cmds = [x for x in self.ac if x.startswith(cur)]

        common = os.path.commonprefix([x for x in self.ac if x.startswith(cur)])
        if common:
            if len(matching_cmds) == 1:
                self.lines[self.line_index].set(common + ' ')
                self.last_status = ''
            else:
                self.lines[self.line_index].set(common)
                self.last_status = 'ambigious: '+ ' '.join([format_text(cmd, 'bold') for cmd in matching_cmds])


    # handle TAB for completing filenames
    def handle_tab_files (self, tokens):

        # only commands with files
        if tokens[0] not in {'start', 'push'}:
            return

        # '-f' with no paramters - no partial and use current dir
        if tokens[-1] == '-f':
            partial = ''
            d = '.'

        # got a partial path
        elif tokens[-2] == '-f':
            partial = tokens.pop()

            # check for dirs
            dirname, basename = os.path.dirname(partial), os.path.basename(partial)
            if os.path.isdir(dirname):
                d = dirname
                partial = basename
            else:
                d = '.'
        else:
            return

        # fetch all dirs and files matching wildcard
        files = []
        for x in os.listdir(d):
            if os.path.isdir(os.path.join(d, x)):
                files.append(x + '/')
            elif x.endswith( ('.py', 'yaml', 'pcap', 'cap', 'erf') ):
                files.append(x)

        # dir might not have the files
        if not files:
            self.last_status = format_text('no loadble files under path', 'bold')
            return


        # find all the matching files
        matching_files = [x for x in files if x.startswith(partial)] if partial else files

        # do we have a longer common than partial ?
        common = os.path.commonprefix([x for x in files if x.startswith(partial)])
        if not common:
            common = partial

        tokens.append(os.path.join(d, common) if d is not '.' else common)

        # reforge the line
        newline = ' '.join(tokens)

        if len(matching_files) == 1:
            if os.path.isfile(tokens[-1]):
                newline += ' '

            self.lines[self.line_index].set(newline)
            self.last_status = ''
        else:
            self.lines[self.line_index].set(newline)
            self.last_status = '    '.join([format_text(f, 'bold') for f in matching_files[:5]])
            if len(matching_files) > 5:
                self.last_status += ' ... [{0} more matches]'.format(len(matching_files) - 5)



    def split_cmd (self, cmd):
        s = cmd.split(' ', 1)
        op = s[0]
        param = s[1] if len(s) == 2 else ''
        return op, param


    def handle_cmd (self):

        cmd = self.lines[self.line_index].get().strip()
        if not cmd:
            return

        op, param = self.split_cmd(cmd)
        
        func = self.ac.get(op)
        if func:
            with self.async.tui_global_lock:
                func_rc = func(param)

        # take out the empty line
        empty_line = self.lines.popleft()
        assert(empty_line.ro_line == '')

        if not self.lines or self.lines[0].ro_line != cmd:
            self.lines.appendleft(CmdLine(cmd))
        
        # back in
        self.lines.appendleft(empty_line)
        self.line_index = 0
        readline.add_history(cmd)
        self.save_console_history()

        # back to readonly
        for line in self.lines:
            line.invalidate()

        assert(self.lines[0].modified == False)
        color = None
        if not func:
            self.last_status = "unknown command: '{0}'".format(format_text(cmd.split()[0], 'bold'))
        else:
            # internal commands
            if isinstance(func_rc, str):
                self.last_status = func_rc

            # RC response
            else:
                # success
                if func_rc:
                    self.last_status = format_text("[OK]", 'green')
                # errors
                else:
                    err_msgs = ascii_split(str(func_rc))
                    self.last_status = format_text(clear_formatting(err_msgs[0]), 'red')
                    if len(err_msgs) > 1:
                        self.last_status += " [{0} more errors messages]".format(len(err_msgs) - 1)
                    color = 'red'



        # trim too long lines
        if ansi_len(self.last_status) > TrexTUI.MIN_COLS:
            self.last_status = format_text(self.last_status[:TrexTUI.MIN_COLS] + "...", color, 'bold')


    def draw (self, buffer):
        buffer.write("\nPress 'ESC' for navigation panel...\n")
        buffer.write("status: \x1b[0K{0}\n".format(self.last_status))
        buffer.write("\n{0}\x1b[0K".format(self.generate_prompt(prefix = 'tui')))
        self.lines[self.line_index].draw(buffer)


# a readline alike command line - can be modified during edit
class CmdLine(object):
    def __init__ (self, line):
        self.ro_line      = line
        self.w_line       = None
        self.modified     = False
        self.cursor_index = len(line)

    def get (self):
        if self.modified:
            return self.w_line
        else:
            return self.ro_line

    def set (self, line, cursor_pos = None):
        self.w_line = line
        self.modified = True

        if cursor_pos is None:
            self.cursor_index = len(self.w_line)
        else:
            self.cursor_index = cursor_pos


    def __add__ (self, other):
        assert(0)


    def __str__ (self):
        return self.get()


    def __iadd__ (self, other):

        self.set(self.get()[:self.cursor_index] + other + self.get()[self.cursor_index:],
                 cursor_pos = self.cursor_index + len(other))

        return self


    def backspace (self):
        if self.cursor_index == 0:
            return

        self.set(self.get()[:self.cursor_index - 1] + self.get()[self.cursor_index:],
                 self.cursor_index - 1)


    def del_key (self):
        if self.cursor_index == len(self.get()):
            return

        self.set(self.get()[:self.cursor_index] + self.get()[self.cursor_index + 1:],
                 self.cursor_index)

    def home_key (self):
        self.cursor_index = 0

    def end_key (self):
        self.cursor_index = len(self.get())

    def invalidate (self):
        self.modified = False
        self.w_line = None
        self.cursor_index = len(self.ro_line)

    def go_left (self):
        self.cursor_index = max(0, self.cursor_index - 1)

    def go_right (self):
        self.cursor_index = min(len(self.get()), self.cursor_index + 1)

    def draw (self, buffer):
        buffer.write(self.get())
        buffer.write('\b' * (len(self.get()) - self.cursor_index))

