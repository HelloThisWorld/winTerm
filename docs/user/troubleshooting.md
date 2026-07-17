# Troubleshooting

| Symptom | Likely cause | Safe resolution |
| --- | --- | --- |
| winTerm does not launch | Incomplete package installation | Confirm package identity, signing status, and the Windows event log. |
| Certificate warning | Development package is unsigned or untrusted | Use a reviewed official release or a locally trusted development certificate. |
| PowerShell integration is unavailable | Profile or execution-policy boundary | Do not set a global unrestricted policy; inspect the module installation and profile selection. |
| Workspace cannot restore | Missing monitor, folder, or profile | Use the safe fallback and create a redacted diagnostic bundle. |
| Docking option is disabled | Runtime docking is deliberately feature-gated | Keep it disabled until a release documents live support. |
| Update check fails | Automatic updates are not enabled | Use the official GitHub Release manually. |
