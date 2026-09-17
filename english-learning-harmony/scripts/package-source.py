"""Create a source-only delivery archive; never describe it as an installable HAP."""
import json
from pathlib import Path
import zipfile

root = Path(__file__).resolve().parents[1]
profile = json.loads((root / 'build-profile.json5').read_text(encoding='utf-8-sig'))
if profile['app'].get('signingConfigs'):
    raise SystemExit('Signing configuration detected. Remove private signing data before creating a source archive.')
output = root.parents[1] / 'deliverables'
output.mkdir(exist_ok=True)
archive = output / 'daily-english-harmony-source.zip'
excluded = {'.git', '.idea', '.hvigor', '.typecheck', 'node_modules', 'oh_modules', 'build', 'dist', 'signing', '__pycache__'}
with zipfile.ZipFile(archive, 'w', zipfile.ZIP_DEFLATED, compresslevel=6) as bundle:
    for file in sorted(root.rglob('*')):
        relative = file.relative_to(root)
        if not file.is_file() or any(part in excluded for part in relative.parts):
            continue
        if file.name == 'local.properties' or file.suffix in {'.p12', '.p7b', '.cer', '.hap'}:
            continue
        bundle.write(file, Path('daily-english-harmony/english-learning-harmony') / relative)
    bundle.write(root.parent / 'daily-english-vocabulary-history.json', 'daily-english-harmony/daily-english-vocabulary-history.json')
print(f'Source archive (NOT an installable app): {archive} ({archive.stat().st_size:,} bytes)')
