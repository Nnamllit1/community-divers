[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][string]$PortableZip,
    [string]$OutputDirectory='dist',
    [string]$BuildDirectory='out/windows-dev',
    [switch]$TestIdentity,
    [switch]$TestInterrupt,
    [switch]$AllowDirty
)
$ErrorActionPreference='Stop'
if (($TestInterrupt -or $AllowDirty) -and -not $TestIdentity) {
    throw 'Interruption and dirty-source fixtures require the separate QA identity.'
}
$zip=(Resolve-Path -LiteralPath $PortableZip).Path
$build=(Resolve-Path -LiteralPath $BuildDirectory).Path
$stub=Join-Path $build 'CommunityDiversSetup.exe'
if (-not (Test-Path -LiteralPath $stub -PathType Leaf)) {throw 'Build the native setup target first.'}
$info=Get-Content -LiteralPath (Join-Path $build 'build-info.json') -Raw | ConvertFrom-Json
$out=[IO.Path]::GetFullPath((Join-Path (Get-Location) $OutputDirectory))
New-Item -ItemType Directory -Path $out -Force | Out-Null
$suffix=if ($TestIdentity) {'-qa'} else {''}
if ($TestInterrupt) {$suffix+='-interrupted'}
$name='CommunityDiversSetup-'+$info.release_id.Replace('+','-')+$suffix+'-windows-x64.exe'
$destination=Join-Path $out $name
if (Test-Path -LiteralPath $destination) {throw 'Setup output already exists.'}
$arguments=@('--pack',$zip,'--output',$destination)
if ($TestIdentity) {$arguments+='--qa'}
if ($TestInterrupt) {$arguments+='--interrupt-fixture'}
if ($AllowDirty) {$arguments+='--allow-dirty'}
# Native packaging verifies source identity, ZIP paths, hashes and the full inventory.
& $stub @arguments | Out-Host
if ($LASTEXITCODE -ne 0) {throw 'Native setup packaging failed.'}
$digest=(Get-FileHash -LiteralPath $destination -Algorithm SHA256).Hash.ToLowerInvariant()
[IO.File]::WriteAllText(($destination+'.sha256'),($digest+'  '+$name+"`n"),[Text.UTF8Encoding]::new($false))
Write-Host ('Built '+$destination)
