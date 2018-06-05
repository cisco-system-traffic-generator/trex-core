
ASTF Client Module 
==================

The TRex Client provides access to the TRex server.
TBD need to fill this doc 




ASTFClient class
----------------

.. autoclass:: trex.astf.trex_astf_client.ASTFClient
    :members: 
    :inherited-members:
    :member-order: bysource
    

ASTFClient snippet
------------------


.. code-block:: python

    # Example 1: Minimal example of client interacting with the TRex server

    c = ASTFClient()

    try:
        # connect to server
        c.connect()

        # prepare our ports (my machine has 0 <--> 1 with static route)
        c.reset(ports = [0, 1])

        c.clear_stats()

        c.start(ports = [0, 1], mult = "5mpps", duration = 10)

        # block until done
        c.wait_on_traffic(ports = [0, 1])

        # check for any warnings
	if c.get_warnings():
	    # handle warnings here
	    pass

    finally:
        c.disconnect()




