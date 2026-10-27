[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][string]$PortableZip,
    [string]$OutputDirectory='dist',
    [switch]$TestIdentity,
    [switch]$AllowDirty
)
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
$zip=(Resolve-Path -LiteralPath $PortableZip).Path
$out=[IO.Path]::GetFullPath((Join-Path (Get-Location) $OutputDirectory))
New-Item -ItemType Directory -Path $out -Force | Out-Null
$work=Join-Path ([IO.Path]::GetTempPath()) ('community-divers-setup-'+[Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $work | Out-Null
$unpack=Join-Path $work 'package'
Expand-Archive -LiteralPath $zip -DestinationPath $unpack
$directories=@(Get-ChildItem -LiteralPath $unpack -Directory)
if($directories.Count -ne 1){throw 'Setup package must have exactly one root.'}
$payload=$directories[0].FullName
$info=Get-Content -LiteralPath (Join-Path $payload 'build-info.json') -Raw | ConvertFrom-Json
if($info.platform -ne 'windows-x64' -or (-not $AllowDirty -and $info.source_dirty)){throw 'Setup requires a clean Windows package.'}
$inventory=Get-Content -LiteralPath (Join-Path $payload 'install-inventory.json') -Raw | ConvertFrom-Json
if($inventory.schema -ne 1 -or $inventory.files.Count -gt 4096){throw 'Unsupported setup inventory.'}
$known=@{}
foreach($file in $inventory.files){
    $relative=[string]$file.path
    if($relative -notmatch '^[A-Za-z0-9_./+ -]+$' -or $relative.Contains('..') -or $relative.StartsWith('/') -or $known.ContainsKey($relative)){throw 'Unsafe setup file path.'}
    $path=Join-Path $payload $relative
    if(-not (Test-Path -LiteralPath $path -PathType Leaf) -or (Get-Item -LiteralPath $path).Length -ne $file.bytes -or (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant() -ne $file.sha256){throw 'Setup inventory content mismatch.'}
    $known[$relative]=$true
}
foreach($file in Get-ChildItem -LiteralPath $payload -File -Recurse){
    $relative=$file.FullName.Substring($payload.Length+1).Replace('\','/')
    if($relative -ne 'install-inventory.json' -and -not $known.ContainsKey($relative)){throw 'Unexpected setup file.'}
}
$cache=Join-Path $root '.local/tools/nsis/3.13'
New-Item -ItemType Directory -Path $cache -Force | Out-Null
$archive=Join-Path $cache 'nsis-3.13.zip'
$sha='ba63dffc4410ee89193e1cb5a41989991bd77c61068da17e3156d136b7b0b3d8'
$verified=(Test-Path -LiteralPath $archive -PathType Leaf) -and (Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash.ToLowerInvariant() -eq $sha
if(-not $verified){
    # Some SourceForge routes return an HTML mirror page with HTTP 200. Never cache or run it.
    $sources=@(
        'https://downloads.sourceforge.net/project/nsis/NSIS%203/3.13/nsis-3.13.zip',
        'https://altushost-net.dl.sourceforge.net/project/nsis/NSIS%203/3.13/nsis-3.13.zip',
        'https://prdownloads.sourceforge.net/nsis/nsis-3.13.zip'
    )
    foreach($source in $sources){
        $candidate=Join-Path $work ('nsis-download-'+[Guid]::NewGuid().ToString('N')+'.zip')
        try {
            Invoke-WebRequest -Uri $source -OutFile $candidate -UseBasicParsing -UserAgent 'CommunityDiversBuild/1.0' -TimeoutSec 30 -MaximumRedirection 5
            $digest=(Get-FileHash -LiteralPath $candidate -Algorithm SHA256).Hash.ToLowerInvariant()
            if($digest -ne $sha){
                Write-Warning ('NSIS mirror returned '+(Get-Item -LiteralPath $candidate).Length+' bytes with an unexpected digest; trying another official route.')
                continue
            }
            Move-Item -LiteralPath $candidate -Destination $archive -Force
            $verified=$true
            break
        } catch { Write-Warning 'NSIS mirror download failed; trying another official route.' }
    }
}
if(-not $verified){throw 'No official NSIS download matched the pinned compiler digest.'}
# Each build uses a fresh extraction from the pinned archive.
$sdk=Join-Path $work 'nsis'
Expand-Archive -LiteralPath $archive -DestinationPath $sdk
$compiler=Join-Path $sdk 'nsis-3.13/makensis.exe'
function Escape-Nsis([string]$text){return $text.Replace('$','$$').Replace('"','$\"')}
$list=Join-Path $work 'files.nsh'
$lines=[Collections.Generic.List[string]]::new()
foreach($file in @($inventory.files)+@([pscustomobject]@{path='install-inventory.json'})){
    $relative=([string]$file.path).Replace('/','\')
    $parent=Split-Path $relative -Parent
    $lines.Add('SetOutPath "$INSTDIR'+$(if($parent){'\'+(Escape-Nsis $parent)}else{''})+'"')
    $lines.Add('File "'+(Escape-Nsis (Join-Path $payload $relative))+'"')
}
[IO.File]::WriteAllLines($list,$lines,[Text.UTF8Encoding]::new($false))
$label='Community Divers'
$key='CommunityDivers'
$suffix=''
if($TestIdentity){$key+='-QA';$label+=' QA';$suffix='-qa'}
$name='CommunityDiversSetup-'+$info.release_id.Replace('+','-')+$suffix+'-windows-x64.exe'
$destination=Join-Path $out $name
if(Test-Path -LiteralPath $destination){throw 'Setup output already exists.'}
$compilerArgs=@('/NOCONFIG','/WX','/V2',('/DCD_NAME='+$label),('/DCD_REGKEY='+$key),('/DCD_VERSION='+$info.release_id),('/DCD_PAYLOAD='+$payload),('/DCD_FILELIST='+$list),('/DCD_OUTPUT='+$destination),(Join-Path $root 'installer/community-divers.nsi'))
& $compiler @compilerArgs
if($LASTEXITCODE -ne 0){throw 'NSIS compilation failed.'}
$digest=(Get-FileHash -LiteralPath $destination -Algorithm SHA256).Hash.ToLowerInvariant()
[IO.File]::WriteAllText(($destination+'.sha256'),($digest+'  '+$name+"`n"),[Text.UTF8Encoding]::new($false))
Write-Host ('Built '+$destination)
