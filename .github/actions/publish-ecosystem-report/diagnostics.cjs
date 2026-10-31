'use strict';
// Diagnostics only: required-result and provenance gates remain in the workflows.
const fs = require('node:fs');
const path = require('node:path');

function files(root, predicate) {
  if (!fs.existsSync(root)) return [];
  const found = [];
  for (const entry of fs.readdirSync(root, {withFileTypes: true})) {
    const file = path.join(root, entry.name);
    if (entry.isDirectory() && !['CMakeFiles', '.git'].includes(entry.name)) found.push(...files(file, predicate));
    else if (entry.isFile() && predicate(file)) found.push(file);
  }
  return found.sort();
}
function save(directory, record) {
  fs.mkdirSync(directory, {recursive: true});
  fs.writeFileSync(path.join(directory, 'result.json'), JSON.stringify(record, null, 2) + '\n');
  const rows = record.stages || [];
  let text = `## ${record.title}\n\n- status: \`${record.status}\`\n- reason: ${record.reason}\n\n`;
  if (rows.length) text += '| Stage | Status | Category | Reason |\n| --- | --- | --- | --- |\n'
    + rows.map(row => `| ${row.name} | ${row.status} | ${row.category} | ${row.reason.replaceAll('|', '\\|').replaceAll('\n', ' ')} |`).join('\n') + '\n';
  if (record.test_counts) text += '\nTest counts (observed output, not inferred passes):\n```json\n'
    + JSON.stringify(record.test_counts, null, 2) + '\n```\n';
  if (record.execution_counts) text += '\nExecuted/reused/skipped units (phase and translation-unit scopes are separate):\n```json\n'
    + JSON.stringify(record.execution_counts, null, 2) + '\n```\n';
  if (record.logs) text += '\n' + record.logs.map(log => `- ${log}`).join('\n') + '\n';
  fs.writeFileSync(path.join(directory, 'summary.md'), text);
  return text;
}
function diagnosticExcerpt(text) {
  const lines = text.split('\n'); const selected = new Set();
  // Authored regression failures are authoritative. Expected negative fixtures
  // can print CMake/libclang errors even while their own test group passes.
  const failures = lines.filter(line => /(?:^|\s)\[failed\] /i.test(line));
  if (failures.length) return failures.slice(0, 24).join('\n');
  for (let index = 0; index < lines.length; index++) {
    if (!/error:\s|error\[|##\[error\]|::error::|CMake Error|Unknown arguments|FAILED:|forbidden repository entries:/i.test(lines[index])) continue;
    // Keep multiline CMake/compiler details, rather than just their headline.
    for (let next = index; next < Math.min(index + 5, lines.length); next++) selected.add(next);
  }
  return [...selected].sort((a, b) => a - b).map(index => lines[index]).slice(-24).join('\n').trim()
    || lines.slice(-12).join('\n').trim();
}
function failureLog(root, id, output) {
  let file = output['log-path'];
  if (id === 'ecosystem') {
    const directory = '.ecosystem/github/reports/00-toolchain';
    const steps = path.join(root, directory, 'steps.tsv');
    const failed = fs.existsSync(steps) && fs.readFileSync(steps, 'utf8').split('\n')
      .map(line => line.split('\t')).find(([name, code]) => /^[a-z-]+$/.test(name) && /^\d+$/.test(code || '') && code !== '0');
    if (failed) file = `${directory}/${failed[0]}.log`;
  }
  if (!file) return null;
  const absolute = path.resolve(root, file); const relative = path.relative(root, absolute);
  if (relative.startsWith('..' + path.sep) || path.isAbsolute(relative) || !fs.existsSync(absolute)) return null;
  return {file: relative, excerpt: diagnosticExcerpt(fs.readFileSync(absolute, 'utf8'))};
}
function nativeDetails(root, steps, kind, scope) {
  if (kind !== 'checks' || scope !== 'full' || steps.required?.outcome !== 'success') return {logs: []};
  const read = name => {
    const file = path.join(root, '.ecosystem/reports', name);
    let current = root;
    for (const part of path.relative(root, file).split(path.sep)) {
      current = path.join(current, part);
      if (fs.existsSync(current) && fs.lstatSync(current).isSymbolicLink()) throw new Error('Unsafe native report path: ' + name);
    }
    if (!fs.existsSync(file)) throw new Error('Missing native verification report: ' + name);
    const destination = path.join(root, '.ecosystem/github/reports/native', name);
    fs.mkdirSync(path.dirname(destination), {recursive: true});
    fs.copyFileSync(file, destination);
    return fs.readFileSync(file, 'utf8');
  };
  try {
    const phases = JSON.parse(read('ci.json'));
    if (!Array.isArray(phases) || !phases.length || phases.some(row => !row.phase || !row.status))
      throw new Error('Invalid native phase report');
    read('ci-commands.log');
    const units = [];
    if (phases.some(row => row.phase === 'tidy' && ['executed', 'failed'].includes(row.status))) {
      const tidy = JSON.parse(read('tidy.json'));
      for (const [scope, rows] of [['ast', tidy.execution], ['tidy', tidy.tidy_execution]]) {
        if (!Array.isArray(rows)) throw new Error('Missing ' + scope + ' execution inventory');
        units.push(...rows.map(row => ({scope, ...row})));
      }
    }
    const count = rows => ({executed: rows.filter(row => ['executed', 'failed'].includes(row.status)).length,
      reused: rows.filter(row => row.status === 'reused').length,
      skipped: rows.filter(row => row.status === 'not-applicable').length,
      blocked: rows.filter(row => row.status === 'blocked').length});
    return {phases, units, counts: {phases: count(phases), ast: count(units.filter(row => row.scope === 'ast')),
      tidy: count(units.filter(row => row.scope === 'tidy'))},
      logs: ['Native command durations: `.ecosystem/github/reports/native/ci-commands.log`',
        'Native phase timings and dispositions: `.ecosystem/github/reports/native/ci.json`',
        ...(units.length ? ['Per-source commands, durations and reuse reasons: `.ecosystem/github/reports/native/tidy.json`'] : [])]};
  } catch (error) { return {error: error.message, logs: []}; }
}
async function snapshot({core, kind, scope, selectionResult, steps, root = process.cwd()}) {
  const definition = kind === 'checks'
    ? [['changes', 'Change resolution', []], ['ecosystem', 'Tooling setup', ['changes']],
       ['classification', 'Native classification', ['ecosystem']],
       ['required', 'Build and required checks', ['ecosystem', 'classification']],
       ['coverage', 'Coverage', ['required']]]
    : [['changes', 'Change resolution', []], ['ecosystem', 'Tooling setup', ['changes']],
       ['classification', 'Native classification', ['ecosystem']], ['repository', 'Repository', ['classification']],
       ['codeql_init', 'CodeQL init', ['repository']], ['codeql_build', 'CodeQL build', ['codeql_init']],
       ['codeql_analyze', 'CodeQL analyze', ['codeql_build']]];
  const stages = [];
  for (const [id, name, dependencies] of definition) {
    const step = steps[id] || {}; const output = step.outputs || {};
    const row = {id, name, outcome: step.outcome || 'not-recorded', exit_code: output['exit-code'] || null};
    const blocked = dependencies.find(dep => stages.find(s => s.id === dep)?.status !== 'passed');
    const narrowed = kind === 'codeql' && id.startsWith('codeql_') && steps.classification?.outputs?.codeql === 'repository';
    const optional = (scope === 'cheap' && id === 'coverage') || narrowed;
    if (step.outcome === 'failure' || output.status === 'failed') {
      Object.assign(row, {status: 'failed', category: output.reason?.startsWith('Log capture failed') ? 'missing-report'
        : ['4', '127'].includes(row.exit_code) ? 'unavailable-environment' : 'actual-failure',
        reason: output.reason || `${name} failed${row.exit_code ? ' (exit ' + row.exit_code + ')' : ''}; see captured logs and hosted step log.`});
    } else if (step.outcome === 'cancelled') {
      Object.assign(row, {status: 'blocked', category: 'unavailable-environment', reason: `${name} was cancelled.`});
    } else if (selectionResult !== 'success') {
      Object.assign(row, {status: 'blocked', category: 'unmet-prerequisite', reason: 'Selection did not complete successfully.'});
    } else if (scope === 'reuse') {
      Object.assign(row, {status: 'not-applicable', category: 'verified-reuse', reason: 'Use the validated PR evidence; this native stage did not run again.'});
    } else if (step.outcome === 'success') {
      const artifact = output['report-path']; const log = output['log-path'];
      if (artifact && (!fs.existsSync(path.resolve(root, artifact)) || !fs.existsSync(path.resolve(root, log || '.missing-log')))) {
        Object.assign(row, {status: 'failed', category: 'missing-report', reason: `${name} output is incomplete; its summary/log is missing.`});
      } else {
        Object.assign(row, {status: output.status === 'skipped' ? 'not-applicable' : 'passed',
          category: output.status === 'skipped' ? 'not-applicable' : 'executed',
          reason: output.reason || (output.status === 'skipped' ? `Unsupported capability (exit ${row.exit_code}).` : 'Step completed successfully.')});
      }
    } else if (optional) {
      Object.assign(row, {status: 'not-applicable', category: 'not-applicable', reason: narrowed
        ? 'Validated PR delta requires repository checks only; native analysis is not required.' : 'Non-build change uses the cheap verification profile.'});
    } else if (blocked) {
      Object.assign(row, {status: 'blocked', category: 'unmet-prerequisite', reason: `Requires ${blocked}, which did not pass.`});
    } else {
      Object.assign(row, {status: 'blocked', category: 'missing-result', reason: `${name} was required but has no completed result.`});
    }
    if (row.status === 'failed') {
      const diagnostic = failureLog(root, id, output);
      if (diagnostic) {
        row.log_path = diagnostic.file; row.diagnostic = diagnostic.excerpt;
        if (diagnostic.excerpt) row.reason += `\n${diagnostic.excerpt}`;
      }
    }
    stages.push(row);
  }
  const testCounts = [];
  const detailCounts = [];
  for (const file of files(path.join(root, '.ecosystem/github/reports'), f => path.basename(f) === 'output.log')) {
    const text = fs.readFileSync(file, 'utf8');
    for (const match of text.matchAll(/(\d+)% tests passed, (\d+) tests failed out of (\d+)/g))
      testCounts.push({source: path.relative(root, file), framework: 'CTest', registered_runners: Number(match[3]), failed_runners: Number(match[2]), passed_percent: Number(match[1])});
  }
  const logs = [];
  const testLogs = files(path.join(root, '.ecosystem/build'), f => ['LastTest.log', 'Test.xml'].includes(path.basename(f)));
  for (const file of testLogs) {
    const relative = path.relative(path.join(root, '.ecosystem/build'), file);
    const destination = path.join(root, '.ecosystem/github/reports/test-details', relative);
    fs.mkdirSync(path.dirname(destination), {recursive: true}); fs.copyFileSync(file, destination);
    logs.push(`Test detail: \`${path.relative(root, destination)}\``);
    const text = fs.readFileSync(file, 'utf8');
    const passed = [...text.matchAll(/^\[ok\] /gm)].length;
    const failed = [...text.matchAll(/^\[failed\] /gm)].length;
    if (passed || failed) detailCounts.push({source: relative, framework: 'Ecosystem self-tests', passed_group_reports: passed, failed_group_reports: failed});
    for (const match of text.matchAll(/Totals: (\d+) passed, (\d+) failed, (\d+) skipped/g))
      testCounts.push({source: relative, framework: 'QtTest', passed: Number(match[1]), failed: Number(match[2]), skipped: Number(match[3])});
    for (const match of text.matchAll(/\[\s*PASSED\s*\]\s*(\d+) tests?/g))
      testCounts.push({source: relative, framework: 'GoogleTest', passed: Number(match[1])});
    for (const match of text.matchAll(/\[\s*FAILED\s*\]\s*(\d+) tests?/g))
      testCounts.push({source: relative, framework: 'GoogleTest', failed: Number(match[1])});
    for (const match of text.matchAll(/\[\s*SKIPPED\s*\]\s*(\d+) tests?/g))
      testCounts.push({source: relative, framework: 'GoogleTest', skipped: Number(match[1])});
  }
  const native = nativeDetails(root, steps, kind, scope);
  if (native.error && !stages.some(row => row.status === 'failed')) stages.push({id: 'native_reports', name: 'Native execution reports',
    status: 'failed', category: 'missing-report', reason: native.error});
  logs.push(...native.logs);
  const bad = stages.find(row => ['failed', 'blocked'].includes(row.status));
  // The final nested CTest summary is the top-level outcome; keep prior probe
  // summaries labelled so they cannot be read as successful project acceptance.
  for (let index = 0; index < testCounts.length; index++) if (testCounts[index].framework === 'CTest')
    testCounts[index].scope = index === testCounts.findLastIndex(row => row.framework === 'CTest' && row.source === testCounts[index].source) ? 'final invocation' : 'earlier invocation/probe';
  testCounts.push(...detailCounts);
  const record = {title: kind === 'checks' ? 'Checks execution' : 'CodeQL execution', scope,
    status: bad ? 'failed' : 'passed', reason: bad ? bad.reason : 'All selected stages completed.', stages,
    native_phases: native.phases || [], execution_counts: native.counts || null,
    native_report_error: native.error || null,
    test_counts: testCounts.length ? testCounts : {status: 'not-observed', reason: 'No completed test count was recorded; do not infer zero tests or passing tests.'}, logs};
  await core.summary.addRaw(save(path.join(root, '.ecosystem/github/reports/00-workflow'), record)).write();
  core.setOutput('status', record.status);
  return record;
}

async function collect({github, context, core, kind, scope, workflowResult, selectionResult, selectionReason, root = process.cwd()}) {
  const directory = path.join(root, '.ecosystem/github/reports');
  const existing = files(directory, f => path.basename(f) === 'summary.md');
  const snapshotFile = path.join(directory, '00-workflow/result.json');
  const excluded = workflowResult === 'skipped' && selectionResult === 'success'
    && (scope === 'skip' || (kind === 'codeql' && scope === 'cheap'));
  const complete = existing.length > 0 && (scope === 'reuse' || fs.existsSync(snapshotFile));
  let status = excluded ? 'not-applicable' : workflowResult === 'success' && complete ? 'passed' : 'failed';
  let reason = excluded ? `Event scope ${scope}: ${selectionReason || 'excluded by the event policy'}.`
    : selectionResult !== 'success' ? `Selection ${selectionResult || 'missing'}; downstream work was blocked.`
    : !complete ? 'Missing complete execution report; uploaded change data is not verification evidence.'
    : `Upstream workflow job result: ${workflowResult || 'missing'}.`;
  let localRecord = null;
  if (fs.existsSync(snapshotFile)) {
    const result = localRecord = JSON.parse(fs.readFileSync(snapshotFile, 'utf8'));
    if (result.status !== 'passed' && !excluded) { status = 'failed'; reason = result.reason; }
  }
  const logs = []; const stages = [];
  try {
    const jobs = await github.paginate(github.rest.actions.listJobsForWorkflowRun, {...context.repo, run_id: context.runId, per_page: 100});
    for (const job of jobs.filter(job => job.name !== 'report')) {
      const firstFailure = (job.steps || []).find(step => step.conclusion === 'failure');
      if (firstFailure) {
        status = 'failed';
        const cause = localRecord?.stages?.find(row => row.status === 'failed');
        if (!cause) reason = `${job.name} failed at ${firstFailure.name} (#${firstFailure.number}); dependent stages did not establish verification.`;
        // A composite stage may return success to permit uploads, then the
        // strict enforcement step fails. Retain the original command failure.
        else logs.push(`Hosted ${job.name}: ${firstFailure.name} (#${firstFailure.number}) enforces the recorded failure in ${cause.name}.`);
      }
      for (const step of job.steps || []) {
        if (!['failure', 'skipped', 'cancelled'].includes(step.conclusion)) continue;
        const blocked = step.conclusion === 'skipped' && firstFailure && step.number > firstFailure.number;
        const nativeId = step.name === 'Classify checked-out changes' ? 'classification'
          : step.name === 'Run required local checks' ? 'required'
          : step.name === 'Run full verification coverage' ? 'coverage'
          : step.name === 'Verify generated tracked surfaces' ? 'repository'
          : step.name.includes('codeql-action/init') ? 'codeql_init'
          : step.name === 'Build project through ecosystem' ? 'codeql_build'
          : step.name === 'Run CodeQL analysis' ? 'codeql_analyze' : null;
        const native = localRecord?.stages?.find(row => row.id === nativeId);
        const excludedReason = step.name === 'Upload reused verification reports' && scope !== 'reuse'
          ? 'Fresh verification selected; no validated PR report is being reused.'
          : step.name === 'Restore verified analysis cache'
          ? 'Only native full selection uses analysis records; unknown inputs still execute.'
          : step.name === 'Record compiler cache statistics' && scope === 'reuse'
          ? 'Validated whole-workflow reuse does not configure or execute the compiler cache.'
          : step.name === 'Save verified compiler cache' || step.name === 'Save verified analysis cache'
          ? 'Only an executed non-PR verification may publish shared caches; cache upload is not a verification result.'
          : step.name === 'Upload change classification' && scope === 'cheap'
          ? 'Cheap non-build change does not request native classification.'
          : step.name === 'Record delta verification' && localRecord?.stages?.find(row => row.id === 'codeql_init')?.category !== 'not-applicable'
          ? 'Full native analysis is selected; a repository-only delta receipt does not apply.'
          : step.name === 'Record reusable verification'
          ? 'Whole-tree evidence requires full scope and all required native gates; this run did not meet those prerequisites.'
          : step.name.includes('verification evidence') || step.name.includes('delta evidence')
          ? 'No matching successful verification receipt was emitted; evidence upload is conditional on that receipt.'
          : scope === 'reuse' ? 'Validated identical-tree PR evidence replaces this native operation.'
          : excluded ? `Excluded by scope ${scope}: ${selectionReason || 'event policy'}.` : null;
        const unexplained = step.conclusion === 'skipped' && !blocked && !native && !excludedReason;
        if (unexplained && !step.name.startsWith('Post ')) {
          status = 'failed'; reason = `Unexplained skipped stage ${job.name}: ${step.name}; it cannot establish verification.`;
        }
        stages.push({name: `${job.name}: ${step.name}`, status: blocked || unexplained ? 'blocked' : step.conclusion === 'skipped' && native ? native.status : step.conclusion,
          category: blocked ? 'unmet-prerequisite' : native?.category || (step.conclusion === 'failure' ? 'actual-failure'
            : step.conclusion === 'cancelled' ? 'unavailable-environment' : unexplained ? 'unresolved-condition' : 'not-applicable'),
          reason: blocked ? `Blocked by failed step ${firstFailure.name} (#${firstFailure.number}).`
            : step.conclusion === 'failure' ? `${native?.reason || localRecord?.stages?.find(row => row.status === 'failed')?.reason || 'Hosted step failed'}; ${job.html_url || context.serverUrl + '/' + context.repo.owner + '/' + context.repo.repo + '/actions/runs/' + context.runId}`
            : native?.reason || excludedReason || `Condition did not admit this step (outcome ${step.conclusion}, scope ${scope || 'missing'}); inspect its workflow condition and preceding step outcomes.`});
      }
      if (job.conclusion !== 'failure' && job.conclusion !== 'cancelled') continue;
      try {
        const response = await github.rest.actions.downloadJobLogsForWorkflowRun({...context.repo, job_id: job.id});
        let content = response.data;
        if (response.headers?.location) {
          const fetched = await fetch(response.headers.location);
          if (!fetched.ok) throw new Error(`Log download HTTP ${fetched.status}`);
          content = await fetched.text();
        }
        if (Buffer.isBuffer(content)) content = content.toString('utf8');
        if (typeof content !== 'string' || !content.trim()) throw new Error('Job log is unavailable');
        const log = path.join(directory, 'hosted-logs', `${job.id}.log`);
        fs.mkdirSync(path.dirname(log), {recursive: true}); fs.writeFileSync(log, content);
        const excerpt = diagnosticExcerpt(content.split('\n')
          .filter(line => !line.includes('\u001b[36;1m')).join('\n'));
        if (firstFailure && !localRecord?.stages?.some(row => row.status === 'failed')) {
          reason = `${job.name} failed at ${firstFailure.name} (#${firstFailure.number}):\n${excerpt}`;
          const failed = stages.find(row => row.name === `${job.name}: ${firstFailure.name}`);
          if (failed) failed.reason = reason;
        }
        logs.push(`Hosted job ${job.id}: \`${path.relative(root, log)}\`\n\n\`\`\`text\n${excerpt}\n\`\`\``);
      } catch (error) { logs.push(`Hosted log for job ${job.id} unavailable: ${error.message}; use the job's GitHub log.`); }
    }
  } catch (error) { logs.push(`Hosted job inventory unavailable: ${error.message}. Local stage logs and upstream results remain authoritative.`); }
  const record = {title: 'Hosted execution diagnosis', status, reason, workflow_result: workflowResult,
    selection_result: selectionResult, scope, selection_reason: selectionReason, stages, logs};
  save(path.join(directory, '00-hosted-run'), record);
  return record;
}
module.exports = {snapshot, collect};
