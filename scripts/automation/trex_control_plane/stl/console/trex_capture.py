from trex_stl_lib.api import *
from trex_stl_lib.utils import parsing_opts, text_tables
import threading
import tempfile
import select

class CaptureMonitorWriter(object):
    def init (self, start_ts):
        raise NotImplementedError

    def deinit(self):
        raise NotImplementedError
        
    def handle_pkts (self, pkts):
        raise NotImplementedError
        
    def periodic_check (self):
        raise NotImplementedError
        
        
class CaptureMonitorWriterStdout(CaptureMonitorWriter):
    def __init__ (self, logger, is_brief):
        self.logger      = logger
        self.is_brief    = is_brief

        self.RX_ARROW = u'\u25c0\u2500\u2500'
        self.TX_ARROW = u'\u25b6\u2500\u2500'

    def init (self, start_ts):
        self.start_ts = start_ts
        
        self.logger.pre_cmd("Starting stdout capture monitor - verbose: '{0}'".format('low' if self.is_brief else 'high'))
        self.logger.post_cmd(RC_OK)
        
        self.logger.log(format_text("\n*** use 'capture monitor stop' to abort capturing... ***\n", 'bold')) 
        
        
    def deinit (self):
        pass
        
       
    def periodic_check (self):
        return RC_OK()
        
    def handle_pkts (self, pkts):
        byte_count = 0
        
        for pkt in pkts:
            byte_count += self.__handle_pkt(pkt)
        
        self.logger.prompt_redraw()
        return RC_OK(byte_count)
        
        
    def get_scapy_name (self, pkt_scapy):
        layer = pkt_scapy
        while layer.payload and layer.payload.name not in('Padding', 'Raw'):
            layer = layer.payload
        
        return layer.name
        
                
    def format_origin (self, origin):
        if origin == 'RX':
            return u'{0} {1}'.format(self.RX_ARROW, 'RX')
        elif origin == 'TX':
            return u'{0} {1}'.format(self.TX_ARROW, 'TX')
        else:
            return '{0}'.format(origin)
            
        
    def __handle_pkt (self, pkt):
        pkt_bin = base64.b64decode(pkt['binary'])

        pkt_scapy = Ether(pkt_bin)
        self.logger.log(format_text(u'\n\n#{} Port: {} {}\n'.format(pkt['index'], pkt['port'], self.format_origin(pkt['origin'])), 'bold', ''))
        self.logger.log(format_text('    Type: {}, Size: {} B, TS: {:.2f} [sec]\n'.format(self.get_scapy_name(pkt_scapy), len(pkt_bin), pkt['ts'] - self.start_ts), 'bold'))

        
        if self.is_brief:
            self.logger.log('    {0}'.format(pkt_scapy.command()))
        else:
            pkt_scapy.show(label_lvl = '    ')
            self.logger.log('')

        return len(pkt_bin)

#
class CaptureMonitorWriterPipe(CaptureMonitorWriter):
    def __init__ (self, logger):
        self.logger   = logger
        
    def init (self, start_ts):
        self.start_ts  = start_ts
        self.fifo_name = tempfile.mktemp()
        
        try:
            self.logger.pre_cmd('Starting pipe capture monitor')
            os.mkfifo(self.fifo_name)
            self.logger.post_cmd(RC_OK)

            self.logger.log(format_text("*** Please run 'wireshark -k -i {0}' ***".format(self.fifo_name), 'bold'))
            
            self.logger.pre_cmd("Waiting for Wireshark pipe connection")
            self.fifo = os.open(self.fifo_name, os.O_WRONLY)
            self.logger.post_cmd(RC_OK())
            
            self.logger.log(format_text('\n*** Capture monitoring started ***\n', 'bold'))
            
            self.writer = RawPcapWriter(self.fifo_name, linktype = 1, sync = True)
            self.writer._write_header(None)
            
            # register a poller
            self.poll = select.poll()
            self.poll.register(self.fifo, select.EPOLLERR)
            
        except KeyboardInterrupt as e:
            self.logger.post_cmd(RC_ERR(""))
            raise STLError("*** pipe monitor aborted...cleaning up")

        except OSError as e:
            self.logger.post_cmd(RC_ERR(""))
            raise STLError("failed to create pipe {0}\n{1}".format(self.fifo_name, str(e)))
        
        
    def deinit (self):
        try:
            os.unlink(self.fifo_name)
        except OSError:
            pass

       
    def periodic_check (self):
        return self.check_pipe()
        
        
    def check_pipe (self):
        if self.poll.poll(0):
            return RC_ERR('*** pipe has been disconnected - aborting monitoring ***')
            
        return RC_OK()
        
        
    def handle_pkts (self, pkts):
        rc = self.check_pipe()
        if not rc:
            return rc
        
        byte_count = 0
        
        for pkt in pkts:
            pkt_bin = base64.b64decode(pkt['binary'])
            ts      = pkt['ts']
            sec     = int(ts)
            usec    = int( (ts - sec) * 1e6 )
                
            try:
                self.writer._write_packet(pkt_bin, sec = sec, usec = usec)
            except IOError:
                return RC_ERR("*** failed to write packet to pipe ***")
             
            byte_count += len(pkt_bin)
               
        return RC_OK(byte_count)
        
        
class CaptureMonitor(object):
    def __init__ (self, client, cmd_lock):
        self.client      = client
        self.cmd_lock    = cmd_lock
        self.active      = False
        self.capture_id  = None
        self.logger      = client.logger
        self.writer      = None
        
    def is_active (self):
        return self.active
        

    def get_capture_id (self):
        return self.capture_id
        
        
    def start (self, tx_port_list, rx_port_list, rate_pps, mon_type):
        try:
            self.start_internal(tx_port_list, rx_port_list, rate_pps, mon_type)
        except Exception as e:
            self.__stop()
            raise e
            
    def start_internal (self,  tx_port_list, rx_port_list, rate_pps, mon_type):
        # stop any previous monitors
        if self.active:
            self.stop()
        
        self.tx_port_list = tx_port_list
        self.rx_port_list = rx_port_list

        if mon_type == 'compact':
            self.writer = CaptureMonitorWriterStdout(self.logger, is_brief = True)
        elif mon_type == 'verbose':
            self.writer = CaptureMonitorWriterStdout(self.logger, is_brief = False)
        elif mon_type == 'pipe':
            self.writer = CaptureMonitorWriterPipe(self.logger)
        else:
            raise STLError('unknown writer type')
            
        
        with self.logger.supress():
            data = self.client.start_capture(tx_port_list, rx_port_list, limit = rate_pps)
        
        self.capture_id = data['id']
        self.start_ts   = data['ts']

        self.writer.init(self.start_ts)

        
        self.t = threading.Thread(target = self.__thread_cb)
        self.t.setDaemon(True)
        
        try:
            self.active = True
            self.t.start()
        except Exception as e:
            self.active = False
            self.stop()
            raise e
        
    # entry point stop 
    def stop (self):

        if self.active:
            self.stop_logged()
        else:
            self.__stop()
        
    # wraps stop with a logging
    def stop_logged (self):
        self.logger.pre_cmd("Stopping capture monitor")
        
        try:
            self.__stop()
        except Exception as e:
            self.logger.post_cmd(RC_ERR(""))
            raise e
        
        self.logger.post_cmd(RC_OK())
            
    # internal stop
    def __stop (self):

        # shutdown thread
        if self.active:
            self.active = False
            self.t.join()
            
        # deinit the writer
        if self.writer is not None:
            self.writer.deinit()
            self.writer = None
            
        # cleanup capture ID if possible
        if self.capture_id is None:
            return

        capture_id = self.capture_id
        self.capture_id = None
        
        # if we are disconnected - we cannot cleanup the capture
        if not self.client.is_connected():
            return
            
        try:
            captures = [x['id'] for x in self.client.get_capture_status()]
            if capture_id not in captures:
                return
                
            with self.logger.supress():
                self.client.stop_capture(capture_id)
            
        except STLError as e:
            self.logger.post_cmd(RC_ERR(""))
            raise e
            
                
    def get_mon_row (self):
        if not self.is_active():
            return None
            
        return [self.capture_id,
                self.pkt_count,
                format_num(self.byte_count, suffix = 'B'),
                ', '.join([str(x) for x in self.tx_port_list] if self.tx_port_list else '-'),
                ', '.join([str(x) for x in self.rx_port_list] if self.rx_port_list else '-')
                ]
        
    
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
            rc = self.__thread_main_loop()
        finally:
            pass
            
        if not rc:
            self.logger.log(str(rc))
            self.logger.log(format_text('\n*** monitor is inactive - please restart the monitor ***\n', 'bold'))
            self.logger.prompt_redraw()
            
            
    def __thread_main_loop (self):
        self.pkt_count  = 0
        self.byte_count = 0
        
        while self.active:
            
            # sleep
            if not self.__sleep():
                break
            
            # check that the writer is ok
            rc = self.writer.periodic_check()
            if not rc:
                return rc

            # try to lock
            if not self.__lock():
                break
                
            try:
                if not self.client.is_connected():
                    return RC_ERR('*** client has been disconnected, aborting monitoring ***')
                rc = self.client._transmit("capture", params = {'command': 'fetch', 'capture_id': self.capture_id, 'pkt_limit': 10})
                if not rc:
                    return rc
                    
            finally:
                self.__unlock()
                

            pkts = rc.data()['pkts']
            if not pkts:
                continue
            
            rc = self.writer.handle_pkts(pkts)
            if not rc:
                return rc
            
            self.pkt_count  += len(pkts)
            self.byte_count += rc.data()
                
        # graceful shutdown
        return RC_OK()
        
     

# main class
class CaptureManager(object):
    def __init__ (self, client, cmd_lock):
        self.c          = client
        self.cmd_lock   = cmd_lock
        self.monitor    = CaptureMonitor(client, cmd_lock)
        self.logger     = client.logger

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
                                              parsing_opts.LIMIT)

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
                                               parsing_opts.MONITOR_TYPE)

        
        
    def stop (self):
        self.monitor.stop()

        
    # main entry point for parsing commands from console
    def parse_line (self, line):
        try:
            self.parse_line_internal(line)
        except STLError as e:
            self.logger.log("\nAction has failed with the following error:\n" + format_text(e.brief() + "\n", 'bold'))
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

        rc = self.c.start_capture(opts.tx_port_list, opts.rx_port_list, opts.limit)
        
        self.logger.log(format_text("*** Capturing ID is set to '{0}' ***".format(rc['id']), 'bold'))
        self.logger.log(format_text("*** Please call 'capture record stop --id {0} -o <out.pcap>' when done ***\n".format(rc['id']), 'bold'))

        
    def parse_record_stop (self, opts):
        captures = self.c.get_capture_status()
        ids = [c['id'] for c in captures]
        
        if opts.capture_id == self.monitor.get_capture_id():
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
        
        self.monitor.stop()
        self.monitor.start(opts.tx_port_list, opts.rx_port_list, 100, mon_type)
    
    def parse_monitor_stop (self, opts):
        self.monitor.stop()
        
    def parse_clear (self, opts):
        self.monitor.stop()
        self.c.remove_all_captures()
        
        
        
    def parse_show (self, opts):
        data = self.c.get_capture_status()

        # captures
        cap_table = text_tables.TRexTextTable()
        cap_table.set_cols_align(["c"] * 6)
        cap_table.set_cols_width([15] * 6)

        # monitor
        mon_table = text_tables.TRexTextTable()
        mon_table.set_cols_align(["c"] * 5)
        mon_table.set_cols_width([15] * 5)

        for elem in data:
            id = elem['id']

            if self.monitor.get_capture_id() == id:
                row = self.monitor.get_mon_row()
                mon_table.add_rows([row], header=False)

            else:
                row = [id,
                       format_text(elem['state'], 'bold'),
                       '[{0}/{1}]'.format(elem['count'], elem['limit']),
                       format_num(elem['bytes'], suffix = 'B'),
                       bitfield_to_str(elem['filter']['tx']),
                       bitfield_to_str(elem['filter']['rx'])]

                cap_table.add_rows([row], header=False)

        cap_table.header(['ID', 'Status', 'Packets', 'Bytes', 'TX Ports', 'RX Ports'])
        mon_table.header(['ID', 'Packets Seen', 'Bytes Seen', 'TX Ports', 'RX Ports'])

        if cap_table._rows:
            text_tables.print_table_with_header(cap_table, '\nActive Recorders')

        if mon_table._rows:
            text_tables.print_table_with_header(mon_table, '\nActive Monitor')


