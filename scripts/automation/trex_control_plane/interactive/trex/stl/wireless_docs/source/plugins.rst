Plugin API
===========

.. toctree::
    :maxdepth: 4


| :ref:`introduction`
| :ref:`writing_plugins`
| :ref:`examples`
| :ref:`advanced`

.. _introduction:

Introduction
------------
| A plugin, or "WirelessService" is a coroutine intended to run on a simulated wireless device (AP or Client).
| For instance DHCP is implemented as a plugin, the tester can then decide to run the DHCP plugin on a client or not.
| Multiple plugins can run together on a single device, as long as the plugins do not block.


.. _writing_plugins:

Writing Plugins
---------------

Check the examples for a better understanding.

Location
~~~~~~~~

| A plugin can be written anywhere, it is loaded on the manager from a module name and a the name of the service class using : 
 
    :meth:`wireless.trex_wireless_manager.WirelessManager.load_ap_service`

    :meth:`wireless.trex_wireless_manager.WirelessManager.load_client_service`

| For example:

    .. code-block:: python

        self.client_asso_service = self.load_client_service(
            "wireless.services.client.client_service_association", "ClientServiceAssociation")


Coroutines
~~~~~~~~~~

| Since services are coroutine, one has to use asynchronous methods prefixed with 'async' using the 'yield' keyword,
    check :ref:`writing_plugins.packets` for example. 
    There are still blocking calls such as :meth:`~wireless.services.trex_wireless_service.WirelessService.send_pkt` but they should not impact performance too much.

.. _writing_plugins.packets:

Sending and Receiving packets
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

| To receive packets:

    .. automethod:: wireless.services.trex_wireless_service.WirelessService.async_recv_pkt
        :noindex:

    .. automethod:: wireless.services.trex_wireless_service.WirelessService.async_recv_pkts
        :noindex:

    These methods should be used with the 'yield' keyword :

    .. code-block:: python

        pkts = yield self.async_recv_pkt(time_sec=self.wait_time)
        if pkts:
            pkt = pkts[0]
            ...

 | To send packets:

    .. automethod:: wireless.services.trex_wireless_service.WirelessService.send_pkt
        :noindex:

.. warning:: Reception of packet

    | When writing an APService, the packets received are the full packets.
    | However, for a ClientService, the packets received from the plugin are the capwap-data-desencapsulated packet.
    | In short, a ClientService would only receive packets as a real client would : 
    | Dot11 / IP / ... in contrary to Ether / IP / CAPWAP DATA / Dot11 / IP / ... 
    

Saving Information
~~~~~~~~~~~~~~~~~~

| A plugin can save information for reporting, this can be useful to report interesting information such as
    number of retries for a protocol, time taken...

The API offers two methdods :

 .. automethod:: wireless.services.trex_wireless_service.WirelessService.add_service_info
 .. automethod:: wireless.services.trex_wireless_service.WirelessService.get_service_info



Events
~~~~~~

| If a client service requires the client to be associated to run, this service should wait on the success of
    the association service.

| This is possible with the use of the class :

.. autoclass:: wireless.services.trex_wireless_service_event.WirelessServiceEvent

| To raise an event, first create a subclass of :class:`~wireless.services.trex_wireless_service_event.WirelessServiceEvent` : 

    .. code-block:: python

        class ClientAssociationAssociatedEvent(WirelessServiceEvent):
            """Raised when client gets associated."""
            def __init__(self, env, device):
                service = ClientServiceAssociation.__name__
                value = "associated"
                super().__init__(env, device, service, value)

| Then raise it in a plugin's run method :

    .. code-block:: python

        self.raise_event(ClientAssociationAssociatedEvent(self.env, client.mac))
    

| To wait for such an event use :

    .. code-block:: python

        yield self.async_wait_for_event(ClientAssociationAssociatedEvent(self.env, client.mac))

| Or to wait on multiple events or a packet:

    .. code-block:: python

        client.logger.debug("ClientServiceAssociation waiting for packet or AP disconnection")
        wait_for_events = [APJoinConnectedEvent(self.env, self.client.ap.mac)]
        events = yield self.async_wait_for_any_events(wait_for_events, wait_packet=True)
        client.logger.debug("ClientServiceAssociation awaken")
        pkts = []
        for event, value in events.items():
            if isinstance(event, PktRX):
                # got packet
                pkts = value
            else:
                # got APJoinConnectedEvent
                ...


.. warning:: Waiting on events

    | An AP should not wait on other AP's event, or care must be taken.
    | As a rule, a wireless device should only wait on its attached device's events :
        An AP can wait on its events, or its clients' events.
        A client can wait on its events or its AP's events.
    | This is because APs do not always live on the same python process.
    
---------

 .. automethod:: wireless.services.trex_wireless_service.WirelessService.raise_event
 .. automethod:: wireless.services.trex_wireless_service.WirelessService.async_wait_for_event
 .. automethod:: wireless.services.trex_wireless_service.WirelessService.async_wait_for_any_events


Concurrent services
~~~~~~~~~~~~~~~~~~~

| If one wants to join 10'000 APs at the same time, if no care is taken, all coroutines will be started at the same time
    wich would probably overwhelm the CPU, wich would cause delays in scheduling the coroutines.
    For instance, a couroutine waiting on a packet may be awaken seconds after the actual reception of the packet.
| This is something to avoid, the chosen solution was to schedule all coroutines when asked, but only start some of them.
    When one finishes processing, another couroutine can start.
    This is what we call the number of concurrent services.
    To enforce this, the services needs to use the two below methods :

    .. automethod:: wireless.services.trex_wireless_service.WirelessService.async_request_start
        :noindex:

    .. automethod:: wireless.services.trex_wireless_service.WirelessService.async_request_stop
        :noindex:

.. _examples:

Examples
--------

AP packets printer
~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. literalinclude:: /../wireless/services/ap/ap_service_printer_example.py
    :linenos:
    :language: python


| This plugin for APs wait for a packet, print its layers and wait for the next packet etc...

| The class inherits from :class:`~wireless.services.ap.ap_service.APService`, meaning that this is a plugin for APs.
| It has a reference to the object :class:`~wireless.trex_wireless_ap.AP`, using its logger to log an information message.

| It request to start, this is needed for plugins that hibernates (stop receiving/sending packets) to save computing ressources
    and to limit the number of concurrent :class:`~wireless.services.ap.ap_service.ap_service_printer_example.APServicePrinter` running concurrently (to save computing ressources).
    The flag 'first_start' is used to indicate that the service requires start for the first time, and 'request_packet' to allow the reception of packets in this plugin.
| Then the main loop starts with a request to receive packets via 'self.async_recv_pkt()', this function returns a list of 0 or 1 packet, in this case always 1 because there is no timeout.
| then the packet is parsed using Scapy and printed.


AP periodic capwap data packets
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. literalinclude:: /../wireless/services/ap/ap_service_periodic_reports_example.py
    :linenos:
    :language: python


| This :class:`~wireless.services.ap.ap_service.APService` sends an empty Capwap Data packet every 30 seconds.
    This could be used to send periodic Cisco IAPP reports to a WLC.


| Firstly, the service requires to start.
| Then the run loop is started
| If the AP is not yet Joined, the service will wait on the Join event.
| Otherwise, it will send the packet.

| However, for ease of use the payload is sent as is, and this is the 
    :class:`~wireless.services.ap.ap_service_periodic_reports_example.PeriodicAPRepors.Connection`
    that does it for us.
| Writing a :class:`~wireless.services.ap.ap_service_periodic_reports_example.PeriodicAPRepors.Connection`
    class inside a 
    :class:`~wireless.services.trex_wireless_service.WirelessService`
    overrides the default Connection, only used to send packets.

| When running the service, it ouputs for example:

::
    
    Sending: Ether / IP / UDP 9.2.0.6:10006 > 9.1.0.1:5247 / CAPWAP_DATA


.. _advanced:

Advanced
--------

Advanced use of Plugins

Subservices
~~~~~~~~~~~

A plugin can be written to launch other plugins and coordinate them.

An example is given for the client association followed by DHCP :


.. literalinclude:: /../wireless/services/client/client_service_association_and_dhcp.py
    :linenos:
    :language: python


Same Plugin for AP and Clients
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

| Some services may share the same logic for APs and Clients, for instance, DHCP.
| To not repeat yourself, it is therefore best to write the logic only once.
| The solution is to write a plugin for a WirelessDevice (super class of simulated APs and Clients) 
    and to inherit from it for the APService and ClientService.
| Therefore a concrete WirelessService (APService or ClientService) would need to inherit from both classes
    :class:`~wireless.services.trex_wireless_service.WirelessService`,
    and another class itself inheriting from :class:`~wireless.services.trex_wireless_service.WirelessService`,
    hopefully this is possible in Python, if care is given.

Example: DHCP

    | DHCP is implemented once, and used for an APService and a ClientService.

    | Check :ref:`dhcp` for the code.

