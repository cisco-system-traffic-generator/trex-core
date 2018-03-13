# $Id: http.py 86 2013-03-05 19:25:19Z andrewflnr@gmail.com $
# -*- coding: utf-8 -*-
"""Hypertext Transfer Protocol."""
from __future__ import print_function
from __future__ import absolute_import
try:
    from collections import OrderedDict
except ImportError:
    # Python 2.6
    OrderedDict = dict

from . import dpkt
from .compat import BytesIO, iteritems


def parse_headers(f):
    """Return dict of HTTP headers parsed from a file object."""
    d = OrderedDict()
    while 1:
        # The following logic covers two kinds of loop exit criteria.
        # 1) If the header is valid, when we reached the end of the header,
        #    f.readline() would return with '\r\n', then after strip(),
        #    we can break the loop.
        # 2) If this is a weird header, which do not ends with '\r\n',
        #    f.readline() would return with '', then after strip(),
        #    we still get an empty string, also break the loop.
        line = f.readline().strip().decode("ascii", "ignore")
        if not line:
            break
        l = line.split(':', 1)
        if len(l[0].split()) != 1:
            raise dpkt.UnpackError('invalid header: %r' % line)
        k = l[0].lower()
        v = len(l) != 1 and l[1].lstrip() or ''
        if k in d:
            if not type(d[k]) is list:
                d[k] = [d[k]]
            d[k].append(v)
        else:
            d[k] = v
    return d


def parse_body(f, headers):
    """Return HTTP body parsed from a file object, given HTTP header dict."""
    if headers.get('transfer-encoding', '').lower() == 'chunked':
        l = []
        found_end = False
        while 1:
            try:
                sz = f.readline().split(None, 1)[0]
            except IndexError:
                raise dpkt.UnpackError('missing chunk size')
            n = int(sz, 16)
            if n == 0:
                found_end = True
            buf = f.read(n)
            if f.readline().strip():
                break
            if n and len(buf) == n:
                l.append(buf)
            else:
                break
        if not found_end:
            raise dpkt.NeedData('premature end of chunked body')
        body = b''.join(l)
    elif 'content-length' in headers:
        n = int(headers['content-length'])
        body = f.read(n)
        if len(body) != n:
            raise dpkt.NeedData('short body (missing %d bytes)' % (n - len(body)))
    elif 'content-type' in headers:
        body = f.read()
    else:
        # XXX - need to handle HTTP/0.9
        body = b''
    return body


class Message(dpkt.Packet):
    """Hypertext Transfer Protocol headers + body.

    TODO: Longer class information....

    Attributes:
        __hdr__: Header fields of HTTP.
        TODO.
    """

    __metaclass__ = type
    __hdr_defaults__ = {}
    headers = None
    body = None

    def __init__(self, *args, **kwargs):
        if args:
            self.unpack(args[0])
        else:
            self.headers = OrderedDict()
            self.body = b''
            self.data = b''
            # NOTE: changing this to iteritems breaks py3 compatibility
            for k, v in self.__hdr_defaults__.items():
                setattr(self, k, v)
            for k, v in iteritems(kwargs):
                setattr(self, k, v)

    def unpack(self, buf, is_body_allowed=True):
        f = BytesIO(buf)
        # Parse headers
        self.headers = parse_headers(f)
        # Parse body
        if is_body_allowed:
            self.body = parse_body(f, self.headers)
        # Save the rest
        self.data = f.read()

    def pack_hdr(self):
        return ''.join(['%s: %s\r\n' % t for t in iteritems(self.headers)])

    def __len__(self):
        return len(str(self))

    def __str__(self):
        return '%s\r\n%s' % (self.pack_hdr(), self.body.decode("utf8", "ignore"))

    def __bytes__(self):
        # Not using byte interpolation to preserve Python 3.4 compatibility. The extra
        # \r\n doesn't get trimmed from the bytes, so it's necessary to omit the spacing
        # one when building the output if there's no body
        if self.body:
            return self.pack_hdr().encode("ascii", "ignore") + b'\r\n' + self.body
        else:
            return self.pack_hdr().encode("ascii", "ignore")


class Request(Message):
    """Hypertext Transfer Protocol Request.

    TODO: Longer class information....

    Attributes:
        __hdr__: Header fields of HTTP request.
        TODO.
    """

    __hdr_defaults__ = {
        'method': 'GET',
        'uri': '/',
        'version': '1.0',
    }
    __methods = dict.fromkeys((
        'GET', 'PUT', 'ICY',
        'COPY', 'HEAD', 'LOCK', 'MOVE', 'POLL', 'POST',
        'BCOPY', 'BMOVE', 'MKCOL', 'TRACE', 'LABEL', 'MERGE',
        'DELETE', 'SEARCH', 'UNLOCK', 'REPORT', 'UPDATE', 'NOTIFY',
        'BDELETE', 'CONNECT', 'OPTIONS', 'CHECKIN',
        'PROPFIND', 'CHECKOUT', 'CCM_POST',
        'SUBSCRIBE', 'PROPPATCH', 'BPROPFIND',
        'BPROPPATCH', 'UNCHECKOUT', 'MKACTIVITY',
        'MKWORKSPACE', 'UNSUBSCRIBE', 'RPC_CONNECT',
        'VERSION-CONTROL',
        'BASELINE-CONTROL'
    ))
    __proto = 'HTTP'

    def unpack(self, buf):
        f = BytesIO(buf)
        line = f.readline().decode("ascii", "ignore")
        l = line.strip().split()
        if len(l) < 2:
            raise dpkt.UnpackError('invalid request: %r' % line)
        if l[0] not in self.__methods:
            raise dpkt.UnpackError('invalid http method: %r' % l[0])
        if len(l) == 2:
            # HTTP/0.9 does not specify a version in the request line
            self.version = '0.9'
        else:
            if not l[2].startswith(self.__proto):
                raise dpkt.UnpackError('invalid http version: %r' % l[2])
            self.version = l[2][len(self.__proto) + 1:]
        self.method = l[0]
        self.uri = l[1]
        Message.unpack(self, f.read())

    def __str__(self):
        return '%s %s %s/%s\r\n' % (self.method, self.uri, self.__proto,
                                    self.version) + Message.__str__(self)

    def __bytes__(self):
        str_out = '%s %s %s/%s\r\n' % (self.method, self.uri, self.__proto,
                                       self.version)
        return str_out.encode("ascii", "ignore") + Message.__bytes__(self)


class Response(Message):
    """Hypertext Transfer Protocol Response.

    TODO: Longer class information....

    Attributes:
        __hdr__: Header fields of HTTP Response.
        TODO.
    """

    __hdr_defaults__ = {
        'version': '1.0',
        'status': '200',
        'reason': 'OK'
    }
    __proto = 'HTTP'

    def unpack(self, buf):
        f = BytesIO(buf)
        line = f.readline()
        l = line.strip().decode("ascii", "ignore").split(None, 2)
        if len(l) < 2 or not l[0].startswith(self.__proto) or not l[1].isdigit():
            raise dpkt.UnpackError('invalid response: %r' % line)
        self.version = l[0][len(self.__proto) + 1:]
        self.status = l[1]
        self.reason = l[2] if len(l) > 2 else ''
        # RFC Sec 4.3.
        # http://www.w3.org/Protocols/rfc2616/rfc2616-sec4.html#sec4.3.
        # For response messages, whether or not a message-body is included with
        # a message is dependent on both the request method and the response
        # status code (section 6.1.1). All responses to the HEAD request method
        # MUST NOT include a message-body, even though the presence of entity-
        # header fields might lead one to believe they do. All 1xx
        # (informational), 204 (no content), and 304 (not modified) responses
        # MUST NOT include a message-body. All other responses do include a
        # message-body, although it MAY be of zero length.
        is_body_allowed = int(self.status) >= 200 and 204 != int(self.status) != 304
        Message.unpack(self, f.read(), is_body_allowed)

    def __str__(self):
        return '%s/%s %s %s\r\n' % (self.__proto, self.version, self.status,
                                    self.reason) + Message.__str__(self)

    def __bytes__(self):
        str_out = '%s/%s %s %s\r\n' % (self.__proto, self.version, self.status,
                                       self.reason) + Message.__str__(self)
        return str_out.encode("ascii", "ignore") + Message.__bytes__(self)


def test_parse_request():
    s = b"""POST /main/redirect/ab/1,295,,00.html HTTP/1.0\r\nReferer: http://www.email.com/login/snap/login.jhtml\r\nConnection: Keep-Alive\r\nUser-Agent: Mozilla/4.75 [en] (X11; U; OpenBSD 2.8 i386; Nav)\r\nHost: ltd.snap.com\r\nAccept: image/gif, image/x-xbitmap, image/jpeg, image/pjpeg, image/png, */*\r\nAccept-Encoding: gzip\r\nAccept-Language: en\r\nAccept-Charset: iso-8859-1,*,utf-8\r\nContent-type: application/x-www-form-urlencoded\r\nContent-length: 61\r\n\r\nsn=em&mn=dtest4&pw=this+is+atest&fr=true&login=Sign+in&od=www"""
    r = Request(s)
    assert r.method == 'POST'
    assert r.uri == '/main/redirect/ab/1,295,,00.html'
    assert r.body == b'sn=em&mn=dtest4&pw=this+is+atest&fr=true&login=Sign+in&od=www'
    assert r.headers['content-type'] == 'application/x-www-form-urlencoded'
    try:
        Request(s[:60])
        assert 'invalid headers parsed!'
    except dpkt.UnpackError:
        pass


def test_format_request():
    r = Request()
    assert str(r) == 'GET / HTTP/1.0\r\n\r\n'
    r.method = 'POST'
    r.uri = '/foo/bar/baz.html'
    r.headers['content-type'] = 'text/plain'
    r.headers['content-length'] = '5'
    r.body = b'hello'
    s = str(r)
    assert s.startswith('POST /foo/bar/baz.html HTTP/1.0\r\n')
    assert s.endswith('\r\n\r\nhello')
    assert '\r\ncontent-length: 5\r\n' in s
    assert '\r\ncontent-type: text/plain\r\n' in s
    s = bytes(r)
    assert s.startswith(b'POST /foo/bar/baz.html HTTP/1.0\r\n')
    assert s.endswith(b'\r\n\r\nhello')
    assert b'\r\ncontent-length: 5\r\n' in s
    assert b'\r\ncontent-type: text/plain\r\n' in s
    r = Request(bytes(r))
    assert bytes(r) == s


def test_chunked_response():
    s = b"""HTTP/1.1 200 OK\r\nCache-control: no-cache\r\nPragma: no-cache\r\nContent-Type: text/javascript; charset=utf-8\r\nContent-Encoding: gzip\r\nTransfer-Encoding: chunked\r\nSet-Cookie: S=gmail=agg:gmail_yj=v2s:gmproxy=JkU; Domain=.google.com; Path=/\r\nServer: GFE/1.3\r\nDate: Mon, 12 Dec 2005 22:33:23 GMT\r\n\r\na\r\n\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\x00\r\n152\r\nm\x91MO\xc4 \x10\x86\xef\xfe\n\x82\xc9\x9eXJK\xe9\xb6\xee\xc1\xe8\x1e6\x9e4\xf1\xe0a5\x86R\xda\x12Yh\x80\xba\xfa\xef\x85\xee\x1a/\xf21\x99\x0c\xef0<\xc3\x81\xa0\xc3\x01\xe6\x10\xc1<\xa7eYT5\xa1\xa4\xac\xe1\xdb\x15:\xa4\x9d\x0c\xfa5K\x00\xf6.\xaa\xeb\x86\xd5y\xcdHY\x954\x8e\xbc*h\x8c\x8e!L7Y\xe6\'\xeb\x82WZ\xcf>8\x1ed\x87\x851X\xd8c\xe6\xbc\x17Z\x89\x8f\xac \x84e\xde\n!]\x96\x17i\xb5\x02{{\xc2z0\x1e\x0f#7\x9cw3v\x992\x9d\xfc\xc2c8\xea[/EP\xd6\xbc\xce\x84\xd0\xce\xab\xf7`\'\x1f\xacS\xd2\xc7\xd2\xfb\x94\x02N\xdc\x04\x0f\xee\xba\x19X\x03TtW\xd7\xb4\xd9\x92\n\xbcX\xa7;\xb0\x9b\'\x10$?F\xfd\xf3CzPt\x8aU\xef\xb8\xc8\x8b-\x18\xed\xec<\xe0\x83\x85\x08!\xf8"[\xb0\xd3j\x82h\x93\xb8\xcf\xd8\x9b\xba\xda\xd0\x92\x14\xa4a\rc\reM\xfd\x87=X;h\xd9j;\xe0db\x17\xc2\x02\xbd\xb0F\xc2in#\xfb:\xb6\xc4x\x15\xd6\x9f\x8a\xaf\xcf)\x0b^\xbc\xe7i\x11\x80\x8b\x00D\x01\xd8/\x82x\xf6\xd8\xf7J(\xae/\x11p\x1f+\xc4p\t:\xfe\xfd\xdf\xa3Y\xfa\xae4\x7f\x00\xc5\xa5\x95\xa1\xe2\x01\x00\x00\r\n0\r\n\r\n"""
    r = Response(s)
    assert r.version == '1.1'
    assert r.status == '200'
    assert r.reason == 'OK'


def test_multicookie_response():
    s = b"""HTTP/1.x 200 OK\r\nSet-Cookie: first_cookie=cookie1; path=/; domain=.example.com\r\nSet-Cookie: second_cookie=cookie2; path=/; domain=.example.com\r\nContent-Length: 0\r\n\r\n"""
    r = Response(s)
    assert type(r.headers['set-cookie']) is list
    assert len(r.headers['set-cookie']) == 2


def test_noreason_response():
    s = b"""HTTP/1.1 200 \r\n\r\n"""
    r = Response(s)
    assert r.reason == ''
    assert bytes(r) == s


def test_body_forbidden_response():
    s = b'HTTP/1.1 304 Not Modified\r\n'\
        b'Content-Type: text/css\r\n'\
        b'Last-Modified: Wed, 14 Jan 2009 16:42:11 GMT\r\n'\
        b'ETag: "3a7-496e15e3"\r\n'\
        b'Cache-Control: private, max-age=414295\r\n'\
        b'Date: Wed, 22 Sep 2010 17:55:54 GMT\r\n'\
        b'Connection: keep-alive\r\n'\
        b'Vary: Accept-Encoding\r\n\r\n'\
        b'HTTP/1.1 200 OK\r\n'\
        b'Server: Sun-ONE-Web-Server/6.1\r\n'\
        b'ntCoent-length: 257\r\n'\
        b'Content-Type: application/x-javascript\r\n'\
        b'Last-Modified: Wed, 06 Jan 2010 19:34:06 GMT\r\n'\
        b'ETag: "101-4b44e5ae"\r\n'\
        b'Accept-Ranges: bytes\r\n'\
        b'Content-Encoding: gzip\r\n'\
        b'Cache-Control: private, max-age=439726\r\n'\
        b'Date: Wed, 22 Sep 2010 17:55:54 GMT\r\n'\
        b'Connection: keep-alive\r\n'\
        b'Vary: Accept-Encoding\r\n'
    result = []
    while s:
        msg = Response(s)
        s = msg.data
        result.append(msg)

    # the second HTTP response should be an standalone message
    assert len(result) == 2


def test_request_version():
    s = b"""GET / HTTP/1.0\r\n\r\n"""
    r = Request(s)
    assert r.method == 'GET'
    assert r.uri == '/'
    assert r.version == '1.0'

    s = b"""GET /\r\n\r\n"""
    r = Request(s)
    assert r.method == 'GET'
    assert r.uri == '/'
    assert r.version == '0.9'

    s = b"""GET / CHEESE/1.0\r\n\r\n"""
    try:
        Request(s)
        assert "invalid protocol version parsed!"
    except:
        pass


def test_invalid_header():
    # valid header.
    s = b'POST /main/redirect/ab/1,295,,00.html HTTP/1.0\r\n' \
        b'Referer: http://www.email.com/login/snap/login.jhtml\r\n' \
        b'Connection: Keep-Alive\r\n' \
        b'User-Agent: Mozilla/4.75 [en] (X11; U; OpenBSD 2.8 i386; Nav)\r\n' \
        b'Host: ltd.snap.com\r\n' \
        b'Accept: image/gif, image/x-xbitmap, image/jpeg, image/pjpeg, image/png, */*\r\n' \
        b'Accept-Encoding: gzip\r\n' \
        b'Accept-Language: en\r\n' \
        b'Accept-Charset: iso-8859-1,*,utf-8\r\n' \
        b'Content-type: application/x-www-form-urlencoded\r\n' \
        b'Content-length: 61\r\n\r\n' \
        b'sn=em&mn=dtest4&pw=this+is+atest&fr=true&login=Sign+in&od=www'
    r = Request(s)
    assert r.method == 'POST'
    assert r.uri == '/main/redirect/ab/1,295,,00.html'
    assert r.body == b'sn=em&mn=dtest4&pw=this+is+atest&fr=true&login=Sign+in&od=www'
    assert r.headers['content-type'] == 'application/x-www-form-urlencoded'

    # invalid header.
    s_weird_end = b'POST /main/redirect/ab/1,295,,00.html HTTP/1.0\r\n' \
        b'Referer: http://www.email.com/login/snap/login.jhtml\r\n' \
        b'Connection: Keep-Alive\r\n' \
        b'User-Agent: Mozilla/4.75 [en] (X11; U; OpenBSD 2.8 i386; Nav)\r\n' \
        b'Host: ltd.snap.com\r\n' \
        b'Accept: image/gif, image/x-xbitmap, image/jpeg, image/pjpeg, image/png, */*\r\n' \
        b'Accept-Encoding: gzip\r\n' \
        b'Accept-Language: en\r\n' \
        b'Accept-Charset: iso-8859-1,*,utf-8\r\n' \
        b'Content-type: application/x-www-form-urlencoded\r\n' \
        b'Cookie: TrackID=1PWdcr3MO_C611BGW'
    r = Request(s_weird_end)
    assert r.method == 'POST'
    assert r.uri == '/main/redirect/ab/1,295,,00.html'
    assert r.headers['content-type'] == 'application/x-www-form-urlencoded'

    # messy header.
    s_messy_header = b'aaaaaaaaa\r\nbbbbbbbbb'
    try:
        r = Request(s_messy_header)
    except dpkt.UnpackError:
        assert True
    # If the http request is built successfully or raised exceptions
    # other than UnpackError, then return a false assertion.
    except:
        assert False
    else:
        assert False


def test_gzip_response():
    import zlib
    # valid response, compressed using gzip
    s = b'HTTP/1.0 200 OK\r\n' \
        b'Server: SimpleHTTP/0.6 Python/2.7.12\r\n' \
        b'Date: Fri, 10 Mar 2017 20:43:08 GMT\r\n' \
        b'Content-type: text/plain\r\n' \
        b'Content-Encoding: gzip\r\n' \
        b'Content-Length: 68\r\n' \
        b'Last-Modified: Fri, 10 Mar 2017 20:40:43 GMT\r\n\r\n' \
        b'\x1f\x8b\x08\x00\x00\x00\x00\x00\x02\x03\x0b\xc9\xc8,V\x00\xa2D' \
        b'\x85\xb2\xd4\xa2J\x85\xe2\xdc\xc4\x9c\x1c\x85\xb4\xcc\x9cT\x85\x92' \
        b'|\x85\x92\xd4\xe2\x12\x85\xf4\xaa\xcc\x02\x85\xa2\xd4\xe2\x82\xfc' \
        b'\xbc\xe2\xd4b=.\x00\x01(m\xad2\x00\x00\x00'
    r = Response(s)
    assert r.version == '1.0'
    assert r.status == '200'
    assert r.reason == 'OK'
    # Make a zlib compressor with the appropriate gzip options
    decompressor = zlib.decompressobj(16 + zlib.MAX_WBITS)
    body = decompressor.decompress(r.body)
    assert body.startswith(b'This is a very small file')


if __name__ == '__main__':
    # Runs all the test associated with this class/file
    test_parse_request()
    test_format_request()
    test_chunked_response()
    test_multicookie_response()
    test_noreason_response()
    test_request_version()
    test_invalid_header()
    test_body_forbidden_response()
    print('Tests Successful...')
