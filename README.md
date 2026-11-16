# Harry Potter I VR

> AI was used during development, including synthetic tutorial speech. / Bei der Entwicklung kam KI zum Einsatz, unter anderem für künstlich erzeugte Lernhinweise.

## English

An unofficial VR mod for **Harry Potter and the Philosopher's Stone / Sorcerer's Stone (2001, Windows PC)**. Explore Hogwarts in stereo VR, look around freely and aim your wand with a motion controller. This is an independent HP1 mod; the HP2 mod is not required.

**First public preview: 0.1.0.** The project is still in development. Several mechanics have passed private tests in the original engine, but this is not a completed or fully headset-tested game conversion. See [testing and limitations](docs/TESTING.md).

### Features

- SteamVR/OpenVR stereo rendering, tracked head and wand, movement, snap turning, jumping and climbing. Aim and cast while moving.
- Original Flat mode and VR mode share six mod save slots, with separate graphics settings and local backup copies of completed saves.
- Controller tracing for spell lessons, broom controls for flying lessons, Quidditch, the Remembrall chase and flying keys.
- Adapted controls for chess, levitating objects, the troll escape and Fluffy's flute encounter.
- Inventory and status at the left hand, readable letters, original boss health displays and stereo mirrors.
- A fixed cinema screen in an entrance-hall environment for menus and flat story scenes.
- In-game VR settings for controller mappings, cinema/3D mode, language and 1024–2048 pixels per eye. Render-size changes apply on the next launch.
- German and English mod interfaces and 16 tutorial instructions, with spoken variants and controller-button highlights. Synthetic voices are not new recordings by the original actors.

### Install and play

1. Download **HP1VR-0.1.0.zip** from [Releases](https://github.com/SwitchCube/Harry-Potter-I-VR/releases).
2. Extract it into your own game installation folder. `Start-HP1VR.cmd` and the `HP1VR` folder must be **next to the existing `System` folder**.
3. For VR, start SteamVR and connect a compatible headset with two controllers.
4. Open `Start-HP1VR.cmd`, select **VR** or **Flat**, and start the game. Choose a free slot in the original game menu to begin, or the same occupied slot to continue.

Windows 10/11 x64 is required. The bundled runtime means no separate Python installation or compiler is needed to play. The currently supported reference is the **German HP1 engine 433 build from 19 October 2001**, checked against the included file fingerprints. English mod text does not imply support for a different English game executable.

First launch prepares a private copy of the verified game inputs under `%LOCALAPPDATA%\HP1VR`. Allow at least **1 GB of additional free space**, plus space for growing saves and backups. Original game files and existing original save slots are left unchanged. Mod saves start empty and are shared between Flat and VR; no personal saves are imported. Keep the launcher open while playing. Use the original game menu, the launcher's Stop button or `Stop-HP1VR.cmd` to exit. `Check-HP1VR.cmd` checks installation compatibility without starting the game.

The repository contains editable C++/Python source, build scripts, tests and tutorial WAV assets. Compiled installation files are supplied in Releases. See [BUILDING.md](docs/BUILDING.md) to compile your own build. No project-wide open-source license has been granted yet; third-party components retain their own licenses. Original game files, voice-reference recordings and voice-model weights are not distributed.

Unofficial fan project, not affiliated with the game's rights holders.

## Deutsch

Eine inoffizielle VR-Mod für **Harry Potter und der Stein der Weisen (2001, Windows-PC)**. Erkunde Hogwarts mit räumlicher Darstellung, freiem Umsehen und einem per Controller geführten Zauberstab. Die Mod ist eigenständig und benötigt Teil 2 nicht.

**Erste öffentliche Testversion: 0.1.0.** Das Projekt ist noch in Entwicklung. Verschiedene Mechaniken wurden im Originalspiel mit privaten Testprofilen geprüft; ein vollständiger Spieldurchgang und die umfassende Abnahme mit echter VR-Brille stehen noch aus. Einzelheiten: [Tests und bekannte Grenzen](docs/TESTING.md).

### Funktionen

- SteamVR/OpenVR mit getrennten Augenbildern, Kopf- und Zauberstabverfolgung, Laufen, schrittweisem Drehen, Springen und Klettern. Zielen und Zaubern funktionieren während der Bewegung.
- Originaler Flat-Modus und VR teilen sechs Mod-Speicherplätze. Grafikeinstellungen bleiben getrennt; fertig geschriebene Spielstände werden zusätzlich lokal gesichert.
- Zaubersymbole mit dem Controller nachzeichnen; Besensteuerung für Flugstunde, Quidditch, Erinnermich-Verfolgung und fliegende Schlüssel.
- Angepasste Bedienung für Schach, schwebende Gegenstände, Troll-Flucht und Fluffys Flöte.
- Inventar und Status an der linken Hand, lesbare Briefe, ursprüngliche Gegner-Energieanzeigen und räumliche Spiegel.
- Feste Kinoleinwand in einer Eingangshalle für Menüs und flache Storysequenzen.
- VR-Einstellungen im Spiel für Controllerbelegung, Kino/3D, Sprache und 1024–2048 Pixel pro Auge. Eine geänderte Auflösung gilt ab dem nächsten Start.
- Deutsche und englische Mod-Oberflächen sowie 16 Lernhinweise mit gesprochenen Varianten und markierten Controller-Tasten. Die künstlichen Stimmen sind keine neuen Aufnahmen der Originalsprecher.

### Installation und Start

1. **HP1VR-0.1.0.zip** unter [Releases](https://github.com/SwitchCube/Harry-Potter-I-VR/releases) herunterladen.
2. In den eigenen Spielordner entpacken. `Start-HP1VR.cmd` und der Ordner `HP1VR` müssen **neben dem vorhandenen Ordner `System`** liegen.
3. Für VR zuerst SteamVR starten und ein kompatibles Headset mit zwei Controllern verbinden.
4. `Start-HP1VR.cmd` öffnen, **VR** oder **Flat** wählen und starten. Im originalen Spielmenü einen freien Speicherplatz zum Beginnen oder denselben belegten Platz zum Fortsetzen auswählen.

Voraussetzung ist Windows 10/11 x64. Python und Compiler müssen zum Spielen nicht separat installiert werden. Unterstützt wird derzeit die geprüfte **deutsche HP1-Version mit Engine 433 vom 19. Oktober 2001**. Die Dateien werden mit den enthaltenen Prüfsummen verglichen. Englische Mod-Texte bedeuten keine automatische Unterstützung anderer Spielversionen.

Beim ersten Start wird unter `%LOCALAPPDATA%\HP1VR` eine private Kopie der geprüften Spieldateien eingerichtet. Dafür mindestens **1 GB zusätzlichen freien Platz** sowie weiteren Platz für Spielstände und Sicherungen vorsehen. Originalinstallation und vorhandene Original-Spielstände bleiben unverändert. Die Mod beginnt mit leeren Speicherplätzen, die Flat und VR gemeinsam verwenden. Persönliche Spielstände werden nicht importiert. Den Launcher während des Spiels geöffnet lassen. Beenden über das Spielmenü, den Beenden-Knopf oder `Stop-HP1VR.cmd`. `Check-HP1VR.cmd` prüft die Installation ohne Spielstart.

Hier liegen bearbeitbarer C++-/Python-Quellcode, Build-Skripte, Tests und Tutorial-WAVs. Die fertigen Installationsdateien gibt es unter Releases. Eine [Bauanleitung](docs/BUILDING.md) ist enthalten. Eine eigene Open-Source-Lizenz wurde zunächst nicht vergeben; Fremdkomponenten behalten ihre jeweiligen Lizenzen. Originalspiel, originale Stimmvorlagen und KI-Modellgewichte sind nicht enthalten.

Inoffizielles Fanprojekt ohne Verbindung zu den Rechteinhabern.
