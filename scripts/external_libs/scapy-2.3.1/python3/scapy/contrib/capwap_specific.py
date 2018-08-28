from abc import ABC, abstractstaticmethod

class CAPWAP_PKTS_BUILDER:

    @abstractstaticmethod
    def parse_message_elements(rx_pkt_buf, capwap_hlen, ap, ap_manager):
        """Parses received capwap control packet and update state on given AP."""
        raise NotImplementedError

    @abstractstaticmethod
    def discovery(ap):
        """Returns a CAPWAP CONTROL packet containing the discovery packet of an AP to a controller,
        not including sublayers.

        Args:
            ap (AP): source of the packet
        Returns:
            capw_ctrl (bytes): capwap control bytes
        """
        raise NotImplementedError
    
    @abstractstaticmethod
    def join(ap):
        """Returns a CAPWAP CONTROL packet containing the join packet of an AP to a controller,
        not including sublayers.

        Args:
            ap (AP): source of the packet
        Returns:
            capw_ctrl (bytes): capwap control bytes
        """
        raise NotImplementedError
    
    @abstractstaticmethod
    def conf_status_req(ap):
        """Returns a CAPWAP CONTROL packet containing the "configuration status update" packet of an AP to a controller,
        not including sublayers.

        Args:
            ap (AP): source of the packet
        Returns:
            capw_ctrl (bytes): capwap control bytes
        """
        raise NotImplementedError

    @abstractstaticmethod
    def change_state(ap, radio_id=0):
        """Returns a CAPWAP CONTROL packet containing the "change state event request" packet of an AP to a controller,
        not including sublayers.

        Args:
            ap (AP): source of the packet
            radio_id (int): id of the concerned radio
        Returns:
            capw_ctrl (bytes): capwap control bytes
        """
        raise NotImplementedError

    @abstractstaticmethod
    def config_update(ap, capwap_seq):
        """Returns a CAPWAP CONTROL packet containing the "configuration update response" packet of an AP to a controller,
        not including sublayers.

        Args:
            ap (AP): source of the packet
            capwap_seq (int): sequence number of the requested response
        Returns:
            capw_ctrl (bytes): capwap control bytes
        """
        raise NotImplementedError


    @abstractstaticmethod
    def echo(ap):
        """Returns a CAPWAP CONTROL packet containing the "echo request" packet of an AP to a controller,
        not including sublayers.

        Args:
            ap (AP): source of the packet
        Returns:
            capw_ctrl (bytes): capwap control bytes
        """
        raise NotImplementedError

    @abstractstaticmethod
    def keep_alive(ap):
        """Returns a CAPWAP DATA packet containing the "keep alive" packet of an AP to a controller,
        not including sublayers.

        Args:
            ap (AP): source of the packet
        Returns:
            capw_data (bytes): capwap data bytes
        """
        raise NotImplementedError

    @abstractstaticmethod
    def client_assoc(ap, vap, client_mac):
        """Returns a CAPWAP DATA packet containing the "association request"
        of a client attached to given AP, intended for the VAP.


        Args:
            ap (AP): source of the packet
            vap (VAP): vap for the AP to associate, on a given frequency.
            client_mac (str): mac address of the associating client
        Returns:
            capw_data (bytes): capwap data bytes, with payload Dot11 association request
        """
        raise NotImplementedError

    @abstractstaticmethod
    def client_disassoc(ap, vap, client_mac):
        """Returns a CAPWAP DATA packet containing the "disassociation" packet
        of a client attached to given AP, intended for the VAP.


        Args:
            ap (AP): source of the packet
            vap (VAP): vap for the AP to associate, on a given frequency.
            client_mac (str): mac address of the disassociating client
        Returns:
            capw_data (bytes): capwap data bytes, with payload Dot11 disassociation
        """
        raise NotImplementedError
