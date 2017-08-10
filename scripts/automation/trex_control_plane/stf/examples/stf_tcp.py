import argparse
import stf_path
from trex_stf_lib.trex_client import CTRexClient
from pprint import pprint


def validate_tcp (tcp_s):
    if 'err' in tcp_s :
        pprint(tcp_s);
        return(False);
    return True;


def run_stateful_tcp_test(server):

    trex_client = CTRexClient(server)

    trex_client.start_trex(
            c = 1, # limitation for now
            m = 1000,
            f = 'cap2/http_simple.yaml',
            k=10,
            d = 20,
            l = 1000,
            tcp =True, #enable TCP 
            nc=True
            )

    result = trex_client.sample_to_run_finish()

    c = result.get_latest_dump()
    pprint(c["tcp-v1"]["data"]);
    tcp_c= c["tcp-v1"]["data"]["client"];
    if not validate_tcp(tcp_c):
       return False
    tcp_s= c["tcp-v1"]["data"]["server"];
    if not validate_tcp(tcp_s):
        return False




if __name__ == '__main__':
    parser = argparse.ArgumentParser(description="tcp example")

    parser.add_argument('-s', '--server',
                        dest='server',
                        help='Remote trex address',
                        default='127.0.0.1',
                        type = str)
    args = parser.parse_args()

    if run_stateful_tcp_test(args.server):
        print("PASS");

