let globalWs = null;

function getApiUrl(path) {
    let host = window.location.host;
    let proto = window.location.protocol;
    
    const urlParams = new URLSearchParams(window.location.search);
    const queryPort = urlParams.get('port');
    
    if (queryPort) {
        host = `localhost:${queryPort}`;
        if (proto === 'file:') proto = 'http:';
    } else if (proto === 'file:' || window.location.port === '3509') {
        host = 'localhost:2409';
        if (proto === 'file:') proto = 'http:';
    }
    return `${proto}//${host}${path}`;
}

function getWsUrl(path) {
    let host = window.location.host;
    let proto = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
    
    const urlParams = new URLSearchParams(window.location.search);
    const queryPort = urlParams.get('port');
    
    if (queryPort) {
        host = `localhost:${queryPort}`;
        if (window.location.protocol === 'file:') proto = 'ws:';
    } else if (window.location.protocol === 'file:' || window.location.port === '3509') {
        host = 'localhost:2409';
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

function appendLiveLog(formattedLog, level) {
    const logBox = document.getElementById('debug-logs');
    if (!logBox) return;
    
    let color = '#00ff00'; // Default retro green
    if (level === 2) color = '#ff3333'; // ERROR
    else if (level === 1) color = '#ffff00'; // WARNING
    else if (level === 3) color = '#00ffff'; // DEBUG
    
    const lineDiv = document.createElement('div');
    lineDiv.className = 'log-line';
    lineDiv.style.color = color;
    lineDiv.innerText = formattedLog.replace(/\n$/, '');
    
    logBox.appendChild(lineDiv);
    logBox.scrollTop = logBox.scrollHeight;
}

async function sha256(message) {
    const msgBuffer = new TextEncoder().encode(message);
    const hashBuffer = await crypto.subtle.digest('SHA-256', msgBuffer);
    const hashArray = Array.from(new Uint8Array(hashBuffer));
    return hashArray.map(b => b.toString(16).padStart(2, '0')).join('');
}

// Charting State and Logic
const chartHistory = [];
const maxHistoryLength = 45;
let lastTotalQueries = null;
let lastStatsTime = null;

function updateChartData(ramAllocatedMB, totalQueries) {
    const now = Date.now();
    let qps = 0;
    if (lastTotalQueries !== null && lastStatsTime !== null) {
        const elapsedSec = (now - lastStatsTime) / 1000;
        if (elapsedSec > 0) {
            qps = Math.max(0, (totalQueries - lastTotalQueries) / elapsedSec);
        }
    }
    lastTotalQueries = totalQueries;
    lastStatsTime = now;
    
    chartHistory.push({
        ram: ramAllocatedMB,
        qps: qps
    });
    
    if (chartHistory.length > maxHistoryLength) {
        chartHistory.shift();
    }
    
    renderChart();
}

function renderChart() {
    const canvas = document.getElementById('performance-chart');
    if (!canvas) return;
    
    const ctx = canvas.getContext('2d');
    const dpr = window.devicePixelRatio || 1;
    const rect = canvas.getBoundingClientRect();
    canvas.width = rect.width * dpr;
    canvas.height = rect.height * dpr;
    ctx.scale(dpr, dpr);
    
    const width = rect.width;
    const height = rect.height;
    
    // Clear canvas
    ctx.fillStyle = '#ffffff';
    ctx.fillRect(0, 0, width, height);
    
    const padding = { top: 25, right: 60, bottom: 25, left: 60 };
    const chartWidth = width - padding.left - padding.right;
    const chartHeight = height - padding.top - padding.bottom;
    
    if (chartWidth <= 0 || chartHeight <= 0) return;
    
    // Outer border
    ctx.strokeStyle = '#000000';
    ctx.lineWidth = 2;
    ctx.strokeRect(padding.left, padding.top, chartWidth, chartHeight);
    
    // Determine scales
    const ramCapacity = 15; // standard cap in MB
    let maxRam = ramCapacity;
    chartHistory.forEach(d => { if (d.ram > maxRam) maxRam = d.ram; });
    maxRam = Math.ceil(maxRam);
    
    let maxQps = 50;
    chartHistory.forEach(d => { if (d.qps > maxQps) maxQps = d.qps; });
    maxQps = Math.ceil(maxQps / 10) * 10;
    
    // Horizontal grid lines
    ctx.strokeStyle = '#e2e8f0';
    ctx.lineWidth = 1;
    ctx.setLineDash([4, 4]);
    for (let i = 1; i < 4; i++) {
        const y = padding.top + (chartHeight * i) / 4;
        ctx.beginPath();
        ctx.moveTo(padding.left, y);
        ctx.lineTo(padding.left + chartWidth, y);
        ctx.stroke();
    }
    ctx.setLineDash([]);
    
    // Grid ticks/labels
    ctx.fillStyle = '#000000';
    ctx.font = 'bold 9px "Fira Code", monospace';
    ctx.textAlign = 'right';
    ctx.textBaseline = 'middle';
    
    // Left axis (RAM MB)
    for (let i = 0; i <= 4; i++) {
        const val = (maxRam * i) / 4;
        const y = padding.top + chartHeight - (chartHeight * i) / 4;
        ctx.fillText(val.toFixed(1) + 'M', padding.left - 8, y);
    }
    
    // Right axis (QPS)
    ctx.textAlign = 'left';
    for (let i = 0; i <= 4; i++) {
        const val = (maxQps * i) / 4;
        const y = padding.top + chartHeight - (chartHeight * i) / 4;
        ctx.fillText(Math.round(val) + 'Q', padding.left + chartWidth + 8, y);
    }
    
    // Axis Titles
    ctx.font = 'bold 8px "Inter", sans-serif';
    ctx.textAlign = 'center';
    ctx.save();
    ctx.translate(20, padding.top + chartHeight / 2);
    ctx.rotate(-Math.PI / 2);
    ctx.fillText('MEMORY USAGE (MB)', 0, 0);
    ctx.restore();
    
    ctx.save();
    ctx.translate(width - 20, padding.top + chartHeight / 2);
    ctx.rotate(Math.PI / 2);
    ctx.fillText('THROUGHPUT (QPS)', 0, 0);
    ctx.restore();
    
    if (chartHistory.length < 2) return;
    
    // Draw QPS line (dashed grey)
    ctx.beginPath();
    chartHistory.forEach((d, idx) => {
        const x = padding.left + (chartWidth * idx) / (maxHistoryLength - 1);
        const y = padding.top + chartHeight - (chartHeight * d.qps) / maxQps;
        if (idx === 0) ctx.moveTo(x, y);
        else ctx.lineTo(x, y);
    });
    ctx.strokeStyle = '#666666';
    ctx.lineWidth = 2;
    ctx.setLineDash([2, 3]);
    ctx.stroke();
    ctx.setLineDash([]);
    
    // Draw RAM line (solid black)
    ctx.beginPath();
    chartHistory.forEach((d, idx) => {
        const x = padding.left + (chartWidth * idx) / (maxHistoryLength - 1);
        const y = padding.top + chartHeight - (chartHeight * d.ram) / maxRam;
        if (idx === 0) ctx.moveTo(x, y);
        else ctx.lineTo(x, y);
    });
    ctx.strokeStyle = '#000000';
    ctx.lineWidth = 2.5;
    ctx.stroke();
    
    // Legends
    ctx.font = 'bold 9px "Inter", sans-serif';
    ctx.textAlign = 'left';
    ctx.fillStyle = '#000000';
    ctx.fillRect(padding.left + 10, 8, 12, 6);
    ctx.fillText('RAM ALLOCATED (MB)', padding.left + 28, 12);
    
    ctx.strokeStyle = '#666666';
    ctx.lineWidth = 2;
    ctx.setLineDash([2, 3]);
    ctx.beginPath();
    ctx.moveTo(padding.left + 160, 11);
    ctx.lineTo(padding.left + 172, 11);
    ctx.stroke();
    ctx.setLineDash([]);
    ctx.fillStyle = '#666666';
    ctx.fillText('THROUGHPUT (QPS)', padding.left + 178, 12);
}

// Stress Test (Continuous Flooding) State and Logic
let stressActive = false;
let stressIntervalId = null;

function toggleStressTest() {
    const btn = document.getElementById('btn-stress');
    const status = document.getElementById('stress-status');
    
    if (stressActive) {
        stressActive = false;
        if (stressIntervalId) {
            clearInterval(stressIntervalId);
            stressIntervalId = null;
        }
        btn.innerText = 'Start Flood';
        btn.className = 'btn btn-primary';
        status.innerText = 'STOPPED';
        status.className = 'value status-badge error';
        addLog('Stress test stopped.');
    } else {
        stressActive = true;
        btn.innerText = 'Stop Flood';
        btn.className = 'btn btn-secondary';
        status.innerText = 'FLOODING';
        status.className = 'value status-badge success';
        addLog('Stress test started.');
        
        runStressLoop();
    }
}

function runStressLoop() {
    if (!stressActive) return;
    
    const rateInput = document.getElementById('stress-rate');
    let targetQps = parseInt(rateInput.value) || 50;
    
    const tickMs = 50;
    let accumulator = 0;
    stressIntervalId = setInterval(() => {
        if (!stressActive) {
            clearInterval(stressIntervalId);
            return;
        }
        targetQps = parseInt(rateInput.value) || 50;
        const requestsPerTick = (targetQps * tickMs) / 1000;
        
        accumulator += requestsPerTick;
        const toSend = Math.floor(accumulator);
        accumulator -= toSend;
        
        for (let i = 0; i < toSend; i++) {
            dispatchRandomExistsRequest();
        }
    }, tickMs);
}

function makeRandomId() {
    const chars = '0123456789abcdef';
    let result = '';
    for (let i = 0; i < 64; i++) {
        result += chars[Math.floor(Math.random() * 16)];
    }
    return result;
}

function dispatchRandomExistsRequest() {
    const randId = makeRandomId();
    const url = getApiUrl(`/api/${randId}`);
    fetch(url)
        .then(res => {})
        .catch(err => {});
}

// WebSocket Connection Management
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
            
            // Handle log events
            if (data.action === 'log') {
                appendLiveLog(data.message, data.level);
            }
            
            // Handle stats events
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
                
                // Update Disk Space
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

                // Update Total RAM Allocation & Capacity
                let allocatedMB = 0;
                if (data.ram_allocated !== undefined && data.ram_capacity !== undefined) {
                    allocatedMB = data.ram_allocated / 1024 / 1024;
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

                // Update Runtime RAM
                if (data.ram_runtime !== undefined && data.ram_capacity !== undefined) {
                    const runtimeMB = data.ram_runtime / 1024 / 1024;
                    document.getElementById('ram-runtime').innerText = runtimeMB.toFixed(2);
                    const runtimeBar = document.getElementById('ram-runtime-bar');
                    if (runtimeBar && data.ram_capacity > 0) {
                        const percent = (data.ram_runtime / data.ram_capacity) * 100;
                        runtimeBar.style.width = `${percent}%`;
                    }
                }

                // Update Request RAM
                if (data.ram_request !== undefined && data.ram_capacity !== undefined) {
                    const requestMB = data.ram_request / 1024 / 1024;
                    document.getElementById('ram-request').innerText = requestMB.toFixed(2);
                    const requestBar = document.getElementById('ram-request-bar');
                    if (requestBar && data.ram_capacity > 0) {
                        const percent = (data.ram_request / data.ram_capacity) * 100;
                        requestBar.style.width = `${percent}%`;
                    }
                }

                // Update Total Transactions
                if (data.total_queries !== undefined) {
                    document.getElementById('total-queries').innerText = data.total_queries;
                    updateChartData(allocatedMB, data.total_queries);
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
        addLog(`POST /api/${hashedIdentifier}${hashedPwk}`);
        
        const url = getApiUrl(`/api/${hashedIdentifier}${hashedPwk}`);
        const res = await fetch(url, {
            method: 'POST'
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
        addLog(`GET /api/${hashedIdentifier}`);
        
        const url = getApiUrl(`/api/${hashedIdentifier}`);
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

// Initial draw and connection
window.addEventListener('resize', renderChart);
connect();
