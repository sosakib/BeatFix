$proc = Start-Process -FilePath ".\build\BeatFix_artefacts\Debug\BeatFix.exe" -PassThru
Start-Sleep -Seconds 2
$is_alive = -not $proc.HasExited
Write-Host "Process running: $is_alive (PID: $($proc.Id))"
if ($is_alive) {
    $proc.Kill()
    Write-Host "Process successfully terminated."
} else {
    Write-Host "Process exited unexpectedly with code $($proc.ExitCode)"
}
