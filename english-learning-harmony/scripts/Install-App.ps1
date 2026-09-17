param(
  [string]$DevEcoRoot = 'C:\Program Files\Huawei\DevEco Studio',
  [string]$HapPath = '',
  [string]$DeviceId = ''
)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
if (-not $HapPath) { $HapPath = Join-Path $projectRoot 'dist/daily-english-signed.hap' }
$hdcBinary = Join-Path $DevEcoRoot 'sdk/default/openharmony/toolchains/hdc.exe'
if (-not (Test-Path -LiteralPath $hdcBinary)) { throw "找不到设备连接工具：$hdcBinary，请指定正确的 -DevEcoRoot。" }
if (-not (Test-Path -LiteralPath $HapPath)) { throw '尚无签名安装包，请先完成签名构建。' }
if ($HapPath -match 'unsigned') { throw '未签名 HAP 不能安装到华为真机。' }
$targets = @(& $hdcBinary list targets | Where-Object { $_.Trim() -and $_ -notmatch '\[Empty\]' } | ForEach-Object { $_.Trim() })
if ($LASTEXITCODE -ne 0) { throw '读取手机连接失败。' }
if (-not $DeviceId) {
  if ($targets.Count -eq 0) { throw '未连接手机。请用 USB 连接，开启 USB 调试并在手机上确认授权。' }
  if ($targets.Count -ne 1) { throw '连接了多台设备，请通过 -DeviceId 指定要安装的手机。' }
  $DeviceId = $targets[0]
}
if ($DeviceId -notin $targets) { throw '指定手机当前未连接或尚未授权。' }
$installResult = & $hdcBinary -t $DeviceId install -r ([System.IO.Path]::GetFullPath($HapPath)) 2>&1
$installExitCode = $LASTEXITCODE
$installResult | Write-Output
if ($installExitCode -ne 0 -or ($installResult -join "`n") -notmatch '(?i)success') {
  throw '安装未确认成功，请检查签名是否包含这台手机；不会自动卸载已有应用。'
}
& $hdcBinary -t $DeviceId shell aa start -a EntryAbility -b com.example.dailyenglish
if ($LASTEXITCODE -ne 0) { throw '应用已安装，但自动启动失败，请在手机桌面打开每日英语。' }
