from trex_stl_lib.api import *
from trex_stl_lib.utils import parsing_opts, text_tables
import threading
import tempfile
import select
from distutils import spawn
from subprocess import Popen
import subprocess

# defines a generic monitor writer
class CaptureMonitorWriter(object):

    def deinit(self):
        # by default - nothing to deinit
        pass
        
    def handle_pkts (self, pkts):
        raise NotImplementedError
        
    def periodic_check (self):
        # by default - nothing to check
        pass
        
   
# a stdout monitor
class CaptureMonitorWriterStdout(CaptureMonitorWriter):
    def __init__ (self, logger, is_brief, start_ts):
        self.logger      = logger
        self.is_brief    = is_brief
        self.start_ts    = start_ts
        
        # unicode arrows
        self.RX_ARROW = u'\u25c0\u2500\u2500'
        self.TX_ARROW = u'\u2500\u2500\u25b6'
        
        # decode issues with Python 2
        if sys.version_info < (3,0):
            self.RX_ARROW = self.RX_ARROW.encode('utf-8')
            self.TX_ARROW = self.TX_ARROW.encode('utf-8')


        self.logger.pre_cmd("Starting stdout capture monitor - verbose: '{0}'".format('low' if self.is_brief else 'high'))
        self.logger.post_cmd(RC_OK())
        
        self.logger.log(format_text("\n*** use 'capture monitor stop' to abort capturing... ***\n", 'bold')) 
        
 
        
    def get_scapy_name (self, pkt_scapy):
        layer = pkt_scapy
        while layer.payload and layer.payload.name not in('Padding', 'Raw'):
            layer = layer.payload
        
        return layer.name
        
                
    def format_origin (self, origin):
        if origin == 'RX':
            return '{0} {1}'.format(self.RX_ARROW, 'RX')
        elif origin == 'TX':
            return '{0} {1}'.format(self.TX_ARROW, 'TX')
        else:
            return '{0}'.format(origin)
            
        
    def __handle_pkt (self, pkt):
        pkt_bin = base64.b64decode(pkt['binary'])

        pkt_scapy = Ether(pkt_bin)
        self.logger.log(format_text('\n\n#{} Port: {} {}\n'.format(pkt['index'], pkt['port'], self.format_origin(pkt['origin'])), 'bold', ''))
        self.logger.log(format_text('    Type: {}, Size: {} B, TS: {:.2f} [sec]\n'.format(self.get_scapy_name(pkt_scapy), len(pkt_bin), pkt['ts'] - self.start_ts), 'bold'))

        if self.is_brief:
            self.logger.log('    {0}'.format(pkt_scapy.command()))
        else:
            pkt_scapy.show(label_lvl = '    ')
            self.logger.log('')

        return len(pkt_bin)


    def handle_pkts (self, pkts):
        try:
            byte_count = 0
            for pkt in pkts:
                byte_count += self.__handle_pkt(pkt)

            return byte_count

        finally:
            # make sure to restore the logger
            self.logger.prompt_redraw()

    
# a pipe based monitor
class CaptureMonitorWriterPipe(CaptureMonitorWriter):
    def __init__ (self, logger, start_ts):
        
        self.logger    = logger
        self.fifo      = None
        self.start_ts  = start_ts
        
        # generate a temp fifo pipe
        self.fifo_name = tempfile.mktemp()
        
        self.wireshark_pid = None
        
        try:
            self.logger.pre_cmd('Starting pipe capture monitor')
            os.mkfifo(self.fifo_name)
            self.logger.post_cmd(RC_OK())

            # try to locate wireshark on the machine
            self.wireshark_exe = self.locate_wireshark()
            
            # we found wireshark - try to launch a process
            if self.wireshark_exe:
                self.wireshark_pid = self.launch_wireshark()
                
            # did we succeed ?
            if not self.wireshark_pid:
                self.logger.log(format_text("*** Please manually run 'wireshark -k -i {0}' ***".format(self.fifo_name), 'bold'))
            
            
            # blocks until pipe is connected
            self.logger.pre_cmd("Waiting for Wireshark pipe connection")
            self.fifo = os.open(self.fifo_name, os.O_WRONLY)
            self.logger.post_cmd(RC_OK())
            
            self.logger.log(format_text('\n*** Capture monitoring started ***\n', 'bold'))
            
            # open for write using a PCAP writer
            self.writer = RawPcapWriter(self.fifo_name, linktype = 1, sync = True)
            self.writer._write_header(None)
            
            # register a poller
            self.poll = select.poll()
            self.poll.register(self.fifo, select.EPOLLERR)
        
            self.is_init = True
                
       
        except KeyboardInterrupt as e:
            self.deinit()
            self.logger.post_cmd(RC_ERR(""))
            raise STLError("*** pipe monitor aborted...cleaning up")
                
        except OSError as e:
            self.deinit()
            self.logger.post_cmd(RC_ERR(""))
            raise STLError("failed to create pipe {0}\n{1}".format(self.fifo_name, str(e)))
        
       
    def locate_wireshark (self):
        self.logger.pre_cmd('Trying to locate Wireshark')
        wireshark_exe = spawn.find_executable('wireshark')
        self.logger.post_cmd(RC_OK() if wireshark_exe else RC_ERR())
        
        if not wireshark_exe:
            return None
            
        dumpcap = os.path.join(os.path.dirname(wireshark_exe), 'dumpcap')
        
        self.logger.pre_cmd("Checking permissions on '{}'".format(dumpcap))
        if not os.access(dumpcap, os.X_OK):
            self.logger.post_cmd(RC_ERR('bad permissions on dumpcap'))
            return None
        
        self.logger.post_cmd(RC_OK())
        
        return wireshark_exe
        
    # try to launch wireshark... returns true on success
    def launch_wireshark (self):
        
        cmd = '{0} -k -i {1}'.format(self.wireshark_exe, self.fifo_name)
        self.logger.pre_cmd("Launching '{0}'".format(cmd))
                
        try:
            devnull = open(os.devnull, 'w')
            self.wireshark_pid = Popen(cmd.split(),
                                       stdout     = devnull,
                                       stderr     = devnull,
                                       stdin      = subprocess.PIPE,
                                       preexec_fn = os.setpgrp,
                                       close_fds  = True)
                            
            self.logger.post_cmd(RC_OK())
            return True
            
        except OSError as e:
            self.wireshark_pid = None
            self.logger.post_cmd(RC_ERR())
            return False
            
        
    def deinit (self):
        try:
            if self.fifo:
                os.close(self.fifo)
                self.fifo = None
                
            if self.fifo_name:
                os.unlink(self.fifo_name)
                self.fifo_name = None
                
        except OSError:
            pass
            

       
    def periodic_check (self):
        self.check_pipe()
        
        
    def check_pipe (self):
        if self.poll.poll(0):
            raise STLError('pipe has been disconnected')
        
        
    def handle_pkts (self, pkts):
        # first check the pipe is alive
        self.check_pipe()

        return self.handle_pkts_internal(pkts)
            
        
    def handle_pkts_internal (self, pkts):
        
        byte_count = 0
        
        for pkt in pkts:
            pkt_bin = base64.b64decode(pkt['binary'])
            ts_sec, ts_usec = sec_split_usec(pkt['ts'] - self.start_ts)
            
            try:
                self.writer._write_packet(pkt_bin, sec = ts_sec, usec = ts_usec)
            except Exception as e:
                raise STLError('fail to write packets to pipe: {}'.format(str(e)))
                
            byte_count += len(pkt_bin)
               
        return byte_count
        
        
# capture monitor - a live capture
class CaptureMonitor(object):
    def __init__ (self, client, cmd_lock, tx_port_list, rx_port_list, rate_pps, mon_type, bpf_filter):
        self.client      = client
        self.logger      = client.logger
        self.cmd_lock    = cmd_lock
        
        self.t           = None
        self.writer      = None
        self.capture_id  = None
        
        self.tx_port_list = tx_port_list
        self.rx_port_list = rx_port_list
        self.rate_pps     = rate_pps
        self.mon_type     = mon_type
        self.bpf_filter   = bpf_filter
        
        # try to launch
        try:
            self.__start()
        except Exception as e:
            self.__stop()
            raise
            
            
    def __start (self):
        
        # create a capture on the server
        with self.logger.supress():
            data = self.client.start_capture(self.tx_port_list,
                                             self.rx_port_list,
                                             limit = self.rate_pps,
                                             mode = 'cyclic',
                                             bpf_filter = self.bpf_filter)

        self.capture_id = data['id']
        self.start_ts   = data['ts']
        

        # create a writer
        if self.mon_type == 'compact':
            self.writer = CaptureMonitorWriterStdout(self.logger, True, self.start_ts)
        elif self.mon_type == 'verbose':
            self.writer = CaptureMonitorWriterStdout(self.logger, False, self.start_ts)
        elif self.mon_type == 'pipe':
            self.writer = CaptureMonitorWriterPipe(self.logger, self.start_ts)
        else:
            raise STLError('Internal error: unknown writer type')
        
        # start the fetching thread
        self.t = threading.Thread(target = self.__thread_cb)
        self.t.setDaemon(True)
        self.active = True
        self.t.start()
  
            
    # internal stop
    def __stop (self):

        # stop the thread
        if self.t and self.t.is_alive():
            self.active = False
            self.t.join()
            self.t = None
            
        # deinit the writer
        if self.writer:
            self.writer.deinit()
            self.writer = None
            
        # take the capture ID
        capture_id = self.capture_id
        self.capture_id = None
        
        # if we are disconnected - we cannot cleanup the capture
        if not self.client.is_connected():
            return
            
        # make sure the capture is active on the server
        captures = self.client.get_capture_status().keys()
        if capture_id not in captures:
            return
            
        # remove the capture                
        with self.logger.supress():
            self.client.stop_capture(capture_id)
            
           
    # user call for stop (adds log)
    def stop (self):
        self.logger.pre_cmd("Stopping capture monitor")
        
        try:
            self.__stop()
        except Exception as e:
            self.logger.post_cmd(RC_ERR(""))
            raise
        
        self.logger.post_cmd(RC_OK())


    def get_mon_row (self):
            
        return [self.capture_id,
                format_text('ACTIVE' if self.t.is_alive() else 'DEAD', 'bold'),
                self.pkt_count,
                format_num(self.byte_count, suffix = 'B'),
                ', '.join([str(x) for x in self.tx_port_list] if self.tx_port_list else '-'),
                ', '.join([str(x) for x in self.rx_port_list] if self.rx_port_list else '-'),
                self.bpf_filter or '-',
                ]
        

    def is_active (self):
        return self.active


    def get_capture_id (self):
        return self.capture_id
        

    # sleeps with high freq checks for active
    def __sleep (self):
        for _ in range(5):
            if not self.active:
                return False
                
            time.sleep(0.1)
            
        return True
            
    def __lock (self):
        while True:
            rc = self.cmd_lock.acquire(False)
            if rc:
                return True
                
            if not self.active:
                return False
            time.sleep(0.1)
        
    def __unlock (self):
        self.cmd_lock.release()
        
    
    def __thread_cb (self):
        try:
            self.__thread_main_loop()
        
        # common errors
        except STLError as e:
            self.logger.log(format_text("\n\nMonitor has encountered the following error: '{}'\n".format(e.brief()), 'bold'))
            self.logger.log(format_text("\n*** monitor is inactive - please restart the monitor ***\n", 'bold'))
            self.logger.prompt_redraw()
            
        # unexpected errors
        except Exception as e:
            self.logger.log("\n\n*** A fatal internal error has occurred: '{}'\n".format(str(e)))
            self.logger.log(format_text("\n*** monitor is inactive - please restart the monitor ***\n", 'bold'))
            self.logger.prompt_redraw()
            
            
    def __thread_main_loop (self):
        self.pkt_count  = 0
        self.byte_count = 0
        
        while self.active:
            
            # sleep - if interrupt by graceful shutdown - go out
            if not self.__sleep():
                return
            
            # check that the writer is ok
            self.writer.periodic_check()

            # try to lock - if interrupt by graceful shutdown - go out
            if not self.__lock():
                return
                
            try:
                if not self.client.is_connected():
                    raise STLError('client has been disconnected')
                    
                rc = self.client._transmit("capture", params = {'command': 'fetch', 'capture_id': self.capture_id, 'pkt_limit': 10})
                if not rc:
                    raise STLError(rc)
                    
            finally:
                self.__unlock()
                

            # no packets - skip
            pkts = rc.data()['pkts']
            if not pkts:
                continue
            
            byte_count = self.writer.handle_pkts(pkts)
            
            self.pkt_count  += len(pkts)
            self.byte_count += byte_count




# main class
class CaptureManager(object):
    def __init__ (self, client, cmd_lock):
        self.c          = client
        self.cmd_lock   = cmd_lock
        self.logger     = client.logger
        self.monitor    = None
        
        # install parsers
        
        self.parser = parsing_opts.gen_parser(self, "capture", self.parse_line_internal.__doc__)
        self.subparsers = self.parser.add_subparsers(title = "commands", dest="commands")

        self.install_record_parser()
        self.install_monitor_parser()
        
        # show
        self.show_parser = self.subparsers.add_parser('show', help = "show all active captures")
     
        # reset
        self.clear_parser = self.subparsers.add_parser('clear', help = "remove all active captures")
       
        # register handlers
        self.cmds = {'record': self.parse_record, 'monitor' : self.parse_monitor, 'clear': self.parse_clear, 'show' : self.parse_show} 
        
        
    def install_record_parser (self):
        # recording
        self.record_parser = self.subparsers.add_parser('record', help = "PCAP recording")
        record_sub = self.record_parser.add_subparsers(title = 'commands', dest = 'record_cmd')
        self.record_start_parser = record_sub.add_parser('start', help = "starts a new buffered capture")
        self.record_stop_parser  = record_sub.add_parser('stop', help = "stops an active buffered capture")

        # start
        self.record_start_parser.add_arg_list(parsing_opts.TX_PORT_LIST,
                                              parsing_opts.RX_PORT_LIST,
                                              parsing_opts.LIMIT,
                                              parsing_opts.BPF_FILTER)

        # stop
        self.record_stop_parser.add_arg_list(parsing_opts.CAPTURE_ID,
                                             parsing_opts.OUTPUT_FILENAME)

        
        
    def install_monitor_parser (self):
        # monitor
        self.monitor_parser = self.subparsers.add_parser('monitor', help = 'live monitoring')
        monitor_sub = self.monitor_parser.add_subparsers(title = 'commands', dest = 'mon_cmd')
        self.monitor_start_parser = monitor_sub.add_parser('start', help = 'starts a monitor')
        self.monitor_stop_parser  = monitor_sub.add_parser('stop', help = 'stops an active monitor')

        self.monitor_start_parser.add_arg_list(parsing_opts.TX_PORT_LIST,
                                               parsing_opts.RX_PORT_LIST,
                                               parsing_opts.MONITOR_TYPE,
                                               parsing_opts.BPF_FILTER)

        
        
    def stop (self):
        if self.monitor:
            self.monitor.stop()
            self.monitor = None

        
    # main entry point for parsing commands from console
    def parse_line (self, line):
        try:
            self.parse_line_internal(line)
        except STLError as e:
            self.logger.log("\nAction has failed with the following error:\n\n" + format_text(e.brief() + "\n", 'bold'))
            return RC_ERR(e.brief())


    def parse_line_internal (self, line):
        '''Manage PCAP recorders'''

        # default
        if not line:
            line = "show"

        opts = self.parser.parse_args(line.split())
        if not opts:
            return opts

        # call the handler
        self.cmds[opts.commands](opts)


    # record methods
    def parse_record (self, opts):
        if opts.record_cmd == 'start':
            self.parse_record_start(opts)
        elif opts.record_cmd == 'stop':
            self.parse_record_stop(opts)
        else:
            self.record_parser.formatted_error("too few arguments")
            
            
    def parse_record_start (self, opts):
        if not opts.tx_port_list and not opts.rx_port_list:
            self.record_start_parser.formatted_error('please provide either --tx or --rx')
            return

        rc = self.c.start_capture(opts.tx_port_list, opts.rx_port_list, opts.limit, mode = 'fixed', bpf_filter = opts.filter)
        
        self.logger.log(format_text("*** Capturing ID is set to '{0}' ***".format(rc['id']), 'bold'))
        self.logger.log(format_text("*** Please call 'capture record stop --id {0} -o <out.pcap>' when done ***\n".format(rc['id']), 'bold'))

        
    def parse_record_stop (self, opts):
        ids = self.c.get_capture_status().keys()
        
        if self.monitor and (opts.capture_id == self.monitor.get_capture_id()):
            self.record_stop_parser.formatted_error("'{0}' is a monitor, please use 'capture monitor stop'".format(opts.capture_id))
            return
            
        if opts.capture_id not in ids:
            self.record_stop_parser.formatted_error("'{0}' is not an active capture ID".format(opts.capture_id))
            return
            
        self.c.stop_capture(opts.capture_id, opts.output_filename)

            
    # monitor methods
    def parse_monitor (self, opts):
        if opts.mon_cmd == 'start':
            self.parse_monitor_start(opts)
        elif opts.mon_cmd == 'stop':
            self.parse_monitor_stop(opts)
        else:
            self.monitor_parser.formatted_error("too few arguments")
            
            
    def parse_monitor_start (self, opts):
        mon_type = 'compact'
        
        if opts.verbose:
            mon_type = 'verbose'
        elif opts.pipe:
            mon_type = 'pipe'
            
        if not opts.tx_port_list and not opts.rx_port_list:
            self.monitor_start_parser.formatted_error('please provide either --tx or --rx')
            return
        
        if self.monitor:
            self.monitor.stop()
            self.monitor = None
            
        self.monitor = CaptureMonitor(self.c, self.cmd_lock, opts.tx_port_list, opts.rx_port_list, 100, mon_type, opts.filter)
        
    
    def parse_monitor_stop (self, opts):
        if self.monitor:
            self.monitor.stop()
            self.monitor = None
            
        
    def parse_clear (self, opts):
        if self.monitor:
            self.monitor.stop()
            self.monitor = None
            
        self.c.remove_all_captures()
        
        
        
    def parse_show (self, opts):
        data = self.c.get_capture_status()

        # captures
        cap_table = text_tables.TRexTextTable()
        cap_table.set_cols_align(["c"] * 7)
        cap_table.set_cols_width([15] * 7)

        # monitor
        mon_table = text_tables.TRexTextTable()
        mon_table.set_cols_align(["c"] * 7)
        mon_table.set_cols_width([15] * 7)

        for capture_id, elem in data.items():

            if self.monitor and (self.monitor.get_capture_id() == capture_id):
                row = self.monitor.get_mon_row()
                mon_table.add_rows([row], header=False)

            else:
                row = [capture_id,
                       format_text(elem['state'], 'bold'),
                       '[{0}/{1}]'.format(elem['count'], elem['limit']),
                       format_num(elem['bytes'], suffix = 'B'),
                       bitfield_to_str(elem['filter']['tx']),
                       bitfield_to_str(elem['filter']['rx']),
                       elem['filter']['bpf'] or '-']

                cap_table.add_rows([row], header=False)

        cap_table.header(['ID', 'Status', 'Packets', 'Bytes', 'TX Ports', 'RX Ports', 'BPF Filter'])
        mon_table.header(['ID', 'Status', 'Packets Seen', 'Bytes Seen', 'TX Ports', 'RX Ports', 'BPF Filter'])

        if cap_table._rows:
            text_tables.print_table_with_header(cap_table, '\nActive Recorders')

        if mon_table._rows:
            text_tables.print_table_with_header(mon_table, '\nActive Monitor')


