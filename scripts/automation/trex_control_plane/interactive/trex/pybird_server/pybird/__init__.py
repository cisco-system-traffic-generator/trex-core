import logging
import re
import os
import socket
from datetime import datetime, timedelta
from subprocess import Popen, PIPE

# TODO change to something aesthetic 
BIRD_PATH = os.path.abspath(os.path.join(__file__ ,"../../../../../../../bird"))  # trex scripts folder 
BIRD_TMP_PATH = "/tmp/trex-bird"
CTL_PATH = "%s/bird.ctl" % BIRD_TMP_PATH  
CONF_PATH = "%s/bird.conf" % BIRD_PATH 
DEFAULT_CFG = """
router id 100.100.100.100;
protocol device {
    scan time 1;
}
"""  
class PyBird(object):

    OK, WELCOME, STATUS_REPORT, ROUTE_DETAIL, TABLE_HEADING, PARSE_ERROR = 0, 1, 13, 1008, 2002, 9001
    ignored_field_numbers = [OK, WELCOME, STATUS_REPORT,
                             ROUTE_DETAIL, TABLE_HEADING, PARSE_ERROR]

    MAX_DATA_RECV = 1024

    def __init__(self, socket_file=CTL_PATH, hostname=None, user=None, password=None, config_file=CONF_PATH, bird_cmd=None):
        """Basic pybird setup."""
        if not os.path.exists(config_file):
           raise Exception('Config file not found: %s' % config_file)
        if not os.path.exists(BIRD_TMP_PATH):
            os.makedirs(BIRD_TMP_PATH)
        self.socket_file    = socket_file
        self.socket         = None
        self.hostname       = hostname
        self.user           = user
        self.password       = password
        if not config_file.endswith('.conf'):
            raise ValueError(
                "config_file: '%s' must ends with .conf" % config_file)
        self.config_file = config_file
        if not bird_cmd:
            self.bird_cmd = 'birdc'
        else:
            self.bird_cmd = bird_cmd

        self.clean_input_re = re.compile(r'\W+')
        self.field_number_re = re.compile(r'^(\d+)[ -]')
        self.routes_field_re = re.compile(r'(\d+) imported, (\d+) exported')
        self.log = logging.getLogger(__name__)

    def connect(self):
        if not os.path.exists(self.socket_file):
           raise Exception('Socket file not found: %s' % self.socket_file)
        if self.socket is None:
            self.socket = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
            self.socket.connect(self.socket_file)
    
    def disconnect(self):
        if self.socket is not None:
            self.socket.close()
            self.socket = None

    def get_config(self):
        """ Return the current BIRD configuration as in bird.conf"""
        self.log.debug("PyBird: getting current config")
        if not self.config_file:
            raise ValueError("config file is not set")
        return self._read_file(self.config_file)

    def get_bird_status(self):
        """Get the status of the BIRD instance. Returns a dict with keys:
        - router_id (string)
        - last_reboot (datetime)
        - last_reconfiguration (datetime)"""
        query = "show status"
        data = self._send_query(query)
        if not self.socket_file:
            return data
        return self._parse_status(data)

    def get_protocols_info(self):
        self.log.debug("PyBird: getting current protocols info")
        data = self._send_query('show protocols')
        lines = data.splitlines()[1:-1]
        data = '\n'.join([l.strip() for l in lines])  # cleanup data spaces
        return self._remove_replay_codes(data)

    def set_config(self, data):
        self.log.debug("PyBird: setting new config")
        temp_conf_path = '%s/temp_bird_config.conf' % BIRD_TMP_PATH
        if type(data) != str:
            data = data.decode("utf-8")
        with open(temp_conf_path, 'w') as f:
            f.write(data)
        os.chmod(temp_conf_path, 777)
        try:
            self.check_config(temp_conf_path)
        except ValueError as e:
            return str(e)
        finally:
            os.remove(temp_conf_path)
        res = self._write_file(data)

        if '\n0003' in res or '\n0004' in res:
            return 'Configured successfully' 
        else:
            res  # Configuration OK code
    
    def set_empty_config(self):
        return self.set_config(DEFAULT_CFG)

    def check_config(self, path):
        """ Check the current BIRD configuration. Raise a ValueError in case of bad conf file. """
        query = 'configure check "%s"' % path
        self.log.debug("PyBird: checking current config")
        data = self._send_query(query)
        self._parse_configure(data)

    def _remove_replay_codes(self, data):
        ''' Gets data with replay codes from socket and return the data cleaned as shown in birdc'''
        result = []
        for raw_line in data.splitlines():
            _, line = self._extract_field_number(raw_line)
            result.append(line)
        return '\n'.join(result)   

    def _parse_status(self, data):
        line_iterator = iter(data.splitlines())
        data = {}

        for line in line_iterator:
            line = line.strip()
            self.log.debug("PyBird: parse status: %s", line)
            (field_number, line) = self._extract_field_number(line)

            if field_number in self.ignored_field_numbers:
                continue

            if field_number == 1000:
                data['version'] = line.split(' ')[1]

            elif field_number == 1011:
                # Parse the status section, which looks like:
                # 1011-Router ID is 195.69.146.34
                # Current server time is 10-01-2012 10:24:37
                # Last reboot on 03-01-2012 12:46:40
                # Last reconfiguration on 03-01-2012 12:46:40
                data['router_id'] = self._parse_router_status_line(line)

                line = next(line_iterator)  # skip current server time
                self.log.debug("PyBird: parse status: %s", line)

                line = next(line_iterator)
                self.log.debug("PyBird: parse status: %s", line)
                data['last_reboot'] = self._parse_router_status_line(
                    line, parse_date=True)

                line = next(line_iterator)
                self.log.debug("PyBird: parse status: %s", line)
                data['last_reconfiguration'] = self._parse_router_status_line(
                    line, parse_date=True)

        return data

    def _parse_configure(self, data):
        """
            returns error on error, True on success
            0001 BIRD 1.4.5 ready.
            0002-Reading configuration from /home/grizz/c/20c/tstbird/dev3.conf
            8002 /home/grizz/c/20c/tstbird/dev3.conf, line 3: syntax error

            0001 BIRD 1.4.5 ready.
            0002-Reading configuration from /home/grizz/c/20c/tstbird/dev3.conf
            0020 Configuration OK

            0004 Reconfiguration in progress
            0018 Reconfiguration confirmed
            0003 Reconfigured

            bogus undo:
            0019 Nothing to do
        """
        error_fields = (19, 8002)
        success_fields = (3, 4, 18, 20)

        for line in data.splitlines():
            self.log.debug("PyBird: parse configure: %s", line)
            fieldnum, line = self._extract_field_number(line)

            if fieldnum == 2:
                if not self.config_file:  # in case config file isn't set, set it
                    self.config_file = line.split(' ')[3]

            elif fieldnum in error_fields:
                raise ValueError('Unable to parse the line:"%s"' % line)

            elif fieldnum in success_fields:
                return True
        raise ValueError("Bad configuration file!")

    def _parse_router_status_line(self, line, parse_date=False):
        """Parse a line like:
            Current server time is 10-01-2012 10:24:37
        optionally (if parse_date=True), parse it into a datetime"""
        data = line.strip().split(' ', 3)[-1]
        if parse_date:
            try:
                return datetime.strptime(data, '%Y-%m-%d %H:%M:%S.%f')
            # old versions of bird used DD-MM-YYYY
            except ValueError:
                return datetime.strptime(data, '%d-%m-%Y %H:%M:%S.%f')
        else:
            return data

    def get_routes(self, prefix=None, peer=None):
        query = "show route all"
        if prefix:
            query += " for {}".format(prefix)
        if peer:
            query += " protocol {}".format(peer)
        data = self._send_query(query)
        return self._parse_route_data(data)

    # deprecated by get_routes_received

    def _parse_route_data(self, data):
        """Parse a blob like:
            0001 BIRD 1.3.3 ready.
            1007-2a02:898::/32      via 2001:7f8:1::a500:8954:1 on eth1 [PS2 12:46] * (100) [AS8283i]
            1008-   Type: BGP unicast univ
            1012-   BGP.origin: IGP
                BGP.as_path: 8954 8283
                BGP.next_hop: 2001:7f8:1::a500:8954:1 fe80::21f:caff:fe16:e02
                BGP.local_pref: 100
                BGP.community: (8954,620)
            [....]
            0000
        """
        print(data)
        lines = data.splitlines()
        routes = []

        route_summary = None

        self.log.debug("PyBird: parse route data: lines=%d", len(lines))
        line_counter = -1
        while line_counter < len(lines) - 1:
            line_counter += 1
            line = lines[line_counter].strip()
            self.log.debug("PyBird: parse route data: %s", line)
            (field_number, line) = self._extract_field_number(line)

            if field_number in self.ignored_field_numbers:
                continue

            if field_number == 1007:
                route_summary = self._parse_route_summary(line)

            route_detail = None

            if field_number == 1012:
                if not route_summary:
                    # This is not detail of a BGP route
                    continue

                # A route detail spans multiple lines, read them all
                route_detail_raw = []
                while 'BGP.' in line:
                    route_detail_raw.append(line)
                    line_counter += 1
                    line = lines[line_counter]
                    self.log.debug("PyBird: parse route data: %s", line)
                # this loop will have walked a bit too far, correct it
                line_counter -= 1

                route_detail = self._parse_route_detail(route_detail_raw)

                # Save the summary+detail info in our result
                route_detail.update(route_summary)
                # Do not use this summary again on the next run
                route_summary = None
                routes.append(route_detail)

            if field_number == 8001:
                # network not in table
                return []

        return routes

    def _re_route_summary(self):
        return re.compile(
            r"(?P<prefix>[a-f0-9\.:\/]+)?\s+"
            r"(?:via\s+(?P<peer>[^\s]+) on (?P<interface>[^\s]+)|(?:\w+)?)?\s*"
            r"\[(?P<source>[^\s]+) (?P<time>[^\]\s]+)(?: from (?P<peer2>[^\s]+))?\]"
        )

    def _parse_route_summary(self, line):
        """Parse a line like:
            2a02:898::/32      via 2001:7f8:1::a500:8954:1 on eth1 [PS2 12:46] * (100) [AS8283i]
        """
        match = self._re_route_summary().match(line)
        if not match:
            raise ValueError("couldn't parse line '{}'".format(line))
        # Note that split acts on sections of whitespace - not just single
        # chars
        route = match.groupdict()

        # python regex doesn't allow group name reuse
        if not route['peer']:
            route['peer'] = route.pop('peer2')
        else:
            del route['peer2']
        return route

    def _parse_route_detail(self, lines):
        """Parse a blob like:
            1012-   BGP.origin: IGP
                BGP.as_path: 8954 8283
                BGP.next_hop: 2001:7f8:1::a500:8954:1 fe80::21f:caff:fe16:e02
                BGP.local_pref: 100
                BGP.community: (8954,620)
        """
        attributes = {}

        for line in lines:
            line = line.strip()
            self.log.debug("PyBird: parse route details: %s", line)
            # remove 'BGP.'
            line = line[4:]
            parts = line.split(": ")
            if len(parts) == 2:
                (key, value) = parts
            else:
                # handle [BGP.atomic_aggr:]
                key = parts[0].strip(":")
                value = True

            if key == 'community':
                # convert (8954,220) (8954,620) to 8954:220 8954:620
                value = value.replace(",", ":").replace(
                    "(", "").replace(")", "")

            attributes[key] = value

        return attributes

    def get_peer_status(self, peer_name=None):
        """Get the status of all peers or a specific peer.

        Optional argument: peer_name: case-sensitive full name of a peer,
        as configured in BIRD.

        If no argument is given, returns a list of peers - each peer represented
        by a dict with fields. See README for a full list.

        If a peer_name argument is given, returns a single peer, represented
        as a dict. If the peer is not found, returns a zero length array.
        """
        if peer_name:
            query = 'show protocols all "%s"' % self._clean_input(peer_name)
        else:
            query = 'show protocols all'

        data = self._send_query(query)
        if not self.socket_file:
            return data

        peers = self._parse_peer_data(data=data, data_contains_detail=True)

        if not peer_name:
            return peers

        if len(peers) == 0:
            return []
        elif len(peers) > 1:
            raise ValueError(
                "Searched for a specific peer, but got multiple returned from BIRD?")
        else:
            return peers[0]

    def _parse_peer_data(self, data, data_contains_detail):
        """Parse the data from BIRD to find peer information."""
        lineiterator = iter(data.splitlines())
        peers = []

        peer_summary = None

        for line in lineiterator:
            line = line.strip()
            (field_number, line) = self._extract_field_number(line)

            if field_number in self.ignored_field_numbers:
                continue

            if field_number == 1002:
                peer_summary = self._parse_peer_summary(line)
                if peer_summary['protocol'] != 'BGP':
                    peer_summary = None
                    continue

            # If there is no detail section to be expected,
            # we are done.
            if not data_contains_detail:
                peers.append_peer_summary()
                continue

            peer_detail = None
            if field_number == 1006:
                if not peer_summary:
                    # This is not detail of a BGP peer
                    continue

                # A peer summary spans multiple lines, read them all
                peer_detail_raw = []
                while line.strip() != "":
                    peer_detail_raw.append(line)
                    line = next(lineiterator)

                peer_detail = self._parse_peer_detail(peer_detail_raw)

                # Save the summary+detail info in our result
                peer_detail.update(peer_summary)
                peers.append(peer_detail)
                # Do not use this summary again on the next run
                peer_summary = None

        return peers

    def _parse_peer_summary(self, line):
        """Parse the summary of a peer line, like:
        PS1      BGP      T_PS1    start  Jun13       Passive

        Returns a dict with the fields:
            name, protocol, last_change, state, up
            ("PS1", "BGP", "Jun13", "Passive", False)

        """
        elements = line.split()

        try:
            if ':' in elements[5]:  # newer versions include a timestamp before the state
                state = elements[6]
            else:
                state = elements[5]
            up = (state.lower() == "established")
        except IndexError:
            state = None
            up = None

        raw_datetime = elements[4]
        last_change = self._calculate_datetime(raw_datetime)

        return {
            'name': elements[0],
            'protocol': elements[1],
            'last_change': last_change,
            'state': state,
            'up': up,
        }

    def _parse_peer_detail(self, peer_detail_raw):
        """Parse the detailed peer information from BIRD, like:

        1006-  Description:    Peering AS8954 - InTouch
          Preference:     100
          Input filter:   ACCEPT
          Output filter:  ACCEPT
          Routes:         24 imported, 23 exported, 0 preferred
          Route change stats:     received   rejected   filtered    ignored   accepted
            Import updates:             50          3          19         0          0
            Import withdraws:            0          0        ---          0          0
            Export updates:              0          0          0        ---          0
            Export withdraws:            0        ---        ---        ---          0
            BGP state:          Established
              Session:          external route-server AS4
              Neighbor AS:      8954
              Neighbor ID:      85.184.4.5
              Neighbor address: 2001:7f8:1::a500:8954:1
              Source address:   2001:7f8:1::a519:7754:1
              Neighbor caps:    refresh AS4
              Route limit:      9/1000
              Hold timer:       112/180
              Keepalive timer:  16/60

        peer_detail_raw must be an array, where each element is a line of BIRD output.

        Returns a dict with the fields, if the peering is up:
            routes_imported, routes_exported, router_id
            and all combinations of:
            [import,export]_[updates,withdraws]_[received,rejected,filtered,ignored,accepted]
            wfor which the value above is not "---"

        """
        result = {}

        route_change_fields = [
            "import updates",
            "import withdraws",
            "export updates",
            "export withdraws"
        ]
        field_map = {
            'description': 'description',
            'neighbor id': 'router_id',
            'neighbor address': 'address',
            'neighbor as': 'asn',
        }
        lineiterator = iter(peer_detail_raw)

        for line in lineiterator:
            line = line.strip()
            (field, value) = line.split(":", 1)
            value = value.strip()

            if field.lower() == "routes":
                routes = self.routes_field_re.findall(value)[0]
                result['routes_imported'] = int(routes[0])
                result['routes_exported'] = int(routes[1])

            if field.lower() in route_change_fields:
                (received, rejected, filtered, ignored, accepted) = value.split()
                key_name_base = field.lower().replace(' ', '_')
                self._parse_route_stats(
                    result, key_name_base + '_received', received)
                self._parse_route_stats(
                    result, key_name_base + '_rejected', rejected)
                self._parse_route_stats(
                    result, key_name_base + '_filtered', filtered)
                self._parse_route_stats(
                    result, key_name_base + '_ignored', ignored)
                self._parse_route_stats(
                    result, key_name_base + '_accepted', accepted)

            if field.lower() in field_map.keys():
                result[field_map[field.lower()]] = value

        return result

    def _parse_route_stats(self, result_dict, key_name, value):
        if value.strip() == "---":
            return
        result_dict[key_name] = int(value)

    def _extract_field_number(self, line):
        """Parse the field type number from a line.
        Line must start with a number, followed by a dash or space.

        Returns a tuple of (field_number, cleaned_line), where field_number
        is None if no number was found, and cleaned_line is the line without
        the field number, if applicable.
        """
        matches = self.field_number_re.findall(line)
        
        if len(matches):
            field_number = int(matches[0])
            cleaned_line = self.field_number_re.sub('', line).strip('-')
            return (field_number, cleaned_line)
        else:
            return (None, line)

    def _calculate_datetime(self, value, now=datetime.now()):
        """Turn the BIRD date format into a python datetime."""

        # Case: YYYY-MM-DD HH:MM:SS
        try:
            return datetime(*map(int, (value[:4], value[5:7], value[8:10], value[11:13], value[14:16], value[17:19])))
        except ValueError:
            pass

        # Case: YYYY-MM-DD
        try:
            return datetime(*map(int, (value[:4], value[5:7], value[8:10])))
        except ValueError:
            pass

        # Case: HH:mm or HH:mm:ss timestamp
        try:
            try:
                parsed_value = datetime.strptime(value, "%H:%M")

            except ValueError:
                parsed_value = datetime.strptime(value, "%H:%M:%S")

            result_date = datetime(
                now.year, now.month, now.day, parsed_value.hour, parsed_value.minute)

            if now.hour < parsed_value.hour or (now.hour == parsed_value.hour and now.minute < parsed_value.minute):
                result_date = result_date - timedelta(days=1)

            return result_date
        except ValueError:
            # It's a different format, keep on processing
            pass

        # Case: "Jun13" timestamp
        try:
            parsed = datetime.strptime(value, '%b%d')

            # if now is past the month, it's this year, else last year
            if now.month == parsed.month:
                # bird shows time for same day
                if now.day <= parsed.day:
                    year = now.year - 1
                else:
                    year = now.year

            elif now.month > parsed.month:
                year = now.year

            else:
                year = now.year - 1

            result_date = datetime(year, parsed.month, parsed.day)
            return result_date
        except ValueError:
            pass

        # Case: plain year
        try:
            year = int(value)
            return datetime(year, 1, 1)
        except ValueError:
            raise ValueError("Can not parse datetime: [%s]" % value)

    def _remote_cmd(self, cmd, inp=None):
        to = '{}@{}'.format(self.user, self.hostname)
        proc = Popen(['ssh', to, cmd], stdin=PIPE, stdout=PIPE)
        res = proc.communicate(input=inp)[0]
        return res

    def _read_file(self, file_name):
        if self.hostname:
            cmd = "cat " + file_name
            return self._remote_cmd(cmd)
        with open(file_name, 'r') as f:
            return f.read()


    def _write_file(self, data):
        if self.hostname:
            cmd = "cat >" + self.config_file
            self._remote_cmd(cmd, inp=data)
            return
        
        if type(data) is bytes:
            data= data.decode('utf-8')
        with open(self.config_file, 'w') as c_file:
            c_file.write(data)

        return self._send_query('configure "%s"' % self.config_file)

    def _send_query(self, query):
        self.log.debug("PyBird: query: %s", query)
        try:
            if self.hostname:
                return self._remote_query(query)
            else:
                return self._socket_query(query)
        except Exception as e:
            return 'Error sending query to bird! details:\n' + str(e)

    def _remote_query(self, query):
        """
        mimic a direct socket connect over ssh
        """
        cmd = "{} -v -s {} '{}'".format(self.bird_cmd, self.socket_file, query)
        res = self._remote_cmd(cmd)
        res += "0000\n"
        return res

    def _socket_query(self, query):
        """Open a socket to the BIRD control socket, send the query and get
        the response.
        """

        def _is_stream_done(stream):
            finish_codes = ["\n0000",  # OK
                            "\n0003",  # Reconfigured
                            "\n0004",  # Reconfiguration in progress
                            "\n0013",  # Status report
                            "\n0020",  # Configuration OK
                            "\n8001",  # Route not found
                            "\n8002",  # Configuration file error
                            "\n8003",  # No protocols match
                            "\n9001"]  # Parse error
            return any(code in stream for code in finish_codes)
        self.socket = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        self.socket.connect(self.socket_file)
        query += '\n'
        self.socket.send(query.encode())
        data_list, prev_data = [''], None
        while not _is_stream_done(data_list[-1]):
            data_list.append(self.socket.recv(self.MAX_DATA_RECV).decode())
            if data_list[-1] == prev_data:
                self.log.debug(data_list[-1])
                raise ValueError("Could not read additional data from BIRD")
            prev_data = data_list[-1]
        return ''.join(data_list)

    def _clean_input(self, inp):
        """Clean the input string of anything not plain alphanumeric chars,
        return the cleaned string."""
        return self.clean_input_re.sub('', inp).strip()