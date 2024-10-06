& $PSScriptRoot/validate-modjson.ps1
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

$modJson = Get-Content "./mod.json" -Raw | ConvertFrom-Json

foreach ($fileName in $modJson.modFiles) {
    & adb shell rm /sdcard/ModData/com.beatgames.beatsaber/Modloader/early_mods/$fileName
}

foreach ($fileName in $modJson.lateModFiles) {
    & adb shell rm /sdcard/ModData/com.beatgames.beatsaber/Modloader/mods/$fileName
}
