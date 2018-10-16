
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

    # connect to server
    c.connect()

    try:
        c.reset()

        c.clear_stats()

        c.load_profile(<profile_path>)

        c.start(mult = 42, duration = 10)

        c.wait_on_traffic()

        stats = c.get_stats()
        # check stats here

        # check for any warnings
        if c.get_warnings():
            # handle warnings here
            pass

    finally:
        c.disconnect()




