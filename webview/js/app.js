// Simple websocket client to connect to Akkon server
function connect() {
    let wsPort = window.location.port === "8000" || window.location.port === "5173" ? "8080" : window.location.port;
    const ws = new WebSocket(`ws://${window.location.hostname}:${wsPort}/ws`);
    
    ws.onopen = () => {
        document.getElementById('server-status').innerText = 'Connected';
        document.querySelector('.dot').style.backgroundColor = '#10b981';
        document.querySelector('.dot').style.boxShadow = '0 0 10px #10b981';
    };
    
    ws.onmessage = (event) => {
        try {
            const data = JSON.parse(event.data);
            if (data.allocated !== undefined) {
                document.getElementById('mem-allocated').innerText = `${(data.allocated / 1024 / 1024).toFixed(2)} MB`;
            }
            if (data.capacity !== undefined) {
                document.getElementById('mem-capacity').innerText = `${(data.capacity / 1024 / 1024).toFixed(2)} MB`;
            }
            if (data.total_queries !== undefined) {
                document.getElementById('total-queries').innerText = data.total_queries;
            }
            if (data.hit_rate !== undefined) {
                document.getElementById('hit-rate').innerText = `${data.hit_rate.toFixed(1)}%`;
            }
        } catch (e) {
            console.error('Failed to parse WS message', e);
        }
    };
    
    ws.onclose = () => {
        document.getElementById('server-status').innerText = 'Disconnected';
        document.querySelector('.dot').style.backgroundColor = '#ef4444';
        document.querySelector('.dot').style.boxShadow = '0 0 10px #ef4444';
        
        // Reconnect after 3 seconds
        setTimeout(connect, 3000);
    };
}

connect();
