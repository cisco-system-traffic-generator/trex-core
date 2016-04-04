#!/bin/sh
#\
exec /usr/bin/expect "$0" "$@"
#

# Sourcing the tcl trex client api
source trex_tcl_client.tcl

#Initializing trex server attributes
set check [TRexTclClient::create_host "localhost"]

#Formulate the command options as a dict
set trex_cmds [dict create c 2 m 10 l 1000 ]

#call the start_trex rpc function by feeding the appropriate arguments
set status [::TRexTclClient::start_trex "cap2/dns.yaml" 40 $trex_cmds]
puts "Status : $status"

#get the result json
set result [::TRexTclClient::get_result_obj]
puts "$result"

#stop the trex server
set ret_value [ ::TRexTclClient::stop_trex $status] 
puts "Stop value : $ret_value"
puts "\n \n"

 
