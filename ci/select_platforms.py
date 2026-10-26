"""Select only explicitly supported native jobs for Actions."""
import argparse
import json
import os
from pathlib import Path

PLATFORMS = [
    {'platform': 'windows', 'runner': 'windows-2022'},
    {'platform': 'macos', 'runner': 'macos-26'},
    {'platform': 'linux', 'runner': 'ubuntu-24.04'},
]


def matrix_for(selection):
    if selection not in ('all', 'windows', 'macos', 'linux'):
        raise ValueError('Unknown build platform: ' + selection)
    return {'include': [entry for entry in PLATFORMS
                        if selection == 'all' or entry['platform'] == selection]}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--event', required=True)
    parser.add_argument('--platform', default='')
    args = parser.parse_args()
    if args.event == 'workflow_dispatch':
        selection = args.platform or 'all'
    elif args.event == 'push':
        selection = (Path(__file__).parent / 'push-platform.txt').read_text().strip()
    else:
        raise ValueError('Unexpected workflow event: ' + args.event)
    matrix = matrix_for(selection)
    with open(os.environ['GITHUB_OUTPUT'], 'a') as target:
        target.write('platform=' + selection + '\n')
        target.write('matrix=' + json.dumps(matrix, separators=(',', ':')) + '\n')
    print('Native build selection:', selection)


if __name__ == '__main__':
    main()
