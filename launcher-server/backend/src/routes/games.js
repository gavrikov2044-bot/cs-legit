/**
 * Games Routes
 */

const express = require('express');
const db = require('../database/db');
const { authenticate } = require('../middleware/auth');
const fs = require('fs');
const path = require('path');

const router = express.Router();

// Status cache - faster updates (2 seconds instead of 60)
let statusCache = {
    data: null,
    timestamp: 0
};
const CACHE_TTL = 2000; // 2 seconds for instant updates

/**
 * GET /api/games/status
 * Public endpoint - returns status for all games
 */
router.get('/status', (req, res) => {
    const now = Date.now();
    
    // Return cached data if fresh
    if (statusCache.data && (now - statusCache.timestamp) < CACHE_TTL) {
        return res.json(statusCache.data);
    }
    
    // Get games with their latest version info
    const games = db.prepare(`
        SELECT g.id, g.name, g.latest_version,
               v.created_at as last_update,
               v.changelog
        FROM games g
        LEFT JOIN versions v ON v.game_id = g.id
        WHERE g.is_active = 1
        ORDER BY v.created_at DESC
    `).all();
    
    // Read offsets to check if they're valid
    const offsetsPath = path.join(__dirname, '../../../storage/offsets');
    
    const gameStatuses = {};
    const processedGames = new Set();
    
    // Create status table if not exists
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
    
    for (const game of games) {
        if (processedGames.has(game.id)) continue;
        processedGames.add(game.id);
        
        // Check custom status from database first
        let customStatus = null;
        try {
            customStatus = db.prepare('SELECT * FROM game_status WHERE game_id = ?').get(game.id);
        } catch (e) {}
        
        let status = customStatus?.status || 'operational';
        let message = customStatus?.message || 'All systems operational';
        
        // Check if offsets file exists and is recent (only if no custom status set)
        const offsetFile = path.join(offsetsPath, `${game.id}.json`);
        let offsetsAge = null;
        
        if (fs.existsSync(offsetFile)) {
            const stats = fs.statSync(offsetFile);
            offsetsAge = Math.floor((now - stats.mtimeMs) / (1000 * 60 * 60)); // hours
            
            // Only auto-change to warning if status is operational
            if (!customStatus && offsetsAge > 48) {
                status = 'warning';
                message = 'Offsets may need update';
            }
        } else if (!customStatus) {
            status = 'maintenance';
            message = 'Offsets not available';
        }
        
        // Check if version exists
        if (!customStatus && !game.latest_version) {
            status = 'maintenance';
            message = 'No version available';
        }
        
        gameStatuses[game.id] = {
            name: game.name,
            status,
            message,
            version: game.latest_version || '---',
            last_update: game.last_update,
            offsets_age_hours: offsetsAge
        };
    }
    
    // Calculate overall status
    const allOperational = Object.values(gameStatuses).every(g => g.status === 'operational');
    const anyMaintenance = Object.values(gameStatuses).some(g => g.status === 'maintenance');
    
    const response = {
        overall: anyMaintenance ? 'maintenance' : (allOperational ? 'operational' : 'warning'),
        overall_message: anyMaintenance ? 'Some systems under maintenance' : 
                        (allOperational ? 'All systems operational' : 'Some systems need attention'),
        games: gameStatuses,
        timestamp: new Date().toISOString()
    };
    
    // Cache the result
    statusCache = { data: response, timestamp: now };
    
    res.json(response);
});

/**
 * GET /api/games
 * Get list of all available games
 */
router.get('/', authenticate, (req, res) => {
    const games = db.prepare(`
        SELECT 
            g.id, 
            g.name, 
            g.description, 
            g.icon_url,
            g.latest_version,
            CASE WHEN l.id IS NOT NULL THEN 1 ELSE 0 END as has_license,
            l.expires_at as license_expires
        FROM games g
        LEFT JOIN licenses l ON l.game_id = g.id AND l.user_id = ?
            AND (l.expires_at IS NULL OR l.expires_at > datetime('now'))
        WHERE g.is_active = 1
        ORDER BY g.name
    `).all(req.user.id);
    
    res.json({ games });
});

/**
 * GET /api/games/:id
 * Get single game details
 */
router.get('/:id', authenticate, (req, res) => {
    const game = db.prepare(`
        SELECT 
            g.*,
            CASE WHEN l.id IS NOT NULL THEN 1 ELSE 0 END as has_license,
            l.expires_at as license_expires
        FROM games g
        LEFT JOIN licenses l ON l.game_id = g.id AND l.user_id = ?
            AND (l.expires_at IS NULL OR l.expires_at > datetime('now'))
        WHERE g.id = ? AND g.is_active = 1
    `).get(req.user.id, req.params.id);
    
    if (!game) {
        return res.status(404).json({ error: 'Game not found' });
    }
    
    res.json({ game });
});

/**
 * GET /api/games/:id/versions
 * Get all versions of a game cheat
 */
router.get('/:id/versions', authenticate, (req, res) => {
    // Check license
    const license = db.prepare(`
        SELECT * FROM licenses 
        WHERE user_id = ? AND game_id = ? 
        AND (expires_at IS NULL OR expires_at > datetime('now'))
    `).get(req.user.id, req.params.id);
    
    if (!license) {
        return res.status(403).json({ error: 'No license for this game' });
    }
    
    const versions = db.prepare(`
        SELECT version, changelog, created_at
        FROM versions
        WHERE game_id = ?
        ORDER BY created_at DESC
    `).all(req.params.id);
    
    res.json({ versions });
});

/**
 * GET /api/games/:id/status
 * Check if game cheat is up to date
 */
router.get('/:id/status', authenticate, (req, res) => {
    const { current_version } = req.query;
    
    const game = db.prepare('SELECT latest_version FROM games WHERE id = ?')
                   .get(req.params.id);
    
    if (!game) {
        return res.status(404).json({ error: 'Game not found' });
    }
    
    const needsUpdate = current_version !== game.latest_version;
    
    res.json({
        current_version,
        latest_version: game.latest_version,
        needs_update: needsUpdate
    });
});

module.exports = router;

