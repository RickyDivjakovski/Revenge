# Offline target compatibility

Run `Revenge.exe compatibility-plan <authorised.exe>` on an accessible AMD64 PE32+ developer executable. No code executes, dependencies load or protected containers are extracted. Console markers are heuristic; import names do not verify ABI signatures. The report combines delay imports, layout, initialization, TLS and GDK diagnostics. A report is never launch authorization.

Optional target in focused validation:

```powershell
powershell -ExecutionPolicy Bypass -File .\Build-And-Test-Revenge.ps1 -FastBuild -BuildJobs 2 -Focused -AuthorisedTarget 'C:\path\test.exe'
```

The report is appended to the combined log after normal tests. Unsupported or malformed files fail analysis. Review logs before sharing; they contain import names and local paths.
