// Rebuilds third_party/pele/pele.js from the pele package in node_modules, for Qt's JavaScript
// engine, which runs about ES2016: newer syntax is lowered by esbuild, missing built-ins come
// from prelude.js, and the regular expressions it cannot read are rewritten.
// Run with `npm run vendor:pele` after changing pele's version in package.json.

import { execFileSync } from 'node:child_process';
import { readFileSync, writeFileSync, copyFileSync } from 'node:fs';
import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';

const root = join(dirname(fileURLToPath(import.meta.url)), '..');
const out = join(root, 'third_party', 'pele');
const pkg = join(root, 'node_modules', 'pele');
const { version } = JSON.parse(readFileSync(join(pkg, 'package.json'), 'utf8'));

let code = execFileSync(
  join(root, 'node_modules', '.bin', 'esbuild'),
  [
    join(pkg, 'dist', 'pele.global.js'),
    '--target=es2017',
    '--minify-syntax',
    '--minify-whitespace',
    '--charset=ascii',
    '--log-level=warning',
  ],
  { encoding: 'utf8', maxBuffer: 1 << 26 },
);

// Each rewrite expects an exact count, so a new pele that changes them fails here rather
// than drawing wrongly.
function rewrite(pattern, replace, expected, what) {
  let count = 0;
  code = code.replace(pattern, (...match) => {
    count++;
    return replace(...match);
  });
  if (count !== expected) throw new Error(`${what}: expected ${expected}, found ${count}`);
}

// Unicode property escapes do not read; letters and numbers are spelled out as ranges.
function ranges(test) {
  const found = [];
  let start = -1;
  for (let c = 0; c <= 0x110000; c++) {
    const hit = c < 0x110000 && test.test(String.fromCodePoint(c));
    if (hit && start < 0) start = c;
    if (!hit && start >= 0) {
      found.push(start === c - 1 ? [start] : [start, c - 1]);
      start = -1;
    }
  }
  const hex = (c) => `\\u{${c.toString(16)}}`;
  return found.map((r) => r.map(hex).join('-')).join('');
}
const classes = { L: ranges(/^\p{L}$/u), N: ranges(/^\p{N}$/u) };
rewrite(/\\p\{([LN])\}/g, (_, name) => classes[name], 4, 'property escapes');
if (/\\[pP]\{/.test(code)) throw new Error('a property escape is left');

// The u flag does not pair surrogates here, so a range of them would strip both halves of
// every emoji. Text from Jot is decoded UTF-8, which never holds a lone surrogate.
rewrite(/\\u[dD]800-\\u[dD][fF]{3}/g, () => '', 5, 'surrogate ranges');

const prelude = readFileSync(join(out, 'prelude.js'), 'utf8');
writeFileSync(join(out, 'pele.js'), `// Pele ${version}, made by tools/vendor-pele.mjs. Do not edit.\n${prelude}${code}`);
copyFileSync(join(pkg, 'LICENSE'), join(out, 'LICENSE'));
console.log(`third_party/pele/pele.js: pele ${version}`);
