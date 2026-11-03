# Plays the agent task-completion chime. Runs from any working directory.
$ErrorActionPreference = 'Stop'
$wav = Join-Path $PSScriptRoot 'sounds\task-complete.wav'
$player = New-Object System.Media.SoundPlayer $wav
$player.PlaySync()
