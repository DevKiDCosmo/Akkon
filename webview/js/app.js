let globalWs = null;

function getApiUrl(path) {
    const port = '2409';
    let host = window.location.host;
    let proto = window.location.protocol;
    
    if (window.location.port !== port || proto === 'file:') {
        host = `localhost:${port}`;
        proto = 'http:';
    }
    return `${proto}//${host}${path}`;
}

function getWsUrl(path) {
    const port = '2409';
    let host = window.location.host;
    let proto = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
    
    if (window.location.port !== port || window.location.protocol === 'file:') {
        host = `localhost:${port}`;
        proto = 'ws:';
    }
    return `${proto}//${host}${path}`;
}

function addLog(msg) {
    const logBox = document.getElementById('debug-logs');
    if (!logBox) return;
    const time = new Date().toLocaleTimeString();
    logBox.innerHTML += `<div class="log-line"><span class="log-time">[${time}]</span> ${msg}</div>`;
    logBox.scrollTop = logBox.scrollHeight;
}

async function sha256(message) {
    const msgBuffer = new TextEncoder().encode(message);
    const hashBuffer = await crypto.subtle.digest('SHA-256', msgBuffer);
    const hashArray = Array.from(new Uint8Array(hashBuffer));
    return hashArray.map(b => b.toString(16).padStart(2, '0')).join('');
}

function connect() {
    const wsUrl = getWsUrl('/ws');
    globalWs = new WebSocket(wsUrl);
    
    globalWs.onopen = () => {
        document.getElementById('server-status').innerText = 'CONNECTED';
        document.querySelector('.status-indicator').classList.remove('offline');
        document.querySelector('.status-indicator').classList.add('online');
        addLog('WebSocket connection established.');
    };
    
    globalWs.onmessage = (event) => {
        try {
            const data = JSON.parse(event.data);
            if (data.action === 'stats') {
                // Update Reliability
                if (data.reliability !== undefined) {
                    document.getElementById('sys-reliability').innerText = `${data.reliability}%`;
                    const relBar = document.getElementById('reliability-bar');
                    if (relBar) {
                        relBar.style.width = `${data.reliability}%`;
                        if (data.reliability < 50) {
                            relBar.className = 'progress-bar error';
                        } else if (data.reliability < 90) {
                            relBar.className = 'progress-bar warning';
                        } else {
                            relBar.className = 'progress-bar success';
                        }
                    }
                }
                
                // Update Disk Space (Healthy threshold: 500MB)
                if (data.disk_space !== undefined) {
                    const mb = data.disk_space / 1024 / 1024;
                    document.getElementById('sys-disk').innerText = `${mb.toFixed(2)} MB`;
                    const diskBar = document.getElementById('disk-bar');
                    if (diskBar) {
                        const percent = Math.min(100, (data.disk_space / (500 * 1024 * 1024)) * 100);
                        diskBar.style.width = `${percent}%`;
                        if (mb < 50) {
                            diskBar.className = 'progress-bar error';
                        } else if (mb < 200) {
                            diskBar.className = 'progress-bar warning';
                        } else {
                            diskBar.className = 'progress-bar info-cyan';
                        }
                    }
                }

                // Update RAM Allocation & Capacity
                if (data.ram_allocated !== undefined && data.ram_capacity !== undefined) {
                    const allocatedMB = data.ram_allocated / 1024 / 1024;
                    const capacityMB = data.ram_capacity / 1024 / 1024;
                    document.getElementById('ram-allocated').innerText = allocatedMB.toFixed(2);
                    document.getElementById('ram-capacity').innerText = capacityMB.toFixed(2);
                    
                    const ramBar = document.getElementById('ram-bar');
                    if (ramBar && data.ram_capacity > 0) {
                        const percent = (data.ram_allocated / data.ram_capacity) * 100;
                        ramBar.style.width = `${percent}%`;
                        if (percent > 95) {
                            ramBar.className = 'progress-bar error';
                        } else if (percent > 85) {
                            ramBar.className = 'progress-bar warning';
                        } else {
                            ramBar.className = 'progress-bar info-purple';
                        }
                    }
                }

                // Update Total Transactions
                if (data.total_queries !== undefined) {
                    document.getElementById('total-queries').innerText = data.total_queries;
                }
            }
        } catch (e) {
            addLog('Failed to parse WS message: ' + e.message);
        }
    };
    
    globalWs.onclose = () => {
        addLog('WebSocket disconnected');
        document.getElementById('server-status').innerText = 'DISCONNECTED';
        document.querySelector('.status-indicator').classList.remove('online');
        document.querySelector('.status-indicator').classList.add('offline');
        setTimeout(connect, 3000);
    };
}

async function insertUser() {
    const idInput = document.getElementById('ins-identifier');
    const pwInput = document.getElementById('ins-pwk');
    const statusBox = document.getElementById('ins-status');
    
    const identifier = idInput.value.trim();
    const pwk = pwInput.value.trim();
    
    if (!identifier || !pwk) {
        statusBox.innerText = 'INVALID INPUT';
        statusBox.className = 'value status-badge error';
        addLog('Error: Identifier and Password Key cannot be empty');
        return;
    }
    
    statusBox.innerText = 'PENDING...';
    statusBox.className = 'value status-badge warning';
    
    try {
        const hashedIdentifier = await sha256(identifier);
        const hashedPwk = await sha256(pwk);
        addLog(`POST /api/v1/insert -> {"identifier":"${hashedIdentifier}", "pwk":"***"}`);
        
        const url = getApiUrl('/api/v1/insert');
        const res = await fetch(url, {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json'
            },
            body: JSON.stringify({identifier: hashedIdentifier, pwk: hashedPwk})
        });
        const data = await res.json();
        if (data.status === 'success') {
            statusBox.innerText = 'SUCCESS';
            statusBox.className = 'value status-badge success';
            addLog(`200 OK: Inserted successfully`);
            idInput.value = '';
            pwInput.value = '';
        } else {
            statusBox.innerText = 'ERROR';
            statusBox.className = 'value status-badge error';
            addLog(`Error: ${data.error}`);
        }
    } catch (e) {
        statusBox.innerText = 'FAILED';
        statusBox.className = 'value status-badge error';
        addLog(`Fetch Error: ${e.message}`);
    }
}

async function queryUser() {
    const idInput = document.getElementById('qry-identifier');
    const resultBox = document.getElementById('qry-result');
    const identifier = idInput.value.trim();
    
    if (!identifier) {
        resultBox.innerText = 'INVALID INPUT';
        resultBox.className = 'value status-badge error';
        addLog('Error: Query identifier cannot be empty');
        return;
    }
    
    resultBox.innerText = 'PENDING...';
    resultBox.className = 'value status-badge warning';
    
    try {
        const hashedIdentifier = await sha256(identifier);
        addLog(`GET /api/v1/exists?identifier=${hashedIdentifier}`);
        
        const url = getApiUrl(`/api/v1/exists?identifier=${hashedIdentifier}`);
        const res = await fetch(url);
        const data = await res.json();
        if (data.result) {
            resultBox.innerText = data.result;
            if (data.result === 'DEFINITELY_YES' || data.result === 'PROBABLY_YES') {
                resultBox.className = 'value status-badge success';
            } else {
                resultBox.className = 'value status-badge error';
            }
            addLog(`200 OK: ${data.result}`);
        } else {
            resultBox.innerText = 'ERROR';
            resultBox.className = 'value status-badge error';
            addLog(`Error: ${data.error}`);
        }
    } catch (e) {
        resultBox.innerText = 'FAILED';
        resultBox.className = 'value status-badge error';
        addLog(`Fetch Error: ${e.message}`);
    }
}

async function fetchDebug() {
    addLog("Polling system stats from /api/stats...");
    try {
        const url = getApiUrl('/api/stats');
        const res = await fetch(url);
        const data = await res.json();
        addLog(`System API: Allocated=${data.allocated}B, Capacity=${data.capacity}B, Queries=${data.total_queries}`);
        document.getElementById('dbg-port').innerText = '2409';
        document.getElementById('dbg-interface').innerText = '0.0.0.0';
    } catch (e) {
        addLog(`Fetch Stats Error: ${e.message}`);
    }
}

connect();
