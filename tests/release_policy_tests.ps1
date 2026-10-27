$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot '../tools/release-policy.ps1')
function Assert-True($Value,[string]$Message) {if (-not $Value) {throw $Message}}
function Rejects([scriptblock]$Action) {$rejected=$false;try {& $Action | Out-Null} catch {$rejected=$true};Assert-True $rejected 'Expected publication rejection was missing.'}
$info=[pscustomobject]@{version='0.1.0-preview.45';release_id='0.1.0-preview.45';source_revision=45;source_commit=('a'*40);source_dirty=$false}
$identity=Get-PreviewIdentity $info
Assert-True ($identity.tag -eq 'v0.1.0-preview.45' -and $identity.zip_name -eq 'CommunityDivers-0.1.0-preview.45-windows-x64.zip') 'Release identity is not human-readable.'
foreach ($bad in @('0.1.0-preview.01','0.1.0-preview.45+aaaaaaaa','0.1.0-preview.44','01.1.0-preview.45','1.0.0','0.1.0-preview.18446744073709551616')) {
 $copy=$info.PSObject.Copy();$copy.version=$bad;$copy.release_id=$bad;Rejects {Get-PreviewIdentity $copy}
}
$copy=$info.PSObject.Copy();$copy.source_dirty=$true;Rejects {Get-PreviewIdentity $copy}
Assert-True ((Compare-PreviewVersion '0.1.0-preview.10' '0.1.0-preview.9') -eq 1) 'Preview numbers were compared lexically.'
Assert-True ((Compare-PreviewVersion '0.2.0-preview.45' '0.1.9-preview.99') -eq 1) 'Base version ordering failed.'
$candidate=[pscustomobject]@{release_id=$info.release_id;source_revision=45;source_commit=('a'*40);sha256=('b'*64)}
$legacy=[pscustomobject]@{release_id='0.1.0-preview.1+cccccccc';source_revision=42;source_commit=('c'*40);sha256=('d'*64)}
Assert-True ((Get-PreviewPublicationDecision $candidate $legacy) -eq 'publish') 'Legacy-to-numbered upgrade rejected.'
$newer=$candidate.PSObject.Copy();$newer.source_revision=46;$newer.release_id='0.1.0-preview.46';$newer.source_commit=('e'*40)
Assert-True ((Get-PreviewPublicationDecision $candidate $newer) -eq 'superseded') 'Late old CI build would publish.'
Assert-True ((Get-PreviewPublicationDecision $candidate $candidate) -eq 'current') 'Idempotent publication failed.'
$collision=$candidate.PSObject.Copy();$collision.sha256=('f'*64);Rejects {Get-PreviewPublicationDecision $candidate $collision}
$regression=$candidate.PSObject.Copy();$regression.source_revision=47;$regression.release_id='0.0.9-preview.47';Rejects {Get-PreviewPublicationDecision $regression $candidate}
$draft=[pscustomobject]@{draft=$true;tag_name='v0.1.0-preview.46'}
$older=[pscustomobject]@{draft=$false;tag_name='v0.1.0-preview.44'}
$legacyRelease=[pscustomobject]@{draft=$false;tag_name='preview-cccccccccccc'}
Assert-True (-not (Test-PreviewSupersededByRelease $info.version @($draft,$older,$legacyRelease))) 'A draft or old release suppressed publication.'
$public=$draft.PSObject.Copy();$public.draft=$false
Assert-True (Test-PreviewSupersededByRelease $info.version @($public)) 'A newer public release without channel promotion was ignored.'
Write-Output 'PASS numbered identities, numeric ordering, legacy updates, late CI runs, collisions and version regression checks'
