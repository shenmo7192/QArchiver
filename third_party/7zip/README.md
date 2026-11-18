# Bundled 7-Zip command-line helper

Version: **26.06** (`VERSION` = `7z2606`)

QArchiver uses 7-Zip as the fallback command-line backend when the in-process
libarchive backend cannot handle an archive (encrypted 7z, RAR, split volumes,
archive creation, tar write, RAR read, ...).  Shipping the full binaries makes
the application independent from the host's `7z` / `tar` / `unrar` installation.

## Contents

```text
linux/<arch>/7zzs   statically linked 7-Zip (no runtime library dependencies)
win/x64/7z.exe      7-Zip console executable
win/x64/7z.dll      7-Zip engine required by 7z.exe (same version!)
```

`7z.exe` cannot run without the matching `7z.dll`; always distribute the pair
together and never mix binaries from different 7-Zip releases.

The GUI front-end (`7zFM.exe`), the GUI module (`7zG.exe`), shell extensions
(`7-zip.dll`, `7-zip32.dll`), SFX modules, the CHM help file and the `Lang/`
localization files are intentionally not bundled.  The command-line tool only
needs the two Windows files above, and English output keeps error parsing
stable.

## Upstream sources

- Linux/macOS: <https://www.7-zip.org/a/7z2606-linux-x64.tar.xz>,
  <https://www.7-zip.org/a/7z2606-linux-x86.tar.xz>,
  <https://www.7-zip.org/a/7z2606-linux-arm64.tar.xz>,
  <https://www.7-zip.org/a/7z2606-linux-arm.tar.xz>
- Windows x64: <https://www.7-zip.org/a/7z2606-x64.exe> (or the `.msi`
  package)

Only `7zzs` and `7z.exe` + `7z.dll` are copied from the upstream packages.

## License

7-Zip is free software distributed under the GNU LGPL, except for the unRAR
code which has additional restrictions.  See `License.txt`; keep it next to the
binaries and in the installed documentation directory.

## Updating

1. Download the new upstream packages for every architecture.
2. Replace the binaries in `linux/<arch>/`, `win/x64/`.
3. Update `VERSION` and the version in this file.
4. Keep `License.txt` in sync with the upstream package.
5. Rebuild and run `ctest`; the CLI backend tests exercise the bundled helper
   through `QARCHIVER_7Z_PATH`.
