
# base object class for a packet builder
class CTrexPktBuilderInterface(object):

    def compile (self):
        """
        Compiles the packet and VM
        """
        raise Exception("implement me")


    def dump_pkt(self):
        """
        Dumps the packet as a decimal array of bytes (each item x gets value between 0-255)

        :parameters:
            None

        :return:
            + packet representation as array of bytes

        :raises:
            + :exc:`CTRexPktBuilder.EmptyPacketError`, in case packet is empty.

        """

        raise Exception("implement me")


    def get_vm_data(self):
        """
        Dumps the instructions 

        :parameters:
            None

        :return:
            + json object of instructions

        """

        raise Exception("implement me")

