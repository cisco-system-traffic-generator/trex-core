#!/router/bin/python

import trex_client
from jsonrpclib import ProtocolError, AppError

class CTRexAdvClient(trex_client.CTRexClient):
    def __init__ (self, trex_host, max_history_size = 100, trex_daemon_port = 8090, trex_zmq_port = 4500, verbose = False):
        super(CTRexAdvClient, self).__init__(trex_host, max_history_size, trex_daemon_port, trex_zmq_port, verbose)
        pass

    # T-REX KIWI advanced methods
    def start_quick_trex(self, pcap_file, d, delay, dual, ipv6, times, interfaces):
        try:
            return self.server.start_quick_trex(pcap_file = pcap_file, duration = d, dual = dual, delay = delay, ipv6 = ipv6, times = times, interfaces = interfaces)
        except AppError as err:
            self.__handle_AppError_exception(err.args[0])
        except ProtocolError:
            raise
        finally:
            self.prompt_verbose_data()

    def stop_quick_trex(self):
        try:
            return self.server.stop_quick_trex()
        except AppError as err:
            self.__handle_AppError_exception(err.args[0])
        except ProtocolError:
            raise
        finally:
            self.prompt_verbose_data()

#   def is_running(self):
#       pass

    def get_running_stats(self):
        try:
            return self.server.get_running_stats()
        except AppError as err:
            self.__handle_AppError_exception(err.args[0])
        except ProtocolError:
            raise
        finally:
            self.prompt_verbose_data()

    def clear_counters(self):
        try:
            return self.server.clear_counters()
        except AppError as err:
            self.__handle_AppError_exception(err.args[0])
        except ProtocolError:
            raise
        finally:
            self.prompt_verbose_data()


if __name__ == "__main__":
    trex = CTRexAdvClient('trex-dan', trex_daemon_port = 8383, verbose = True)
    print trex.start_quick_trex(delay = 10, 
        dual = True, 
        d = 20,
        interfaces = ["gig0/0/1", "gig0/0/2"], 
        ipv6 = False, 
        pcap_file="avl/http_browsing.pcap", 
        times=3)
    print trex.stop_quick_trex()
    print trex.get_running_stats()
    print trex.clear_counters()
    pass


