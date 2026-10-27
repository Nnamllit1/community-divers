# Pure release identity/order rules shared by CI publication and regression checks.
function Get-PreviewVersionParts([string]$Version) {
    if ($Version -notmatch '^(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)-preview\.(0|[1-9][0-9]*)(?:\+[a-f0-9]{8})?$') {
        throw 'Unsupported preview version.'
    }
    $parts=[ordered]@{}
    foreach ($entry in @(@('major',1),@('minor',2),@('patch',3),@('preview',4))) {
        $number=[uint64]0
        if (-not [uint64]::TryParse($Matches[$entry[1]],[ref]$number)) {throw 'Preview version number is out of range.'}
        $parts[$entry[0]]=$number
    }
    return [pscustomobject]$parts
}
function Compare-PreviewVersion([string]$Left,[string]$Right) {
    $a=Get-PreviewVersionParts $Left
    $b=Get-PreviewVersionParts $Right
    foreach ($part in @('major','minor','patch','preview')) {
        if ($a.$part -gt $b.$part) {return 1}
        if ($a.$part -lt $b.$part) {return -1}
    }
    return 0
}
function Get-PreviewIdentity($Info) {
    $parts=Get-PreviewVersionParts $Info.version
    $revision=[uint64]0
    if ($Info.source_dirty -ne $false -or $Info.source_commit -notmatch '^[a-f0-9]{40}$' -or
        -not [uint64]::TryParse([string]$Info.source_revision,[ref]$revision) -or $revision -eq 0 -or
        $parts.preview -ne $revision -or $Info.release_id -ne $Info.version -or $Info.version.Contains('+')) {
        throw 'Publication requires a clean numbered preview matching its source revision.'
    }
    return [pscustomobject]@{
        version=$Info.version;revision=$revision;tag=('v'+$Info.version);
        title=('Community Divers '+$Info.version);
        zip_name=('CommunityDivers-'+$Info.version+'-windows-x64.zip');
        setup_name=('CommunityDiversSetup-'+$Info.version+'-windows-x64.exe')
    }
}
function Get-PreviewPublicationDecision($Candidate,$Current) {
    $candidateRevision=[uint64]0;$currentRevision=[uint64]0
    if (-not [uint64]::TryParse([string]$Candidate.source_revision,[ref]$candidateRevision) -or
        -not [uint64]::TryParse([string]$Current.source_revision,[ref]$currentRevision) -or
        -not $candidateRevision -or -not $currentRevision) {throw 'Invalid publication ordering.'}
    if ($currentRevision -gt $candidateRevision) {return 'superseded'}
    if ($currentRevision -eq $candidateRevision) {
        if ($Current.source_commit -ne $Candidate.source_commit -or $Current.sha256 -ne $Candidate.sha256 -or
            $Current.release_id -ne $Candidate.release_id) {throw 'Published revision identity collision.'}
        return 'current'
    }
    if ((Compare-PreviewVersion $Candidate.release_id $Current.release_id) -le 0) {
        throw 'A later source revision must also advance the public version.'
    }
    return 'publish'
}

function Test-PreviewSupersededByRelease([string]$Version, $Releases) {
    foreach ($release in $Releases) {
        if (-not $release.draft -and $release.tag_name -match '^v[0-9]+[.][0-9]+[.][0-9]+-preview[.][0-9]+$' -and
            (Compare-PreviewVersion $release.tag_name.Substring(1) $Version) -gt 0) {return $true}
    }
    return $false
}
