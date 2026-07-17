# winTerm v0.6 performance baseline

Performance results are collected only from named commands and environments. This document does not establish synthetic release gates from a developer machine.

| Scenario | Baseline | Current | Regression | Result | Evidence |
| --- | --- | --- | --- | --- | --- |
| Workspace model benchmark | Pending | Pending | N/A | Not tested | Run `scripts/winterm/benchmark-workspace.ps1` on provisioned Windows hardware. |
| Docking model benchmark | Pending | Pending | N/A | Not tested | Run `scripts/winterm/benchmark-docking.ps1` on provisioned Windows hardware. |
| Cold startup | Pending | Pending | N/A | Not tested | Requires packaged app launch measurement. |
| Warm startup | Pending | Pending | N/A | Not tested | Requires packaged app launch measurement. |
| Large Workspace restore | Pending | Pending | N/A | Not tested | Requires an instrumented runtime scenario. |

Investigate a median startup regression above 15 percent and a Workspace restore or docking commit regression above 20 percent. Record build type, hardware, date, run count, median, and dispersion before making a release decision.
