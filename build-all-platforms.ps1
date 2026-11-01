param(
    [ValidateSet("all", "windows", "macos", "macos-intel", "linux")][string]$Platform = "all",
    [switch]$InstallWindows
)
$ErrorActionPreference = "Stop"
if ($InstallWindows -and $Platform -notin @("all", "windows")) { throw "InstallWindows requires Platform all or windows." }
Set-Location $PSScriptRoot
$repository = "wzaggle/SimpleCamDirect"
if (-not (Get-Command git -ErrorAction SilentlyContinue)) { throw "Install Git for Windows first." }
if (-not (Get-Command gh -ErrorAction SilentlyContinue)) { throw "Install GitHub CLI: winget install --id GitHub.cli -e" }
& gh auth status
if ($LASTEXITCODE -ne 0) { throw "Sign in once with: gh auth login" }
$revision = & git rev-parse HEAD
if ($LASTEXITCODE -ne 0) { throw "Use a Git clone of $repository for this target. ZIP folders can use Actions -> Run workflow on GitHub." }
$dirty = & git status --porcelain
if ($LASTEXITCODE -ne 0) { throw "Cannot check working tree." }
if ($dirty) { throw "Commit and push your source changes first. Cloud builds cannot see uncommitted local edits." }
& gh api "repos/$repository/commits/$revision" --silent
if ($LASTEXITCODE -ne 0) { throw "This source revision is not on GitHub yet. Push it first." }
$existing = & gh run list --repo $repository --workflow build-all.yml --limit 20 --json databaseId
if ($LASTEXITCODE -ne 0) { throw "Cannot read workflow runs." }
$oldIds = @((ConvertFrom-Json ($existing -join "`n")) | ForEach-Object { $_.databaseId })
& gh workflow run build-all.yml --repo $repository --ref main -f "source_revision=$revision" -f "platform=$Platform"
if ($LASTEXITCODE -ne 0) { throw "Could not start Build All Platforms." }
$runId = $null
for ($attempt = 0; $attempt -lt 20; $attempt++) {
    Start-Sleep -Seconds 3
    $runs = & gh run list --repo $repository --workflow build-all.yml --event workflow_dispatch --limit 20 --json databaseId,displayTitle
    if ($LASTEXITCODE -ne 0) { throw "Cannot find the newly started workflow." }
    $newRun = (ConvertFrom-Json ($runs -join "`n")) | Where-Object { $_.databaseId -notin $oldIds -and $_.displayTitle -like "*$revision*" } | Select-Object -First 1
    if ($newRun) { $runId = $newRun.databaseId; break }
}
if (-not $runId) { throw "Workflow started but no run ID appeared. Check the GitHub Actions page." }
& gh run watch $runId --repo $repository --exit-status
if ($LASTEXITCODE -ne 0) { throw "One or more platform builds failed. Review Actions logs; successful platforms retain their individual artifacts." }
$spec = Get-Content (Join-Path $PSScriptRoot "buildspec.json") -Raw | ConvertFrom-Json
$artifact = if ($Platform -eq "all") { "SimpleCamPTZ-$($spec.version)-$($revision.Substring(0,9))-AllPlatforms" } else { "packages-$Platform" }
$destination = Join-Path $PSScriptRoot "installers\$artifact-run$runId"
& gh run download $runId --repo $repository --name $artifact --dir $destination
if ($LASTEXITCODE -ne 0) { throw "Builds succeeded but package download failed." }
Write-Output "Installers are ready: $destination"
if ($InstallWindows) {
    if (Get-Process obs64 -ErrorAction SilentlyContinue) { throw "Packages downloaded. Close OBS before installing the Windows version." }
    $setup = Get-ChildItem (Join-Path $destination "Windows") -Filter "SimpleCamPTZ-Setup-*-x64.exe" | Select-Object -First 1
    if (-not $setup) { throw "Downloaded Windows installer missing." }
    Start-Process -FilePath $setup.FullName -Wait
}
