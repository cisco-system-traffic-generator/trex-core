import stl_path
from trex.stl.api import *
from pprint import pprint

import time
import re
import random 


class DynamicProfileTest:

    def __init__(self,
                 client, 
                 streams,
                 min_rand_duration,
                 max_rand_duration,
                 min_tick,
                 max_tick,
                 duration,
                 rate
                 ):

        self.rate =rate
        self.c = client
        self.streams = streams
        self.min_rand_duration = min_rand_duration 
        self.max_rand_duration = max_rand_duration
        self.min_tick = min_tick
        self.max_tick = max_tick
        self.duration = duration

    def is_profile_end_msg(self,msg):
        m = re.match("Profile (\d+).profile_(\d+) job done", msg)
        if m:
            return [int(m.group(1)),int(m.group(2))]
        else:
           return None

    def build_profile_id (self,port_id,profile_id):

       profile_name = "{}.profile_{}".format(port_id,profile_id)

       return profile_name


    def build_streams (self):
        streams_all = []
        packet = (Ether() /
                         IP(src="16.0.0.1",dst="48.0.0.1") /
                         UDP(sport=1025,dport=1025) /
                         Raw(load='\x55' * 10)
                )    
        for o in range(0,self.streams):
           s1 = STLStream(packet = STLPktBuilder(pkt = packet),
                                      mode = STLTXCont())
           streams_all.append(s1)

        return (streams_all)

    def run_test (self):

        passed = True

        c = self.c;

        try:

             # connect to server
             c.connect()

             # prepare our ports
             c.reset(ports = [0])

             port_id = 0
             profile_id = 0
             tick_action = 0
             profiles ={}
             tick = 1
             max_tick = self.duration
             stop = False

             c.clear_stats()

             c.clear_events();


             while True:

                 if tick > tick_action and (tick<max_tick):
                     profile_name = self.build_profile_id(port_id,profile_id) 
                     duration = random.randrange(self.min_rand_duration,self.max_rand_duration)
                     stream_ids = c.add_streams(streams = self.build_streams(), ports = [profile_name])
                     profiles[profile_id] = 1
                     print(" {} new profile {} {} {}".format(tick,profile_name,duration,len(profiles)) )
                     c.start(ports = [profile_name], mult = self.rate, duration = duration)
                     profile_id += 1
                     tick_action = tick + random.randrange(self.min_tick,self.max_tick) # next action 

                 time.sleep(1);
                 tick += 1


                 # check events 
                 while True:
                    event = c.pop_event ()
                    if event == None:
                        break;
                    else:
                        profile = self.is_profile_end_msg(event.msg)
                        if profile:
                            p_id = profile[1]
                            assert(profiles[p_id]==1)
                            del profiles[p_id]
                            print(" {} del profile {} {}".format(tick,p_id,len(profiles)))
                            if tick>=max_tick and (len(profiles)==0):
                                print("stop");
                                stop=True
                 if stop:
                     break;

             r = c.get_profiles_with_state("active")
             print(r)
             assert( len(r) == 0 )

             stats = c.get_stats()
             diff = stats[1]["ipackets"] - stats[0]["opackets"] 
             print(" diff {} ".format(diff))

             assert(diff<2)


        except STLError as e:
             passed = False
             print(e)

        finally:
             c.disconnect()

        if c.get_warnings():
                print("\n\n*** test had warnings ****\n\n")
                for w in c.get_warnings():
                    print(w)

        if passed and not c.get_warnings():
            return True
        else:
            return False


def simple_multi_burst (server):


    c = STLClient(server = server)

    test = DynamicProfileTest(c,100,1,50,1,2,20,"10kpps");

    print(test.run_test())

simple_multi_burst ("csi-kiwi-02")




