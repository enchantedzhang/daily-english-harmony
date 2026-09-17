param(
  [string]$DevEcoRoot = 'C:\Program Files\Huawei\DevEco Studio',
  [switch]$AllowUnsigned
)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$devRoot = [System.IO.Path]::GetFullPath($DevEcoRoot)
$nodeBinary = Join-Path $devRoot 'tools/node/node.exe'
$hvigorScript = Join-Path $devRoot 'tools/hvigor/bin/hvigorw.js'
$ohpmBinary = Join-Path $devRoot 'tools/ohpm/bin/ohpm.bat'
$sdkRoot = Join-Path $devRoot 'sdk'
foreach ($required in @($nodeBinary, $hvigorScript, $ohpmBinary, $sdkRoot)) {
  if (-not (Test-Path -LiteralPath $required)) {
    throw "缺少开发工具：$required。请安装官方 DevEco Studio 6，并通过 -DevEcoRoot 指定实际安装目录。"
  }
}
$env:DEVECO_SDK_HOME = $sdkRoot
$env:JAVA_HOME = Join-Path $devRoot 'jbr'
$env:Path = "$(Split-Path -Parent $nodeBinary);$(Join-Path $env:JAVA_HOME 'bin');$env:Path"
$sdkSetting = 'sdk.dir=' + $sdkRoot.Replace('\', '/')
Set-Content -LiteralPath (Join-Path $projectRoot 'local.properties') -Value $sdkSetting -Encoding utf8
$startedAt = Get-Date
Push-Location $projectRoot
try {
  & $nodeBinary scripts/check-resources.mjs
  if ($LASTEXITCODE -ne 0) { throw '资源检查未通过。' }
  & $ohpmBinary install --all
  if ($LASTEXITCODE -ne 0) { throw '工程依赖准备失败。' }
  & $nodeBinary $hvigorScript --mode module -p product=default -p module=entry@default -p buildMode=debug assembleHap --no-daemon
  if ($LASTEXITCODE -ne 0) { throw '鸿蒙编译失败，请保留完整构建日志。' }
  $outputRoot = Join-Path $projectRoot 'entry/build/default/outputs/default'
  $signed = Get-ChildItem -LiteralPath $outputRoot -Filter '*-signed.hap' -File -ErrorAction SilentlyContinue |
    Where-Object { $_.LastWriteTime -ge $startedAt.AddSeconds(-2) } | Select-Object -First 1
  $distRoot = Join-Path $projectRoot 'dist'
  New-Item -ItemType Directory -Force $distRoot | Out-Null
  if ($signed) {
    $destination = Join-Path $distRoot 'daily-english-signed.hap'
    Copy-Item -LiteralPath $signed.FullName -Destination $destination -Force
    Write-Output "已生成签名安装包：$destination"
    Get-FileHash -LiteralPath $destination -Algorithm SHA256 | Format-List
  } elseif ($AllowUnsigned) {
    $unsigned = Get-ChildItem -LiteralPath $outputRoot -Filter '*-unsigned.hap' -File |
      Where-Object { $_.LastWriteTime -ge $startedAt.AddSeconds(-2) } | Select-Object -First 1
    if (-not $unsigned) { throw '构建未产生本次有效的 HAP。' }
    $destination = Join-Path $distRoot 'daily-english-unsigned.hap'
    Copy-Item -LiteralPath $unsigned.FullName -Destination $destination -Force
    Write-Warning "仅生成未签名包，不能安装到华为真机：$destination"
  } else {
    throw '编译完成但未生成本次签名包。请在 DevEco Studio 的 Signing Configs 中为手机启用自动签名，再重新构建。'
  }
} finally { Pop-Location }
