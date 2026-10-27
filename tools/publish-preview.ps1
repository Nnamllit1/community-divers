# Promotes only an already-tested CI package. Never run for pull_request code.
[CmdletBinding()]
param([string]$ArtifactDirectory = 'dist-ci')
$ErrorActionPreference = 'Stop'
$PSNativeCommandUseErrorActionPreference = $true
. (Join-Path $PSScriptRoot 'release-policy.ps1')
function Assert-Native { if ($LASTEXITCODE -ne 0) { throw 'A native release command failed.' } }
$repo = $env:GITHUB_REPOSITORY
$source = $env:GITHUB_SHA
if ($repo -notmatch '^[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+$' -or $source -notmatch '^[a-f0-9]{40}$') { throw 'Invalid release repository or source SHA.' }
if ($env:GITHUB_REF -ne 'refs/heads/main') { throw 'Preview publication requires main.' }
if (-not $env:GH_TOKEN -or -not $env:CD_UPDATE_SIGNING_KEY) { throw 'Release token or publisher signing secret is missing.' }
if ($env:GITHUB_EVENT_NAME -eq 'push') {
    $before = $env:CD_BEFORE_SHA
    if ($before -notmatch '^[a-f0-9]{40}$') { throw 'Invalid previous source SHA.' }
    if ($before -match '^0+$') { $changed = @(git ls-tree -r --name-only $source); Assert-Native }
    else { $changed = @(git diff --name-only $before $source --); Assert-Native }
    $components = @($changed | Where-Object { $_ -match '^(src/|include/|tests/|cmake/|compatibility/|assets/|installer/|docs/release-notes\.md$|\.github/workflows/|tools/|CMakeLists\.txt$|CMakePresets\.json$|Build\.cmd$|Update\.cmd$)' })
    if (-not $components.Count) { Write-Host 'No component changes; preview channel stays unchanged.'; exit 0 }
    if ('docs/release-notes.md' -notin $changed) { throw 'Update docs/release-notes.md for this release before publishing component changes.' }
}
$work = Join-Path $env:RUNNER_TEMP ('community-divers-release-' + $source)
if (Test-Path -LiteralPath $work) { throw 'Release work directory already exists.' }
New-Item -ItemType Directory -Path $work | Out-Null
$zips = @(Get-ChildItem -LiteralPath $ArtifactDirectory -Filter '*.zip' -File)
if ($zips.Count -ne 1) { throw 'Expected exactly one Windows package.' }
$zip = $zips[0]
$setups = @(Get-ChildItem -LiteralPath $ArtifactDirectory -Filter 'CommunityDiversSetup-*-windows-x64.exe' -File)
if ($setups.Count -ne 1 -or $setups[0].Name.Contains('-qa-')) { throw 'Expected one production Windows installer.' }
$setup = $setups[0]
$setupChecksum = $setup.FullName + '.sha256'
$setupDigest = (Get-FileHash -LiteralPath $setup.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
if (-not (Test-Path -LiteralPath $setupChecksum) -or [IO.File]::ReadAllText($setupChecksum).Trim() -notmatch ('^' + $setupDigest + '\s+\*?' + [regex]::Escape($setup.Name) + '$')) { throw 'Installer checksum mismatch.' }
$digest = (Get-FileHash -LiteralPath $zip.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
$checksum = $zip.FullName + '.sha256'
if (-not (Test-Path -LiteralPath $checksum)) { throw 'Package checksum is missing.' }
$checksumText = [IO.File]::ReadAllText($checksum).Trim()
if ($checksumText -notmatch ('^' + $digest + '\s+\*?' + [regex]::Escape($zip.Name) + '$')) { throw 'Package checksum does not match.' }
$unpacked = Join-Path $work 'unpacked'
Expand-Archive -LiteralPath $zip.FullName -DestinationPath $unpacked
$roots = @(Get-ChildItem -LiteralPath $unpacked -Directory)
if ($roots.Count -ne 1) { throw 'Package must contain one root directory.' }
$info = Get-Content -LiteralPath (Join-Path $roots[0].FullName 'build-info.json') -Raw | ConvertFrom-Json
if ($info.source_commit -ne $source -or $info.source_dirty -ne $false -or $info.platform -ne 'windows-x64' -or $info.launcher_update_protocol -ne 1) { throw 'Package is not the clean supported CI source build.' }
$identity = Get-PreviewIdentity $info
if ($zip.Name -ne $identity.zip_name -or $setup.Name -ne $identity.setup_name) { throw 'Package names do not match the numbered release.' }
$tag = $identity.tag
$updater = Join-Path $roots[0].FullName 'CommunityDiversUpdate.exe'
$payload = [ordered]@{
    schema=1; channel='preview'; platform='windows-x64'; release_id=$info.release_id;
    source_commit=$source; source_revision=$info.source_revision; service_protocol=$info.service_protocol;
    filename=$zip.Name; bytes=$zip.Length; sha256=$digest;
    url=('https://github.com/' + $repo + '/releases/download/' + $tag + '/' + $zip.Name)
}
$payloadPath = Join-Path $work 'payload.json'
$feedPath = Join-Path $work 'preview.json'
[IO.File]::WriteAllText($payloadPath, ($payload | ConvertTo-Json -Depth 8) + "`n", [Text.UTF8Encoding]::new($false))
& $updater --sign-env $payloadPath $feedPath; Assert-Native
& $updater --verify-package $zip.FullName $feedPath (Join-Path $work 'verified'); Assert-Native
$headers = @{ Authorization=('Bearer ' + $env:GH_TOKEN); Accept='application/vnd.github+json'; 'User-Agent'='CommunityDivers-CI' }
$base = 'https://api.github.com/repos/' + $repo
function Api-Get([string]$Url) {
    try { return Invoke-RestMethod -Uri $Url -Headers $headers -Method Get }
    catch { if ([int]$_.Exception.Response.StatusCode -eq 404) { return $null }; throw 'GitHub release API request failed.' }
}
# Publication is serialized by the workflow, but queued jobs need not start in source order.
# Authenticate the channel before creating a release, and recheck before making a draft public.
function Read-CurrentChannel([string]$Stage) {
    $current = Api-Get ($base + '/contents/updates/preview.json?ref=main')
    if (-not $current) { throw 'The existing preview channel is missing.' }
    $oldFeed = Join-Path $work ('previous-' + $Stage + '.json')
    [IO.File]::WriteAllBytes($oldFeed, [Convert]::FromBase64String($current.content))
    $old = (& $updater --inspect-feed $oldFeed) | ConvertFrom-Json; Assert-Native
    return [pscustomobject]@{file=$current; payload=$old}
}
function Test-PublicationNeeded([string]$Stage) {
    $channel = Read-CurrentChannel $Stage
    $decision = Get-PreviewPublicationDecision $payload $channel.payload
    if ($decision -ne 'publish') {
        Write-Host ('Skipping publication: ' + $decision + '.')
        return $false
    }
    # A newer release may be public even if its subsequent channel promotion failed.
    $page = 1
    do {
        $releases = @(Api-Get ($base + '/releases?per_page=100&page=' + $page))
        if (Test-PreviewSupersededByRelease $identity.version $releases) {
            Write-Host 'Skipping publication: a newer numbered release is already public.'
            return $false
        }
        $page++
    } while ($releases.Count -eq 100)
    return $true
}
if (-not (Test-PublicationNeeded 'preflight')) { exit 0 }
$tagRef = Api-Get ($base + '/git/ref/tags/' + $tag)
if (-not $tagRef) {
    $tagRequest = @{ref=('refs/tags/' + $tag); sha=$source} | ConvertTo-Json
    try { Invoke-RestMethod -Uri ($base + '/git/refs') -Headers $headers -Method Post -ContentType 'application/json' -Body $tagRequest | Out-Null }
    catch { throw 'Cannot create the immutable source tag.' }
    $tagRef = Api-Get ($base + '/git/ref/tags/' + $tag)
}
if ($tagRef.object.type -ne 'commit' -or $tagRef.object.sha -ne $source) { throw 'Release tag points to a different source revision.' }
function Find-Release {
    $found = Api-Get ($base + '/releases/tags/' + $tag)
    if ($found) { return $found }
    # GitHub's release-by-tag endpoint omits unpublished drafts. Resolve those from the authorized list.
    $drafts = @(Api-Get ($base + '/releases?per_page=100') | Where-Object { $_.tag_name -eq $tag })
    if ($drafts.Count -gt 1) { throw 'More than one release draft uses this source tag.' }
    if ($drafts.Count -eq 1) { return $drafts[0] }
    return $null
}
$existing = Find-Release
$notesPath = Join-Path $work 'release-notes.md'
$notesSource = [IO.File]::ReadAllText((Join-Path $roots[0].FullName 'docs/release-notes.md'))
if ($notesSource.Length -lt 60 -or [Text.Encoding]::UTF8.GetByteCount($notesSource) -gt 65536 -or -not $notesSource.StartsWith('# ')) { throw 'A bounded, user-facing Markdown release note is required.' }
[IO.File]::WriteAllText($notesPath, $notesSource, [Text.UTF8Encoding]::new($false))
# Relative images are the same immutable source files used by the native viewer.
$notes = [regex]::Replace($notesSource, '!\[([^\]]*)\]\((images/[A-Za-z0-9_./-]+)\)', {
    param($match)
    '![' + $match.Groups[1].Value + '](https://raw.githubusercontent.com/' + $repo + '/' + $source + '/docs/' + $match.Groups[2].Value + ')'
})
$notes += "`n`n---`nDownload the **Setup executable** asset for a per-user installation, or the **ZIP** for portable use. Both include the native release-note viewer. Game integration and community co-op remain in development.`n"
function Api-Json([string]$Url, [string]$Method, $Body) {
    $encoded = [Text.Encoding]::UTF8.GetBytes(($Body | ConvertTo-Json -Depth 10))
    try { return Invoke-RestMethod -Uri $Url -Headers $headers -Method $Method -ContentType 'application/json' -Body $encoded }
    catch { throw 'GitHub release write failed; the previous channel is retained.' }
}
if (-not $existing) {
    # The create response is authoritative. A new draft may not be visible in tag/list queries yet.
    $existing = Api-Json ($base + '/releases') 'Post' @{
        tag_name=$tag; target_commitish=$source; draft=$true; prerelease=$true;
        name=$identity.title; body=$notes
    }
}
if (-not $existing.id -or $existing.tag_name -ne $tag) { throw 'Release identity mismatch.' }
if ($existing.draft) {
    $uploadBase = $existing.upload_url.Split('{')[0]
    $expectedUpload = 'https://uploads.github.com/repos/' + $repo + '/releases/' + $existing.id + '/assets'
    if ($uploadBase -ne $expectedUpload) { throw 'Unexpected GitHub asset upload endpoint.' }
    $assets = @($existing.assets)
    $allowed = @($zip.Name, [IO.Path]::GetFileName($checksum), $setup.Name, [IO.Path]::GetFileName($setupChecksum), 'release-notes.md', 'preview.json')
    if (@($assets | Where-Object { $_.name -notin $allowed }).Count) { throw 'Draft contains unexpected assets; refusing to publish them.' }
    foreach ($file in @($zip.FullName, $checksum, $setup.FullName, $setupChecksum, $notesPath, $feedPath)) {
        $name = [IO.Path]::GetFileName($file)
        foreach ($asset in @($assets | Where-Object { $_.name -eq $name })) {
            # Only unpublished draft assets can be replaced on a retry.
            try { Invoke-RestMethod -Uri ($base + '/releases/assets/' + $asset.id) -Headers $headers -Method Delete | Out-Null }
            catch { throw 'Cannot replace an unpublished draft asset.' }
        }
        $type = if ($name.EndsWith('.exe')) { 'application/vnd.microsoft.portable-executable' } elseif ($name.EndsWith('.zip')) { 'application/zip' } elseif ($name.EndsWith('.json')) { 'application/json' } else { 'text/plain' }
        try {
            Invoke-RestMethod -Uri ($uploadBase + '?name=' + [Uri]::EscapeDataString($name)) -Headers $headers -Method Post -ContentType $type -InFile $file | Out-Null
        } catch { throw 'Draft asset upload failed; no channel promotion occurred.' }
    }
    if (-not (Test-PublicationNeeded 'before-publish')) { exit 0 }
    $existing = Api-Json ($base + '/releases/' + $existing.id) 'Patch' @{draft=$false;prerelease=$true;name=$identity.title;body=$notes}
} else {
    $asset = @($existing.assets | Where-Object { $_.name -eq $zip.Name })
    if ($asset.Count -ne 1 -or ($asset[0].digest -and $asset[0].digest -ne ('sha256:' + $digest))) { throw 'Published package differs; refusing to overwrite it.' }
    $publishedSetup = @($existing.assets | Where-Object { $_.name -eq $setup.Name })
    if ($publishedSetup.Count -ne 1 -or ($publishedSetup[0].digest -and $publishedSetup[0].digest -ne ('sha256:' + $setupDigest))) { throw 'Published installer differs; refusing to overwrite it.' }
    $manifestAsset = @($existing.assets | Where-Object { $_.name -eq 'preview.json' })
    if ($manifestAsset.Count -ne 1) { throw 'Published signed manifest is missing.' }
    $publishedFeed = Join-Path $work 'published-preview.json'
    Invoke-WebRequest -Uri $manifestAsset[0].browser_download_url -OutFile $publishedFeed
    $publishedPayload = (& $updater --inspect-feed $publishedFeed) | ConvertFrom-Json; Assert-Native
    if ($publishedPayload.source_commit -ne $source -or $publishedPayload.sha256 -ne $digest) { throw 'Published signed manifest differs from this package.' }
    $feedPath = $publishedFeed
}
# Public download verification does not send the repository token across CDN redirects.
if ($existing.draft) { throw 'Release is still unpublished.' }
$publicZip = Join-Path $work 'public-package.zip'
$downloaded = $false
for ($attempt = 0; $attempt -lt 5; $attempt++) {
    try { Invoke-WebRequest -Uri $payload.url -OutFile $publicZip; $downloaded = $true; break }
    catch { if ($attempt -lt 4) { Start-Sleep -Seconds 2 } }
}
if (-not $downloaded -or (Get-FileHash -LiteralPath $publicZip -Algorithm SHA256).Hash.ToLowerInvariant() -ne $digest) { throw 'Public package download differs from the tested artifact.' }
$publicSetup = Join-Path $work 'public-setup.exe'
$downloaded = $false
for ($attempt = 0; $attempt -lt 5; $attempt++) {
    try { Invoke-WebRequest -Uri ('https://github.com/' + $repo + '/releases/download/' + $tag + '/' + $setup.Name) -OutFile $publicSetup; $downloaded = $true; break }
    catch { if ($attempt -lt 4) { Start-Sleep -Seconds 2 } }
}
if (-not $downloaded -or (Get-FileHash -LiteralPath $publicSetup -Algorithm SHA256).Hash.ToLowerInvariant() -ne $setupDigest) { throw 'Public installer differs from the tested artifact.' }
$feedBytes = [IO.File]::ReadAllBytes($feedPath)
for ($attempt = 0; $attempt -lt 3; $attempt++) {
    $channel = Read-CurrentChannel ('promote-' + $attempt)
    $decision = Get-PreviewPublicationDecision $payload $channel.payload
    if ($decision -ne 'publish') { Write-Host ('Channel promotion: ' + $decision + '.'); exit 0 }
    $request = @{message=('Promote ' + $identity.version); branch='main'; sha=$channel.file.sha; content=[Convert]::ToBase64String($feedBytes)} | ConvertTo-Json -Depth 8
    try {
        Invoke-RestMethod -Uri ($base + '/contents/updates/preview.json') -Headers $headers -Method Put -ContentType 'application/json' -Body ([Text.Encoding]::UTF8.GetBytes($request)) | Out-Null
        Write-Host ('Published and promoted ' + $tag)
        exit 0
    } catch {
        if ([int]$_.Exception.Response.StatusCode -notin @(409,422) -or $attempt -eq 2) { throw 'Signed channel promotion failed; the previous channel is retained.' }
    }
}
throw 'Channel promotion did not finish.'
