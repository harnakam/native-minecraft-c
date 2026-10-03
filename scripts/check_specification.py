"""Check public Markdown consistency separately from specification completeness."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import sys
from urllib.parse import unquote

ROOT = Path(__file__).resolve().parents[1]
SPEC = ROOT / 'docs/specification'
REQUIRED = {'README.md', '01-runtime.md', '02-world-storage.md', '03-network.md',
            '04-gameplay.md', '05-client-resources.md', '06-conformance.md',
            '07-material-block-state.md', 'coverage.md'}
COUNTS = {'namedTypes', 'anonymousBodies', 'fields', 'methods', 'constructors'}
DOMAIN_IDS = {'RUNTIME', 'WORLD', 'NETWORK', 'GAMEPLAY', 'SERVER', 'CLIENT', 'INTEGRATIONS'}
FIXED_COUNTS = {'namedTypes': 2067, 'anonymousBodies': 344, 'fields': 7446,
                'methods': 15232, 'constructors': 2006}
FIXED_DOMAINS = {'RUNTIME': 89, 'WORLD': 215, 'NETWORK': 139, 'GAMEPLAY': 563,
                 'SERVER': 97, 'CLIENT': 476, 'INTEGRATIONS': 34}
DEPENDENCY_IDS = {'JDK8', 'Guava17', 'Gson224', 'Netty4023', 'Authlib1521',
                  'GraphicsAudioPlatform', 'RealmsTwitchServices'}


def integer(value):
    return type(value) is int and value >= 0


def packet_rows(text):
    rows = []
    play_direction = None
    for line in text.splitlines():
        if line.startswith('## 5. PLAY serverbound '):
            play_direction = 'SB'
        elif line.startswith('## 6. PLAY clientbound '):
            play_direction = 'CB'
        full = re.match(r'^\| (HANDSHAKING|STATUS|LOGIN) \| (SB|CB) \| (0x[0-9A-Fa-f]+) \| ([^|]+) \| (.+) \|$', line)
        play = re.match(r'^\| (0x[0-9A-Fa-f]+) \| ([^|]+) \| (.+) \|$', line)
        if full:
            state, direction, number, name, payload = full.groups()
        elif play and play_direction:
            number, name, payload = play.groups()
            state, direction = 'PLAY', play_direction
        else:
            continue
        rows.append((state, direction, int(number, 16), name.strip().strip('`')))
    return rows


def source_packets(text):
    result = []
    state = None
    counters = {}
    for line in text.splitlines():
        marker = re.match(r'^    (HANDSHAKING|PLAY|STATUS|LOGIN)\(-?\d+\)', line)
        if marker:
            state = marker.group(1)
        call = re.search(r'registerPacket\(EnumPacketDirection\.(CLIENTBOUND|SERVERBOUND), ([\w.]+)\.class\)', line)
        if call and state:
            direction = 'CB' if call.group(1) == 'CLIENTBOUND' else 'SB'
            key = (state, direction)
            number = counters.get(key, 0)
            counters[key] = number + 1
            result.append((state, direction, number, call.group(2)))
    return result


def inspect(spec_dir=SPEC, source_root=None):
    errors = []
    docs = {}
    for name in sorted(REQUIRED):
        path = spec_dir / name
        try:
            docs[name] = path.read_text(encoding='utf-8')
        except (OSError, UnicodeError) as exc:
            errors.append(f'{name}: {exc}')
            continue
        if '\x00' in docs[name] or '\ufffd' in docs[name]:
            errors.append(f'{name}: NUL/replacement character')
        if not docs[name].startswith('# '):
            errors.append(f'{name}: missing title')
        # Fences are removed before reading normal Markdown links.
        visible = re.sub(r'^```[^\n]*\n.*?^```\s*$', '', docs[name], flags=re.M | re.S)
        for destination in re.findall(r'\[[^\]\n]*\]\(([^)\n]+)\)', visible):
            target = destination.strip().split(' "', 1)[0].strip('<>')
            if re.match(r'^[A-Za-z][A-Za-z0-9+.-]*:', target) or target.startswith('#'):
                continue
            relative = unquote(target.split('#', 1)[0])
            if relative and not (path.parent / relative).is_file():
                errors.append(f'{name}: missing local link {relative}')
    section_ids = [key for text in docs.values() for key in re.findall(r'^## ((?:RT|CL|CF|BS|WS)-\d+)\b', text, re.M)]
    if len(section_ids) != len(set(section_ids)):
        errors.append('duplicate specification section IDs')
    packets = packet_rows(docs.get('03-network.md', ''))
    expected_counts = {('HANDSHAKING', 'SB'): 1, ('STATUS', 'SB'): 2, ('STATUS', 'CB'): 2,
                       ('LOGIN', 'SB'): 2, ('LOGIN', 'CB'): 4, ('PLAY', 'SB'): 26, ('PLAY', 'CB'): 74}
    if len(packets) != 111 or len({p[:3] for p in packets}) != 111:
        errors.append('network: missing/duplicate packet registration rows')
    for key, count in expected_counts.items():
        if sorted(p[2] for p in packets if p[:2] == key) != list(range(count)):
            errors.append(f'network: noncontiguous/wrong registration count {key}')
    body = docs.get('coverage.md', '')
    match = re.search(r'<!-- coverage:start -->\s*```json\s*\n(.*?)\n```\s*<!-- coverage:end -->', body, re.S)
    if not match:
        return errors + ['coverage.md: missing machine-readable coverage'], None, None
    try:
        data = json.loads(match.group(1))
    except json.JSONDecodeError as exc:
        return errors + [f'coverage.md: {exc}'], None, None
    if not isinstance(data, dict):
        return errors + ['coverage data must be an object'], None, None
    if data.get('schemaVersion') != 1 or type(data.get('complete')) is not bool:
        errors.append('invalid schemaVersion/complete')
    # Schema1 deliberately describes a draft, not a certified member ledger.
    # No count, flag or fabricated review can promote it to full compatibility.
    if data.get('complete') is True:
        errors.append('schema1 is draft-only; complete=true requires a new closed binding/review schema')
    if (data.get('sourceJavaFiles'), data.get('sourceBytes')) != (1613, 10283211):
        errors.append('schema1 source profile census was changed')
    if type(data.get('syntheticAndInitializerCoverageComplete')) is not bool:
        errors.append('synthetic/initializer coverage must be boolean')
    if not integer(data.get('sourceJavaFiles')) or not integer(data.get('sourceBytes')):
        errors.append('invalid source census')
    if not re.fullmatch(r'[0-9a-f]{64}', str(data.get('sourceFingerprintSHA256', ''))):
        errors.append('invalid fingerprint')
    census = data.get('declarationCensus', {})
    if not isinstance(census, dict):
        return errors + ['declaration census must be an object'], data, None
    if set(census) != COUNTS or not all(integer(v) for v in census.values()):
        errors.append('invalid declaration census')
    elif census != FIXED_COUNTS:
        errors.append('schema1 declaration profile census was changed')
    domains = data.get('domains', [])
    if not isinstance(domains, list) or not all(isinstance(d, dict) for d in domains):
        return errors + ['source domains must be object entries'], data, None
    if not isinstance(domains, list) or {d.get('id') for d in domains} != DOMAIN_IDS or len(domains) != len(DOMAIN_IDS):
        errors.append('missing/duplicate source domains')
    elif not all(integer(d.get('sourceFiles')) for d in domains) or sum(d['sourceFiles'] for d in domains) != data.get('sourceJavaFiles'):
        errors.append('source domain totals disagree')
    elif {d['id']: d['sourceFiles'] for d in domains} != FIXED_DOMAINS:
        errors.append('schema1 source domain census was changed')
    dependencies = data.get('externalDependencies', [])
    if not isinstance(dependencies, list) or not all(isinstance(d, dict) for d in dependencies):
        return errors + ['dependencies must be object entries'], data, None
    if not isinstance(dependencies, list) or len({d.get('id') for d in dependencies}) != len(dependencies):
        errors.append('invalid/duplicate dependency IDs')
    elif {d.get('id') for d in dependencies} != DEPENDENCY_IDS:
        errors.append('missing schema1 required dependency domains')
    for domain in domains:
        if domain.get('document') not in REQUIRED:
            errors.append(f"{domain.get('id')}: invalid document binding")
    for entry in domains + dependencies:
        gaps = entry.get('gaps')
        if entry.get('status') not in {'partial', 'specified'} or not isinstance(gaps, list) or not all(isinstance(g, str) and g.strip() for g in gaps):
            errors.append(f"{entry.get('id')}: invalid status/gaps")
        elif entry['status'] == 'partial' and not gaps:
            errors.append(f"{entry.get('id')}: partial without concrete gap")
        elif entry['status'] == 'specified' and gaps:
            errors.append(f"{entry.get('id')}: specified with unresolved gaps")
    contracts = data.get('memberContracts')
    if not isinstance(contracts, list):
        errors.append('memberContracts must be a list')
        contracts = []
    ids = []
    for entry in contracts:
        if not isinstance(entry, dict) or not all(k in entry for k in ('id', 'document', 'anchor', 'cases', 'dependencies', 'evidence')) or not entry.get('id') or not entry.get('cases'):
            errors.append('member contract without required bindings')
            continue
        ids.append(entry['id'])
        if entry['document'] not in REQUIRED:
            errors.append(f"{entry['id']}: invalid document")
    if len(ids) != len(set(ids)):
        errors.append('duplicate member contract IDs')
    if data.get('explicitScopeExclusions'):
        errors.append('full profile may not silently exclude source domains')
    source_result = None
    if source_root is not None:
        source_root = Path(source_root)
        if not source_root.is_dir():
            errors.append('source root does not exist')
        else:
            files = sorted(source_root.rglob('*.java'), key=lambda file: file.relative_to(source_root).as_posix().encode('utf-8'))
            digest = hashlib.sha256()
            total_bytes = 0
            for file in files:
                raw = file.read_bytes()
                total_bytes += len(raw)
                relative = file.relative_to(source_root).as_posix().encode('utf-8')
                digest.update(relative + b'\0' + hashlib.sha256(raw).hexdigest().encode('ascii') + b'\n')
            source_result = {'javaFiles': len(files), 'sourceBytes': total_bytes, 'fingerprint': digest.hexdigest()}
            if (len(files), total_bytes, digest.hexdigest()) != (data.get('sourceJavaFiles'), data.get('sourceBytes'), data.get('sourceFingerprintSHA256')):
                errors.append('source census/fingerprint mismatch')
            registry = source_root / 'net/minecraft/network/EnumConnectionState.java'
            if not registry.is_file():
                errors.append('source packet registry is missing')
            elif sorted(source_packets(registry.read_text(encoding='utf-8'))) != sorted(packets):
                errors.append('source packet registration names/IDs differ from document')
    return errors, data, source_result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source-root', type=Path, help='Optional local supplied Java source; never exported')
    parser.add_argument('--require-complete', action='store_true', help='Exit 2 while the full specification is incomplete')
    args = parser.parse_args()
    errors, data, source = inspect(source_root=args.source_root)
    if errors:
        print('Specification audit FAILED:\n' + '\n'.join(errors), file=sys.stderr)
        return 1
    summary = {'documents': len(REQUIRED), 'consistency': 'passed', 'complete': data['complete'],
               'sourceJavaFiles': data['sourceJavaFiles'], 'boundMemberContracts': len(data['memberContracts']),
               'incompleteDomains': [d['id'] for d in data['domains'] if d['status'] != 'specified'],
               'packetRegistrations': 111, 'sourceVerified': source is not None}
    print(json.dumps(summary, ensure_ascii=False))
    if args.require_complete and not data['complete']:
        print('Specification is incomplete; document consistency is not a completeness proof.', file=sys.stderr)
        return 2
    return 0


if __name__ == '__main__':
    sys.exit(main())
