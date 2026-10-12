'use strict';

// Event/Git transport only. A complete delta is not a classification or a
// verification receipt; manifest-aware check policy belongs in Marx/Engels.
const fs = require('node:fs');
const path = require('node:path');
const {execFileSync} = require('node:child_process');
const {TextDecoder} = require('node:util');

const maxPaths = 2000;
const maxRecordBytes = 128 * 1024;
const commit = value => typeof value === 'string' && /^[0-9a-f]{40}$/.test(value)
  && value !== '0'.repeat(40) ? value : null;
function requireValue(ok, reason) {
  if (!ok) throw Object.assign(new Error(reason), {reason});
}
function git(root, ...args) {
  return execFileSync('git', ['--no-replace-objects', '-C', root, ...args], {
    env: {...process.env, GIT_OPTIONAL_LOCKS: '0'},
    maxBuffer: 4 * 1024 * 1024, timeout: 15000, stdio: ['ignore', 'pipe', 'pipe'],
  });
}
function gitText(root, ...args) { return git(root, ...args).toString('utf8').trim(); }

function parseDiff(data) {
  // NUL delimiters preserve whitespace, quotes, backslashes and newlines. An
  // undecodable name must not turn into a different, apparently harmless path.
  let text;
  try { text = new TextDecoder('utf-8', {fatal: true, ignoreBOM: true}).decode(data); }
  catch { throw Object.assign(new Error('non-utf8-path'), {reason: 'non-utf8-path'}); }
  if (!text) return {changes: [], paths: []};
  requireValue(text.endsWith('\0'), 'invalid-diff');
  const fields = text.slice(0, -1).split('\0');
  const changes = [];
  const paths = new Set();
  let index = 0;
  const takePath = () => {
    const name = fields[index++];
    requireValue(name && !name.startsWith('/')
      && name.split('/').every(part => part && part !== '.' && part !== '..'), 'invalid-diff-path');
    paths.add(name);
    requireValue(paths.size <= maxPaths, 'too-many-paths');
    return name;
  };
  while (index < fields.length) {
    const token = fields[index++];
    requireValue(/^(?:[ADMT]|[RC][0-9]{1,3})$/.test(token)
      && (token.length === 1 || Number(token.slice(1)) <= 100), 'unsupported-diff-status');
    const status = token[0];
    const previous = status === 'R' || status === 'C' ? takePath() : null;
    changes.push({status, ...(previous === null ? {} : {previous_path: previous}), path: takePath()});
    requireValue(changes.length <= maxPaths, 'too-many-paths');
  }
  return {changes, paths: [...paths].sort()};
}

function resolve({context, root = process.cwd()}) {
  const event = ['push', 'pull_request', 'workflow_dispatch', 'schedule'].includes(context.eventName)
    ? context.eventName : 'unsupported';
  const payload = context.payload || {};
  const result = {schema: 1, event, event_commit: commit(context.sha), checkout: null,
    base: null, head: null, comparison_base: null, complete: false,
    reason: 'unsupported-event', changes: [], paths: []};
  // Manual and scheduled runs have no trusted delta, even with a full checkout.
  if (event !== 'push' && event !== 'pull_request') {
    result.reason = event === 'schedule' ? 'scheduled-baseline'
      : event === 'workflow_dispatch' ? 'manual-baseline' : 'unsupported-event';
    return result;
  }
  let failure = 'history-unavailable';
  try {
    requireValue(result.event_commit, 'invalid-event-commit');
    result.checkout = gitText(root, 'rev-parse', '--verify', 'HEAD');
    requireValue(result.checkout === result.event_commit, 'checkout-mismatch');
    requireValue(gitText(root, 'rev-parse', '--show-prefix') === '', 'checkout-not-root');
    requireValue(gitText(root, 'rev-parse', '--is-shallow-repository') === 'false', 'shallow-history');
    requireValue(git(root, 'status', '--porcelain=v1', '-z', '--untracked-files=normal',
      '--ignore-submodules=none').length === 0, 'dirty-checkout');
    if (event === 'push') {
      requireValue(context.ref?.startsWith('refs/heads/'), 'unsupported-push-ref');
      requireValue(!payload.deleted, 'deleted-ref');
      requireValue(!payload.forced, 'rewritten-history');
      result.base = commit(payload.before);
      result.head = commit(payload.after);
      requireValue(payload.before !== '0'.repeat(40), 'zero-base');
      requireValue(result.base, 'invalid-base');
      requireValue(result.head === result.event_commit, 'push-head-mismatch');
      requireValue(gitText(root, 'rev-parse', '--verify', result.base + '^{commit}') === result.base,
        'invalid-base');
      failure = 'rewritten-history';
      git(root, 'merge-base', '--is-ancestor', result.base, result.head);
      result.comparison_base = result.base;
      result.reason = 'push-before-to-head';
    } else {
      result.base = commit(payload.pull_request?.base?.sha);
      result.head = commit(payload.pull_request?.head?.sha);
      requireValue(result.base && result.head, 'invalid-pr-commits');
      requireValue(gitText(root, 'rev-parse', '--verify', result.base + '^{commit}') === result.base
        && gitText(root, 'rev-parse', '--verify', result.head + '^{commit}') === result.head, 'invalid-pr-commits');
      if (result.checkout !== result.head) {
        const parents = gitText(root, 'show', '-s', '--format=%P', result.checkout).split(' ');
        requireValue(parents.length === 2 && parents[0] === result.base
          && parents[1] === result.head, 'pr-checkout-mismatch');
      }
      failure = 'merge-base-unavailable';
      const bases = gitText(root, 'merge-base', '--all', result.base, result.head).split('\n');
      requireValue(bases.length === 1 && commit(bases[0]), 'ambiguous-merge-base');
      result.comparison_base = bases[0];
      result.reason = 'pull-request-merge-base';
    }
    failure = 'comparison-failed';
    const diff = parseDiff(git(root, 'diff', '--no-ext-diff', '--no-textconv', '--no-relative',
      '--ignore-submodules=none', '--name-status', '-z', '--find-renames=50%',
      result.comparison_base, result.head, '--'));
    const complete = {...result, ...diff, complete: true};
    requireValue(Buffer.byteLength(JSON.stringify(complete)) <= maxRecordBytes, 'change-record-too-large');
    return complete;
  } catch (error) {
    result.reason = error.reason || failure;
    return result;
  }
}

function writeReport(root, name, value) {
  // Never follow a checkout-controlled symlink or overwrite an authored file.
  let directory = root;
  for (const part of ['.ecosystem', 'github']) {
    directory = path.join(directory, part);
    try { fs.mkdirSync(directory); }
    catch (error) { if (error.code !== 'EEXIST') throw error; }
    requireValue(fs.lstatSync(directory).isDirectory() && !fs.lstatSync(directory).isSymbolicLink(),
      'unsafe-change-report-directory');
  }
  const report = path.join(directory, name);
  fs.writeFileSync(report, JSON.stringify(value) + '\n', {flag: 'wx'});
  return report;
}

function collect({context, core, root = process.cwd()}) {
  const result = resolve({context, root});
  const serialized = JSON.stringify(result);
  const report = writeReport(root, 'changes.json', result);
  core.setOutput('changes', serialized);
  core.setOutput('changes-file', report);
  core.info(result.complete ? `Resolved ${result.paths.length} changed paths (${result.reason}).`
    : `Change set unavailable (${result.reason}); verification cannot narrow from this record.`);
  return result;
}

// The adapter transports the native policy without interpreting relevance.
// Execution or output failures fail the stage; native uncertainty returns full.
function inspect({root = process.cwd(), binary}) {
  requireValue(typeof binary === 'string' && path.isAbsolute(binary), 'missing-engels-binary');
  const output = execFileSync(binary, ['report', 'changes', '--json'], {
    cwd: root, encoding: 'utf8', maxBuffer: 4 * 1024 * 1024, timeout: 60000,
    stdio: ['ignore', 'pipe', 'pipe'],
  });
  const result = JSON.parse(output);
  requireValue(result?.schema === 1 && typeof result.project === 'string'
    && typeof result.delta_validated === 'boolean'
    && ['full', 'affected', 'presentation', 'none'].includes(result.classification)
    && ['full', 'repository'].includes(result.execution?.codeql),
  'invalid-native-classification');
  return result;
}

function classify({core, root = process.cwd(), binary}) {
  const result = inspect({root, binary});
  const report = writeReport(root, 'classification.json', result);
  core.setOutput('classification-file', report);
  core.setOutput('codeql', result.execution.codeql);
  core.info(`Change classification: ${result.classification} (${result.reason}).`);
  return result;
}

module.exports = {resolve, collect, inspect, classify};
