/**
 * Authentication Middleware
 */

const jwt = require('jsonwebtoken');
const db = require('../database/db');

const JWT_SECRET = process.env.JWT_SECRET || 'default-secret-change-me';

/**
 * Verify JWT token
 */
function authenticate(req, res, next) {
    const authHeader = req.headers.authorization;
    
    if (!authHeader || !authHeader.startsWith('Bearer ')) {
        return res.status(401).json({ error: 'No token provided' });
    }
    
    const token = authHeader.substring(7);
    
    try {
        const decoded = jwt.verify(token, JWT_SECRET);
        
        // Get user from database
        const user = db.prepare('SELECT id, username, is_admin, is_banned, ban_reason, hwid FROM users WHERE id = ?')
                       .get(decoded.userId);
        
        if (!user) {
            return res.status(401).json({ error: 'User not found' });
        }
        
        // Check if user is banned
        if (user.is_banned) {
            return res.status(403).json({ 
                error: 'Account banned', 
                reason: user.ban_reason || 'Contact support for details'
            });
        }
        
        req.user = user;
        next();
    } catch (err) {
        return res.status(401).json({ error: 'Invalid token' });
    }
}

/**
 * Require admin privileges
 */
function requireAdmin(req, res, next) {
    if (!req.user || !req.user.is_admin) {
        return res.status(403).json({ error: 'Admin access required' });
    }
    next();
}

/**
 * Verify HWID matches
 */
function verifyHwid(req, res, next) {
    const hwid = req.headers['x-hwid'];
    
    if (!hwid) {
        return res.status(400).json({ error: 'HWID required' });
    }
    
    // If user has HWID set, verify it matches
    if (req.user.hwid && req.user.hwid !== hwid) {
        return res.status(403).json({ error: 'HWID mismatch' });
    }
    
    // If no HWID set, bind this one
    if (!req.user.hwid) {
        db.prepare('UPDATE users SET hwid = ? WHERE id = ?')
          .run(hwid, req.user.id);
    }
    
    next();
}

/**
 * Check license for game
 */
function checkLicense(gameId) {
    return (req, res, next) => {
        const license = db.prepare(`
            SELECT * FROM licenses 
            WHERE user_id = ? AND game_id = ? 
            AND (expires_at IS NULL OR expires_at > datetime('now'))
        `).get(req.user.id, gameId);
        
        if (!license) {
            return res.status(403).json({ error: 'No valid license for this game' });
        }
        
        req.license = license;
        next();
    };
}

module.exports = {
    authenticate,
    requireAdmin,
    verifyHwid,
    checkLicense
};

