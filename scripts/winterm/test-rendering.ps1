# Copyright (c) winTerm contributors.
# Licensed under the MIT license.

[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

function Write-Ansi
{
    param(
        [Parameter(Mandatory)]
        [AllowEmptyString()]
        [string]$Text
    )

    [System.Console]::Write($Text)
}

function Write-AnsiLine
{
    param(
        [Parameter()]
        [AllowEmptyString()]
        [string]$Text = ''
    )

    [System.Console]::WriteLine($Text)
}

$escape = [char]27
$reset = "$escape[0m"
$unicodeFixturePath = Join-Path (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path 'assets\winterm\samples\unicode-test.txt'

try
{
    Write-AnsiLine 'winTerm rendering fixture'
    Write-AnsiLine 'Emitting a fixture does not prove that the terminal rendered it correctly.'
    Write-AnsiLine ''

    Write-AnsiLine 'ANSI 16 background colors'
    for ($index = 0; $index -lt 8; $index++)
    {
        Write-Ansi (("{0}[48;5;{1}m {1,3} " -f $escape, $index))
    }
    Write-AnsiLine $reset
    for ($index = 8; $index -lt 16; $index++)
    {
        Write-Ansi (("{0}[48;5;{1}m {1,3} " -f $escape, $index))
    }
    Write-AnsiLine $reset
    Write-AnsiLine ''

    Write-AnsiLine 'ANSI attributes: normal, bold, dim, reverse'
    Write-AnsiLine ("normal | {0}[1mbold{1} | {0}[2mdim{1} | {0}[7mreverse{1}" -f $escape, $reset)
    Write-AnsiLine ''

    Write-AnsiLine 'ANSI 256 color cube (indexes 16-231)'
    for ($red = 0; $red -lt 6; $red++)
    {
        for ($green = 0; $green -lt 6; $green++)
        {
            for ($blue = 0; $blue -lt 6; $blue++)
            {
                $index = 16 + (36 * $red) + (6 * $green) + $blue
                Write-Ansi (("{0}[48;5;{1}m  " -f $escape, $index))
            }
        }
        Write-AnsiLine $reset
    }
    Write-AnsiLine ''

    Write-AnsiLine 'ANSI 256 grayscale ramp (indexes 232-255)'
    for ($index = 232; $index -le 255; $index++)
    {
        Write-Ansi (("{0}[48;5;{1}m  " -f $escape, $index))
    }
    Write-AnsiLine $reset
    Write-AnsiLine ''

    Write-AnsiLine '24-bit True Color RGB gradient'
    for ($index = 0; $index -lt 64; $index++)
    {
        $red = [int](255 * $index / 63)
        $green = [int](255 * (63 - $index) / 63)
        $blue = [int](128 + (127 * [Math]::Sin($index * [Math]::PI / 63)))
        Write-Ansi (("{0}[48;2;{1};{2};{3}m " -f $escape, $red, $green, $blue))
    }
    Write-AnsiLine $reset
    Write-AnsiLine ''

    Write-AnsiLine '24-bit foreground and background combinations'
    $combinations = @(
        @(255, 255, 255, 128, 0, 32),
        @(16, 16, 16, 255, 196, 64),
        @(224, 255, 240, 0, 96, 80),
        @(255, 240, 255, 96, 32, 112)
    )
    foreach ($combination in $combinations)
    {
        Write-Ansi (("{0}[38;2;{1};{2};{3};48;2;{4};{5};{6}m foreground/background " -f $escape, $combination[0], $combination[1], $combination[2], $combination[3], $combination[4], $combination[5]))
    }
    Write-AnsiLine $reset
    Write-AnsiLine ''

    if (-not (Test-Path -LiteralPath $unicodeFixturePath -PathType Leaf))
    {
        throw "Unicode fixture does not exist: $unicodeFixturePath"
    }
    Write-AnsiLine 'Unicode, CJK, symbols, and emoji'
    $unicodeFixture = [System.IO.File]::ReadAllText($unicodeFixturePath, [System.Text.Encoding]::UTF8).TrimEnd()
    Write-AnsiLine $unicodeFixture
    Write-AnsiLine ''
    Write-AnsiLine 'Manual checks: alignment, color, cursor movement, selection, backspace, and copied Unicode text.'
}
catch
{
    try
    {
        Write-AnsiLine $reset
    }
    catch
    {
    }
    Write-Error "winTerm rendering fixture failed: $($_.Exception.Message)"
    exit 1
}
