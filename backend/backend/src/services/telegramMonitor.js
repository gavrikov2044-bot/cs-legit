/**
 * Telegram Channel Monitor for CS2 Updates
 * Monitors @cstwoupdate channel and updates game status when CS2 update is detected
 * Sends notifications to admin via Telegram
 */

const https = require('https');
const db = require('../database/db');

// Configuration
const CHANNEL_URL = 'https://t.me/s/cstwoupdate';
const CHECK_INTERVAL = 5 * 60 * 1000; // Check every 5 minutes
const BOT_TOKEN = process.env.TELEGRAM_BOT_TOKEN;
const ADMIN_CHAT_ID = process.env.TELEGRAM_ADMIN_ID;

// Store last seen update date
let lastSeenDate = null;
let isRunning = false;

/**
 * Send notification to admin via Telegram
 */
function sendNotification(message) {
    const url = `https://api.telegram.org/bot${BOT_TOKEN}/sendMessage`;
    const data = JSON.stringify({
        chat_id: ADMIN_CHAT_ID,
        text: message,
        parse_mode: 'HTML'
    });
    
    const req = https.request(url, {
        method: 'POST',
        headers: {
            'Content-Type': 'application/json',
            'Content-Length': Buffer.byteLength(data)
        }
    }, (res) => {
        let body = '';
        res.on('data', chunk => body += chunk);
        res.on('end', () => {
            if (res.statusCode === 200) {
                console.log('[TelegramMonitor] Notification sent to admin');
            } else {
                console.error('[TelegramMonitor] Failed to send notification:', body);
            }
        });
    });
    
    req.on('error', (e) => {
        console.error('[TelegramMonitor] Notification error:', e.message);
    });
    
    req.write(data);
    req.end();
}

/**
 * Keywords that indicate a CS2 update
 */
const UPDATE_KEYWORDS = [
    'обновление counter strike 2',
    '#cs2update',
    'версия клиента',
    'counter strike 2 от'
];

/**
 * Parse date from message like "Обновление Counter Strike 2 от 08.12.2025"
 */
function parseUpdateDate(text) {
    const match = text.match(/(\d{2})\.(\d{2})\.(\d{4})/);
    if (match) {
        const [_, day, month, year] = match;
        return new Date(`${year}-${month}-${day}`);
    }
    return null;
}

/**
 * Check if text contains update keywords
 */
function isUpdateMessage(text) {
    if (!text) return false;
    const lowerText = text.toLowerCase();
    return UPDATE_KEYWORDS.some(keyword => lowerText.includes(keyword));
}

/**
 * Ensure game_status table exists
 */
function ensureStatusTable() {
    try {
        db.prepare(`
            CREATE TABLE IF NOT EXISTS game_status (
                game_id TEXT PRIMARY KEY,
                status TEXT DEFAULT 'operational',
                message TEXT,
                updated_at TEXT DEFAULT CURRENT_TIMESTAMP,
                updated_by TEXT
            )
        `).run();
    } catch (e) {}
}

/**
 * Set game status in database
 */
function setGameStatus(gameId, status, message, updatedBy = 'system') {
    ensureStatusTable();
    
    db.prepare(`
        INSERT OR REPLACE INTO game_status (game_id, status, message, updated_at, updated_by)
        VALUES (?, ?, ?, datetime('now'), ?)
    `).run(gameId, status, message, updatedBy);
    
    console.log(`[TelegramMonitor] Set ${gameId} status to: ${status}`);
}

/**
 * Get current game status
 */
function getGameStatus(gameId) {
    ensureStatusTable();
    
    try {
        const status = db.prepare('SELECT * FROM game_status WHERE game_id = ?').get(gameId);
        return status || { game_id: gameId, status: 'operational', message: null };
    } catch (e) {
        return { game_id: gameId, status: 'operational', message: null };
    }
}

/**
 * Fetch and parse channel page
 */
async function checkChannel() {
    return new Promise((resolve) => {
        https.get(CHANNEL_URL, (res) => {
            let data = '';
            res.on('data', chunk => data += chunk);
            res.on('end', () => {
                try {
                    // Find all messages with update keywords
                    const messages = data.match(/<div class="tgme_widget_message_text[^>]*>[\s\S]*?<\/div>/g) || [];
                    
                    for (const msg of messages) {
                        // Remove HTML tags
                        const text = msg.replace(/<[^>]*>/g, ' ').replace(/\s+/g, ' ').trim();
                        
                        if (isUpdateMessage(text)) {
                            const updateDate = parseUpdateDate(text);
                            
                            if (updateDate) {
                                // Check if this is a new update
                                if (!lastSeenDate || updateDate > lastSeenDate) {
                                    const dateStr = updateDate.toLocaleDateString('ru-RU');
                                    console.log(`[TelegramMonitor] NEW CS2 UPDATE DETECTED: ${dateStr}`);
                                    
                                    // Update last seen date
                                    lastSeenDate = updateDate;
                                    
                                    // Save to database
                                    try {
                                        db.prepare(`
                                            CREATE TABLE IF NOT EXISTS update_log (
                                                id INTEGER PRIMARY KEY AUTOINCREMENT,
                                                game_id TEXT,
                                                detected_at TEXT,
                                                update_date TEXT,
                                                message TEXT
                                            )
                                        `).run();
                                        
                                        db.prepare(`
                                            INSERT INTO update_log (game_id, detected_at, update_date, message)
                                            VALUES ('cs2', datetime('now'), ?, ?)
                                        `).run(updateDate.toISOString(), text.substring(0, 200));
                                    } catch (e) {}
                                    
                                    // Set status to updating
                                    const currentStatus = getGameStatus('cs2');
                                    if (currentStatus.status === 'operational') {
                                        setGameStatus('cs2', 'updating', 
                                            `CS2 обновление от ${dateStr} - чит обновляется`, 
                                            'telegram_bot'
                                        );
                                        
                                        // Send notification to admin
                                        sendNotification(
                                            `🚨 <b>CS2 UPDATE DETECTED!</b>\n\n` +
                                            `📅 Дата: ${dateStr}\n` +
                                            `🎮 Статус чита: <b>UPDATING</b>\n\n` +
                                            `⚠️ Нужно обновить offsets!\n\n` +
                                            `После обновления зайди в админку и нажми ✅ WORKING`
                                        );
                                    }
                                    
                                    resolve({ detected: true, date: updateDate });
                                    return;
                                }
                            }
                        }
                    }
                    
                    resolve({ detected: false });
                } catch (e) {
                    console.error('[TelegramMonitor] Parse error:', e.message);
                    resolve({ detected: false, error: e.message });
                }
            });
        }).on('error', (e) => {
            console.error('[TelegramMonitor] Request error:', e.message);
            resolve({ detected: false, error: e.message });
        });
    });
}

/**
 * Initialize last seen date from database
 */
function initLastSeenDate() {
    try {
        db.prepare(`
            CREATE TABLE IF NOT EXISTS update_log (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                game_id TEXT,
                detected_at TEXT,
                update_date TEXT,
                message TEXT
            )
        `).run();
        
        const lastUpdate = db.prepare(`
            SELECT update_date FROM update_log 
            WHERE game_id = 'cs2' 
            ORDER BY detected_at DESC LIMIT 1
        `).get();
        
        if (lastUpdate && lastUpdate.update_date) {
            lastSeenDate = new Date(lastUpdate.update_date);
            console.log(`[TelegramMonitor] Last known update: ${lastSeenDate.toISOString()}`);
        }
    } catch (e) {
        console.error('[TelegramMonitor] Init error:', e.message);
    }
}

/**
 * Start monitoring loop
 */
function startMonitoring() {
    if (isRunning) return;
    isRunning = true;
    
    console.log('[TelegramMonitor] Started monitoring @cstwoupdate');
    initLastSeenDate();
    
    // Initial check
    checkChannel();
    
    // Periodic checks
    setInterval(() => {
        if (isRunning) {
            checkChannel();
        }
    }, CHECK_INTERVAL);
}

/**
 * Stop monitoring
 */
function stopMonitoring() {
    isRunning = false;
    console.log('[TelegramMonitor] Stopped');
}

/**
 * Manual check (for API)
 */
async function manualCheck() {
    return await checkChannel();
}

module.exports = {
    startMonitoring,
    stopMonitoring,
    setGameStatus,
    getGameStatus,
    manualCheck,
    checkChannel
};
