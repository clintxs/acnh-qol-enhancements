"""Generate ACNH-specific NPDM configuration from the user's extracted files."""
import argparse
import json
from pathlib import Path
import struct
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
EXPECTED_BUILD = 'FF1D1C05670DB6021C85B624A710B963'
EXTRA_SYSCALLS = {'svcCreateSession': '0x40', 'svcReplyAndReceive': '0x43',
                 'svcMapProcessMemory': '0x74', 'svcUnmapProcessMemory': '0x75'}

def configure(original, header):
    result = json.loads(json.dumps(original))
    assert int(result['title_id'], 16) == 0x01006F8002326000
    assert header[:4] == b'META'
    # The older hactool JSON exporter omits these original META fields.
    result['system_resource_size'] = hex(struct.unpack_from('<I', header, 0x14)[0])
    result['signature_key_generation'] = struct.unpack_from('<I', header, 4)[0]
    for suffix in ('', '_range_min', '_range_max'):
        result['program_id' + suffix] = result['title_id' + suffix]
    for name, bit in [('optimize_memory_allocation', 4),
                      ('disable_device_address_space_merge', 5),
                      ('enable_alias_region_extra_size', 6), ('prevent_code_reads', 7)]:
        result[name] = bool(header[0xc] & (1 << bit))
    for capability in result['kernel_capabilities']:
        if capability['type'] == 'debug_flags':
            capability['value']['force_debug_prod'] = False
    syscalls = next(c['value'] for c in result['kernel_capabilities'] if c['type'] == 'syscalls')
    syscalls.update(EXTRA_SYSCALLS)
    return result

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('exefs', type=Path)
    parser.add_argument('--hactool', required=True, type=Path)
    args = parser.parse_args()
    with (args.exefs / 'main').open('rb') as source:
        header = source.read(0x100)
    if header[:4] != b'NSO0' or header[0x40:0x50].hex().upper() != EXPECTED_BUILD:
        raise SystemExit('Unsupported main build ID; configuration not changed.')
    with tempfile.TemporaryDirectory(prefix='optimized-ui-npdm-') as temporary:
        output = Path(temporary) / 'original.json'
        # Explicit empty keyset: NPDM is plaintext and needs no console keys.
        subprocess.run([str(args.hactool.resolve()), '-k', '/dev/null', '-t', 'npdm',
                        f'--json={output}', str(args.exefs / 'main.npdm')],
                       check=True, stdout=subprocess.DEVNULL)
        original = json.loads(output.read_text())
    updated = configure(original, (args.exefs / 'main.npdm').read_bytes())
    (ROOT / 'config.json').write_text(json.dumps(updated, indent=2) + '\n')
    print('Configured ACNH 3.0.3; preserved services, filesystem access and META flags.')
    print('Added only four Exlaunch mapping/process-handle syscall permissions.')

if __name__ == '__main__':
    main()
