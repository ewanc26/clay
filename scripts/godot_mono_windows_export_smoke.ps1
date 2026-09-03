param(
    [string]$Godot = $env:GODOT_MONO_BIN,
    [string]$NativeLibrary = $env:CLAY_NATIVE_LIBRARY
)

$ErrorActionPreference = 'Stop'

$repoDir = Split-Path -Parent $PSScriptRoot
$projectDir = Join-Path $repoDir 'integrations\godot-mono'

if ([string]::IsNullOrWhiteSpace($Godot)) {
    $command = Get-Command godot-mono -ErrorAction SilentlyContinue
    if (-not $command) { $command = Get-Command godot -ErrorAction SilentlyContinue }
    if ($command) { $Godot = $command.Source }
}
if ([string]::IsNullOrWhiteSpace($Godot) -or -not (Test-Path $Godot)) {
    throw "Godot .NET editor not found: $Godot"
}

if ([string]::IsNullOrWhiteSpace($NativeLibrary)) {
    $candidates = @(
        (Join-Path $repoDir 'build\Debug\clay_engine.dll'),
        (Join-Path $repoDir 'build\clay_engine.dll')
    )
    $NativeLibrary = $candidates | Where-Object { Test-Path $_ } | Select-Object -First 1
}
if ([string]::IsNullOrWhiteSpace($NativeLibrary) -or -not (Test-Path $NativeLibrary)) {
    throw "Clay native library not found: $NativeLibrary"
}

$tempRoot = if ($env:RUNNER_TEMP) { $env:RUNNER_TEMP } else { [IO.Path]::GetTempPath() }
$stageDir = Join-Path $tempRoot "clay-godot-windows-export-$PID"
$exportDir = Join-Path $stageDir 'export'
New-Item -ItemType Directory -Force -Path $exportDir | Out-Null

try {
    & dotnet build (Join-Path $projectDir 'ClayGodotSample.csproj') --nologo --ignore-failed-sources
    if ($LASTEXITCODE -ne 0) { throw "Godot sample managed build failed ($LASTEXITCODE)" }

    $editorLog = Join-Path $stageDir 'editor.log'
    & $Godot --headless --path $projectDir --editor --build-solutions --quit-after 30 2>&1 |
        Tee-Object -FilePath $editorLog
    if ($LASTEXITCODE -ne 0) { throw "Godot editor build failed ($LASTEXITCODE)" }

    $exportPath = Join-Path $exportDir 'ClayGodotSample.exe'
    $exportLog = Join-Path $stageDir 'export.log'
    & $Godot --headless --path $projectDir --export-debug 'Windows Desktop' $exportPath 2>&1 |
        Tee-Object -FilePath $exportLog
    if ($LASTEXITCODE -ne 0) { throw "Godot Windows export failed ($LASTEXITCODE)" }
    if (-not (Test-Path $exportPath)) { throw "Godot did not produce $exportPath" }

    Copy-Item -Force $NativeLibrary (Join-Path $exportDir 'clay_engine.dll')

    $runLog = Join-Path $stageDir 'player.log'
    & $exportPath --headless --quit-after 30 2>&1 | Tee-Object -FilePath $runLog
    if ($LASTEXITCODE -ne 0) { throw "Exported Godot player failed ($LASTEXITCODE)" }

    $errors = 'Cannot instantiate C# script|Cannot load|Unable to load|DllNotFoundException|EntryPointNotFoundException|BadImageFormatException|Clay runtime creation failed|Clay error:'
    if (Select-String -Path $runLog -Pattern $errors -Quiet) {
        throw 'Exported Godot player reported a native/managed load error'
    }
    if (-not (Select-String -Path $runLog -SimpleMatch 'Clay Godot sample rendered native frame' -Quiet)) {
        throw 'Exported Godot player did not render a frame through Clay'
    }

    Write-Host 'Godot Windows exported-player smoke test passed'
}
finally {
    if (Test-Path $stageDir) { Remove-Item -Recurse -Force $stageDir }
}
