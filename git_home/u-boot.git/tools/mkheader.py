#
# Copyright (c) 2013 The Linux Foundation. All rights reserved.
#

import sys
import struct

def create_header(base, size):
    """Returns a packed MBN header image with the specified base and size.

    @arg base: integer, specifies the image load address in RAM
    @arg size: integer, specifies the size of the image
    @returns: string, the MBN header
    """

    # We currently do not support appending certificates. Signing GPL
    # code might violate the GPL. So U-Boot will never be signed. So
    # this is not required for U-Boot.

    header = [
        0x5,         # Type: APPSBL
        0x3,         # Version: 3
        0x0,         # Image source pointer
        base,        # Image destination pointer
        size,        # Code Size + Cert Size + Signature Size
        size,        # Code Size
        base + size, # Destination + Code Size
        0x0,         # Signature Size
        base + size, # Destination + Code Size + Signature Size
        0x0,         # Cert Size
    ]

    header_packed = struct.pack('<10I', *header)
    return header_packed

def mkheader(base_addr, infname, outfname):
    """Prepends the image with the MBN header.

    @arg base_addr: integer, specifies the image load address in RAM
    @arg infname: string, image filename
    @arg outfname: string, output image with header prepended
    @raises IOError: if reading/writing input/output file fails
    """
    with open(infname, "rb") as infp:
        image = infp.read()
        insize = len(image)

    if base_addr > 0xFFFFFFFF:
        raise ValueError("invalid base address")

    if base_addr + insize > 0xFFFFFFFF:
        raise ValueError("invalid destination range")

    header = create_header(base_addr, insize)
    with open(outfname, "wb") as outfp:
        outfp.write(header)
        outfp.write(image)

def usage(msg=None):
    """Print command usage.

    @arg msg: string, error message if any (default: None)
    """
    if msg != None:
        sys.stderr.write("mkheader: %s\n" % msg)

    print "Usage: mkheader.py <base-addr> <input-file> <output-file>"

    if msg != None:
        exit(1)

def main():
    """Main entry function"""

    if len(sys.argv) != 4:
        usage("incorrect no. of arguments")

    try:
        base_addr = int(sys.argv[1], 0)
        infname = sys.argv[2]
        outfname = sys.argv[3]
    except ValueError as e:
        sys.stderr.write("mkheader: invalid base address '%s'\n" % sys.argv[1])
        exit(1)

    try:
        mkheader(base_addr, infname, outfname)
    except IOError as e:
        sys.stderr.write("mkheader: %s\n" % e)
        exit(1)
    except ValueError as e:
        sys.stderr.write("mkheader: %s\n" % e)
        exit(1)

if __name__ == "__main__":
    main()

