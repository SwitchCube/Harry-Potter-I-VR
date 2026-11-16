[CmdletBinding()]
param([switch]$SelfTest)
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing
$packageRoot = $PSScriptRoot
$gameRoot = Split-Path -Parent $packageRoot
$pythonPath = Join-Path $packageRoot 'python/python.exe'
$entryPath = Join-Path $packageRoot 'scripts/portable_launcher.py'
$german = [System.Globalization.CultureInfo]::CurrentUICulture.TwoLetterISOLanguageName -eq 'de'
function Text([string]$de, [string]$en) { if ($german) { $de } else { $en } }
$script:gameProcess = $null
$script:stdoutTask = $null
$script:stderrTask = $null
$form = New-Object System.Windows.Forms.Form
$form.Text = 'Harry Potter I VR 0.1.0'
$form.ClientSize = New-Object System.Drawing.Size(580, 420)
$form.StartPosition = 'CenterScreen'
$form.FormBorderStyle = 'FixedDialog'
$form.MaximizeBox = $false
$form.Font = New-Object System.Drawing.Font('Segoe UI', 11)
$form.BackColor = [System.Drawing.Color]::FromArgb(16, 24, 39)
$form.ForeColor = [System.Drawing.Color]::WhiteSmoke
function Label([string]$text, [int]$y, [int]$height) {
    $label = New-Object System.Windows.Forms.Label
    $label.Text = $text
    $label.Location = New-Object System.Drawing.Point(25, $y)
    $label.Size = New-Object System.Drawing.Size(530, $height)
    $label.TextAlign = 'MiddleCenter'
    $form.Controls.Add($label)
    return $label
}
$title = Label 'HARRY POTTER I VR' 20 45
$title.Font = New-Object System.Drawing.Font('Georgia', 23)
$title.ForeColor = [System.Drawing.Color]::FromArgb(235, 204, 134)
$subtitle = Label (Text 'Der Stein der Weisen' "The Philosopher's Stone") 66 30
$mode = New-Object System.Windows.Forms.ComboBox
$mode.DropDownStyle = 'DropDownList'
$mode.Items.AddRange(@('VR - SteamVR', 'Flat - Original'))
$mode.SelectedIndex = 0
$mode.Location = New-Object System.Drawing.Point(100, 112)
$mode.Size = New-Object System.Drawing.Size(380, 32)
$form.Controls.Add($mode)
$hint = Label (Text "Fuer VR zuerst SteamVR starten und das Headset verbinden.`nFlat und VR teilen den Fortschritt in separaten Mod-Spielstaenden." "For VR, start SteamVR and connect the headset first.`nFlat and VR share progress in separate mod save slots.") 155 65
$start = New-Object System.Windows.Forms.Button
$start.Text = Text 'Spiel starten' 'Start game'
$start.Location = New-Object System.Drawing.Point(100, 230)
$start.Size = New-Object System.Drawing.Size(380, 45)
$start.BackColor = [System.Drawing.Color]::FromArgb(215, 181, 109)
$start.ForeColor = [System.Drawing.Color]::Black
$form.Controls.Add($start)
$status = Label (Text 'Testversion. Der erste Start richtet eine private Spielkopie ein.' 'Preview build. First launch prepares a private game copy.') 285 65
$stop = New-Object System.Windows.Forms.Button
$stop.Text = Text 'Spiel beenden' 'Stop game'
$stop.Location = New-Object System.Drawing.Point(100, 360)
$stop.Size = New-Object System.Drawing.Size(380, 35)
$stop.Enabled = $false
$form.Controls.Add($stop)
function Start-Worker([string]$arguments) {
    $info = New-Object System.Diagnostics.ProcessStartInfo
    $info.FileName = $pythonPath
    $info.Arguments = '-B "' + $entryPath + '" ' + $arguments
    $info.WorkingDirectory = $gameRoot
    $info.UseShellExecute = $false
    $info.CreateNoWindow = $true
    $info.RedirectStandardOutput = $true
    $info.RedirectStandardError = $true
    $process = New-Object System.Diagnostics.Process
    $process.StartInfo = $info
    [void]$process.Start()
    return $process
}
$start.Add_Click({
    try {
        $selectedMode = if ($mode.SelectedIndex -eq 0) { 'VR' } else { 'Flat' }
        $script:gameProcess = Start-Worker ('--mode ' + $selectedMode)
        $script:stdoutTask = $script:gameProcess.StandardOutput.ReadToEndAsync()
        $script:stderrTask = $script:gameProcess.StandardError.ReadToEndAsync()
        $start.Enabled = $false; $mode.Enabled = $false; $stop.Enabled = $true
        $status.Text = Text 'Vorbereitung / Spiel laeuft. Der erste Start kann etwas dauern.' 'Preparing / game running. First launch can take a little longer.'
    } catch { [void][System.Windows.Forms.MessageBox]::Show($_.Exception.Message, $form.Text) }
})
$stop.Add_Click({
    try {
        $worker = Start-Worker '--stop'
        $output = $worker.StandardOutput.ReadToEnd()
        $errors = $worker.StandardError.ReadToEnd()
        $worker.WaitForExit()
        if ($worker.ExitCode -ne 0) { throw $errors }
        $status.Text = if (($output | ConvertFrom-Json).stop_requested) {
            Text 'Das Spiel wird regulaer geschlossen.' 'Closing the game normally.'
        } else { Text 'Der Start wird noch vorbereitet. Bitte kurz warten.' 'Still preparing. Please wait.' }
        $worker.Dispose()
    } catch { [void][System.Windows.Forms.MessageBox]::Show($_.Exception.Message, $form.Text) }
})
$timer = New-Object System.Windows.Forms.Timer
$timer.Interval = 250
$timer.Add_Tick({
    if ($null -ne $script:gameProcess -and $script:gameProcess.HasExited) {
        $script:gameProcess.WaitForExit()
        $errors = $script:stderrTask.Result
        if ($script:gameProcess.ExitCode -ne 0) {
            $status.Text = Text 'Start oder Spielsitzung fehlgeschlagen.' 'Launch or game session failed.'
            [void][System.Windows.Forms.MessageBox]::Show($errors, $form.Text)
        } else { $status.Text = Text 'Sitzung beendet. Fortschritt bleibt erhalten.' 'Session ended. Progress has been kept.' }
        $script:gameProcess.Dispose(); $script:gameProcess = $null
        $start.Enabled = $true; $mode.Enabled = $true; $stop.Enabled = $false
        $form.WindowState = 'Normal'
    }
})
$form.Add_FormClosing({
    if ($null -ne $script:gameProcess -and -not $script:gameProcess.HasExited) {
        $_.Cancel = $true
        $form.WindowState = 'Minimized'
    }
})
if ($SelfTest) {
    if ($form.Controls.Count -ne 7 -or $mode.Items.Count -ne 2) { throw 'Launcher control check failed.' }
    $form.CreateControl()
    $form.Dispose(); $timer.Dispose()
    Write-Output 'Portable launcher controls initialized successfully.'
} else {
    $timer.Start()
    [void]$form.ShowDialog()
    $timer.Dispose(); $form.Dispose()
}
