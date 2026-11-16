[CmdletBinding()]
param()
$ErrorActionPreference='Stop'
$projectRoot=Split-Path -Parent $PSScriptRoot
try {
    $python = (Get-Command python -ErrorAction Stop).Source
    $pythonw = Join-Path (Split-Path -Parent $python) 'pythonw.exe'
    if(-not (Test-Path -LiteralPath $pythonw)){$pythonw=$python}
    $env:PYTHONDONTWRITEBYTECODE='1'
    Start-Process -FilePath $pythonw -ArgumentList @('-B',('"'+(Join-Path $PSScriptRoot 'launcher.py')+'"')) -WorkingDirectory $projectRoot -WindowStyle Hidden
} catch {
    Add-Type -AssemblyName System.Windows.Forms
    [System.Windows.Forms.MessageBox]::Show($_.Exception.Message,'Harry Potter I - Starter') | Out-Null
    exit 1
}
