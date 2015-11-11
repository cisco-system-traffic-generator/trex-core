
# main console object
class TRexConsole1(cmd.Cmd):
    """Trex Console"""

    def __init__(self, stateless_client, verbose):
        cmd.Cmd.__init__(self)

        self.stateless_client = stateless_client

        self.do_connect("")

        self.intro  = "\n-=TRex Console v{ver}=-\n".format(ver=__version__)
        self.intro += "\nType 'help' or '?' for supported actions\n"

        self.verbose = False
        self._silent = True

        self.postcmd(False, "")

        self.user_streams = {}
        self.streams_db = CStreamsDB()


    # a cool hack - i stole this function and added space
    def completenames(self, text, *ignored):
        dotext = 'do_'+text
        return [a[3:]+' ' for a in self.get_names() if a.startswith(dotext)]


    # set verbose on / off
    def do_verbose(self, line):
        '''Shows or set verbose mode\n'''
        if line == "":
            print "\nverbose is " + ("on\n" if self.verbose else "off\n")

        elif line == "on":
            self.verbose = True
            self.stateless_client.set_verbose(True)
            print green("\nverbose set to on\n")

        elif line == "off":
            self.verbose = False
            self.stateless_client.set_verbose(False)
            print green("\nverbose set to off\n")

        else:
            print magenta("\nplease specify 'on' or 'off'\n")

    # query the server for registered commands
    def do_query_server(self, line):
        '''query the RPC server for supported remote commands\n'''

        res_ok, msg = self.stateless_client.get_supported_cmds()
        if not res_ok:
            print format_text("[FAILED]\n", 'red', 'bold')
            return
        print "\nRPC server supports the following commands:\n"
        for func in msg:
            if func:
                print func
        print ''
        print format_text("[SUCCESS]\n", 'green', 'bold')
        return

    def do_ping(self, line):
        '''Pings the RPC server\n'''

        print "\n-> Pinging RPC server"

        res_ok, msg = self.stateless_client.ping()
        if res_ok:
            print format_text("[SUCCESS]\n", 'green', 'bold')
        else:
            print "\n*** " + msg + "\n"
            return

    def do_force_acquire(self, line):
        '''Acquires ports by force\n'''

        self.do_acquire(line, True)

    def complete_force_acquire(self, text, line, begidx, endidx):
        return self.port_auto_complete(text, line, begidx, endidx, acquired=False)

    def extract_port_ids_from_line(self, line):
        return {int(x) for x in line.split()}

    def extract_port_ids_from_list(self, port_list):
        return {int(x) for x in port_list}

    def parse_ports_from_line (self, line):
        port_list = set()
        if line:
            for port_id in line.split(' '):
                if (not port_id.isdigit()) or (int(port_id) < 0) or (int(port_id) >= self.stateless_client.get_port_count()):
                    print "Please provide a list of ports separated by spaces between 0 and {0}".format(self.stateless_client.get_port_count() - 1)
                    return None

                port_list.add(int(port_id))

            port_list = list(port_list)

        else:
            port_list = [i for i in xrange(0, self.stateless_client.get_port_count())]

        return port_list


    def do_acquire(self, line, force=False):
        '''Acquire ports\n'''

        # make sure that the user wants to acquire all
        args = line.split()
        if len(args) < 1:
            print magenta("Please provide a list of ports separated by spaces, or specify 'all' to acquire all available ports")
            return

        if args[0] == "all":
            ask = ConfirmMenu('Are you sure you want to acquire all ports ? ')
            rc = ask.show()
            if rc == False:
                print yellow("[ABORTED]\n")
                return
            else:
                port_list = self.stateless_client.get_port_ids()
        else:
            port_list = self.extract_port_ids_from_line(line)

        # rc, resp_list = self.stateless_client.take_ownership(port_list, force)
        try:
            res_ok, log = self.stateless_client.acquire(port_list, force)
            self.prompt_response(log)
            if not res_ok:
                print format_text("[FAILED]\n", 'red', 'bold')
                return
            print format_text("[SUCCESS]\n", 'green', 'bold')
        except ValueError as e:
            print magenta(str(e))
            print format_text("[FAILED]\n", 'red', 'bold')


    def port_auto_complete(self, text, line, begidx, endidx, acquired=True, active=False):
        if acquired:
            if not active:
                ret_list = [x
                            for x in map(str, self.stateless_client.get_acquired_ports())
                            if x.startswith(text)]
            else:
                ret_list = [x
                            for x in map(str, self.stateless_client.get_active_ports())
                            if x.startswith(text)]
        else:
            ret_list = [x
                        for x in map(str, self.stateless_client.get_port_ids())
                        if x.startswith(text)]
        ret_list.append("all")
        return ret_list


    def complete_acquire(self, text, line, begidx, endidx):
        return self.port_auto_complete(text, line, begidx, endidx, acquired=False)

    def do_release (self, line):
        '''Release ports\n'''

        # if line:
        #     port_list = self.parse_ports_from_line(line)
        # else:
        #     port_list = self.stateless_client.get_owned_ports()
        args = line.split()
        if len(args) < 1:
            print "Please provide a list of ports separated by spaces, or specify 'all' to acquire all available ports"
        if args[0] == "all":
            ask = ConfirmMenu('Are you sure you want to release all acquired ports? ')
            rc = ask.show()
            if rc == False:
                print yellow("[ABORTED]\n")
                return
            else:
                port_list = self.stateless_client.get_acquired_ports()
        else:
            port_list = self.extract_port_ids_from_line(line)

        try:
            res_ok, log = self.stateless_client.release(port_list)
            self.prompt_response(log)
            if not res_ok:
                print format_text("[FAILED]\n", 'red', 'bold')
                return
            print format_text("[SUCCESS]\n", 'green', 'bold')
        except ValueError as e:
            print magenta(str(e))
            print format_text("[FAILED]\n", 'red', 'bold')
            return

    def complete_release(self, text, line, begidx, endidx):
        return self.port_auto_complete(text, line, begidx, endidx)

    def do_connect (self, line):
        '''Connects to the server\n'''

        if line == "":
            res_ok, msg = self.stateless_client.connect()
        else:
            sp = line.split()
            if (len(sp) != 2):
                print "\n[usage] connect [server] [port] or without parameters\n"
                return

            res_ok, msg = self.stateless_client.connect(sp[0], sp[1])

        if res_ok:
            print format_text("[SUCCESS]\n", 'green', 'bold')
        else:
            print "\n*** " + msg + "\n"
            print format_text("[FAILED]\n", 'red', 'bold')
            return

        self.supported_rpc = self.stateless_client.get_supported_cmds().data

    # def do_rpc (self, line):
    #     '''Launches a RPC on the server\n'''
    #
    #     if line == "":
    #         print "\nUsage: [method name] [param dict as string]\n"
    #         print "Example: rpc test_add {'x': 12, 'y': 17}\n"
    #         return
    #
    #     sp = line.split(' ', 1)
    #     method = sp[0]
    #
    #     params = None
    #     bad_parse = False
    #     if len(sp) > 1:
    #
    #         try:
    #             params = ast.literal_eval(sp[1])
    #             if not isinstance(params, dict):
    #                 bad_parse = True
    #
    #         except ValueError as e1:
    #             bad_parse = True
    #         except SyntaxError as e2:
    #             bad_parse = True
    #
    #     if bad_parse:
    #         print "\nValue should be a valid dict: '{0}'".format(sp[1])
    #         print "\nUsage: [method name] [param dict as string]\n"
    #         print "Example: rpc test_add {'x': 12, 'y': 17}\n"
    #         return
    #
    #     res_ok, msg = self.stateless_client.transmit(method, params)
    #     if res_ok:
    #         print "\nServer Response:\n\n" + pretty_json(json.dumps(msg)) + "\n"
    #     else:
    #         print "\n*** " + msg + "\n"
    #         #print "Please try 'reconnect' to reconnect to server"
    #
    #
    # def complete_rpc (self, text, line, begidx, endidx):
    #     return [x
    #             for x in self.supported_rpc
    #             if x.startswith(text)]

    def do_status (self, line):
        '''Shows a graphical console\n'''

        if not self.stateless_client.is_connected():
            print "Not connected to server\n"
            return

        self.do_verbose('off')
        trex_status.show_trex_status(self.stateless_client)

    def do_quit(self, line):
        '''Exit the client\n'''
        return True

    def do_disconnect (self, line):
        '''Disconnect from the server\n'''
        if not self.stateless_client.is_connected():
            print "Not connected to server\n"
            return

        res_ok, msg = self.stateless_client.disconnect()
        if res_ok:
            print format_text("[SUCCESS]\n", 'green', 'bold')
        else:
            print msg + "\n"

    def do_whoami (self, line):
        '''Prints console user name\n'''
        print "\n" + self.stateless_client.user + "\n"

    def postcmd(self, stop, line):
        if self.stateless_client.is_connected():
            self.prompt = "TRex > "
        else:
            self.supported_rpc = None
            self.prompt = "TRex (offline) > "

        return stop

    def default(self, line):
        print "'{0}' is an unrecognized command. type 'help' or '?' for a list\n".format(line)

    # def do_help (self, line):
    #     '''Shows This Help Screen\n'''
    #     if line:
    #         try:
    #             func = getattr(self, 'help_' + line)
    #         except AttributeError:
    #             try:
    #                 doc = getattr(self, 'do_' + line).__doc__
    #                 if doc:
    #                     self.stdout.write("%s\n"%str(doc))
    #                     return
    #             except AttributeError:
    #                 pass
    #             self.stdout.write("%s\n"%str(self.nohelp % (line,)))
    #             return
    #         func()
    #         return
    #
    #     print "\nSupported Console Commands:"
    #     print "----------------------------\n"
    #
    #     cmds =  [x[3:] for x in self.get_names() if x.startswith("do_")]
    #     for cmd in cmds:
    #         if cmd == "EOF":
    #             continue
    #
    #         try:
    #             doc = getattr(self, 'do_' + cmd).__doc__
    #             if doc:
    #                 help = str(doc)
    #             else:
    #                 help = "*** Undocumented Function ***\n"
    #         except AttributeError:
    #             help = "*** Undocumented Function ***\n"
    #
    #         print "{:<30} {:<30}".format(cmd + " - ", help)

    def do_stream_db_add(self, line):
        '''Loads a YAML stream list serialization into user console \n'''
        args = line.split()
        if len(args) >= 2:
            name = args[0]
            yaml_path = args[1]
            try:
                multiplier = args[2]
            except IndexError:
                multiplier = 1
            stream_list = CStreamList()
            loaded_obj = stream_list.load_yaml(yaml_path, multiplier)
            # print self.stateless_client.pretty_json(json.dumps(loaded_obj))
            try:
                compiled_streams = stream_list.compile_streams()
                res_ok = self.streams_db.load_streams(name, LoadedStreamList(loaded_obj,
                                                                             [StreamPack(v.stream_id, v.stream.dump())
                                                                              for k, v in compiled_streams.items()]))
                if res_ok:
                    print green("Stream pack '{0}' loaded and added successfully\n".format(name))
                else:
                    print magenta("Picked name already exist. Please pick another name.\n")
            except Exception as e:
                print "adding new stream failed due to the following error:\n", str(e)
                print format_text("[FAILED]\n", 'red', 'bold')

            return
        else:
            print magenta("please provide load name and YAML path, separated by space.\n"
                          "Optionally, you may provide a third argument to specify multiplier.\n")

    @staticmethod
    def tree_autocomplete(text):
        dir = os.path.dirname(text)
        if dir:
            path = dir
        else:
            path = "."
        start_string = os.path.basename(text)
        return [x
                for x in os.listdir(path)
                if x.startswith(start_string)]


    def complete_stream_db_add(self, text, line, begidx, endidx):
        arg_num = len(line.split()) - 1
        if arg_num == 2:
            return TRexConsole.tree_autocomplete(line.split()[-1])
        else:
            return [text]

    def do_stream_db_show(self, line):
        '''Shows the loaded stream list named [name] \n'''
        args = line.split()
        if args:
            list_name = args[0]
            try:
                stream = self.streams_db.get_stream_pack(list_name)#user_streams[list_name]
                if len(args) >= 2 and args[1] == "full":
                    print pretty_json(json.dumps(stream.compiled))
                else:
                    print pretty_json(json.dumps(stream.loaded))
            except KeyError as e:
                print "Unknown stream list name provided"
        else:
            print "Available stream packs:\n{0}".format(', '.join(sorted(self.streams_db.get_loaded_streams_names())))

    def complete_stream_db_show(self, text, line, begidx, endidx):
        return [x
                for x in self.streams_db.get_loaded_streams_names()
                if x.startswith(text)]

    def do_stream_db_remove(self, line):
        '''Removes a single loaded stream packs from loaded stream pack repository\n'''
        args = line.split()
        if args:
            removed_streams = self.streams_db.remove_stream_packs(*args)
            if removed_streams:
                print green("The following stream packs were removed:")
                print bold(", ".join(sorted(removed_streams)))
                print format_text("[SUCCESS]\n", 'green', 'bold')
            else:
                print red("No streams were removed. Make sure to provide valid stream pack names.")
        else:
            print magenta("Please provide stream pack name(s), separated with spaces.")

    def do_stream_db_clear(self, line):
        '''Clears all loaded stream packs from loaded stream pack repository\n'''
        self.streams_db.clear()
        print format_text("[SUCCESS]\n", 'green', 'bold')


    def complete_stream_db_remove(self, text, line, begidx, endidx):
        return [x
                for x in self.streams_db.get_loaded_streams_names()
                if x.startswith(text)]


    def do_attach(self, line):
        '''Assign loaded stream pack into specified ports on TRex\n'''
        args = line.split()
        if len(args) >= 2:
            stream_pack_name = args[0]
            stream_list = self.streams_db.get_stream_pack(stream_pack_name) #user_streams[args[0]]
            if not stream_list:
                print "Provided stream list name '{0}' doesn't exists.".format(stream_pack_name)
                print format_text("[FAILED]\n", 'red', 'bold')
                return
            if args[1] == "all":
                ask = ConfirmMenu('Are you sure you want to release all acquired ports? ')
                rc = ask.show()
                if rc == False:
                    print yellow("[ABORTED]\n")
                    return
                else:
                    port_list = self.stateless_client.get_acquired_ports()
            else:
                port_list = self.extract_port_ids_from_line(' '.join(args[1:]))
            owned = set(self.stateless_client.get_acquired_ports())
            try:
                if set(port_list).issubset(owned):
                    res_ok, log = self.stateless_client.add_stream_pack(stream_list.compiled, port_id=port_list)
                    # res_ok, msg = self.stateless_client.add_stream(port_list, stream_list.compiled)
                    self.prompt_response(log)
                    if not res_ok:
                        print format_text("[FAILED]\n", 'red', 'bold')
                        return
                    print format_text("[SUCCESS]\n", 'green', 'bold')
                    return
                else:
                    print "Not all desired ports are acquired.\n" \
                          "Acquired ports are: {acq}\n" \
                          "Requested ports:    {req}\n" \
                          "Missing ports:      {miss}".format(acq=list(owned),
                                                              req=port_list,
                                                              miss=list(set(port_list).difference(owned)))
                    print format_text("[FAILED]\n", 'red', 'bold')
            except ValueError as e:
                print magenta(str(e))
                print format_text("[FAILED]\n", 'red', 'bold')
        else:
            print magenta("Please provide list name and ports to attach to, "
                          "or specify 'all' to attach all owned ports.\n")

    def complete_attach(self, text, line, begidx, endidx):
        arg_num = len(line.split()) - 1
        if arg_num == 1:
            # return optional streams packs
            if line.endswith(" "):
                return self.port_auto_complete(text, line, begidx, endidx)
            return [x
                    for x in self.streams_db.get_loaded_streams_names()
                    if x.startswith(text)]
        elif arg_num >= 2:
            # return optional ports to attach to
            return self.port_auto_complete(text, line, begidx, endidx)
        else:
            return [text]

    def prompt_response(self, response_obj):
        resp_list = response_obj if isinstance(response_obj, list) else [response_obj]
        def format_return_status(return_status):
            if return_status:
                return green("OK")
            else:
                return red("FAIL")

        for response in resp_list:
            response_str = "{id:^3} - {msg} ({stat})".format(id=response.id,
                                                             msg=response.msg,
                                                             stat=format_return_status(response.success))
            print response_str
        return

    def do_remove_all_streams(self, line):
        '''Acquire ports\n'''

        # make sure that the user wants to acquire all
        args = line.split()
        if len(args) < 1:
            print magenta("Please provide a list of ports separated by spaces, "
                          "or specify 'all' to remove from all acquired ports")
            return
        if args[0] == "all":
            ask = ConfirmMenu('Are you sure you want to remove all stream packs from all acquired ports? ')
            rc = ask.show()
            if rc == False:
                print yellow("[ABORTED]\n")
                return
            else:
                port_list = self.stateless_client.get_acquired_ports()
        else:
            port_list = self.extract_port_ids_from_line(line)

        # rc, resp_list = self.stateless_client.take_ownership(port_list, force)
        try:
            res_ok, log = self.stateless_client.remove_all_streams(port_list)
            self.prompt_response(log)
            if not res_ok:
                print format_text("[FAILED]\n", 'red', 'bold')
                return
            print format_text("[SUCCESS]\n", 'green', 'bold')
        except ValueError as e:
            print magenta(str(e))
            print format_text("[FAILED]\n", 'red', 'bold')

    def complete_remove_all_streams(self, text, line, begidx, endidx):
        return self.port_auto_complete(text, line, begidx, endidx)

    def do_start(self, line):
        '''Start selected traffic in specified ports on TRex\n'''
        # make sure that the user wants to acquire all
        parser = parsing_opts.gen_parser("start", self.do_start.__doc__,
                                         parsing_opts.PORT_LIST_WITH_ALL,
                                         parsing_opts.FORCE,
                                         parsing_opts.STREAM_FROM_PATH_OR_FILE,
                                         parsing_opts.DURATION,
                                         parsing_opts.MULTIPLIER)
        opts = parser.parse_args(line.split())
        if opts is None:
            # avoid further processing in this command
            return
        # print opts
        port_list = self.extract_port_list(opts)
        # print port_list
        if opts.force:
            # stop all active ports, if any
            res_ok = self.stop_traffic(set(self.stateless_client.get_active_ports()).intersection(port_list))
            if not res_ok:
                print yellow("[ABORTED]\n")
                return
        # remove all traffic from ports
        res_ok = self.remove_all_streams(port_list)
        if not res_ok:
            print yellow("[ABORTED]\n")
            return
        # decide which traffic to use
        stream_pack_name = None
        if opts.db:
            # use pre-loaded traffic
            print format_text('{:<30}'.format("Load stream pack (from DB):"), 'bold'),
            if opts.db not in self.streams_db.get_loaded_streams_names():
                print format_text("[FAILED]\n", 'red', 'bold')
                print yellow("[ABORTED]\n")
                return
            else:
                stream_pack_name = opts.db
        else:
            # try loading a YAML file
            print format_text('{:<30}'.format("Load stream pack (from file):"), 'bold'),
            stream_list = CStreamList()
            loaded_obj = stream_list.load_yaml(opts.file[0])
            # print self.stateless_client.pretty_json(json.dumps(loaded_obj))
            try:
                compiled_streams = stream_list.compile_streams()
                res_ok = self.streams_db.load_streams(opts.file[1],
                                                      LoadedStreamList(loaded_obj,
                                                                       [StreamPack(v.stream_id, v.stream.dump())
                                                                        for k, v in compiled_streams.items()]))
                if not res_ok:
                    print format_text("[FAILED]\n", 'red', 'bold')
                    print yellow("[ABORTED]\n")
                    return
                print format_text("[SUCCESS]\n", 'green', 'bold')
                stream_pack_name = opts.file[1]
            except Exception as e:
                print format_text("[FAILED]\n", 'red', 'bold')
                print yellow("[ABORTED]\n")
        res_ok = self.attach_to_port(stream_pack_name, port_list)
        if not res_ok:
            print yellow("[ABORTED]\n")
            return
        # finally, start the traffic
        res_ok = self.start_traffic(opts.mult, port_list)
        if not res_ok:
            print yellow("[ABORTED]\n")
            return
        return

    def help_start(self):
        self.do_start("-h")

    def do_stop(self, line):
        '''Stop active traffic in specified ports on TRex\n'''
        parser = parsing_opts.gen_parser("stop", self.do_stop.__doc__,
                                         parsing_opts.PORT_LIST_WITH_ALL)
        opts = parser.parse_args(line.split())
        if opts is None:
            # avoid further processing in this command
            return
        port_list = self.extract_port_list(opts)
        res_ok = self.stop_traffic(port_list)
        return


    def help_stop(self):
        self.do_stop("-h")


    def do_debug(self, line):
        '''Enter DEBUG mode of the console to invoke smaller building blocks with server'''
        i = DebugTRexConsole(self)
        i.prompt = self.prompt[:-3] + ':' + blue('debug') + ' > '
        i.cmdloop()

    # aliasing
    do_exit = do_EOF = do_q = do_quit

    # ----- utility methods ----- #

    def start_traffic(self, multiplier, port_list):#, silent=True):
        print format_text('{:<30}'.format("Start traffic:"), 'bold'),
        try:
            res_ok, log = self.stateless_client.start_traffic(multiplier, port_id=port_list)
            if not self._silent:
                print ''
                self.prompt_response(log)
            if not res_ok:
                print format_text("[FAILED]\n", 'red', 'bold')
                return False
            print format_text("[SUCCESS]\n", 'green', 'bold')
            return True
        except ValueError as e:
            print ''
            print magenta(str(e))
            print format_text("[FAILED]\n", 'red', 'bold')
            return False

    def attach_to_port(self, stream_pack_name, port_list):
        print format_text('{:<30}'.format("Attaching traffic to ports:"), 'bold'),
        stream_list = self.streams_db.get_stream_pack(stream_pack_name) #user_streams[args[0]]
        if not stream_list:
            print "Provided stream list name '{0}' doesn't exists.".format(stream_pack_name)
            print format_text("[FAILED]\n", 'red', 'bold')
            return
        try:
            res_ok, log = self.stateless_client.add_stream_pack(stream_list.compiled, port_id=port_list)
            if not self._silent:
                print ''
                self.prompt_response(log)
            if not res_ok:
                print format_text("[FAILED]\n", 'red', 'bold')
                return False
            print format_text("[SUCCESS]\n", 'green', 'bold')
            return True
        except ValueError as e:
            print ''
            print magenta(str(e))
            print format_text("[FAILED]\n", 'red', 'bold')
            return False

    def stop_traffic(self, port_list):
        print format_text('{:<30}'.format("Stop traffic:"), 'bold'),
        try:
            res_ok, log = self.stateless_client.stop_traffic(port_id=port_list)
            if not self._silent:
                print ''
                self.prompt_response(log)
            if not res_ok:
                print format_text("[FAILED]\n", 'red', 'bold')
                return
            print format_text("[SUCCESS]\n", 'green', 'bold')
            return True
        except ValueError as e:
            print ''
            print magenta(str(e))
            print format_text("[FAILED]\n", 'red', 'bold')

    def remove_all_streams(self, port_list):
        '''Remove all streams from given port_list'''
        print format_text('{:<30}'.format("Remove all streams:"), 'bold'),
        try:
            res_ok, log = self.stateless_client.remove_all_streams(port_id=port_list)
            if not self._silent:
                print ''
                self.prompt_response(log)
            if not res_ok:
                print format_text("[FAILED]\n", 'red', 'bold')
                return
            print format_text("[SUCCESS]\n", 'green', 'bold')
            return True
        except ValueError as e:
            print ''
            print magenta(str(e))
            print format_text("[FAILED]\n", 'red', 'bold')





    def extract_port_list(self, opts):
        if opts.all_ports or "all" in opts.ports:
            # handling all ports
            port_list = self.stateless_client.get_acquired_ports()
        else:
            port_list = self.extract_port_ids_from_list(opts.ports)
        return port_list

    def decode_multiplier(self, opts_mult):
        pass


class DebugTRexConsole(cmd.Cmd):

    def __init__(self, trex_main_console):
        cmd.Cmd.__init__(self)
        self.trex_console = trex_main_console
        self.stateless_client = self.trex_console.stateless_client
        self.streams_db = self.trex_console.streams_db
        self.register_main_console_methods()
        self.do_silent("on")
        pass

    # ----- super methods overriding ----- #
    def completenames(self, text, *ignored):
        dotext = 'do_'+text
        return [a[3:]+' ' for a in self.get_names() if a.startswith(dotext)]

    def get_names(self):
        result = cmd.Cmd.get_names(self)
        result += self.trex_console.get_names()
        return list(set(result))

    def register_main_console_methods(self):
        main_names = set(self.trex_console.get_names()).difference(set(dir(self.__class__)))
        for name in main_names:
            for prefix in 'do_', 'help_', 'complete_':
                if name.startswith(prefix):
                    self.__dict__[name] = getattr(self.trex_console, name)

            # if (name[:3] == 'do_') or (name[:5] == 'help_') or (name[:9] == 'complete_'):
            #     chosen.append(name)
            #     self.__dict__[name] = getattr(self.trex_console, name)
            #     # setattr(self, name, classmethod(getattr(self.trex_console, name)))

        # print chosen
        # self.get_names()

        # return result


    # ----- DEBUGGING methods ----- #
    # set silent on / off
    def do_silent(self, line):
        '''Shows or set silent mode\n'''
        if line == "":
            print "\nsilent mode is " + ("on\n" if self.trex_console._silent else "off\n")

        elif line == "on":
            self.verbose = True
            self.stateless_client.set_verbose(True)
            print green("\nsilent set to on\n")

        elif line == "off":
            self.verbose = False
            self.stateless_client.set_verbose(False)
            print green("\nsilent set to off\n")

        else:
            print magenta("\nplease specify 'on' or 'off'\n")

    def do_quit(self, line):
        '''Exit the debug client back to main console\n'''
        self.do_silent("off")
        return True

    def do_start_traffic(self, line):
        '''Start pre-submitted traffic in specified ports on TRex\n'''
        # make sure that the user wants to acquire all
        parser = parsing_opts.gen_parser("start_traffic", self.do_start_traffic.__doc__,
                                         parsing_opts.PORT_LIST_WITH_ALL, parsing_opts.MULTIPLIER)
        opts = parser.parse_args(line.split())
        # print opts
        # return
        if opts is None:
            # avoid further processing in this command
            return
        try:
            port_list = self.trex_console.extract_port_list(opts)
            return self.trex_console.start_traffic(opts.mult, port_list)
        except Exception as e:
            print e
            return

    def do_stop_traffic(self, line):
        '''Stop active traffic in specified ports on TRex\n'''
        parser = parsing_opts.gen_parser("stop_traffic", self.do_stop_traffic.__doc__,
                                         parsing_opts.PORT_LIST_WITH_ALL)
        opts = parser.parse_args(line.split())
        # print opts
        # return
        if opts is None:
            # avoid further processing in this command
            return
        try:
            port_list = self.trex_console.extract_port_list(opts)
            return self.trex_console.stop_traffic(port_list)
        except Exception as e:
            print e
            return


    def complete_stop_traffic(self, text, line, begidx, endidx):
        return self.port_auto_complete(text, line, begidx, endidx, active=True)

        # return
        # # return
        # # if not opts.port_list:
        # #     print magenta("Please provide a list of ports separated by spaces, "
        # #                   "or specify 'all' to start traffic on all acquired ports")
        # #     return
        #


        return
        args = line.split()
        if len(args) < 1:
            print magenta("Please provide a list of ports separated by spaces, "
                          "or specify 'all' to start traffic on all acquired ports")
            return
        if args[0] == "all":
            ask = ConfirmMenu('Are you sure you want to start traffic at all acquired ports? ')
            rc = ask.show()
            if rc == False:
                print yellow("[ABORTED]\n")
                return
            else:
                port_list = self.stateless_client.get_acquired_ports()
        else:
            port_list = self.extract_port_ids_from_line(line)

        try:
            res_ok, log = self.stateless_client.start_traffic(1.0, port_id=port_list)
            self.prompt_response(log)
            if not res_ok:
                print format_text("[FAILED]\n", 'red', 'bold')
                return
            print format_text("[SUCCESS]\n", 'green', 'bold')
        except ValueError as e:
            print magenta(str(e))
            print format_text("[FAILED]\n", 'red', 'bold')

    def complete_start_traffic(self, text, line, begidx, endidx):
        # return self.port_auto_complete(text, line, begidx, endidx)
        return [text]

    def help_start_traffic(self):
        self.do_start_traffic("-h")

    def help_stop_traffic(self):
        self.do_stop_traffic("-h")

    # def do_help(self):

    def do_rpc (self, line):
        '''Launches a RPC on the server\n'''

        if line == "":
            print "\nUsage: [method name] [param dict as string]\n"
            print "Example: rpc test_add {'x': 12, 'y': 17}\n"
            return

        sp = line.split(' ', 1)
        method = sp[0]

        params = None
        bad_parse = False
        if len(sp) > 1:

            try:
                params = ast.literal_eval(sp[1])
                if not isinstance(params, dict):
                    bad_parse = True

            except ValueError as e1:
                bad_parse = True
            except SyntaxError as e2:
                bad_parse = True

        if bad_parse:
            print "\nValue should be a valid dict: '{0}'".format(sp[1])
            print "\nUsage: [method name] [param dict as string]\n"
            print "Example: rpc test_add {'x': 12, 'y': 17}\n"
            return

        res_ok, msg = self.stateless_client.transmit(method, params)
        if res_ok:
            print "\nServer Response:\n\n" + pretty_json(json.dumps(msg)) + "\n"
        else:
            print "\n*** " + msg + "\n"
            #print "Please try 'reconnect' to reconnect to server"


    def complete_rpc (self, text, line, begidx, endidx):
        return [x
                for x in self.trex_console.supported_rpc
                if x.startswith(text)]

    # aliasing
    do_exit = do_EOF = do_q = do_quit

#
