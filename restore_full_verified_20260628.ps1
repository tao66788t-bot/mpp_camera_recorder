$proj = "D:\BaiduNetdiskDownload\全志mpp文档资料和源代码\mpp_camera_recorder"
$bak = Join-Path $proj "backup_20260628_full_verified"

if (-not (Test-Path $bak)) {
    throw "Backup directory not found: $bak"
}

Copy-Item -LiteralPath (Join-Path $bak "src") -Destination $proj -Recurse -Force
Copy-Item -LiteralPath (Join-Path $bak "include") -Destination $proj -Recurse -Force
Copy-Item -LiteralPath (Join-Path $bak "conf") -Destination $proj -Recurse -Force
Copy-Item -LiteralPath (Join-Path $bak "docs") -Destination $proj -Recurse -Force
Copy-Item -LiteralPath (Join-Path $bak "Makefile") -Destination $proj -Force
Copy-Item -LiteralPath (Join-Path $bak "AI交接文档.md") -Destination $proj -Force
Get-ChildItem -LiteralPath $bak -Filter "*.sh" | Copy-Item -Destination $proj -Force

Write-Host "Restored source baseline from $bak"
Write-Host "Next step on Ubuntu:"
Write-Host "cd ~/mpp_camera_recorder && make clean && make"
