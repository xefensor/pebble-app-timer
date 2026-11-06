'use strict';
const fs = require('node:fs');
const path = require('node:path');
const html = require('../src/pkjs/settings-page').config();
const target = path.resolve(__dirname, '../config.html');
if (process.argv.includes('--check')) {
  if (fs.readFileSync(target, 'utf8') !== html) throw Error('config.html is stale; run node scripts/generate-config.cjs');
  console.log('Shared configuration template: PASS');
} else fs.writeFileSync(target, html);
