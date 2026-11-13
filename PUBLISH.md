# Publishing DimLite

The repository is already initialized on branch `main` with the release commit and the `v1.1.0` tag.

## One-time GitHub step

Create a **public empty repository** named `DimLite` under `CloudW4lker250`.

Do not initialize it with a README, license or `.gitignore`: those files are already in this checkout.

## Push the prepared release

From this directory, run:

```powershell
.\scripts\publish.ps1
```

The script checks that the working tree is clean, creates the `v1.1.0` tag, pushes `main` and the tag, and prints the release asset paths. It never force-pushes.

Then create a GitHub Release from tag `v1.1.0` and upload:

- `release\DimLite-1.1.0-win-x64.zip`
- `release\DimLite-1.1.0-win-x64.sha256`
- `release\DimLite-1.1.0-Setup.exe`
- `release\DimLite-1.1.0-Setup.sha256`

Use [RELEASE_NOTES-v1.1.0.md](RELEASE_NOTES-v1.1.0.md) as the release description.


