#!/usr/bin/env python

import os, os.path
import email
from pprint import pprint

outfile = 'INDEX'

def main(args):
    results = dict()
    for root, dirs, files in os.walk('.'):
        print 'Scanning "%s"...' % (root)
        # ignore hidden dirs like '.bzr' and '.git'
        for d in dirs:
            if d.startswith('.'):
                dirs.remove(d)
        if 'meta' in files:
            path = os.path.join(root, 'meta')
            parse(path, results)

    lines = summarize(results)
    print '=========='
    print '\n'.join(lines)

def parse(path, results):
    print 'Parsing "%s"...' % (path)
    msg = email.message_from_file(open(path))
    pprint(msg.items())

    path = os.path.dirname(path)
    if path.startswith('./'):
        path = path[2:]

    for k,v in msg.items():
        if k.strip() in ('', 'Description', ):
            continue
        for v2 in v.split(', '):
            if v2.strip() in ('', ):
                continue
            key = (k,v2)
            if key not in results:
                results[key] = []
            results[key].append(path)

def summarize(results):
    lines = []
    keys = results.keys()
    keys.sort()
    prev_lk = ''
    for lk,rk in keys:
        if lk != prev_lk:
            lines.append('')
            lines.append('%s' % (lk))
            lines.append('')
        prev_lk = lk
        lines.append('    %s:' % (rk))
        for v in results[(lk,rk)]:
            lines.append('        %s' % (v))

    return lines

if __name__ == "__main__":
    import sys
    main(sys.argv[1:])

