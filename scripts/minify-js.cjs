'use strict';

// Release-only whitespace/local-name reduction. Keep compression disabled:
// settings functions are serialized with Function#toString, so moving expressions
// across their lexical boundaries is inappropriate even when ordinary JS agrees.
const fs = require('node:fs');
const path = require('node:path');
const terser = require('terser');
const version = require('terser/package.json').version;
if (version !== '5.46.0') throw Error('Install the locked Terser 5.46.0 build dependency.');

async function optimize(code, inputMap, filename) {
  const result = await terser.minify({[filename]: code}, {
    ecma: 5,
    compress: false,
    mangle: {eval: false, keep_fnames: true},
    keep_fnames: true,
    format: {comments: /@license|@preserve|copyright/i, ascii_only: true},
    sourceMap: {content: inputMap || undefined, filename, includeSources: true}
  });
  if (!result.code || !result.map) throw Error('Minifier did not produce code and a debug map.');
  if (/\/\/[#@]\s*sourceMappingURL=/.test(result.code)) throw Error('Release code must not reference an absent map.');
  return {code: result.code + '\n', map: result.map + '\n'};
}

async function main(argv) {
  const [input, output, outputMap, inputMap] = argv;
  if (!input || !output || !outputMap || argv.length > 4) {
    throw Error('Usage: node scripts/minify-js.cjs INPUT OUTPUT OUTPUT_MAP [INPUT_MAP]');
  }
  const result = await optimize(fs.readFileSync(input, 'utf8'),
    inputMap ? fs.readFileSync(inputMap, 'utf8') : null, path.basename(output));
  for (const destination of [output, outputMap]) fs.mkdirSync(path.dirname(destination), {recursive: true});
  fs.writeFileSync(output, result.code); fs.writeFileSync(outputMap, result.map);
}

module.exports = {optimize, version};
if (require.main === module) main(process.argv.slice(2)).catch(error => {console.error(error); process.exitCode = 1;});
