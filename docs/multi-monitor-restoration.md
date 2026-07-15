# Multi-monitor restoration

Every window stores absolute bounds, normalized monitor-relative bounds, monitor device and stable identifiers, friendly name, work area, and horizontal and vertical DPI. Display ordinal names alone are not treated as stable identity.

Resolution attempts exact device ID, stable monitor ID, friendly name plus work-area resolution, physical-position similarity, and a safe available monitor. When the original display cannot be used, normalized bounds are applied to a safe monitor. Same-monitor bounds with materially different DPI are scaled from the saved work area and DPI.

The safety pass rejects non-finite or non-positive geometry, constrains size to the current work area, applies minimum logical dimensions of 480 by 320 where the work area permits, and keeps at least 64 logical pixels of the title area reachable. Negative virtual-desktop coordinates remain valid. An invalid rectangle becomes a centered safe default.

Minimized windows restore as normal unless explicitly enabled. Fullscreen restore is also opt-in; otherwise it restores as normal before any later user action. Monitor and DPI adjustments are recorded in the restore report.

The pure resolver has automated source fixtures for missing-monitor, DPI, work-area, and off-screen behavior. Mixed-DPI physical displays still require manual testing on a built package.
