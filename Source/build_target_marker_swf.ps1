param(
    [string]$FlexSdk = $env:FLEX_SDK,
    [string]$PlayerGlobalSwc = $env:PLAYERGLOBAL_SWC,
    [string]$Output = (Join-Path $PSScriptRoot "build\TargetMarkerNative.swf")
)

$ErrorActionPreference = "Stop"

$source = Join-Path $PSScriptRoot "HUD\TargetMarkerNative.as"

if ([string]::IsNullOrWhiteSpace($FlexSdk) -or
    [string]::IsNullOrWhiteSpace($PlayerGlobalSwc)) {
    throw "Set FLEX_SDK and PLAYERGLOBAL_SWC, or pass the corresponding parameters."
}

$compiler = Join-Path $FlexSdk "bin\mxmlc.bat"
$playerGlobalDir = Join-Path $FlexSdk "frameworks\libs\player\11.1"

if (!(Test-Path -LiteralPath $compiler)) {
    throw "Apache Flex compiler not found: $compiler"
}
if (!(Test-Path -LiteralPath $PlayerGlobalSwc)) {
    throw "playerglobal.swc not found: $PlayerGlobalSwc"
}

New-Item -ItemType Directory -Path $playerGlobalDir -Force | Out-Null
Copy-Item -LiteralPath $PlayerGlobalSwc -Destination (Join-Path $playerGlobalDir "playerglobal.swc") -Force
New-Item -ItemType Directory -Path (Split-Path -Parent $Output) -Force | Out-Null

& $compiler `
    "-source-path=$(Split-Path -Parent $source)" `
    "-target-player=11.1" `
    "-default-size=1280,720" `
    "-default-frame-rate=60" `
    "-debug=false" `
    "-optimize=true" `
    "-output=$Output" `
    $source

if ($LASTEXITCODE -ne 0) {
    throw "Target marker SWF compilation failed with exit code $LASTEXITCODE"
}

Get-Item -LiteralPath $Output
