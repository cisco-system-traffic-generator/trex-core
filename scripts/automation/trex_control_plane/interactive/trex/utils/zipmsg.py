import zlib
import struct

class ZippedMsg:

    MSG_COMPRESS_THRESHOLD    = 0
    MSG_COMPRESS_HEADER_MAGIC = 0xABE85CEA

    def check_threshold(self, msg):
        return len(msg) >= self.MSG_COMPRESS_THRESHOLD

    def compress (self, msg):
        # compress
        compressed = zlib.compress(msg)
        new_msg = struct.pack(">II", self.MSG_COMPRESS_HEADER_MAGIC, len(msg)) + compressed
        return new_msg


    def decompress(self, msg):
        if len(msg) < 8:
            return None

        t = struct.unpack(">II", msg[:8])
        if (t[0] != self.MSG_COMPRESS_HEADER_MAGIC):
            return None

        x = zlib.decompress(msg[8:])
        if len(x) != t[1]:
            return None

        return x


    def is_compressed(self, msg):
        if len(msg) < 8:
            return False

        t = struct.unpack(">II", msg[:8])
        if (t[0] != self.MSG_COMPRESS_HEADER_MAGIC):
            return False

        return True


