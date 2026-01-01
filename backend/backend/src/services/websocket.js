/**
 * WebSocket Service for Real-Time Updates
 * Notifies all connected clients when game status changes
 */

const WebSocket = require('ws');

let wss = null;
const clients = new Set();

/**
 * Initialize WebSocket server
 */
function initialize(server) {
    wss = new WebSocket.Server({ 
        server,
        path: '/ws'
    });
    
    wss.on('connection', (ws, req) => {
        const clientIp = req.headers['x-forwarded-for'] || req.socket.remoteAddress;
        console.log(`[WebSocket] Client connected: ${clientIp}`);
        
        clients.add(ws);
        
        // Send initial connection confirmation
        ws.send(JSON.stringify({
            type: 'connected',
            message: 'WebSocket connection established',
            timestamp: new Date().toISOString()
        }));
        
        ws.on('close', () => {
            clients.delete(ws);
            console.log(`[WebSocket] Client disconnected: ${clientIp}`);
        });
        
        ws.on('error', (error) => {
            console.error(`[WebSocket] Error:`, error.message);
            clients.delete(ws);
        });
        
        // Handle ping/pong for keep-alive
        ws.isAlive = true;
        ws.on('pong', () => {
            ws.isAlive = true;
        });
    });
    
    // Ping clients every 30 seconds to keep connection alive
    const interval = setInterval(() => {
        wss.clients.forEach((ws) => {
            if (ws.isAlive === false) {
                return ws.terminate();
            }
            ws.isAlive = false;
            ws.ping();
        });
    }, 30000);
    
    wss.on('close', () => {
        clearInterval(interval);
    });
    
    console.log('[WebSocket] Server initialized on /ws');
}

/**
 * Broadcast message to all connected clients
 */
function broadcast(data) {
    if (!wss) return;
    
    const message = JSON.stringify(data);
    let successCount = 0;
    
    clients.forEach((client) => {
        if (client.readyState === WebSocket.OPEN) {
            try {
                client.send(message);
                successCount++;
            } catch (error) {
                console.error('[WebSocket] Broadcast error:', error.message);
            }
        }
    });
    
    if (successCount > 0) {
        console.log(`[WebSocket] Broadcasted to ${successCount} clients:`, data.type);
    }
}

/**
 * Notify all clients about game status change
 */
function notifyStatusChange(gameId, status, message) {
    broadcast({
        type: 'status_update',
        game_id: gameId,
        status,
        message,
        timestamp: new Date().toISOString()
    });
}

/**
 * Notify all clients about new version upload
 */
function notifyVersionUpdate(gameId, version) {
    broadcast({
        type: 'version_update',
        game_id: gameId,
        version,
        timestamp: new Date().toISOString()
    });
}

/**
 * Get connected clients count
 */
function getClientsCount() {
    return clients.size;
}

module.exports = {
    initialize,
    broadcast,
    notifyStatusChange,
    notifyVersionUpdate,
    getClientsCount
};

