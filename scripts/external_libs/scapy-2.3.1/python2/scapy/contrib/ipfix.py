"""
IPFIX protocol a.k.a. NetFlow Version 10

References:
https://tools.ietf.org/html/rfc5101
https://tools.ietf.org/html/rfc6759
https://www.iana.org/assignments/ipfix/ipfix.xhtml
https://www.cisco.com/c/en/us/td/docs/routers/access/ISRG2/AVC/api/guide/AVC_Metric_Definition_Guide/5_AVC_Metric_Def.html
https://www.cisco.com/c/en/us/td/docs/routers/access/ISRG2/AVC/api/guide/AVC_Metric_Definition_Guide/avc_app_exported_fields.html

Not implemented:
Options Template Set
Paddings
Lengths auto-fill at crafting
"""

from scapy.layers.netflow import *
import os
import csv

tmpl_sets = {}
ietf_csv = 'ipfix-ietf-information-elements.csv'
cisco_csv = 'ipfix-cisco-information-elements.csv'

def _fill_from_csv(filename):
    d = {}
    file_path = os.path.abspath(os.path.join(os.path.dirname(__file__), os.pardir, os.pardir, os.pardir, filename))
    assert os.path.isfile(file_path), 'CSV path does not exist: %s' % file_path
    with open(file_path, 'r') as csvfile:
        for i, row in enumerate(csv.reader(csvfile)):
            assert type(row) is list and len(row) == 2 and row[1] and row[0].isdigit(), 'Bad CSV %s, row number %s' % (filename, i)
            d[int(row[0]) % 0x8000] = row[1]
    return d

ietf_field_type_names = _fill_from_csv(ietf_csv)
cisco_field_type_names = _fill_from_csv(cisco_csv)


class _NetflowV10MessageHeader(Packet):
    fields_desc = [
            ShortField('version', 10),
            ShortField('length', 0),
            IntField('export_time', 0),
            IntField('sequence_number',0),
            IntField('domain_id', 0) ]


class NetflowV10Set(Raw):
    _set_types = {}
    @classmethod
    def register_variant(cls):
        if cls.__name__ == 'NetflowV10Set':
            return
        cls._set_types[cls.set_id.default] = cls


    @classmethod
    def dispatch_hook(cls, pkt=None, *a, **k):
        if pkt and len(pkt) >= 4:
            set_id = str2int(pkt[:2])
            if set_id > 255:
                return generate_data_record(set_id, pkt, *a, **k)
            return cls._set_types.get(set_id, cls)
        return cls


class _NetflowV10TemplateRecordHeader(Packet):
    fields_desc = [
            ShortField('template_id', 255),
            FieldLenField('field_count', None, count_of='field_specifiers') ]


class FieldNameByType(BitField):
    def i2repr(self, pkt, x):
        return _get_type_name(pkt.enterprise_bit, x, pkt.enterprise_number)


class NetflowV10FieldSpecifier(Packet):
    name = 'Netflow V10 Field Specifier'
    fields_desc = [
            BitField('enterprise_bit', 0, 1),
            FieldNameByType('information_element_identifier', 0, 15),
            ShortField('field_length', 0),
            ConditionalField(IntField('enterprise_number', None), lambda pkt:pkt.enterprise_bit) ]

    def extract_padding(self, s):
        return '', s


class NetflowV10TemplateRecord(Packet):
    fields_desc = [
            _NetflowV10TemplateRecordHeader,
            PacketListField('field_specifiers', [], NetflowV10FieldSpecifier, count_from = lambda p:p.field_count) ]

    def extract_padding(self, s):
        return '', s

    def post_dissect(self, *a, **k):
        tmpl_sets[self.template_id] = [field_desc.fields for field_desc in  self.field_specifiers]
        return Packet.post_dissect(self, *a, **k)


class _NetflowV10SetHeader(Packet):
    fields_desc = [ 
            ShortField('set_id', None),
            FieldLenField('length', None, length_of = 'records', adjust = lambda pkt,x:x+4) ]


class NetflowV10SetTemplate(NetflowV10Set):
    name = 'Netflow V10 Template set'
    set_id = 2
    fields_desc = [
            _NetflowV10SetHeader,
            PacketListField('records', [], NetflowV10TemplateRecord, length_from = lambda pkt: pkt.length-4) ]

    def extract_padding(self, s):
        return '', s


class NetflowV10OptionRecord(Raw): pass


class NetflowV10SetOption(NetflowV10Set):
    name = 'Netflow V10 Option template set'
    set_id = 3
    fields_desc = [
            _NetflowV10SetHeader,
            PacketListField('records', [], NetflowV10OptionRecord, length_from = lambda pkt: pkt.length-4) ]

    def extract_padding(self, s):
        return '', s


class NetflowV10SetDataRaw(Packet):
    name = 'Netflow V10 Data set (no template)'
    fields_desc = [
            _NetflowV10SetHeader,
            StrFixedLenField('raw', None, length_from = lambda pkt: pkt.length-4) ]

    def extract_padding(self, s):
        return '', s


def get_var_length(pkt_buf):
    return length


class VariableLengthField(StrField):
    def getfield(self, pkt, s):
        if len(s) < 1:
            raise Exception('Variable length field does not have enough bytes! Must contain at least 1')
        length = str2int(s[:1]) # common case
        if length < 255:
            return s[1+length:], s[1:1+length]

        if len(s) < 4:
            raise Exception('Variable length field does not have enough bytes! Must contain at least 4')
        length = str2int(s[1:4]) # rare case
        return s[4+length:], s[4:4+length]


def _get_type_name(enter_bit, field_type, enter_num):
    if enter_bit:
        if enter_num == 9: # Cisco
            field_name = cisco_field_type_names.get(field_type)
            assert field_name, 'Missing field type %s in %s' % (field_type, cisco_csv)
            return field_name
        raise Exception('No support for enterprise number %s' % enter_num)
    field_name = ietf_field_type_names.get(field_type)
    assert field_name, 'Missing field type %s in %s' % (field_type, ietf_csv)
    return field_name


def _get_field_desc(tmpl_set):
    _fields_desc = []
    for index, desc in enumerate(tmpl_set):
        type_name = _get_type_name(desc['enterprise_bit'], desc['information_element_identifier'], desc['enterprise_number'])
        #name = 'Field %s/%s, %s ' % (index, len(tmpl_set), type_name)
        length = desc['field_length']
        if length == 65535: # Variable length field
            field = VariableLengthField(type_name, '')
        else:
            field = StrFixedLenField(type_name, '', length)
        _fields_desc.append(field)
    return _fields_desc


def generate_data_record(set_id, pkt, *a, **k):
    tmpl_set = tmpl_sets.get(set_id)
    if not tmpl_set: # we did not see template
        return NetflowV10SetDataRaw

    _fields_desc = _get_field_desc(tmpl_set)
    class NetflowV10DataRecord(Packet):
        fields_desc = _fields_desc

        def extract_padding(self, s):
            return '', s

    class NetflowV10SetData(NetflowV10SetDataRaw):
        name = 'Netflow V10 Data set (ID %s)' % set_id
        fields_desc = [
                _NetflowV10SetHeader,
                PacketListField('records', [], NetflowV10DataRecord, length_from = lambda pkt: pkt.length-4) ]

    return NetflowV10SetData


class NetflowV10(Packet):
    name = 'Netflow V10'
    fields_desc = [
            _NetflowV10MessageHeader,
            PacketListField('sets', [], NetflowV10Set) ]




bind_layers( NetflowHeader, NetflowV10, version=10 )

