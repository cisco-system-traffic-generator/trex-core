import termios
import sys
import os
import time
from collections import OrderedDict, deque
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


    def show (self):
        stats = self.stateless_client._get_formatted_stats(self.get_showed_ports())
        # print stats to screen
        for stat_type, stat_data in stats.items():
            text_tables.print_table_with_header(stat_data.text_table, stat_type)


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


# latency stats
class TrexTUILatencyStats(TrexTUIPanel):
    def __init__ (self, mng):
        super(TrexTUILatencyStats, self).__init__(mng, "lstats")
        self.key_actions = OrderedDict()
        self.key_actions['c'] = {'action': self.action_clear,  'legend': 'clear', 'show': True}
        self.key_actions['h'] = {'action': self.action_toggle_histogram,  'legend': 'histogram toggle', 'show': True}
        self.is_histogram = False


    def show (self):
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
            text_tables.print_table_with_header(stat_data.text_table, stat_type, untouched_header = untouched_header)

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

    def show (self):
        stats = self.stateless_client._get_formatted_stats(port_id_list = None, stats_mask = trex_stl_stats.UT_COMPAT)
        # print stats to screen
        for stat_type, stat_data in stats.items():
            text_tables.print_table_with_header(stat_data.text_table, stat_type)

    def get_key_actions (self):
        return self.key_actions 


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


    def print_connection_status (self):
        if self.tui.get_state() == self.tui.STATE_ACTIVE:
            self.conn_bar.show()
        else:
            self.dis_bar.show()

    def print_legend (self):
        print(format_text(self.legend, 'bold'))


    # on window switch or turn on / off of the TUI we call this
    def init (self, show_log = False, locked = False):
        self.show_log = show_log
        self.locked = locked
        self.generate_legend()

    def show (self, show_legend):
        self.main_panel.show()
        self.print_connection_status()

        if show_legend:
            self.generate_legend()
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

# shows a textual top style window
class TrexTUI():

    STATE_ACTIVE     = 0
    STATE_LOST_CONT  = 1
    STATE_RECONNECT  = 2
    is_graph = False

    def __init__ (self, stateless_client):
        self.stateless_client = stateless_client

        self.pm = TrexTUIPanelManager(self)
                    

    def clear_screen (self):
        #os.system('clear')
        # maybe this is faster ?
        sys.stdout.write("\x1b[2J\x1b[H")


    def show (self, client, show_log = False, locked = False):
        with AsyncKeys(client, locked) as async_keys:
            self.async_keys = async_keys
            self.show_internal(show_log, locked)



    def show_internal (self, show_log, locked):

        self.pm.init(show_log, locked)

        self.state = self.STATE_ACTIVE
        self.draw_policer = 0

        try:
            while True:
                # draw and handle user input
                status = self.async_keys.tick(self.pm)

                self.draw_screen(status)
                if status == AsyncKeys.STATUS_NONE:
                    time.sleep(0.001)

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


        except TUIQuit:
            print("\nExiting TUI...")

        print("")


    # draw once
    def draw_screen (self, status):

        # only redraw the keys line
        if status == AsyncKeys.STATUS_REDRAW_KEYS:
            self.clear_screen()
            sys.stdout.write(self.last_snap)
            self.async_keys.draw()
            sys.stdout.flush()
            return

        if (self.draw_policer >= 500) or (status == AsyncKeys.STATUS_REDRAW_ALL):

            # capture stdout to a string
            old_stdout = sys.stdout
            sys.stdout = mystdout = StringIO()
            self.pm.show(show_legend = self.async_keys.is_legend_mode())
            self.last_snap = mystdout.getvalue()

            self.async_keys.draw()
            sys.stdout = old_stdout

            self.clear_screen()

            sys.stdout.write(mystdout.getvalue())
           
            sys.stdout.flush()

            self.draw_policer = 0
        else:
            self.draw_policer += 1 


    def get_state (self):
        return self.state





# handles async IO
class AsyncKeys:

    MODE_LEGEND  = 1
    MODE_CONSOLE = 2

    STATUS_NONE        = 0
    STATUS_REDRAW_KEYS = 1
    STATUS_REDRAW_ALL  = 2

    def __init__ (self, client, locked = False):
        self.engine_console = AsyncKeysEngineConsole(self, client)
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
        return self

    def __exit__ (self, type, value, traceback):
        termios.tcsetattr(sys.stdin, termios.TCSADRAIN, self.old_settings)


    def is_legend_mode (self):
        return self.engine.get_type() == AsyncKeys.MODE_LEGEND

    def is_console_mode (self):
        return self.engine.get_type == AsyncKeys.MODE_CONSOLE

    def switch (self):
        if self.is_legend_mode():
            self.engine = self.engine_console
        else:
            self.engine = self.engine_legend


    def tick (self, pm):
        seq = ''
        # drain all chars
        while True:
            ch = os.read(sys.stdin.fileno(), 1).decode()
            if not ch:
                break
            seq += ch

        if not seq:
            return self.STATUS_NONE

        # ESC for switch
        if seq == '\x1b':
            if not self.locked:
                self.switch()
            return self.STATUS_REDRAW_ALL

        # EOF (ctrl + D)
        if seq == '\x04':
            raise TUIQuit()

        # pass tick to engine
        return self.engine.tick(seq, pm)


    def draw (self):
        self.engine.draw()
 

    
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

    def draw (self):
        pass


# console engine
class AsyncKeysEngineConsole:
    def __init__ (self, async, client):
        self.async = async
        self.lines = deque(maxlen = 100)

        self.ac = {'start' : client.start_line,
                   'stop'  : client.stop_line,
                   'pause' : client.pause_line,
                   'resume': client.resume_line,
                   'update': client.update_line,
                   'quit'  : self.action_quit,
                   'q'     : self.action_quit,
                   'exit'  : self.action_quit,
                   'help'  : self.action_help,
                   '?'     : self.action_help}

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
            cur = self.lines[self.line_index].get()
            if not cur:
                return

            matching_cmds = [x for x in self.ac if x.startswith(cur)]
            
            common = os.path.commonprefix([x for x in self.ac if x.startswith(cur)])
            if common:
                if len(matching_cmds) == 1:
                    self.lines[self.line_index].set(common + ' ')
                    self.last_status = ''
                else:
                    self.lines[self.line_index].set(common)
                    self.last_status = 'ambigious: '+ ' '.join([format_text(cmd, 'bold') for cmd in matching_cmds])


        # simple char
        else:
            self.lines[self.line_index] += ch

        return AsyncKeys.STATUS_REDRAW_KEYS


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

        # back to readonly
        for line in self.lines:
            line.invalidate()

        assert(self.lines[0].modified == False)
        if not func:
            self.last_status = "unknown command: '{0}'".format(format_text(cmd.split()[0], 'bold'))
        else:
            if isinstance(func_rc, str):
                self.last_status = func_rc
            else:
                self.last_status = format_text("[OK]", 'green') if func_rc else format_text(str(func_rc).replace('\n', ''), 'red')


    def draw (self):
        sys.stdout.write("\nPress 'ESC' for navigation panel...\n")
        sys.stdout.write("status: {0}\n".format(self.last_status))
        sys.stdout.write("\ntui>")
        self.lines[self.line_index].draw()


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

    def draw (self):
        sys.stdout.write(self.get())
        sys.stdout.write('\b' * (len(self.get()) - self.cursor_index))

