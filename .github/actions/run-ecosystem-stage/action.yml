name: Run Ecosystem Stage
description: Run one ecosystem-owned CI stage, keep the workflow alive, and emit a Markdown report.

inputs:
  stage-id:
    description: Stable identifier for the stage report directory.
    required: true
  stage-label:
    description: Human-readable label shown in the generated report.
    required: true
  shell-command:
    description: Shell command block that runs the stage.
    required: true
  skipped-exit-codes:
    description: Space-separated exit codes that should be treated as skipped rather than failed.
    required: false
    default: '3'

outputs:
  status:
    description: Stage status as passed, failed, or skipped.
    value: ${{ steps.run.outputs.status }}
  exit-code:
    description: Raw shell exit code from the stage command.
    value: ${{ steps.run.outputs.exit-code }}
  report-path:
    description: Generated Markdown summary path.
    value: ${{ steps.run.outputs.report-path }}
  log-path:
    description: Captured stage log path.
    value: ${{ steps.run.outputs.log-path }}
  reason:
    description: Completion, unsupported capability, environment or command failure reason.
    value: ${{ steps.run.outputs.reason }}

runs:
  using: composite
  steps:
    - name: Run stage command
      id: run
      shell: bash
      run: |
        report_root=".ecosystem/github/reports/${{ inputs.stage-id }}"
        command_path="$report_root/command.sh"
        log_path="$report_root/output.log"
        report_path="$report_root/summary.md"
        mkdir -p "$report_root"
        cat <<'EOF' > "$command_path"
        ${{ inputs.shell-command }}
        EOF
        chmod +x "$command_path"

        set +e
        started=$SECONDS
        bash -e -o pipefail "$command_path" 2>&1 | tee "$log_path"
        pipeline_status=("${PIPESTATUS[@]}")
        command_code=${pipeline_status[0]}
        log_code=${pipeline_status[1]:-0}
        exit_code=$command_code
        if [ "$command_code" -eq 0 ] && [ "$log_code" -ne 0 ]; then exit_code=$log_code; fi
        elapsed=$((SECONDS - started))
        set -e

        status="failed"
        if [ "$exit_code" -eq 0 ]; then
          status="passed"
        else
          for skipped_code in ${{ inputs.skipped-exit-codes }}; do
            if [ "$exit_code" -eq "$skipped_code" ]; then
              status="skipped"
              break
            fi
          done
        fi

        if [ "$log_code" -ne 0 ]; then status="failed"; fi

        reason="Command completed successfully."
        category="executed"
        if [ "$status" = skipped ]; then
          reason="Capability is unsupported (exit $exit_code); see the command log."
          category="not-applicable"
        elif [ "$status" = failed ]; then
          reason="Command failed (exit $exit_code); see the command log."
          category="actual-failure"
          if [ "$exit_code" = 4 ] || [ "$exit_code" = 127 ]; then
            reason="Required tool/environment unavailable (exit $exit_code); see the command log."
            category="unavailable-environment"
          fi
        fi

        if [ "$log_code" -ne 0 ]; then
          reason="Log capture failed (exit $log_code); command exit $command_code."
          category="missing-report"
        fi

        log_lines=0
        if [ -f "$log_path" ]; then
          log_lines=$(wc -l < "$log_path")
        fi

        {
          printf '## %s\n\n' "${{ inputs.stage-label }}"
          printf -- '- status: `%s`\n' "$status"
          printf -- '- exit code: `%s`\n' "$exit_code"
          printf -- '- duration seconds: `%s`\n' "$elapsed"
          printf -- '- category: `%s`\n' "$category"
          printf -- '- reason: %s\n' "$reason"
          printf -- '- command:\n\n```bash\n'
          cat "$command_path"
          printf '```\n\n'
          printf '<details><summary>Log excerpt</summary>\n\n```text\n'
          if [ "$log_lines" -gt 120 ]; then
            sed -n '1,40p' "$log_path"
            printf '\n... omitted %s middle lines; full log retained ...\n' "$((log_lines - 120))"
            tail -n 80 "$log_path"
          else
            cat "$log_path"
          fi
          printf '```\n</details>\n'
        } > "$report_path"

        cat "$report_path" >> "$GITHUB_STEP_SUMMARY"
        echo "status=$status" >> "$GITHUB_OUTPUT"
        echo "exit-code=$exit_code" >> "$GITHUB_OUTPUT"
        echo "report-path=$report_path" >> "$GITHUB_OUTPUT"
        echo "log-path=$log_path" >> "$GITHUB_OUTPUT"
        echo "reason=$reason" >> "$GITHUB_OUTPUT"
