param(
    [string]$OutputDirectory = (Join-Path $PSScriptRoot "build\papyrus"),
    [string]$Caprica = $env:CAPRICA_PATH,
    [string]$PapyrusFlags = $env:PAPYRUS_FLAGS,
    [string]$GameSource = $env:PAPYRUS_GAME_SOURCE
)

$ErrorActionPreference = "Stop"

$source = Join-Path $PSScriptRoot "Scripts\Source\User"

if ([string]::IsNullOrWhiteSpace($Caprica) -or
    [string]::IsNullOrWhiteSpace($PapyrusFlags) -or
    [string]::IsNullOrWhiteSpace($GameSource)) {
    throw "Set CAPRICA_PATH, PAPYRUS_FLAGS, and PAPYRUS_GAME_SOURCE, or pass the corresponding parameters."
}

if (!(Test-Path -LiteralPath $Caprica)) { throw "Caprica not found: $Caprica" }
if (!(Test-Path -LiteralPath $PapyrusFlags)) { throw "Papyrus flags not found: $PapyrusFlags" }
if (!(Test-Path -LiteralPath $GameSource)) { throw "Papyrus game source not found: $GameSource" }

New-Item -ItemType Directory -Path $OutputDirectory -Force | Out-Null
Get-ChildItem -LiteralPath $OutputDirectory -Filter "*.pex" -File -ErrorAction SilentlyContinue |
    Remove-Item -Force

foreach ($script in @(
    "SimpleAimAssistNative.psc",
    "SimpleAimAssist.psc"
)) {
    & $Caprica (Join-Path $source $script) `
        -i $source `
        -i $GameSource `
        -f $PapyrusFlags `
        -o $OutputDirectory

    if ($LASTEXITCODE -ne 0) {
        throw "Caprica failed while compiling $script with exit code $LASTEXITCODE"
    }
}

Get-ChildItem -LiteralPath $OutputDirectory -Filter "*.pex" |
    Sort-Object Name |
    Select-Object Name, Length, LastWriteTime
