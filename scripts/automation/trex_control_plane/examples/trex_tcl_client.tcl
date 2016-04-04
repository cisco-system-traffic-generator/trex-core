#!/bin/sh
#\
exec /usr/bin/expect "$0" "$@"
#

package require JSONRPC
package require json



################################################################
#   Author: Vamsi Kalapala
#   Version: 0.1
#   Description: This Trex Tcl Client will help in conecting to the trex_host 
#   by sending JSOMN RPC calls to the python trex server. 
#
#
#
################################################################

namespace eval TRexTclClient {
    variable version 0.1
    ################################################################
    # trex_host should be either the IPV4 address or
    # dns name accompanied by http://
    # Accepted trex_host names:
    # http://172.18.73.122,
    # http://up-trex-1-10g
    # http://up-trex-1-10g.cisco.com,
    ################################################################

    variable TRex_Status [dict create 1 "Idle" 2 "Starting" 3 "Running"]
    
    variable trex_host "localhost"
    variable server_port "8090"
    variable trex_zmq_port "4500"

    variable current_seq_num 0

    variable trex_debug 0

}


proc TRexTclClient::create_host {trex_host {server_port 8090} {trex_zmq_port 4500} {trex_debug 0}} {
    puts "\n"

    if (![string match "http://*" $trex_host]) {        
        append temp_host "http://" $trex_host
        set trex_host $temp_host       
        #puts $trex_host
        #puts stderr "The trex_host should contain http:// in it"
        #exit 1
    }

    set ::TRexTclClient::trex_host $trex_host
    if ($TRexTclClient::trex_debug) {
        puts "The server port is : $server_port"
    }
    set TRexTclClient::server_port $server_port
    set TRexTclClient::trex_zmq_port $trex_zmq_port
    set TRexTclClient::trex_debug $trex_debug

    set trex_host_url [TRexTclClient::appendHostName]
    puts "Host attributes have been initialized."
    puts "Establishing connection to server at $trex_host_url ..."
    puts "\n"
    return 1
}

proc TRexTclClient::appendHostName {} {
    ################################################################
    # Please check for the need of more args here.
    #
    # ** Do sanity checks **
    ################################################################
    if {[string is integer $TRexTclClient::server_port] && $TRexTclClient::server_port < 65535 } {
        return [append newHost  $TRexTclClient::trex_host ":"  $TRexTclClient::server_port]
    } else {
        puts stderr "\[ERROR\]: Invalid server port. Valid server port range is 0 - 65535 !"
        puts "\nExiting client initialization ... \n"
        exit 1
    }
}

# ** Chnage the args list to keyed lists or dictionaries**
proc TRexTclClient::start_trex  {f  d  trex_cmd_options {user "root"}  {block_to_success True} {timeout 40}} {
    
    ################################################################
    # This proc sends out the RPC call to start trex.
    # 'f' should string
    # 'd' should be integer and greater than 30
    # 'trex_cmd_options' SHOULD be a dict
    #
    ################################################################
    
    set trex_host_url [TRexTclClient::appendHostName]
    if {$d<30} {
        puts stderr "\[ERROR\]: The test duration should be at least 30 secs."
        puts "\nExiting start_trex process ... \n"
        exit 1
    }

    if ($TRexTclClient::trex_debug) {
        puts "\[start_trex :: before call\] : The arguements are: "
        puts "URL: $trex_host_url"
        puts "f: $f"
        puts "d: $d"
        puts "trex_cmd_options: $trex_cmd_options"
        puts "user: $user"
        puts "block_to_success: $block_to_success"
        puts "timeout: $timeout\n"
    }

    JSONRPC::create start_trex -proxy $trex_host_url -params {"trex_cmd_options" "object" "user" "string" "block_to_success" "string" "timeout" "int"}
    puts "Connecting to Trex host at $trex_host_url ....."

    dict append trex_cmd_options d $d
    dict append trex_cmd_options f $f
    if ($TRexTclClient::trex_debug) {
        puts "\[start_trex :: before call\] :  trex_cmd_options: $trex_cmd_options \n"
    }
    set ret_value [ start_trex $trex_cmd_options $user $block_to_success $timeout] 

    if ($TRexTclClient::trex_debug) {
        puts "\[start_trex :: after call\] : The returned result: $ret_value"
    }

    if ($ret_value!=0) {
        puts "Connection successful! \n"
        puts "TRex started running successfully! \n"
        puts "Trex Run Sequence number: $ret_value"
        set TRexTclClient::current_seq_num $ret_value
        return $ret_value
    }    

    puts "\n \n"
    return 0
}

proc TRexTclClient::stop_trex {seq} {
    set trex_host_url [TRexTclClient::appendHostName]
    JSONRPC::create stop_trex -proxy $trex_host_url -params {seq int} 
    set ret_value [ stop_trex $TRexTclClient::current_seq_num]
    if ($ret_value) {
        puts "TRex Run successfully stopped!"
    } else {
        puts "Unable to stop the server. Either provided sequence number is incorrect or you dont have sufficient permissions!"
    }
     puts "\n"
    return $ret_value
}

proc TRexTclClient::get_trex_files_path {} {

    set trex_host_url [TRexTclClient::appendHostName]
    JSONRPC::create get_files_path -proxy $trex_host_url
    set ret_value [get_files_path]
    puts "The Trex file path is $ret_value"
    puts "\n"
    return $ret_value
}

proc TRexTclClient::get_running_status {} {

    set trex_host_url [TRexTclClient::appendHostName]
    JSONRPC::create get_running_status -proxy $trex_host_url
    set ret_value [get_running_status]
    if ($TRexTclClient::trex_debug) {
        puts "\[get_running_status :: after call\] : The result is: $ret_value"        
    }
    set current_status [dict get $TRexTclClient::TRex_Status [dict get $ret_value "state" ]]
    set current_status_decr [dict get $ret_value "verbose"]
    puts "Current TRex Status: $current_status"
    puts "TRex Status Verbose: $current_status_decr"
    puts "\n"
    return $ret_value
}

proc TRexTclClient::get_result_obj { {copy_obj True} } {

    set trex_host_url [TRexTclClient::appendHostName]
    JSONRPC::create get_running_info -proxy $trex_host_url

    set result_json_obj [get_running_info ]
    set result_dict_obj [json::json2dict $result_json_obj]
    if ($TRexTclClient::trex_debug) {
        puts "\[get_result_obj :: after call\] : The result json is: "
        puts "################################################################"
        puts "$result_json_obj"
        puts "################################################################"
        puts "\[get_result_obj :: after call\] : The result dict is: "
        puts "################################################################"
        puts "$result_dict_obj"
        puts "################################################################"
    }
    puts "\n"
    return $result_dict_obj
}

proc TRexTclClient::is_reserved {} {
    set trex_host_url [TRexTclClient::appendHostName]
    JSONRPC::create is_reserved -proxy $trex_host_url
    puts "\n"
    set ret_value [is_reserved]
    if ($TRexTclClient::trex_debug) {
        puts "\[is_reserved :: after call\] : The result json is: $ret_value"
    }
    return $ret_value
}

proc TRexTclClient::get_trex_version  {} {
    set trex_host_url [TRexTclClient::appendHostName]
    JSONRPC::create get_trex_version -proxy $trex_host_url
    set ret_value [get_trex_version]
    puts "\n"
    return $ret_value
}





################################################################
#
#
#
################################################################
