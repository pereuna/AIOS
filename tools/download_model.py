"""Download and verify one file from the pinned model manifest."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--directory', type=Path, required=True)
    parser.add_argument('--base-url', required=True)
    parser.add_argument('--file', required=True)
    args = parser.parse_args()
    manifest = json.loads(Path('model.json').read_text())
    expected = manifest['source_sha256'][args.file]
    args.directory.mkdir(parents=True, exist_ok=True)
    target = args.directory / args.file

    def valid(path):
        if not path.exists():
            return False
        with path.open('rb') as source:
            return hashlib.file_digest(source, 'sha256').hexdigest() == expected

    if valid(target):
        target.touch()
        return
    part = target.with_name(target.name + '.part')
    subprocess.run(['curl', '--fail', '--location', '--retry', '3',
                    '--continue-at', '-', '--output', str(part),
                    args.base_url + '/' + args.file], check=True)
    if not valid(part):
        part.unlink()
        raise SystemExit(f'{args.file}: SHA-256 mismatch')
    part.replace(target)


if __name__ == '__main__':
    main()
