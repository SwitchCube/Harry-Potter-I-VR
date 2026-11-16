[CmdletBinding()]
param([ValidateScript({$_ -eq 0 -or ($_ -ge 15 -and $_ -le 180)})][int]$Seconds=0,
      [ValidateSet(1280,1536,2048)][int]$Resolution=2048,
      [switch]$Lesson,
      [switch]$Flight)
$ErrorActionPreference='Stop'
$projectRoot=Split-Path -Parent $PSScriptRoot
Set-Location -LiteralPath $projectRoot
try {
    if(Get-Process HP -ErrorAction SilentlyContinue){throw 'Harry Potter laeuft bereits. Bitte diese Sitzung zuerst regulär beenden.'}
    if(-not (Get-Process vrserver -ErrorAction SilentlyContinue)){
        throw 'SteamVR ist nicht gestartet. Bitte SteamVR starten, die Quest mit Steam Link verbinden und die Brille aufsetzen. Danach Test-VR.cmd erneut starten.'
    }
    if(-not (Get-Command python -ErrorAction SilentlyContinue)){throw 'Der Entwicklungs-Teststarter braucht die lokale Python-Installation.'}
    Write-Host 'Harry Potter I VR - interner Entwicklungsstand'
    if($Seconds -eq 0){
        Write-Host 'Keine automatische Zeitbegrenzung. Beenden: Stop-VR.cmd oder Alt+F4 im Spielfenster.'
    } else {
        Write-Host "Begrenzter Diagnosetest: Das Spiel wird nach $Seconds Sekunden automatisch geschlossen."
    }
    Write-Host 'Eigenes Testprofil, keine Originalspielstaende. Dieses Fenster waehrend des Tests offen lassen.'
    Write-Host 'Linker Stick: laufen. Rechter Stick: 45 Grad drehen. A: springen.'
    Write-Host 'Rechten Trigger halten: zielen; loslassen: zaubern. X: Szene abkuerzen.'
    Write-Host 'Linke untere Grifftaste halten: Energie, Bohnen, Sterne, Karten, Feuersamen und Hauspunkte.'
    Write-Host 'B: Menuebuch. Rechts zeigen und Trigger druecken: auswaehlen. Linker Stickklick: neu zentrieren.'
    Write-Host 'Noch kein vollstaendiger Spieldurchgang: weitere Lernaufgaben, Flugspiele und Speicherablauf sind in Arbeit.'
    Write-Host "VR-Aufloesung: $Resolution x $Resolution pro Auge."
    & python -B (Join-Path $PSScriptRoot 'stage_probe.py') --resolution $Resolution
    if($LASTEXITCODE -ne 0){throw 'Testprofil konnte nicht vorbereitet werden.'}
    if($Lesson -and $Flight){throw 'Bitte entweder Zauberuebung oder Flugstunde waehlen.'}
    $probeArgs=@('--mode','native-vr','--seconds',"$Seconds")
    if($Flight){$probeArgs+=@('--map','Lev_Tut2.unr');Write-Host 'Flugstunde: linker Stick lenken/steigen/sinken; loslassen: waagerecht. Trigger schneller; A halten: langsam und genau. Flugmarkierung auf den Ring ausrichten. X Szene abkuerzen, B Menue.'}
    if($Lesson){$probeArgs+='--start-at-lesson';Write-Host 'Nach der Einleitung startet direkt die erste Zauberuebung. X kuerzt die Einleitung ab.'}
    Write-Host 'Zauberuebung: mit dem rechten Stab zeigen, Trigger halten zum Zeichnen, loslassen zum Bewerten.'
    & python -B (Join-Path $PSScriptRoot 'run_probe.py') @probeArgs
    if($LASTEXITCODE -ne 0){throw 'Der VR-Test wurde nicht erfolgreich abgeschlossen. Die Diagnose steht im Projektordner unter logs und cache/profiles.'}
    if($Seconds -eq 0){Write-Host 'Sitzung beendet. Originaldateien wurden erneut geprueft.'}
    else {Write-Host "Diagnosetest beendet (Zeitlimit: $Seconds Sekunden). Originaldateien wurden erneut geprueft."}
} catch { Write-Host $_.Exception.Message -ForegroundColor Red; exit 1 }
