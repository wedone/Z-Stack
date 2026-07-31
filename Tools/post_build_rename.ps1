# 编译后重命名固件脚本
# 用法: powershell -ExecutionPolicy Bypass -File tools\post_build_rename.ps1 [-Version "0.1.0"]
# 从zcl_samplesw_data.c中读取SwBuildId版本号, 将HGZBSwitch.hex复制为HGZBSwitch_vX.Y.Z.hex

param(
    [string]$Version = ""
)

$ErrorActionPreference = "Stop"

# 如果未指定版本号, 从源码中读取
if ($Version -eq "") {
    $sourceFile = "Projects\zstack\HomeAutomation\HGZBSwitch\Source\zcl_samplesw_data.c"
    $content = Get-Content $sourceFile -Raw
    # 提取SwBuildId行中所有单引号内的字符
    $line = ($content -split "`n") | Where-Object { $_ -match "SwBuildId\[\]" } | Select-Object -First 1
    if ($line) {
        $chars = [regex]::Matches($line, "'(.)'")
        $fullStr = ""
        foreach ($c in $chars) { $fullStr += $c.Groups[1].Value }
        if ($fullStr -match "^HA-SPA4C1-(.+)$") {
            $Version = $matches[1]
        }
    }
    if ($Version -eq "") {
        Write-Host "无法从源码中读取版本号, 请用 -Version 参数指定" -ForegroundColor Red
        exit 1
    }
}

$hexPath = "Projects\zstack\HomeAutomation\HGZBSwitch\CC2530DB\RouterEB\Exe\HGZBSwitch.hex"
$binPath = "Projects\zstack\HomeAutomation\HGZBSwitch\CC2530DB\RouterEB\Exe\HGZBSwitch.bin"

if (-not (Test-Path $hexPath)) {
    Write-Host "固件未找到: $hexPath" -ForegroundColor Red
    exit 1
}

$versionedHex = $hexPath -replace "HGZBSwitch\.hex$", "HGZBSwitch_v$Version.hex"
$versionedBin = $binPath -replace "HGZBSwitch\.bin$", "HGZBSwitch_v$Version.bin"

Copy-Item $hexPath $versionedHex -Force
if (Test-Path $binPath) {
    Copy-Item $binPath $versionedBin -Force
    Write-Host "固件已重命名:" -ForegroundColor Green
    Write-Host "  $versionedHex"
    Write-Host "  $versionedBin"
} else {
    Write-Host "固件已重命名:" -ForegroundColor Green
    Write-Host "  $versionedHex"
}
