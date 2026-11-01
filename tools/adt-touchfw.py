#!/usr/bin/env python3
"""Add the Novatek touch firmware to an Apple Device Tree as a /sweet-touch node.

  adt-touchfw.py IN.adt OUT.adt FIRMWARE.bin

The Poco X3 NFC's NT36672C has no firmware of its own: Heartwood's surya touch driver (patch 0222)
downloads it over SPI at boot, before any root filesystem exists, so the blob rides in the ADT as
property nvt-firmware (the Huaxing panel's vendor/firmware/novatek_ts_huaxing_fw.bin). It is
Novatek's: keep it and the resulting ADT out of git.
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import adt  # noqa: E402


def main():
    if len(sys.argv) != 4:
        raise SystemExit(__doc__)
    inp, out, fw = sys.argv[1:]
    root, _ = adt.parse(adt.load(inp))
    root.children = [c for c in root.children if c.name != 'sweet-touch']
    n = adt.Node()
    n.props.append(('name', 0, b'sweet-touch\0'))
    n.props.append(('nvt-firmware', 0, open(fw, 'rb').read()))
    root.children.append(n)
    open(out, 'wb').write(adt.serialize(root))
    print('%s: /sweet-touch nvt-firmware (%d bytes)' % (out, len(n.props[1][2])))


if __name__ == '__main__':
    main()
