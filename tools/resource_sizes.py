"""Read the local ACNH Yaz0-wrapped RSTC without modifying the shared table."""
import struct
import zlib


def decompress_yaz0(data):
    assert data[:4] == b'Yaz0'
    expected = int.from_bytes(data[4:8],'big')
    output = bytearray()
    offset = 16
    while len(output) < expected:
        bits = data[offset]; offset += 1
        for shift in range(7,-1,-1):
            if len(output) == expected: break
            if bits >> shift & 1:
                output.append(data[offset]); offset += 1
            else:
                first,second = data[offset:offset+2]; offset += 2
                distance = ((first & 15) << 8 | second) + 1
                length = first >> 4
                if length:
                    length += 2
                else:
                    length = data[offset] + 18; offset += 1
                assert distance <= len(output) and len(output)+length <= expected
                for _ in range(length): output.append(output[-distance])
    return bytes(output)


def read_rstc(data):
    raw = decompress_yaz0(data)
    assert raw[:4] == b'RSTC'
    count,names = struct.unpack_from('<II',raw,4)
    assert len(raw) == 12 + count*12 + names*136
    return raw,count,names


def allocation(raw,count,path):
    key = zlib.crc32(path.encode('ascii'))
    records = [struct.unpack_from('<III',raw,12+i*12) for i in range(count)]
    return next((size for crc,size,flags in records if crc == key),None)
