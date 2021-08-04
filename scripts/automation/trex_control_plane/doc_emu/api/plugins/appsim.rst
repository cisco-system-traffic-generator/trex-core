APPSIM
======

**Example:**
  layer to simulate L7 application similar to the ASTF instruction set.
  This is a simple example for a UDP program. client send one pkt (index buf_list[0]) and expect one packet. 
  The server expect a packet (request) and send a one response packet (index buf_list[1]).

  The port is 80. tunables are use here. client will use tos==7

.. code-block:: python

        program ={
            "buf_list": [
                "R0VUIC8zMzg0IEhUVFAvMS4xD",
                "SFRUUC8xLjEgMjAwIE9LDQpTZ"
            ],
            "tunable_list": [ {"tos":7},{"tos":2,"mss":7}
            ],            
            "program_list": [
                {
                    "commands": [
                        {"msec": 10,
                        "name": "keepalive"
                        },
                        {
                            "buf_index": 0,
                            "name": "tx_msg"
                        },
                        {
                            "min_pkts": 1,
                            "name": "rx_msg"
                        }
                    ]
                },
                {
                    "commands": [
                        {
                        "msec": 10,
                        "name": "keepalive"
                        },
                        {
                            "min_pkts": 1,
                            "name": "rx_msg"
                        },
                        {
                            "buf_index": 1,
                            "name": "tx_msg"
                        }
                    ]
                }
            ],
            "templates": [{
                            "client_template" :{
                                    "program_index": 0,
                                    "tunable_index":0,
                                    "port": 80,
                                    "cps": 1
                            },
                            "server_template" : {"assoc": [{"port": 80}],
                                                "program_index": 1}
                                                }
                        ],
            }

        client_ini_json ={ "data" : { "s-1" : 
            { "cps": 1.0, 
              "t": "c", 
             "tid": 0, 
             "ipv6": false, 
             "stream": false,
             "dst" :"239.255.255.250:1900"}} }


**program**
    
+--------------+------------+------------------------------------------+
| Name         | Type       | Description                              |
+==============+============+==========================================+
| buf_list     | []string   | The string are base64 of []byte          |
+--------------+------------+------------------------------------------+
| program_list | object     | include one field {"commands": [{},{}] } |
|              |            | with an array of commands objects        |
+--------------+------------+------------------------------------------+
| templates    | []objects  | array of objects each include a          |
|              |            | client and server object                 |
+--------------+------------+------------------------------------------+
| tunable_list | [] objects | each object include name:value tuples    | 
|              |            |    [ {"tos":7},{"tos":2,"mss":7}         |
+--------------+------------+------------------------------------------+

**commands objects**

+--------------+-----------------+--------------------------------------------------+
| Name         | args            | Description                                      |
+==============+=================+==================================================+
| tx           |  buf_index:int  | send buffer (TCP) buffer index to buf_list       |
+--------------+-----------------+--------------------------------------------------+
| rx           | min_bytes:int,  | waits for min_bytes                              |
|              | clear:bool      |                                                  |
+--------------+-----------------+--------------------------------------------------+
| keepalive    |msec:int,        | works in case of UDP msec is the keepalive time  |
|              |                 | to stop the flow in case of inactivity           |
+--------------+-----------------+--------------------------------------------------+
| rx_msg       |min_pkts:int,    | number of packets to wait in the rx side in case |
|              |                 | UDP                                              |
+--------------+-----------------+--------------------------------------------------+
| tx_msg       |buf_index:int,   | same as tx for UDP                               |
|              |                 |                                                  |
+--------------+-----------------+--------------------------------------------------+
| connect      |                 | not in use                                       |       
|              |                 |                                                  |
+--------------+-----------------+--------------------------------------------------+
| reset        |                 | terminate the flow in RST                        |       
|              |                 |                                                  |
+--------------+-----------------+--------------------------------------------------+
| nc           |                 | wait for other side to close the flow            |       
|              |                 |                                                  |
+--------------+-----------------+--------------------------------------------------+
| close_msg    |                 | force close in UDP                               |       
|              |                 |                                                  |
+--------------+-----------------+--------------------------------------------------+
| delay        | usec:int        | delay in usec                                    |       
|              |                 |                                                  |
+--------------+-----------------+--------------------------------------------------+
| delay_rnd    | min_usec:int    | delay rand(min,max)                              |       
|              | max_usec:int    |                                                  |
+--------------+-----------------+--------------------------------------------------+
| set_var      | id:int          | set value into a variable (64bit)                |       
|              | val:int         |                                                  |
+--------------+-----------------+--------------------------------------------------+
| jmp_nz       | id:int          | reduce 1 from a var in case it is non zero jump  |       
|              | offset:int      | to offset in cmds array                          |
+--------------+-----------------+--------------------------------------------------+

**tunable key:val**

+------------------+-----------------+--------------------------------------------------+
| Name             | type            | Description                                      |
+==================+=================+==================================================+
| tos              |         ip/int  | set the tos                                      |
+------------------+-----------------+--------------------------------------------------+
| ttl              |         ip/int  | set the ttl                                      |
+------------------+-----------------+--------------------------------------------------+
| mss              |         tcp/int | set the tcp mss (bytes)                          |
+------------------+-----------------+--------------------------------------------------+
| initwnd          |         tcp/int | init window value in MSS units.                  |
+------------------+-----------------+--------------------------------------------------+
| no_delay         |        tcp/int  | 1 - disable nagle,                               |
|                  |                 | 2 - force PUSH flag                              |
+------------------+-----------------+--------------------------------------------------+
| no_delay_counter |        tcp/int  | number of recv bytes to wait until ack is sent.  | 
|                  |                 | notice ack can be triggered by tcp timer, in     |  
|                  |                 | order to ensure fixed number of sent packets     | 
|                  |                 | until ack you should increase the tcp.           | 
|                  |                 | initwnd tunable.                                 | 
+------------------+-----------------+--------------------------------------------------+
| delay_ack_msec   |        tcp/int  | delay ack timeout in msec.                       |
|                  |                 |                                                  |
|                  |                 |                                                  |
+------------------+-----------------+--------------------------------------------------+
| rxbufsize        |        tcp/int  | socket rx buffer size in bytes.                  |
|                  |                 |                                                  |
+------------------+-----------------+--------------------------------------------------+
| txbufsize        |        tcp/int  | socket tx buffer size in bytes.                  |
|                  |                 |                                                  |
+------------------+-----------------+--------------------------------------------------+
    
**client template object**

+------------------+-----------------+--------------------------------------------------+
| Name             | type            | Description                                      |
+==================+=================+==================================================+
| program_index    |    int          | index to the program list objects                |
+------------------+-----------------+--------------------------------------------------+
| tunable_index    |    int          | index to the program list objects                |
+------------------+-----------------+--------------------------------------------------+
| port             |    int          | destination port                                 |
+------------------+-----------------+--------------------------------------------------+
| cps              |    int          | not used, taken from the client json             |
+------------------+-----------------+--------------------------------------------------+


**server template object**

+------------------+-----------------+--------------------------------------------------+
| Name             | type            | Description                                      |
+==================+=================+==================================================+
| program_index    |    int          | index to the program list objects                |
+------------------+-----------------+--------------------------------------------------+
| tunable_index    |    int          | index to the program list objects                |
+------------------+-----------------+--------------------------------------------------+
| assoc            |   []            | [{"port": 80}]  list of ports                    |
+------------------+-----------------+--------------------------------------------------+

.. code-block:: python

    "templates": [{
    "client_template" :{
            "program_index": 0,
            "tunable_index":0,
            "port": 80,
            "cps": 1
    },
    "server_template" : {"assoc": [{"port": 80}],
                        "program_index": 1}
                        }
    ],


**per client init json**

This json can define multiple streams that point to a tid (template id) inside the namespace 
global program each with different poperies.

 uses {data: {"stream-1" :{} ,"stream-2" :{}} to define multiple streams 

+------------------+-----------------+--------------------------------------------------+
| Name             | type            | Description                                      |
+==================+=================+==================================================+
| cps              |   float         | new connections in one second                    |
+------------------+-----------------+--------------------------------------------------+
| t                |   string        | "c" for client and "s" for server                |
+------------------+-----------------+--------------------------------------------------+
| tid              |   int           | index to a template id inside the namespace      |
|                  |                 | program                                          |
+------------------+-----------------+--------------------------------------------------+
| ipv6             |   bool          | use ipv6 transport                               |
+------------------+-----------------+--------------------------------------------------+
| stream           |   bool          | true for TCP and false for UDP                   |
+------------------+-----------------+--------------------------------------------------+
| dst              |   string        |  the ip:port to use in case of client side       |
+------------------+-----------------+--------------------------------------------------+
                
.. code-block:: python

    client_ini_json ={ "data" : { "s-1" : 
               { "cps": 1.0, 
                 "t": "c", 
                "tid": 0, 
                "ipv6": false, 
                "stream": false,
                "dst" :"239.255.255.250:1900"}} }

.. automodule:: trex.emu.emu_plugins.emu_plugin_appsim
    :members:
    :inherited-members:
    :member-order: bysource
