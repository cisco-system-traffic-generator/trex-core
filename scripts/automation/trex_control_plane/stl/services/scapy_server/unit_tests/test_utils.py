# run with 'nosetests' utility

from basetest import *
from scapy_service import *

def test_generate_random_bytes():
    res = generate_random_bytes(10, 333, ord('0'), ord('9'))
    print(res)
    assert(len(res) == 10)
    assert(res == b'5390532937') # random value with this seed

def test_generate_bytes_from_template_empty():
    res = generate_bytes_from_template(5, b"")
    print(res)
    assert(res == b"")

def test_generate_bytes_from_template_neg():
    res = generate_bytes_from_template(-5, b"qwe")
    assert(res == b"")

def test_generate_bytes_from_template_less():
    res = generate_bytes_from_template(5, b"qwe")
    print(res)
    assert(res == b"qweqw")

def test_generate_bytes_from_template_same():
    res = generate_bytes_from_template(5, b"qwert")
    print(res)
    assert(res == b"qwert")

def test_generate_bytes_from_template_more():
    res = generate_bytes_from_template(5, b"qwerty")
    print(res)
    assert(res == b"qwert")

def test_parse_template_code_with_trash():
    res = parse_template_code("0xDE AD\n be ef \t0xDEAD")
    print(res)
    assert(res == bytearray.fromhex('DEADBEEFDEAD'))

def test_generate_bytes():
    res = generate_bytes({"generate":"random_bytes", "seed": 123, "size": 12})
    print(res)
    assert(len(res) == 12)

def test_generate_ascii_default_seed():
    res = generate_bytes({"generate":"random_ascii", "size": 14})
    print(res)
    assert(len(res) == 14)


def test_generate_template_code():
    res = generate_bytes({"generate":"template_code", "template_code": "0xDEAD 0xBEEF", "size": 6})
    print(res)
    assert(res == bytearray.fromhex('DE AD BE EF DE AD'))

def test_generate_template_base64():
    res = generate_bytes({"generate":"template", "template_base64": bytes_to_b64(b'hi'), "size": 5})
    print(res)
    assert(res == b'hihih')


