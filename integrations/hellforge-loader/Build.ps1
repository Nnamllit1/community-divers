# GPL-3.0-only with the additional mod permission in LICENSE-EXCEPTION.
# Builds the isolated native component and synthetic fixtures, never the game.
param([string]$CMake, [string]$Ninja)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$ProgressPreference = 'SilentlyContinue'
$projectRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$lock = Get-Content -LiteralPath (Join-Path $PSScriptRoot 'upstream.lock.json') -Raw | ConvertFrom-Json
if ($lock.schema -ne 1) { throw 'Unsupported dependency lock' }
$toolRoot = Join-Path $projectRoot '.local/tools/hellforge-loader'
$buildRoot = Join-Path $projectRoot 'out/hellforge-loader'
[IO.Directory]::CreateDirectory($toolRoot) | Out-Null
[IO.Directory]::CreateDirectory($buildRoot) | Out-Null
# A failed reconfigure/build must not leave a previous successful receipt looking current.
$verificationPath = Join-Path $buildRoot 'verification.json'
[ordered]@{ schema=1; state='unverified'; synthetic_tests_passed=$false; deployable=$false; current_client_compatible=$false } |
    ConvertTo-Json | Set-Content -LiteralPath $verificationPath -Encoding UTF8

if (-not $CMake -or -not $Ninja) {
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
    if (-not (Test-Path -LiteralPath $vswhere)) { throw 'Pass -CMake and -Ninja paths, or install Visual Studio CMake tools.' }
    $vs = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.CMake.Project -property installationPath
    if (-not $vs) { throw 'Visual Studio CMake tools not found; pass -CMake and -Ninja.' }
    if (-not $CMake) { $CMake = Join-Path $vs 'Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe' }
    if (-not $Ninja) { $Ninja = Join-Path $vs 'Common7/IDE/CommonExtensions/Microsoft/CMake/Ninja/ninja.exe' }
}
$CMake = (Resolve-Path -LiteralPath $CMake).Path
$Ninja = (Resolve-Path -LiteralPath $Ninja).Path
$CTest = Join-Path (Split-Path -Parent $CMake) 'ctest.exe'
if (-not (Test-Path -LiteralPath $CTest)) { throw 'ctest.exe must be beside cmake.exe' }

$archive = Join-Path $toolRoot ('zig-' + $lock.zig.sha256 + '.zip')
if (-not (Test-Path -LiteralPath $archive)) {
    $partial = $archive + '.' + [Guid]::NewGuid().ToString('N') + '.partial'
    Write-Output ('Downloading pinned Zig ' + $lock.zig.version)
    Invoke-WebRequest -Uri $lock.zig.url -OutFile $partial -UseBasicParsing
    if ((Get-FileHash -LiteralPath $partial -Algorithm SHA256).Hash -ine $lock.zig.sha256) { throw 'Zig archive checksum mismatch; partial retained for inspection' }
    Move-Item -LiteralPath $partial -Destination $archive
}
if ((Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash -ine $lock.zig.sha256) { throw 'Cached Zig archive checksum mismatch' }
$zigRoot = Join-Path $toolRoot ('zig-' + $lock.zig.sha256.Substring(0,16))
$zigExe = Join-Path $zigRoot 'zig.exe'
Add-Type -AssemblyName System.IO.Compression.FileSystem
$zip = [IO.Compression.ZipFile]::OpenRead($archive)
try {
    # Validate all paths before extracting a pinned archive into a new directory.
    $seen = New-Object 'System.Collections.Generic.HashSet[string]' ([StringComparer]::OrdinalIgnoreCase)
    foreach ($entry in $zip.Entries) {
        $parts = $entry.FullName.Replace('\','/').Split('/')
        if ($parts[0] -cne $lock.zig.directory -or $parts -contains '..' -or $parts -contains '.' -or
            $entry.FullName.Contains(':') -or (($entry.ExternalAttributes -shr 16) -band 0xF000) -eq 0xA000) { throw 'Unsafe tool archive member' }
        if (-not $seen.Add($entry.FullName.TrimEnd('/'))) { throw 'Duplicate tool archive member' }
    }
    $compilerEntry = $zip.GetEntry($lock.zig.directory + '/zig.exe')
    if (-not $compilerEntry) { throw 'Pinned compiler missing from archive' }
    $stream = $compilerEntry.Open(); $sha = [Security.Cryptography.SHA256]::Create()
    try { $compilerHash = ([BitConverter]::ToString($sha.ComputeHash($stream))).Replace('-','').ToLowerInvariant() }
    finally { $sha.Dispose(); $stream.Dispose() }
} finally { $zip.Dispose() }
if (-not (Test-Path -LiteralPath $zigExe)) {
    if (Test-Path -LiteralPath $zigRoot) { throw 'Incomplete tool cache; preserve it and choose a fresh cache directory' }
    $staging = Join-Path $toolRoot ('extract-' + [Guid]::NewGuid().ToString('N'))
    [IO.Compression.ZipFile]::ExtractToDirectory($archive, $staging)
    $source = [IO.Path]::GetFullPath((Join-Path $staging $lock.zig.directory))
    $destination = [IO.Path]::GetFullPath($zigRoot)
    $allowedRoot = [IO.Path]::GetFullPath($toolRoot).TrimEnd('\') + '\'
    if (-not $source.StartsWith($allowedRoot,[StringComparison]::OrdinalIgnoreCase) -or
        -not $destination.StartsWith($allowedRoot,[StringComparison]::OrdinalIgnoreCase)) { throw 'Tool move escaped its cache' }
    Move-Item -LiteralPath $source -Destination $destination
}
if ((Get-FileHash -LiteralPath $zigExe -Algorithm SHA256).Hash -ine $compilerHash) { throw 'Cached compiler differs from pinned archive' }
$version = & $zigExe version
if ($LASTEXITCODE -ne 0 -or $version -cne $lock.zig.version) { throw 'Unexpected compiler version' }
$previousCache = $env:ZIG_GLOBAL_CACHE_DIR
try {
    $env:ZIG_GLOBAL_CACHE_DIR = Join-Path $toolRoot 'cache'
    & $CMake -S $PSScriptRoot -B $buildRoot -G Ninja "-DCMAKE_MAKE_PROGRAM=$Ninja" `
        "-DCMAKE_C_COMPILER=$zigExe" '-DCMAKE_C_COMPILER_ARG1=cc' `
        '-DCMAKE_C_FLAGS=-target x86_64-windows-gnu' '-DCMAKE_BUILD_TYPE=Release' '-DBUILD_TESTING=ON'
    if ($LASTEXITCODE -ne 0) { throw 'Loader configure failed' }
    & $CMake --build $buildRoot --parallel 2
    if ($LASTEXITCODE -ne 0) { throw 'Loader build failed' }
    $testJson = & $CTest --test-dir $buildRoot --show-only=json-v1
    if ($LASTEXITCODE -ne 0) { throw 'Cannot inspect loader fixtures' }
    $testPlan = ($testJson -join "`n") | ConvertFrom-Json
    $expected = @('hellforge_synthetic_stats','hellforge_loader_smoke')
    if (@($testPlan.tests).Count -ne $expected.Count) { throw 'Expected loader fixtures are missing' }
    foreach ($name in $expected) { if ($testPlan.tests.name -notcontains $name) { throw ('Missing fixture: ' + $name) } }
    & $CTest --test-dir $buildRoot --output-on-failure
    if ($LASTEXITCODE -ne 0) { throw 'Loader fixtures failed' }
    $info = Get-Content -LiteralPath (Join-Path $buildRoot 'build-info.json') -Raw | ConvertFrom-Json
    $info | Add-Member -NotePropertyName state -NotePropertyValue 'verified_synthetic'
    $info | Add-Member -NotePropertyName compiler_sha256 -NotePropertyValue $compilerHash
    $info | Add-Member -NotePropertyName loader_sha256 -NotePropertyValue ((Get-FileHash -LiteralPath (Join-Path $buildRoot 'bin/hellforge_modloader.dll') -Algorithm SHA256).Hash.ToLowerInvariant())
    $info | Add-Member -NotePropertyName synthetic_tests_passed -NotePropertyValue $true
    $info | Add-Member -NotePropertyName test_names -NotePropertyValue $expected
    $info | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $verificationPath -Encoding UTF8
    Write-Output 'Standalone GPL loader built and synthetic tests passed. No game integration was enabled.'
} finally { $env:ZIG_GLOBAL_CACHE_DIR = $previousCache }
