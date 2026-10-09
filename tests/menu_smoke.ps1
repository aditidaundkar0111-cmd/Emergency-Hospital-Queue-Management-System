param(
    [string]$Executable = (Join-Path (Split-Path -Parent $PSScriptRoot) 'review_app.exe')
)

if (-not (Test-Path -LiteralPath $Executable)) {
    throw "Application executable not found: $Executable"
}

$dataDirectory = Join-Path (Get-Location) 'data'
$savedFiles = @{}
foreach ($fileName in @('patients.txt', 'doctors.txt', 'treatments.txt')) {
    $filePath = Join-Path $dataDirectory $fileName
    if (Test-Path -LiteralPath $filePath) {
        $savedFiles[$fileName] = [System.IO.File]::ReadAllBytes($filePath)
    } else {
        $savedFiles[$fileName] = $null
    }
}

try {
    # Deliberately distinct IDs and text catch argument-order errors in menu prompts.
    $workflow = @(
        'invalid',
        '1', '111', 'Ada', '30', 'Checkup', '1',
        '2', '112', 'Ben', '40', 'Injury', '5', '1',
        '3', '113', 'Cal', '20', 'Cold', '2',
        '4', '5', '6', '111',
        '7', '777', 'Dr Demo', 'General Physician',
        '8', '9', '10', '112', '777', '11', '112',
        '12', '112', '777', 'DX-ALPHA', 'TX-BETA',
        '13', '14', '15'
    ) -join "`n"

    $output = $workflow | & $Executable 2>&1
    $exitCode = $LASTEXITCODE
    $text = $output -join "`n"
    if ($exitCode -ne 0) { throw "Menu workflow exited with code $exitCode.`n$text" }
    foreach ($expected in @(
        'Invalid number. Please try again.',
        'Patient registered in WAITING status.',
        'Doctor assigned; patient is UNDER_TREATMENT.',
        'Treatment completed.',
        'Data saved. Exiting.',
        'Diagnosis: DX-ALPHA | Treatment: TX-BETA | Completed',
        'Goodbye.'
    )) {
        if (-not $text.Contains($expected)) { throw "Menu output did not contain '$expected'.`n$text" }
    }
    if ($text.Contains('Operation failed:')) { throw "A menu action failed.`n$text" }

    # New process: add an unsaved patient, load the saved state, and verify it is replaced.
    $reloadWorkflow = @(
        '3', '114', 'ShouldNotRemain', '40', 'Transient', '2',
        '16', '5', '13', '14', '4', '0'
    ) -join "`n"
    $reloadOutput = $reloadWorkflow | & $Executable 2>&1
    if ($LASTEXITCODE -ne 0) { throw "Reload workflow failed.`n$($reloadOutput -join "`n")" }
    $reloadText = $reloadOutput -join "`n"
    if (-not $reloadText.Contains('Data loaded.') -or -not $reloadText.Contains('Goodbye.')) {
        throw "Load or exit option 0 did not finish cleanly.`n$reloadText"
    }
    if ($reloadText.Contains('ShouldNotRemain')) {
        throw "Load did not replace the unsaved menu state.`n$reloadText"
    }
    if ($reloadText.Contains('Operation failed:')) { throw "A reload menu action failed.`n$reloadText" }

    Write-Output 'Menu save/load smoke checks passed.'
} finally {
    foreach ($fileName in @('patients.txt', 'doctors.txt', 'treatments.txt')) {
        $filePath = Join-Path $dataDirectory $fileName
        if ($null -eq $savedFiles[$fileName]) {
            Remove-Item -LiteralPath $filePath -ErrorAction SilentlyContinue
        } else {
            [System.IO.File]::WriteAllBytes($filePath, $savedFiles[$fileName])
        }
    }
}
