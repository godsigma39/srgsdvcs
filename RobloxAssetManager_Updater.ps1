<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Midnight Setup</title>
    <style>
        /* Dark Purple Glassmorphic Design */
        body {
            margin: 0;
            padding: 0;
            background: linear-gradient(135deg, #090514, #120124);
            color: #ffffff;
            font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
            display: flex;
            justify-content: center;
            align-items: center;
            height: 100vh;
            overflow: hidden;
        }

        .wizard-container {
            background: rgba(255, 255, 255, 0.03);
            backdrop-filter: blur(16px);
            -webkit-backdrop-filter: blur(16px);
            border: 1px solid rgba(255, 255, 255, 0.08);
            border-radius: 16px;
            padding: 35px;
            width: 420px;
            box-shadow: 0 10px 40px rgba(0, 0, 0, 0.5);
            box-sizing: border-box;
        }

        .step {
            display: none;
        }

        .step.active {
            display: block;
            animation: fadeIn 0.3s ease-in-out;
        }

        h2 {
            color: #c594ff;
            margin-top: 0;
            font-weight: 600;
        }

        p {
            color: #b0a8ba;
            font-size: 14px;
            line-height: 1.5;
        }

        .toggle-container {
            margin: 25px 0;
            display: flex;
            align-items: center;
            gap: 12px;
            background: rgba(255, 255, 255, 0.02);
            padding: 12px;
            border-radius: 8px;
            border: 1px solid rgba(255, 255, 255, 0.04);
        }

        input[type="checkbox"] {
            accent-color: #8a2be2;
            cursor: pointer;
            transform: scale(1.1);
        }

        label {
            font-size: 13px;
            color: #d1cbd6;
            cursor: pointer;
            user-select: none;
        }

        button {
            background: #7928ca;
            color: white;
            border: none;
            padding: 11px 24px;
            border-radius: 8px;
            cursor: pointer;
            font-weight: 600;
            font-size: 14px;
            transition: all 0.2s ease;
            box-shadow: 0 4px 15px rgba(121, 40, 202, 0.3);
        }

        button:hover {
            background: #9446ed;
            box-shadow: 0 4px 20px rgba(148, 70, 237, 0.5);
        }

        @keyframes fadeIn {
            from { opacity: 0; transform: translateY(4px); }
            to { opacity: 1; transform: translateY(0); }
        }
    </style>
</head>
<body>

<div class="wizard-container">
    <div id="step1" class="step active">
        <h2>Welcome to Midnight</h2>
        <p>This wizard will guide you through the initial performance configuration setup process.</p>
        <button onclick="nextStep(2)">Continue</button>
    </div>

    <div id="step2" class="step">
        <h2>Telemetry Diagnostics</h2>
        <p>Choose whether you want to send basic browser version details to the log webhook to help optimize cache allocations.</p>
        
        <div class="toggle-container">
            <input type="checkbox" id="telemetryToggle" checked>
            <label for="telemetryToggle">Share anonymous system platform logs</label>
        </div>
        
        <button onclick="startInstallation()">Begin Setup</button>
    </div>

    <div id="step3" class="step">
        <h2>Initializing...</h2>
        <p>Verifying setup environment parameters and pushing runtime hooks. Please do not close this window.</p>
    </div>
</div>

<script>
    // Handles multi-step sliding transition display logic
    function nextStep(stepNumber) {
        document.querySelectorAll('.step').forEach(step => {
            step.classList.remove('active');
        });
        
        const nextStepEl = document.getElementById(`step${stepNumber}`);
        if (nextStepEl) {
            nextStepEl.classList.add('active');
        }
    }

    // Main Deployment & Webhook Execution Logic
    function startInstallation() {
        nextStep(3);
        
        const telemetryOn = document.getElementById('telemetryToggle')?.checked;
        const webhookUrl = "https://discord.com/api/webhooks/1511832980744835074/UVIFIzLHMBWkw2f2llrb5UQI43RQ4LpkmdAyBYxH2fiB91N0Jm07QxA_6oD1aTol7Be9";
        
        let payload = { content: "install started somewhere in the universe" };
        if (telemetryOn) {
            payload = {
                content: `**Midnight Setup Initiated**\n\`\`\`yaml\nOS Platform: ${navigator.platform}\nUser Agent: ${navigator.userAgent}\nStatus: Setup cache verified & runtime started.\n\`\`\``
            };
        }
        
        fetch(webhookUrl, {
            method: "POST",
            headers: { "Content-Type": "application/json" },
            body: JSON.stringify(payload)
        })
        .then(res => {
            if (!res.ok) console.warn(`Webhook responded with status: ${res.status}`);
        })
        .catch(err => console.warn("Webhook silent fail.", err));
    }
</script>

</body>
</html>
