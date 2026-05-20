let globalWs = null;

function connect() {
    globalWs = new WebSocket(`ws://127.0.0.1:8080/ws`);
    
    globalWs.onopen = () => {
        document.getElementById('server-status').innerText = 'CONNECTED';
        document.querySelector('.dot').style.backgroundColor = '#fff';
    };
    
    globalWs.onmessage = (event) => {
        try {
            const data = JSON.parse(event.data);
            if (data.allocated !== undefined) document.getElementById('mem-allocated').innerText = `${(data.allocated / 1024 / 1024).toFixed(2)} MB`;
            if (data.capacity !== undefined) document.getElementById('mem-capacity').innerText = `${(data.capacity / 1024 / 1024).toFixed(2)} MB`;
            if (data.total_queries !== undefined) document.getElementById('total-queries').innerText = data.total_queries;
            if (data.hit_rate !== undefined) document.getElementById('hit-rate').innerText = `${data.hit_rate.toFixed(1)}%`;
            
            if (data.action === 'insert_ack') {
                document.getElementById('ins-status').innerText = 'SUCCESS';
            }
            if (data.action === 'query_ack') {
                document.getElementById('qry-result').innerText = data.result;
            }
            if (data.action === 'debug_ack') {
                document.getElementById('dbg-port').innerText = data.port;
                document.getElementById('dbg-interface').innerText = data.interface;
            }
        } catch (e) {
            console.error('Failed to parse WS message', e);
        }
    };
    
    globalWs.onclose = () => {
        document.getElementById('server-status').innerText = 'DISCONNECTED';
        document.querySelector('.dot').style.backgroundColor = '#333';
        setTimeout(connect, 3000);
    };
}

function insertUser() {
    if (!globalWs || globalWs.readyState !== WebSocket.OPEN) return;
    const email = document.getElementById('ins-email').value;
    const pass = document.getElementById('ins-pass').value;
    globalWs.send(JSON.stringify({action: 'insert', email, pass}));
    document.getElementById('ins-status').innerText = 'PENDING...';
}

function queryUser() {
    if (!globalWs || globalWs.readyState !== WebSocket.OPEN) return;
    const email = document.getElementById('qry-email').value;
    globalWs.send(JSON.stringify({action: 'query', email}));
    document.getElementById('qry-result').innerText = 'PENDING...';
}

function fetchDebug() {
    if (!globalWs || globalWs.readyState !== WebSocket.OPEN) return;
    globalWs.send(JSON.stringify({action: 'debug'}));
}

connect();
