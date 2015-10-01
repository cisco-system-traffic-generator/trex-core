
TRex JSON Template
==================

Whenever TRex is publishing live data, it uses JSON notation to describe the data-object.

Each client may parse it diffrently, however this page will describe the values meaning when published by TRex server.


Main Fields
-----------

Each TRex server-published JSON object contains data divided to main fields under which the actual data lays.

These main fields are:

+-----------------------------+----------------------------------------------------+---------------------------+
| Main field                  | Contains                                           | Comments                  |
+=============================+====================================================+===========================+
| :ref:`trex-global-field`    | Must-have data on TRex run,                        |                           |
|                             | mainly regarding Tx/Rx and packet drops            |                           |
+-----------------------------+----------------------------------------------------+---------------------------+
| :ref:`tx-gen-field`         | Data indicate the quality of the transmit process. |                           |
|                             | In case histogram is zero it means that all packets|                           |
|                             | were injected in the right time.                   |                           |
+-----------------------------+----------------------------------------------------+---------------------------+
| :ref:`trex-latecny-field`   | Latency reports, containing latency data on        | - Generated when latency  |
|                             | generated data and on response traffic             |   test is enabled (``l``  |
|                             |                                                    |   param)                  |
|                             |                                                    | - *typo* on field key:    |
+-----------------------------+----------------------------------------------------+   will be fixed on next   |   
| :ref:`trex-latecny-v2-field`| Extended latency information                       |   release                 |
+-----------------------------+----------------------------------------------------+---------------------------+


Each of these fields contains keys for field general data (such as its name) and its actual data, which is always stored under the **"data"** key.

For example, in order to access some trex-global data, the access path would look like::

   AllData -> trex-global -> data -> desired_info

   


Detailed explanation
--------------------

.. _trex-global-field:

trex-global field
~~~~~~~~~~~~~~~~~


+--------------------------------+-------+-----------------------------------------------------------+
|      Sub-key                   | Type  |                          Meaning                          |
+================================+=======+===========================================================+
| m_cpu_util                     | float | CPU utilization (0-100)                                   |
+--------------------------------+-------+-----------------------------------------------------------+
| m_platform_factor              | float | multiplier factor                                         |
+--------------------------------+-------+-----------------------------------------------------------+
| m_tx_bps                       | float | total tx bit per second                                   |
+--------------------------------+-------+-----------------------------------------------------------+
| m_rx_bps                       | float | total rx bit per second                                   |
+--------------------------------+-------+-----------------------------------------------------------+
| m_tx_pps                       | float | total tx packet per second                                |
+--------------------------------+-------+-----------------------------------------------------------+
| m_tx_cps                       | float | total tx connection per second                            |
+--------------------------------+-------+-----------------------------------------------------------+
| m_tx_expected_cps              | float | expected tx connection per second                         |
+--------------------------------+-------+-----------------------------------------------------------+
| m_tx_expected_pps              | float | expected tx packet per second                             |
+--------------------------------+-------+-----------------------------------------------------------+
| m_tx_expected_bps              | float | expected tx bit per second                                |
+--------------------------------+-------+-----------------------------------------------------------+
| m_rx_drop_bps                  | float | drop rate in bit per second                               |
+--------------------------------+-------+-----------------------------------------------------------+
| m_active_flows                 | float | active trex flows                                         |
+--------------------------------+-------+-----------------------------------------------------------+
| m_open_flows                   | float | open trex flows from startup (monotonically incrementing) |
+--------------------------------+-------+-----------------------------------------------------------+
| m_total_tx_pkts                |  int  | total tx in packets                                       |
+--------------------------------+-------+-----------------------------------------------------------+
| m_total_rx_pkts                |  int  | total rx in packets                                       |
+--------------------------------+-------+-----------------------------------------------------------+
| m_total_tx_bytes               |  int  | total tx in bytes                                         |
+--------------------------------+-------+-----------------------------------------------------------+
| m_total_rx_bytes               |  int  | total rx in bytes                                         |
+--------------------------------+-------+-----------------------------------------------------------+
| opackets-#                     |  int  | output packets (per interface)                            |
+--------------------------------+-------+-----------------------------------------------------------+
| obytes-#                       |  int  | output bytes (per interface)                              |
+--------------------------------+-------+-----------------------------------------------------------+
| ipackets-#                     |  int  | input packet (per interface)                              |
+--------------------------------+-------+-----------------------------------------------------------+
| ibytes-#                       |  int  | input bytes (per interface)                               |
+--------------------------------+-------+-----------------------------------------------------------+
| ierrors-#                      |  int  | input errors (per interface)                              |
+--------------------------------+-------+-----------------------------------------------------------+
| oerrors-#                      |  int  | input errors (per interface)                              |
+--------------------------------+-------+-----------------------------------------------------------+
| m_total_tx_bps-#               | float | total transmitted data in bit per second                  |
+--------------------------------+-------+-----------------------------------------------------------+
| unknown                        |  int  |                                                           |
+--------------------------------+-------+-----------------------------------------------------------+
| m_total_nat_learn_error [#f1]_ |  int  |                                                           |
+--------------------------------+-------+-----------------------------------------------------------+
| m_total_nat_active [#f2]_      |  int  |                                                           |
+--------------------------------+-------+-----------------------------------------------------------+
| m_total_nat_no_fid [#f2]_      |  int  |                                                           |
+--------------------------------+-------+-----------------------------------------------------------+
| m_total_nat_time_out [#f2]_    |  int  |                                                           |
+--------------------------------+-------+-----------------------------------------------------------+
| m_total_nat_open [#f2]_    	 |  int  |                                                           |
+--------------------------------+-------+-----------------------------------------------------------+


.. _tx-gen-field:

tx-gen field
~~~~~~~~~~~~~~

+-------------------+-------+-----------------------------------------------------------+
|      Sub-key      | Type  |                          Meaning                          |
+===================+=======+===========================================================+
| realtime-hist     | dict  | histogram of transmission. See extended information about |
|                   |       | histogram object under :ref:`histogram-object-fields`.    |
|                   |       | The attribute analyzed is time packet has been sent       |
|                   |       | before/after it was intended to be                        |
+-------------------+-------+-----------------------------------------------------------+
| unknown           | int   |                                                           |
+-------------------+-------+-----------------------------------------------------------+

.. _trex-latecny-field:

trex-latecny field
~~~~~~~~~~~~~~~~~~

+---------+-------+---------------------------------------------------------+
| Sub-key | Type  |                         Meaning                         |
+=========+=======+=========================================================+
| avg-#   | float | average latency in usec (per interface)                 |
+---------+-------+---------------------------------------------------------+
| max-#   | float | max latency in usec from the test start (per interface) |
+---------+-------+---------------------------------------------------------+
| c-max-# | float | max in the last 1 sec window (per interface)            |
+---------+-------+---------------------------------------------------------+
| error-# | float | errors in latency packets (per interface)               |
+---------+-------+---------------------------------------------------------+
| unknown |  int  |                                                         |
+---------+-------+---------------------------------------------------------+

.. _trex-latecny-v2-field:

trex-latecny-v2 field
~~~~~~~~~~~~~~~~~~~~~

+--------------------------------------+-------+--------------------------------------+
|               Sub-key                | Type  |               Meaning                |
+======================================+=======+======================================+
| cpu_util                             | float | rx thread cpu % (this is not trex DP |
|                                      |       | threads cpu%%)                       |
+--------------------------------------+-------+--------------------------------------+
| port-#                               |       | Containing per interface             |
|                                      | dict  | information. See extended            |
|                                      |       | information under ``port-# ->        |
|                                      |       | key_name -> sub_key``                |
+--------------------------------------+-------+--------------------------------------+
| port-#->hist                         | dict  | histogram of latency. See extended   |
|                                      |       | information about histogram object   |
|                                      |       | under :ref:`histogram-object-fields`.|
+--------------------------------------+-------+--------------------------------------+
| port-#->stats                        |       | Containing per interface             |
|                                      | dict  | information. See extended            |
|                                      |       | information under ``port-# ->        |
|                                      |       | key_name -> sub_key``                |
+--------------------------------------+-------+--------------------------------------+
| port-#->stats->m_tx_pkt_ok           | int   | total of try sent packets            |
+--------------------------------------+-------+--------------------------------------+
| port-#->stats->m_pkt_ok              | int   | total of packets sent from hardware  |
+--------------------------------------+-------+--------------------------------------+
| port-#->stats->m_no_magic            | int   | rx error with no magic               |
+--------------------------------------+-------+--------------------------------------+
| port-#->stats->m_no_id               | int   | rx errors with no id                 |
+--------------------------------------+-------+--------------------------------------+
| port-#->stats->m_seq_error           | int   | error in seq number                  |
+--------------------------------------+-------+--------------------------------------+
| port-#->stats->m_length_error        | int   |                                      |
+--------------------------------------+-------+--------------------------------------+
| port-#->stats->m_rx_check            | int   | packets tested in rx                 |
+--------------------------------------+-------+--------------------------------------+
| unknown                              | int   |                                      |
+--------------------------------------+-------+--------------------------------------+



.. _histogram-object-fields:

Histogram object fields
~~~~~~~~~~~~~~~~~~~~~~~

The histogram object is being used in number of place throughout the JSON object.
The following section describes its fields in detail.


+-----------+-------+-----------------------------------------------------------------------------------+
|  Sub-key  | Type  |                                      Meaning                                      |
+===========+=======+===================================================================================+
| min_usec  |  int  | min attribute value in usec. pkt with latency less than this value is not counted |
+-----------+-------+-----------------------------------------------------------------------------------+
| max_usec  |  int  | max attribute value in usec                                                       |
+-----------+-------+-----------------------------------------------------------------------------------+
| high_cnt  |  int  | how many packets on which its attribute > min_usec                                |
+-----------+-------+-----------------------------------------------------------------------------------+
| cnt       |  int  | total packets from test startup                                                   |
+-----------+-------+-----------------------------------------------------------------------------------+
| s_avg     | float | average value from test startup                                                   |
+-----------+-------+-----------------------------------------------------------------------------------+
| histogram |       | histogram of relevant object by the following keys:                               |
|           | array |  - key: value in usec                                                             |
|           |       |  - val: number of packets                                                         |
+-----------+-------+-----------------------------------------------------------------------------------+


Access Examples
---------------



.. rubric:: Footnotes

.. [#f1] Available only in NAT and NAT learning operation (``learn`` and ``learn-verify`` flags)

.. [#f2] Available only in NAT operation (``learn`` flag)