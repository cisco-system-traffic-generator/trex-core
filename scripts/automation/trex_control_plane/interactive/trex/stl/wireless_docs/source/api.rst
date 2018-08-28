API - Wireless Manager Documentation
====================================

.. toctree::
    :maxdepth: 4


| :ref:`introduction`
| :ref:`example_use`
| :ref:`initialization`
| :ref:`creating_devices`
| :ref:`helpers`
| :ref:`joining`
| :ref:`plugins`
| :ref:`info_stats`


.. _introduction:

Introduction
------------
| The WirelessManager is the main class of TRex Wireless, it is the entry point for most uses.
| It provides an API to use the wireless simulation, for scripting, or higher level APIs such as TRex Client Console.
| It abstracts the lower level work done, like load balancing, subprocesses, ...

.. _example_use:

Example of Scripting use
------------------------

| Here is an example of how to instanciate and use a WirelessManager.

    :ref:`example_min`

.. _initialization:

Initialization
--------------

| To create a WirelessManager, one has to provide a configuration yaml file.

    .. automethod:: wireless.trex_wireless_manager.WirelessManager.__init__
        :noindex:

| The WirelessManager created, now it has to be started:

    .. automethod:: wireless.trex_wireless_manager.WirelessManager.start
        :noindex:

.. warning:: Load Plugins before starting the WirelessManager.

    See :ref:`plugins`.


| An example of a configuration file:

    :ref:`configuration`


.. _creating_devices:

Creating Wireless Devices (AP & Client)
---------------------------------------

| These methods creates APs and Clients on a Manager.
| Each device can then be programmed to join, or launch a plugin.
| The user should keep track of the MAC addresses of each device to identify them.

    .. automethod:: wireless.trex_wireless_manager.WirelessManager.create_aps
        :noindex:

    .. automethod:: wireless.trex_wireless_manager.WirelessManager.create_clients
        :noindex:


| For ease of use, check :ref:`helpers` to generate the parameters for these functions.

.. _helpers:

Helpers Functions
-----------------

| When creating APs or Clients on the Manager, one has to provide their parameters (IP, MAC addresses, ...).
| For parameter generation, one can use, with a configuration file containing the "base values".

    .. automethod:: wireless.trex_wireless_manager.WirelessManager.create_aps_params
        :noindex:

    .. automethod:: wireless.trex_wireless_manager.WirelessManager.create_clients_params
        :noindex:

| To change the base values specified in the configuration file, in script, one can use:

    .. automethod:: wireless.trex_wireless_manager.WirelessManager.set_base_values
        :noindex:

.. _joining:

Join and Association
--------------------

| Methods to join APs or Associate clients:

    .. automethod:: wireless.trex_wireless_manager.WirelessManager.join_aps
        :noindex:

    .. automethod:: wireless.trex_wireless_manager.WirelessManager.join_clients
        :noindex:

.. _plugins:

Plugins Use
-----------

| To use plugins, first thing to do is to load them in the Manager.
| This should be done before starting the Manager.

    .. automethod:: wireless.trex_wireless_manager.WirelessManager.load_ap_service
        :noindex:

    .. automethod:: wireless.trex_wireless_manager.WirelessManager.load_client_service
        :noindex:

| These methods returns a ServiceInfo used to start services.
| Once the plugins loaded, and devices created, one can start services on a set of devices :

    .. automethod:: wireless.trex_wireless_manager.WirelessManager.add_aps_service
        :noindex:

    .. automethod:: wireless.trex_wireless_manager.WirelessManager.add_clients_service
        :noindex:

.. _info_stats:

Retrieving stats
----------------

| Plugins can store information and statistics into the APs and Clients.
| To retrieve this information, one can use :

    .. automethod:: wireless.trex_wireless_manager.WirelessManager.get_aps_services_info
        :noindex:

    .. automethod:: wireless.trex_wireless_manager.WirelessManager.get_clients_services_info
        :noindex:

| These function return a list of "services info" for each device:
| a "services info" is a dictionnary "service_name -> infos"
| where "infos" is a dictionnary "info_name -> value" 
