
Stream Export YAML syntax
=========================

In order to provide a fluent work-flow that utilize the best TRex user's time, an export-import mini language has been created.

This enables a work-flow that supports saving and sharing a built packets and its scenarios, so that other tools
(such as TRex Console) could use them.

The TRex Packet Builder module supports (using ___ method) the export of built stream according to the format described below.

Guidelines
----------

1. The YAML file can either contain Byte representation of the packet of refer to a .pcap file that contains it.
2. The YAML file is similar as much as possible to the `add_stream method <http://trex-tgn.cisco.com/trex/doc/trex_rpc_server_spec.html#_add_stream>`_ of TRex RPC server spec, which defines the raw interaction with TRex server.
3. Only packet binary data and VM instructions are to be saved. Any meta-data packet builder module used while creating the packet will be stripped out.

Export Format
-------------

.. literalinclude:: export_format.yaml
    :lines: 4-
    :linenos:

Example
-------

The following files snapshot represents each of the options (Binary/pcap) for the very same HTTP GET request packet.
