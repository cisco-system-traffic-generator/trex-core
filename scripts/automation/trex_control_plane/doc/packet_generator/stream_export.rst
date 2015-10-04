
Stream Export YAML syntax
=========================

In order to provide a fluent work-flow that utilize the best TRex user's time, an export-import mini language has been created.

This enables a work-flow that supports saving and sharing a built packets and its scenarios, so that other tools
(such as TRex Console) could use them.

The TRex Packet Builder module supports (using ___ method) the export of built stream according to the format described below.

Guidelines
----------

1. One
2. Two
3. Three

Export Format
-------------

.. literalinclude:: export_format.yaml

Example
-------

Whenever TRex is publishing live data, it uses JSON notation to describe the data-object.
