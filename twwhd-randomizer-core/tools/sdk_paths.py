"""Locate the separately maintained port SDK/tool source checkout explicitly."""
import os
from pathlib import Path


def port_source():
    selected = os.environ.get('WWHD_PORT_SOURCE')
    if not selected:
        raise ValueError('Set WWHD_PORT_SOURCE to the WWHD Recomp source checkout containing the required SDK/tools')
    root = Path(selected).expanduser().resolve()
    required = ('tools/rpx.py', 'tools/recomp/randomizer_compat.py',
                'tools/guestmod/build_guest_mod.py', 'runtime/guest/include/wwhd_guest.h',
                'runtime/include/wwhd_guest_abi.h')
    missing = [name for name in required if not (root/name).is_file()]
    if missing:
        raise ValueError('WWHD_PORT_SOURCE is not a compatible port source checkout; missing: ' + ', '.join(missing))
    return root


def add_port_tools():
    import sys
    root = port_source()
    sys.path[:0] = [str(Path(__file__).resolve().parent), str(root/'tools/recomp'), str(root/'tools')]
    return root
