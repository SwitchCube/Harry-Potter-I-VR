# Building HP1VR / HP1VR selbst bauen

## English

Use Windows 10/11 x64, PowerShell and Python 3.11 or newer. A supported original HP1 installation is required. Clone this repository into a writable directory. Generated files, tools, original-game copies, profiles and builds are ignored by Git.

From the repository root:

```powershell
python -B scripts/configure_local.py --game-directory 'D:\Games\Harry Potter'
if ($LASTEXITCODE) { throw 'Unsupported game inputs' }
python -B scripts/setup_toolchain.py
if ($LASTEXITCODE) { throw 'Compiler setup failed' }
python -B scripts/setup_native_dependencies.py
if ($LASTEXITCODE) { throw 'Native dependencies failed' }
python -B scripts/setup_release_dependencies.py
if ($LASTEXITCODE) { throw 'Runtime dependencies failed' }
python -B scripts/build.py
if ($LASTEXITCODE) { throw 'Build failed' }
python -B -m unittest discover -s tests -p 'test_*.py'
if ($LASTEXITCODE) { throw 'Python tests failed' }
python -B scripts/test_native.py
if ($LASTEXITCODE) { throw 'Native tests failed' }
$build = (Get-Content config/last-build.json -Raw | ConvertFrom-Json).executable | Split-Path -Parent
python -B scripts/package_release.py --build $build --output dist/HP1VR-0.1.0.zip
if ($LASTEXITCODE) { throw 'Packaging failed' }
```

Replace the example game path with your own. Configuration verifies and copies only fingerprinted stock files to the ignored `Harry Potter TM` directory; it does not import your saves or change the original installation. Other game builds fail verification deliberately. Do not bypass the fingerprints or reuse HP2 offsets.

The dependency scripts download pinned Zig 0.15.2, OpenVR, MinHook and the Python runtime using checked-in hashes. The build packs the included synthetic WAVs into the mod's own `.uax` files and derives the HP1 interfaces from verified originals before compiling the x86 DLL and observer. No AI service, API key, voice model or new voice generation is required to build or play.

`src/` contains native code, `scripts/` build and session tools, `tests/` test sources, `config/vr/` SteamVR action definitions, `assets/tutorial/` editable WAVs and metadata, and `release/` the portable launcher and instructions. Package creation checks that the compiled build still matches its recorded source inputs. The ZIP contains the mod's binaries, synthetic sound packages and licensed runtimes, with a SHA-256 checksum alongside it.

Existing deeper integration scripts use disposable profiles and sometimes require manually prepared checkpoint fixtures. They are not normal game launchers. `Start-Spiel.cmd` is a development entry point and needs a regular Python installation with Tk; the published ZIP uses its own runtime and `Start-HP1VR.cmd`.

## Deutsch

Benötigt werden Windows 10/11 x64, PowerShell, Python ab 3.11 und eine unterstützte Originalinstallation. Die Befehle oben werden im Repository-Ordner ausgeführt; den Beispielpfad durch den eigenen Spielordner ersetzen.

Die Einrichtung kopiert ausschließlich geprüfte Originaldateien in den von Git ausgeschlossenen Ordner `Harry Potter TM`. Sie übernimmt keine Spielstände und verändert die Originalinstallation nicht. Die Werkzeuge laden festgelegte Abhängigkeiten mit Prüfsummen herunter. Beim Bauen werden die enthaltenen künstlichen WAV-Aufnahmen verpackt und die Mod für das 32-Bit-Spiel kompiliert. Dafür werden keine API-Schlüssel oder KI-Dienste benötigt.

Quellcode und Tests bleiben im Repository, fertige Dateien in `build/` und `dist/`. Originalspiel, lokale Profile, Zugangsdaten und Build-Ausgaben dürfen nicht in Commits gelangen. Zum Spielen das selbst erstellte ZIP nach der README-Anleitung installieren. Der Entwicklungsstarter `Start-Spiel.cmd` benötigt Python mit Tk; das veröffentlichte ZIP enthält seine eigene Laufzeitumgebung.
