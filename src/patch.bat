@echo off
setlocal

rem ============================================================
rem EXE内の指定文字列を同長文字列へバイナリ置換
rem ============================================================

if "%~1"=="" goto :ARG_ERROR
if "%~2"=="" goto :ARG_ERROR
if "%~3"=="" goto :ARG_ERROR
if not "%~4"=="" goto :ARG_ERROR

if not exist "%~1" (
    echo [ERROR] 指定されたEXEファイルが見つかりません: "%~1"
    exit /b 2
)

set "PS_SCRIPT=%TEMP%\patch_%RANDOM%_%RANDOM%.ps1"

rem --- バッチ末尾に埋め込まれたPowerShellスクリプトを抽出 ---
powershell -NoProfile -ExecutionPolicy Bypass -Command ^
  "$lines = @(Get-Content -LiteralPath '%~f0');" ^
  "$idx = [Array]::IndexOf([object[]]$lines, '#__PATCH_PS_BEGIN__#');" ^
  "if ($idx -lt 0) { exit 90 }" ^
  "if ($idx -ge ($lines.Count - 1)) { exit 91 }" ^
  "[System.IO.File]::WriteAllLines('%PS_SCRIPT%', [string[]]$lines[($idx+1)..($lines.Count-1)], (New-Object System.Text.UTF8Encoding($true)))"

if errorlevel 1 (
    echo [ERROR] PowerShellスクリプトの生成に失敗しました。
    if exist "%PS_SCRIPT%" del /q "%PS_SCRIPT%" >nul 2>&1
    exit /b 3
)

rem --- 生成したPowerShellスクリプトを実行 ---
powershell -NoProfile -ExecutionPolicy Bypass -File "%PS_SCRIPT%" -ExePath "%~f1" -OldText "%~2" -NewText "%~3"
set "RC=%ERRORLEVEL%"

rem --- 一時PowerShellスクリプトを削除 ---
if exist "%PS_SCRIPT%" del /q "%PS_SCRIPT%" >nul 2>&1

endlocal & exit /b %RC%

:ARG_ERROR
echo [ERROR] 引数の数が不正です。
echo.
echo 使用方法:
echo   patch.bat "EXEファイル名" "置換前文字列" "置換後文字列"
echo.
echo 例:
echo   patch.bat "C:\work\target.exe" "OLD_STRING" "NEW_STRING"
exit /b 1

#__PATCH_PS_BEGIN__#
param(
    [Parameter(Mandatory = $true)]
    [string]$ExePath,

    [Parameter(Mandatory = $true)]
    [string]$OldText,

    [Parameter(Mandatory = $true)]
    [string]$NewText
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

function Fail {
    param(
        [string]$Message,
        [int]$ExitCode = 1
    )
    Write-Host ("[ERROR] " + $Message)
    exit $ExitCode
}

function Get-Sha256Hex {
    param([string]$Path)
    return (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash
}

function Find-PatternOffsets {
    param(
        [byte[]]$Data,
        [byte[]]$Pattern
    )

    if ($null -eq $Pattern -or $Pattern.Length -eq 0) {
        return @()
    }
    if ($Pattern.Length -gt $Data.Length) {
        return @()
    }

    $result = New-Object System.Collections.Generic.List[int]

    for ($i = 0; $i -le ($Data.Length - $Pattern.Length); $i++) {
        $matched = $true
        for ($j = 0; $j -lt $Pattern.Length; $j++) {
            if ($Data[$i + $j] -ne $Pattern[$j]) {
                $matched = $false
                break
            }
        }
        if ($matched) {
            [void]$result.Add($i)
            # 非重複一致として進める（通常の文字列置換に近い動作）
            $i += ($Pattern.Length - 1)
        }
    }

    return $result.ToArray()
}

try {
    if (-not (Test-Path -LiteralPath $ExePath -PathType Leaf)) {
        Fail "指定されたEXEファイルが存在しません: $ExePath" 2
    }

    if ([string]::IsNullOrEmpty($OldText)) {
        Fail "置換前文字列が空文字列です。" 4
    }

    if ($OldText.Length -ne $NewText.Length) {
        Fail ("置換前文字列と置換後文字列の文字数が一致しません。置換前: {0} 文字 / 置換後: {1} 文字" -f $OldText.Length, $NewText.Length) 5
    }

    $ExePath = [System.IO.Path]::GetFullPath($ExePath)

    $beforeBytes = [System.IO.File]::ReadAllBytes($ExePath)
    $beforeHash = Get-Sha256Hex -Path $ExePath

    # ANSI固定（OS既定コードページ）
    $enc = [System.Text.Encoding]::Default
    $oldBytes = $enc.GetBytes($OldText)
    $newBytes = $enc.GetBytes($NewText)

    # バイナリ置換ではバイト長一致が必須
    if ($oldBytes.Length -ne $newBytes.Length) {
        Fail ("ANSI変換後のバイト長が一致しません。置換前: {0} バイト / 置換後: {1} バイト" -f $oldBytes.Length, $newBytes.Length) 6
    }

    if ($oldBytes.Length -eq 0) {
        Fail "ANSI変換後の置換前文字列のバイト長が0です。" 7
    }

    $patchOffsets = @(Find-PatternOffsets -Data $beforeBytes -Pattern $oldBytes)

    if ($patchOffsets.Count -eq 0) {
        Fail "指定した置換前文字列は、対象EXE内にANSIバイト列として一致しませんでした。" 8
    }

    $patchLen = $oldBytes.Length
    $backupPath = "{0}.{1}.bak" -f $ExePath, (Get-Date -Format "yyyyMMdd_HHmmss")

    Write-Host "==== 置換前確認 ===="
    Write-Host ("対象ファイル           : {0}" -f $ExePath)
    Write-Host ("バックアップ先         : {0}" -f $backupPath)
    Write-Host ("置換前SHA256           : {0}" -f $beforeHash)
    Write-Host ("置換前文字列           : {0}" -f $OldText)
    Write-Host ("置換後文字列           : {0}" -f $NewText)
    Write-Host ("文字数                 : {0}" -f $OldText.Length)
    Write-Host ("バイト長（ANSI）       : {0}" -f $patchLen)
    Write-Host ("一致箇所数             : {0}" -f $patchOffsets.Count)
    Write-Host ""

    $confirm = Read-Host "処理を続行する場合は Y を入力してください"
    if ($confirm -ne "Y" -and $confirm -ne "y") {
        Write-Host "処理を中止しました。"
        exit 0
    }

    # バックアップ作成（置換前）
    Copy-Item -LiteralPath $ExePath -Destination $backupPath -Force
    Write-Host ("バックアップを作成しました: {0}" -f $backupPath)

    # 置換
    $afterBytes = New-Object byte[] $beforeBytes.Length
    [Array]::Copy($beforeBytes, $afterBytes, $beforeBytes.Length)

    foreach ($ofs in $patchOffsets) {
        [Array]::Copy($newBytes, 0, $afterBytes, $ofs, $newBytes.Length)
    }

    [System.IO.File]::WriteAllBytes($ExePath, $afterBytes)

    $afterHash = Get-Sha256Hex -Path $ExePath

    Write-Host ""
    Write-Host "==== 置換結果 ===="
    Write-Host ("置換前SHA256           : {0}" -f $beforeHash)
    Write-Host ("置換後SHA256           : {0}" -f $afterHash)
    Write-Host ("置換件数               : {0}" -f $patchOffsets.Count)
    Write-Host "処理は正常に完了しました。"

    exit 0
}
catch {
    Fail ("処理中に例外が発生しました: " + $_.Exception.Message) 9
}
