'use strict';

// GitHub transport/provenance only. Build and check policy stays in Marx/Engels.
const fs = require('node:fs');
const path = require('node:path');
const os = require('node:os');
const crypto = require('node:crypto');
const {execFileSync} = require('node:child_process');

const kinds = {
  checks: {workflow: '.github/workflows/tests.yml', job: 'checks', gate: 'Enforce required result',
    stages: ['01-required-checks', '02-coverage-check']},
  codeql: {workflow: '.github/workflows/codeql.yml', job: 'analyze (cpp)', gate: 'Enforce CodeQL result',
    stages: ['01-tracked-surface', '02-codeql-build']},
};
const maxBytes = 256 * 1024 * 1024;
const hash = data => crypto.createHash('sha256').update(data).digest('hex');
const equal = (a, b) => JSON.stringify(a) === JSON.stringify(b);
function requireValue(ok, message) { if (!ok) throw new Error(message); }
function git(root, ...args) {
  return execFileSync('git', ['-C', root, ...args], {encoding: 'utf8', maxBuffer: maxBytes}).trim();
}
function safePath(root, relative) {
  requireValue(relative && !relative.startsWith('/') && !relative.includes('\\')
    && relative.split('/').every(p => p && p !== '.' && p !== '..'), 'Unsafe evidence path');
  let current = root;
  for (const part of relative.split('/')) {
    current = path.join(current, part);
    try { requireValue(!fs.lstatSync(current).isSymbolicLink(), 'Evidence path uses a symlink'); }
    catch (error) { if (error.code !== 'ENOENT') throw error; }
  }
  return current;
}
function readFile(root, name, nonempty = false) {
  const file = safePath(root, name);
  const stat = fs.lstatSync(file);
  requireValue(stat.isFile() && stat.size <= maxBytes && (!nonempty || stat.size > 0), 'Invalid evidence file: ' + name);
  return fs.readFileSync(file);
}
function policy(root) {
  const configPath = path.join(root, 'manifesto.github.vars.json');
  const config = fs.existsSync(configPath) ? JSON.parse(fs.readFileSync(configPath, 'utf8')) : {};
  requireValue(!config.manifesto_setup_action || config.manifesto_setup_action === './.github/actions/setup-manifesto',
    'Evidence reuse requires the local setup action');
  const bootstrap = config.manifesto_bootstrap || 'repository';
  requireValue(bootstrap === 'checkout' || (bootstrap === 'repository'
    && /^[0-9a-f]{40}$/.test(config.manifesto_ref || '') && config.manifesto_repository),
    'Evidence reuse requires checkout bootstrap or an immutable tooling commit');
  return {
    project: JSON.parse(fs.readFileSync(path.join(root, 'manifest.json'), 'utf8')).id,
    github_tree: git(root, 'rev-parse', 'HEAD:.github'),
    manifest: git(root, 'rev-parse', 'HEAD:manifest.json'),
    variables: fs.existsSync(configPath) ? git(root, 'rev-parse', 'HEAD:manifesto.github.vars.json') : null,
    tooling: {bootstrap, repository: config.manifesto_repository || '',
      revision: config.manifesto_ref || '', source_path: config.manifesto_source_path || '.'},
  };
}
function fileNames(kind, coverage) {
  requireValue(kinds[kind], 'Unknown evidence kind');
  const names = kinds[kind].stages.flatMap(stage =>
    ['summary.md', 'output.log'].map(file => `github/reports/${stage}/${file}`));
  if (kind === 'codeql') names.push('github/reports/03-codeql-analysis/summary.md');
  if (kind === 'checks' && coverage === 'passed') names.push('reports/coverage.json');
  return names.sort();
}
function successful(env, prefix, optional = false) {
  const status = env[prefix + '_STATUS'];
  const code = env[prefix + '_EXIT_CODE'];
  requireValue(env[prefix + '_OUTCOME'] === 'success'
    && ((status === 'passed' && code === '0') || (optional && status === 'skipped' && code === '3')),
  'Incomplete native result: ' + prefix);
  return status;
}
function artifactName(kind, runId, attempt) {
  return `manifesto-evidence-${kind}-${runId}-${attempt}`;
}
function workflowMatches(run, kind) {
  const file = kinds[kind].workflow;
  if (run.path === file) return true;
  if (!run.path?.startsWith(file + '@')) return false;
  const refs = [run.head_sha, run.head_branch, run.head_branch && 'refs/heads/' + run.head_branch];
  for (const pr of run.pull_requests || [])
    refs.push(pr.base.ref, 'refs/heads/' + pr.base.ref, `refs/pull/${pr.number}/merge`);
  return refs.filter(Boolean).includes(run.path.slice(file.length + 1));
}
function writeBundle(root, receipt, destination, names) {
  requireValue(!fs.existsSync(destination), 'Evidence destination already exists');
  let total = 0;
  for (const name of names) {
    const data = readFile(path.join(root, '.ecosystem'), name, !name.endsWith('/output.log'));
    total += data.length;
    requireValue(total <= maxBytes, 'Evidence exceeds the size limit');
    if (name === 'reports/coverage.json') {
      const report = JSON.parse(data);
      requireValue(report.type === 'llvm.coverage.json.export' && Array.isArray(report.data) && report.data.length,
        'Coverage evidence is not an LLVM export');
    }
    const output = path.join(destination, name);
    fs.mkdirSync(path.dirname(output), {recursive: true});
    fs.writeFileSync(output, data, {flag: 'wx'});
    receipt.files[name] = {bytes: data.length, sha256: hash(data)};
  }
  fs.writeFileSync(path.join(destination, 'receipt.json'), JSON.stringify(receipt, null, 2) + '\n', {flag: 'wx'});
}

// Manual Pages records only its own successful repository/coverage prerequisites.
// This kind cannot authorize cross-run reuse or claim full Checks/CodeQL success.
async function presentation({context, core, root = process.cwd(), env = process.env}) {
  requireValue(context.eventName === 'workflow_dispatch'
    && context.ref === 'refs/heads/' + context.payload.repository.default_branch,
  'Manual Pages evidence requires a default-branch dispatch');
  successful(env, 'REPOSITORY');
  const coverage = successful(env, 'COVERAGE', true);
  const tested = git(root, 'rev-parse', 'HEAD');
  requireValue(tested === context.sha, 'Checkout is not the event revision');
  git(root, 'diff', '--quiet', '--ignore-submodules=none', 'HEAD');
  requireValue(!git(root, 'ls-files', '--others', '--exclude-standard'), 'Untracked authored inputs after verification');
  const attempt = Number(env.GITHUB_RUN_ATTEMPT);
  requireValue(Number.isSafeInteger(attempt) && attempt > 0, 'Missing Pages attempt');
  const receipt = {schema: 1, kind: 'pages',
    repository: {id: context.payload.repository.id, name: `${context.repo.owner}/${context.repo.repo}`},
    run_id: context.runId, run_attempt: attempt, tested_commit: tested,
    tested_tree: git(root, 'rev-parse', 'HEAD^{tree}'),
    policy: {project: JSON.parse(readFile(root, 'manifest.json', true)).id}, coverage, files: {}};
  const names = ['01-tracked-surface', '02-coverage-check'].flatMap(stage =>
    ['summary.md', 'output.log'].map(file => `github/reports/${stage}/${file}`));
  if (coverage === 'passed') names.push('reports/coverage.json');
  const destination = safePath(root, '.ecosystem/github/presentation');
  writeBundle(root, receipt, destination, names.sort());
  core.setOutput('directory', destination);
}

async function record({github, context, core, root = process.cwd(), env = process.env}) {
  const kind = env.EVIDENCE_KIND;
  requireValue(kinds[kind], 'Unknown evidence kind');
  let config;
  try { config = policy(root); }
  catch (error) { core.notice('No reusable receipt: ' + error.message); return; }
  const tested = git(root, 'rev-parse', 'HEAD');
  requireValue(tested === context.sha, 'Checkout is not the event revision');
  git(root, 'diff', '--quiet', '--ignore-submodules=none', 'HEAD');
  requireValue(!git(root, 'ls-files', '--others', '--exclude-standard'), 'Untracked authored inputs after verification');
  const tooling = git(env.TOOL_SOURCE_ROOT, 'rev-parse', 'HEAD');
  requireValue(tooling === (config.tooling.bootstrap === 'checkout' ? tested : config.tooling.revision),
    'Tooling checkout differs from the selected immutable revision');
  let coverage = null;
  if (kind === 'checks') { successful(env, 'CHECK'); coverage = successful(env, 'COVERAGE', true); }
  else {
    successful(env, 'REPOSITORY'); successful(env, 'CODEQL_BUILD');
    requireValue(env.CODEQL_INIT_STATUS === 'success' && env.CODEQL_ANALYZE_STATUS === 'success', 'Incomplete CodeQL result');
  }
  const {data: run} = await github.rest.actions.getWorkflowRun({...context.repo, run_id: context.runId});
  const attempt = Number(env.GITHUB_RUN_ATTEMPT);
  requireValue(run.id === context.runId && run.run_attempt === attempt && Number.isSafeInteger(attempt) && attempt > 0
    && workflowMatches(run, kind) && run.repository.id === context.payload.repository.id
    && run.event === context.eventName, 'Run metadata does not identify this verification');
  const pr = context.payload.pull_request;
  requireValue(run.head_sha === tested || run.head_sha === pr?.head.sha, 'Run does not identify the tested commit');
  const receipt = {
    schema: 1, kind, repository: {id: context.payload.repository.id, name: `${context.repo.owner}/${context.repo.repo}`},
    run_id: run.id, run_attempt: attempt, workflow_id: run.workflow_id, workflow: run.path, event: run.event,
    run_head: run.head_sha, tested_commit: tested, tested_tree: git(root, 'rev-parse', 'HEAD^{tree}'),
    pr: pr ? {number: pr.number, head: pr.head.sha, base: pr.base.sha, base_ref: pr.base.ref,
      head_repository: pr.head.repo.id} : null,
    policy: config, tooling_commit: tooling, coverage, files: {},
  };
  const destination = safePath(root, `.ecosystem/github/evidence/${kind}`);
  writeBundle(root, receipt, destination, fileNames(kind, coverage));
  core.setOutput('name', artifactName(kind, run.id, attempt));
  core.setOutput('directory', destination);
}

// Only known report files are extracted. Never execute code from an artifact.
const unpackProgram = `
import json, pathlib, stat, sys, zipfile
source, destination, names = sys.argv[1:]
allowed = set(json.loads(names))
limit = 256 * 1024 * 1024
with zipfile.ZipFile(source) as archive:
    entries = archive.infolist()
    if len(entries) > 64 or sum(e.file_size for e in entries) > limit:
        raise ValueError('Evidence archive exceeds limits')
    seen = set()
    for entry in entries:
        name = entry.filename
        parts = name.rstrip('/').split('/')
        if any(p in ('', '.', '..') for p in parts) or '\\\\' in name:
            raise ValueError('Unsafe archive path')
        mode = stat.S_IFMT(entry.external_attr >> 16)
        if mode not in (0, stat.S_IFREG, stat.S_IFDIR) or name in seen:
            raise ValueError('Archive alias or duplicate')
        seen.add(name)
        if entry.is_dir():
            if not any(a.startswith(name) for a in allowed):
                raise ValueError('Unexpected directory')
        elif name not in allowed or entry.file_size > limit:
            raise ValueError('Unexpected evidence file')
    for entry in entries:
        if entry.is_dir():
            continue
        output = pathlib.Path(destination, entry.filename)
        output.parent.mkdir(parents=True, exist_ok=True)
        with output.open('xb') as stream:
            stream.write(archive.read(entry))
`;

function runMatches(run, kind, repository, pr) {
  const pointer = run.pull_requests?.find(item => item.number === pr.number);
  return run.repository?.id === repository.id && workflowMatches(run, kind)
    && run.event === 'pull_request' && pointer?.head.sha === pr.head.sha
    && pointer.base.ref === repository.default_branch && pointer.base.repo?.id === repository.id;
}
function apiFor(github, context) {
  return async (route, params = {}) => (await github.request(route,
    {...context.repo, ...params, request: {timeout: 30000}})).data;
}
function checkedCheckout(root, context) {
  requireValue(git(root, 'rev-parse', 'HEAD') === context.sha, 'Checkout differs from the selected revision');
  git(root, 'diff', '--quiet', '--ignore-submodules=none', 'HEAD');
  requireValue(!git(root, 'ls-files', '--others', '--exclude-standard'), 'Untracked authored inputs after verification');
  return {tree: git(root, 'rev-parse', 'HEAD^{tree}'), config: policy(root)};
}
function completed(run, matches) {
  requireValue(matches(run) && run.status === 'completed' && run.conclusion === 'success'
    && Number.isSafeInteger(run.run_attempt) && run.run_attempt > 0, 'Incomplete or unrelated verification run');
}
function stepResult(job, name, result) {
  const steps = job.steps.filter(step => step.name === name);
  return steps.length === 1 && steps[0].status === 'completed' && steps[0].conclusion === result;
}
async function requiredJob(api, run, kind) {
  const jobs = await api('GET /repos/{owner}/{repo}/actions/runs/{run_id}/attempts/{attempt_number}/jobs',
    {run_id: run.id, attempt_number: run.run_attempt, per_page: 100});
  let required;
  for (const name of ['select', kinds[kind].job]) {
    const matching = jobs.jobs.filter(job => job.name === name);
    requireValue(matching.length === 1 && matching[0].run_id === run.id
      && matching[0].status === 'completed' && matching[0].conclusion === 'success', 'Required job did not succeed: ' + name);
    if (name === kinds[kind].job) required = matching[0];
  }
  requireValue(stepResult(required, kinds[kind].gate, 'success'), 'Required result gate did not succeed');
  return required;
}
async function unchangedRun(api, run, matches) {
  const current = await api('GET /repos/{owner}/{repo}/actions/runs/{run_id}', {run_id: run.id});
  completed(current, matches);
  requireValue(current.run_attempt === run.run_attempt && current.head_sha === run.head_sha
    && current.workflow_id === run.workflow_id, 'Run changed during verification');
}
async function verifiedRun({api, run, kind, repository, context, tree, config, temporary, matches, pr}) {
  completed(run, matches);
  const job = await requiredJob(api, run, kind);
  for (const name of ['Record reusable verification', 'Upload verification evidence'])
    requireValue(stepResult(job, name, 'success'), 'Required evidence step did not succeed: ' + name);
  const name = artifactName(kind, run.id, run.run_attempt);
  const artifacts = await api('GET /repos/{owner}/{repo}/actions/runs/{run_id}/artifacts', {run_id: run.id, per_page: 100});
  const matching = artifacts.artifacts.filter(artifact => artifact.name === name);
  requireValue(matching.length === 1, 'Missing or ambiguous evidence artifact');
  const artifact = await api('GET /repos/{owner}/{repo}/actions/artifacts/{artifact_id}', {artifact_id: matching[0].id});
  requireValue(artifact.id === matching[0].id && artifact.name === name && artifact.expired === false
    && Date.parse(artifact.expires_at) > Date.now() && artifact.size_in_bytes > 0 && artifact.size_in_bytes <= maxBytes
    && /^sha256:[0-9a-f]{64}$/.test(artifact.digest || '') && artifact.workflow_run?.id === run.id
    && artifact.workflow_run.repository_id === repository.id && artifact.workflow_run.head_sha === run.head_sha,
  'Expired or unverifiable evidence artifact');
  const zip = Buffer.from(await api('GET /repos/{owner}/{repo}/actions/artifacts/{artifact_id}/{archive_format}',
    {artifact_id: artifact.id, archive_format: 'zip'}));
  requireValue(zip.length <= maxBytes && 'sha256:' + hash(zip) === artifact.digest, 'Artifact archive digest mismatch');
  const archive = path.join(temporary, kind + '.zip');
  const directory = path.join(temporary, kind);
  fs.writeFileSync(archive, zip, {flag: 'wx'});
  const allowed = ['receipt.json', ...fileNames(kind, 'passed')];
  execFileSync('python3', ['-c', unpackProgram, archive, directory, JSON.stringify(allowed)], {stdio: 'pipe'});
  const receipt = JSON.parse(readFile(directory, 'receipt.json', true));
  requireValue(receipt.schema === 1 && receipt.kind === kind && receipt.repository?.id === repository.id
    && receipt.repository.name === `${context.repo.owner}/${context.repo.repo}` && receipt.run_id === run.id
    && receipt.run_attempt === run.run_attempt && receipt.workflow_id === run.workflow_id
    && receipt.workflow === run.path && receipt.event === run.event && receipt.run_head === run.head_sha
    && /^[0-9a-f]{40}$/.test(receipt.tested_commit || '') && receipt.tested_tree === tree
    && equal(receipt.policy, config) && receipt.tooling_commit === (config.tooling.bootstrap === 'checkout'
      ? receipt.tested_commit : config.tooling.revision), 'Receipt provenance differs from the source tree/configuration');
  if (pr) {
    const pointer = run.pull_requests.find(item => item.number === pr.number);
    requireValue(receipt.pr?.number === pr.number && receipt.pr.head === pr.head.sha && receipt.pr.base === pointer.base.sha
      && receipt.pr.base_ref === repository.default_branch && receipt.pr.head_repository === pr.head.repo.id,
    'Receipt PR identity differs');
    requireValue(run.head_sha === receipt.pr.head || run.head_sha === receipt.tested_commit, 'Unrelated run revision');
    const tested = await api('GET /repos/{owner}/{repo}/git/commits/{commit_sha}', {commit_sha: receipt.tested_commit});
    requireValue(tested.sha === receipt.tested_commit && tested.tree.sha === tree && tested.parents.length === 2
      && tested.parents[0].sha === receipt.pr.base && tested.parents[1].sha === receipt.pr.head,
    'Tested merge commit does not represent the merged source tree');
  } else {
    requireValue(run.event === 'push' && receipt.pr === null && receipt.tested_commit === context.sha
      && receipt.run_head === context.sha, 'Push receipt does not identify this exact revision');
  }
  requireValue(kind === 'checks' ? ['passed', 'skipped'].includes(receipt.coverage) : receipt.coverage === null,
    'Unknown coverage result');
  const names = fileNames(kind, receipt.coverage);
  requireValue(equal(Object.keys(receipt.files).sort(), names), 'Receipt has missing or unexpected reports');
  // The allowlist permits optional coverage; reject an unreceipted stale file too.
  requireValue(receipt.coverage === 'passed' || !fs.existsSync(path.join(directory, 'reports/coverage.json')),
    'Unreceipted coverage report');
  for (const file of names) {
    const bytes = readFile(directory, file, !file.endsWith('/output.log'));
    requireValue(receipt.files[file].bytes === bytes.length && receipt.files[file].sha256 === hash(bytes), 'Report digest mismatch');
  }
  await unchangedRun(api, run, matches);
  return {receipt, directory, source: {run_id: run.id, run_attempt: run.run_attempt,
    artifact_id: artifact.id, artifact_digest: artifact.digest}};
}
async function reusablePair({github, context, root, temporary}) {
  const repository = context.payload.repository;
  const {tree, config} = checkedCheckout(root, context);
  const api = apiFor(github, context);
  const associated = await api('GET /repos/{owner}/{repo}/commits/{commit_sha}/pulls', {commit_sha: context.sha, per_page: 100});
  const candidate = associated.find(pr => pr.merged_at && pr.merge_commit_sha === context.sha
    && pr.base?.repo?.id === repository.id && pr.base.ref === repository.default_branch);
  requireValue(candidate, 'No merged PR identifies this default-branch commit');
  const pr = await api('GET /repos/{owner}/{repo}/pulls/{pull_number}', {pull_number: candidate.number});
  requireValue(pr.merged && pr.merge_commit_sha === context.sha && pr.base.repo.id === repository.id
    && pr.base.ref === repository.default_branch, 'Merged PR identity changed');
  const pair = {};
  const runs = {};
  for (const kind of Object.keys(kinds)) {
    const listed = await api('GET /repos/{owner}/{repo}/actions/workflows/{workflow_id}/runs',
      {workflow_id: path.basename(kinds[kind].workflow), event: 'pull_request', per_page: 100});
    const matches = run => runMatches(run, kind, repository, pr);
    const candidateRun = listed.workflow_runs.find(matches);
    requireValue(candidateRun, 'No matching ' + kind + ' PR run');
    const run = await api('GET /repos/{owner}/{repo}/actions/runs/{run_id}', {run_id: candidateRun.id});
    pair[kind] = await verifiedRun({api, run, kind, repository, context, tree, config, temporary, matches, pr});
    runs[kind] = run;
  }
  requireValue(pair.checks.receipt.tested_commit === pair.codeql.receipt.tested_commit, 'Checks and CodeQL tested different revisions');
  for (const [kind, run] of Object.entries(runs))
    await unchangedRun(api, run, current => runMatches(current, kind, repository, pr));
  return {pair, tree, pr: pr.number};
}

function pushMatches(run, kind, context) {
  const repository = context.payload.repository;
  return run.repository?.id === repository.id && run.head_repository?.id === repository.id
    && workflowMatches(run, kind) && run.event === 'push'
    && run.head_branch === repository.default_branch && run.head_sha === context.sha;
}
async function currentDefault(api, context) {
  const repository = await api('GET /repos/{owner}/{repo}');
  requireValue(repository.id === context.payload.repository.id
    && repository.default_branch === context.payload.repository.default_branch
    && context.ref === 'refs/heads/' + repository.default_branch, 'Default branch identity changed');
  const branch = await api('GET /repos/{owner}/{repo}/branches/{branch}', {branch: repository.default_branch});
  requireValue(branch.name === repository.default_branch && branch.commit.sha === context.sha,
    'Publication revision is no longer the default-branch tip');
}
async function latestPush(api, kind, context) {
  const listed = await api('GET /repos/{owner}/{repo}/actions/workflows/{workflow_id}/runs',
    {workflow_id: path.basename(kinds[kind].workflow), event: 'push', head_sha: context.sha,
      branch: context.payload.repository.default_branch, per_page: 100});
  // Do not filter by success: the latest failed/incomplete attempt blocks Pages.
  const candidate = listed.workflow_runs.find(run => pushMatches(run, kind, context));
  requireValue(candidate, 'No matching default-branch ' + kind + ' run');
  const run = await api('GET /repos/{owner}/{repo}/actions/runs/{run_id}', {run_id: candidate.id});
  completed(run, current => pushMatches(current, kind, context));
  return run;
}
async function publicationPair({github, context, root, temporary}) {
  const trigger = context.payload.workflow_run;
  const triggerKind = Object.keys(kinds).find(kind => trigger && workflowMatches(trigger, kind));
  requireValue(context.eventName === 'workflow_run' && triggerKind
    && pushMatches(trigger, triggerKind, context) && trigger.status === 'completed'
    && trigger.conclusion === 'success', 'Not a successful default-branch push completion');
  const api = apiFor(github, context);
  await currentDefault(api, context);
  const {tree, config} = checkedCheckout(root, context);
  const repository = context.payload.repository;
  const runs = {};
  const pair = {};
  let reused;
  for (const kind of Object.keys(kinds)) {
    const run = await latestPush(api, kind, context);
    runs[kind] = run;
    if (kind === triggerKind)
      requireValue(run.id === trigger.id && run.run_attempt === trigger.run_attempt,
        'Trigger identifies an obsolete run or attempt');
    const job = await requiredJob(api, run, kind);
    if (stepResult(job, 'Record reusable verification', 'success')) {
      const directory = path.join(temporary, 'push-' + kind);
      fs.mkdirSync(directory);
      pair[kind] = await verifiedRun({api, run, kind, repository, context, tree, config,
        temporary: directory, matches: current => pushMatches(current, kind, context)});
    } else {
      for (const name of ['Record reusable verification', 'Upload verification evidence', 'Setup manifesto tool',
        kind === 'checks' ? 'Run required local checks' : 'Verify generated tracked surfaces'])
        requireValue(stepResult(job, name, 'skipped'), 'Incomplete fresh verification cannot masquerade as reuse');
      if (!reused) {
        const directory = path.join(temporary, 'pr');
        fs.mkdirSync(directory);
        reused = await reusablePair({github, context, root, temporary: directory});
      }
      pair[kind] = reused.pair[kind];
    }
  }
  const pushes = {};
  for (const [kind, run] of Object.entries(runs)) {
    const current = await latestPush(api, kind, context);
    requireValue(current.id === run.id && current.run_attempt === run.run_attempt,
      'Push verification changed before publication');
    pushes[kind] = {run_id: run.id, run_attempt: run.run_attempt};
  }
  for (const kind of Object.keys(kinds)) {
    const item = pair[kind];
    const current = await api('GET /repos/{owner}/{repo}/actions/runs/{run_id}', {run_id: item.source.run_id});
    requireValue(current.status === 'completed' && current.conclusion === 'success'
      && current.run_attempt === item.source.run_attempt && current.head_sha === item.receipt.run_head,
      'Source evidence changed before publication');
  }
  await currentDefault(api, context);
  const source = {schema: 1, repository_id: repository.id, commit: context.sha, tree,
    pushes, checks: pair.checks.source, codeql: pair.codeql.source};
  return {pair, source};
}
async function publication({github, context, core, root = process.cwd()}) {
  core.setOutput('ready', 'false');
  const temporary = fs.mkdtempSync(path.join(os.tmpdir(), 'manifesto-publication-'));
  try {
    const {pair, source} = await publicationPair({github, context, root, temporary});
    const directory = safePath(root, '.ecosystem/github/presentation');
    requireValue(!fs.existsSync(directory), 'Presentation destination already exists');
    const receipt = {schema: 1, kind: 'publication', repository: pair.checks.receipt.repository,
      tested_commit: context.sha, tested_tree: source.tree, policy: pair.checks.receipt.policy,
      coverage: pair.checks.receipt.coverage, verification: source, files: {}};
    let total = 0;
    for (const kind of Object.keys(kinds)) {
      for (const name of fileNames(kind, pair[kind].receipt.coverage)) {
        const data = readFile(pair[kind].directory, name);
        total += data.length;
        requireValue(total <= maxBytes, 'Combined publication reports exceed the size limit');
        const target = path.join(directory, name);
        fs.mkdirSync(path.dirname(target), {recursive: true});
        fs.writeFileSync(target, data, {flag: 'wx'});
        receipt.files[name] = {bytes: data.length, sha256: hash(data)};
      }
    }
    fs.writeFileSync(path.join(directory, 'receipt.json'), JSON.stringify(receipt, null, 2) + '\n', {flag: 'wx'});
    core.setOutput('directory', directory);
    core.setOutput('coverage', receipt.coverage);
    core.setOutput('source', JSON.stringify(source));
    core.setOutput('ready', 'true');
    core.info('Publication evidence is ready for default-branch revision ' + context.sha);
  } catch (error) {
    core.notice('Publication deferred: ' + error.message);
  } finally {
    fs.rmSync(temporary, {recursive: true, force: true});
  }
}
async function revalidatePublication({github, context, root = process.cwd(), env = process.env}) {
  const api = apiFor(github, context);
  await currentDefault(api, context);
  requireValue(git(root, 'rev-parse', 'HEAD') === context.sha, 'Deployment checkout differs');
  if (context.eventName === 'workflow_dispatch') return;
  const expected = JSON.parse(env.PUBLICATION_SOURCE || 'null');
  requireValue(expected?.commit === context.sha, 'Publication source is missing');
  const temporary = fs.mkdtempSync(path.join(os.tmpdir(), 'manifesto-revalidate-'));
  try {
    const {source} = await publicationPair({github, context, root, temporary});
    requireValue(equal(source, expected), 'Selected publication evidence changed');
  } finally {
    fs.rmSync(temporary, {recursive: true, force: true});
  }
}

async function select({github, context, core, root = process.cwd(), env = process.env}) {
  core.setOutput('scope', env.BASE_SCOPE);
  core.setOutput('verified', 'false');
  if (env.BASE_SCOPE !== 'full' || context.eventName !== 'push'
    || context.ref !== 'refs/heads/' + context.payload.repository.default_branch) return;
  const temporary = fs.mkdtempSync(path.join(os.tmpdir(), 'manifesto-reuse-'));
  try {
    const {pair, tree, pr} = await reusablePair({github, context, root, temporary});
    requireValue(kinds[env.EVIDENCE_KIND], 'Unknown evidence consumer');
    const source = {schema: 1, repository_id: context.payload.repository.id, commit: context.sha,
      tree, pr, checks: pair.checks.source, codeql: pair.codeql.source};
    const selected = pair[env.EVIDENCE_KIND];
    const directory = safePath(root, `.ecosystem/github/reuse/${env.EVIDENCE_KIND}`);
    requireValue(!fs.existsSync(directory), 'Reuse destination already exists');
    fs.mkdirSync(directory, {recursive: true});
    for (const name of [...fileNames(env.EVIDENCE_KIND, selected.receipt.coverage), 'receipt.json']) {
      fs.mkdirSync(path.dirname(path.join(directory, name)), {recursive: true});
      fs.copyFileSync(path.join(selected.directory, name), path.join(directory, name), fs.constants.COPYFILE_EXCL);
    }
    fs.writeFileSync(path.join(directory, 'source.json'), JSON.stringify(source, null, 2) + '\n', {flag: 'wx'});
    const summary = path.join(directory, 'github/reports/00-verification-source/summary.md');
    fs.mkdirSync(path.dirname(summary), {recursive: true});
    fs.writeFileSync(summary, '## Reused PR verification\n\n```json\n' + JSON.stringify(source, null, 2) + '\n```\n', {flag: 'wx'});
    core.setOutput('scope', 'reuse');
    core.setOutput('verified', 'true');
    core.setOutput('source', JSON.stringify(source));
    core.setOutput('reports', env.EVIDENCE_KIND === 'checks' ? directory : path.join(directory, 'github/reports'));
    core.info(`Reusing PR #${pr} verification for tree ${tree}; Checks run ${source.checks.run_id}, CodeQL run ${source.codeql.run_id}.`);
  } catch (error) {
    core.setOutput('scope', 'full');
    core.setOutput('verified', 'false');
    core.notice('Fresh verification required: ' + error.message);
  } finally {
    fs.rmSync(temporary, {recursive: true, force: true});
  }
}

module.exports = {record, presentation, select, publication, revalidatePublication, reusablePair, policy, fileNames, artifactName, hash,
  checkPublicationTip: ({github, context}) => currentDefault(apiFor(github, context), context)};
