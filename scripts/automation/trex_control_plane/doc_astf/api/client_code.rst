
ASTF Client Module 
==================

The TRex Client provides access to the TRex server.
        

ASTFClient class
----------------

.. autoclass:: trex.astf.trex_astf_client.ASTFClient
    :members: 
    :inherited-members:
    :member-order: bysource
    

ASTFClient snippet
------------------


.. code-block:: python

            c = ASTFClient(server = server)
        
            c.connect()
        
            try:
                c.reset()
        
                c.load_profile(profile_path)
        
                c.clear_stats()
        
                c.start(mult = mult, duration = duration, nc = True)
        
                c.wait_on_traffic()
        
                stats = c.get_stats()
        
                pprint.pprint(stats)
        
                if c.get_warnings():
                    for w in c.get_warnings():
                        print(w)
        
        
            except TRexError as e:
                print(e)
        
            except AssertionError as e:
                print(e)
        
            finally:
                c.disconnect()
        

.. code-block:: python

            c = ASTFClient(server = server)
        
            c.connect()
        
            try:
                c.reset()

                c.load_profile(profile_full_path)
        
                c.clear_stats()
        
                c.start(mult = mult, duration = duration, nc = True)
        
                while c.is_traffic_active():
                   stats = c.get_stats()
                   # sample the stats 
                   time.sleep(1);
   
            except Exception as e:
                print(e)
        
            finally:
                c.disconnect()
        


.. code-block:: python

    # Example: tempate group name and counters

        self.c.load_profile(profile)

        self.c.clear_stats()

        self.c.start(duration = 60, mult = 100)

        self.c.wait_on_traffic()

        names = self.c.get_tg_names()                  

        stats = self.c.get_traffic_tg_stats(names[:2])  

