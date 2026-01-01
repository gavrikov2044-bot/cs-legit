/**
 * Authentication Routes
 */

const express = require('express');
const bcrypt = require('bcryptjs');
const jwt = require('jsonwebtoken');
const db = require('../database/db');

const router = express.Router();
const JWT_SECRET = process.env.JWT_SECRET || 'default-secret-change-me';

/**
 * POST /api/auth/login
 * Login and get JWT token
 */
router.post('/login', (req, res) => {
    const { username, password, hwid } = req.body;
    
    if (!username || !password) {
        return res.status(400).json({ error: 'Username and password required' });
    }
    
    // Find user
    const user = db.prepare('SELECT * FROM users WHERE username = ?').get(username);
    
    if (!user) {
        return res.status(401).json({ error: 'Invalid credentials' });
    }
    
    // Verify password
    if (!bcrypt.compareSync(password, user.password_hash)) {
        return res.status(401).json({ error: 'Invalid credentials' });
    }
    
    // Check HWID if set
    if (user.hwid && hwid && user.hwid !== hwid) {
        return res.status(403).json({ error: 'HWID mismatch - contact support' });
    }
    
    // Update HWID if not set
    if (!user.hwid && hwid) {
        db.prepare('UPDATE users SET hwid = ? WHERE id = ?').run(hwid, user.id);
    }
    
    // Update last login
    db.prepare("UPDATE users SET last_login = datetime('now') WHERE id = ?").run(user.id);
    
    // Generate JWT
    const token = jwt.sign(
        { userId: user.id, username: user.username },
        JWT_SECRET,
        { expiresIn: '7d' }
    );
    
    // Get license info
    const license = db.prepare(`
        SELECT game_id, expires_at,
               CASE 
                   WHEN expires_at IS NULL THEN -1
                   ELSE CAST((julianday(expires_at) - julianday('now')) AS INTEGER)
               END as days_remaining
        FROM licenses 
        WHERE user_id = ? AND game_id = 'cs2'
        AND (expires_at IS NULL OR expires_at > datetime('now'))
    `).get(user.id);
    
    res.json({
        token,
        user: {
            id: user.id,
            username: user.username,
            is_admin: !!user.is_admin
        },
        has_license: !!license,
        hwid_bound: !!user.hwid,
        days_remaining: license ? license.days_remaining : 0,
        license_expires: license ? license.expires_at : null
    });
});

/**
 * POST /api/auth/register
 * Register new user with license key (required)
 */
router.post('/register', (req, res) => {
    const { username, password, license_key } = req.body;
    
    console.log('[Register] Attempt:', { username, license_key: license_key ? '***' + license_key.slice(-4) : 'none' });
    
    if (!username || !password) {
        console.log('[Register] Error: Missing username or password');
        return res.status(400).json({ error: 'Username and password required' });
    }
    
    if (!license_key) {
        console.log('[Register] Error: No license key');
        return res.status(400).json({ error: 'License key required for registration' });
    }
    
    if (username.length < 3 || username.length > 32) {
        return res.status(400).json({ error: 'Username must be 3-32 characters' });
    }
    
    if (password.length < 6) {
        return res.status(400).json({ error: 'Password must be at least 6 characters' });
    }
    
    // Check if username exists
    const existing = db.prepare('SELECT id FROM users WHERE username = ?').get(username);
    if (existing) {
        console.log('[Register] Error: Username taken -', username);
        return res.status(409).json({ error: 'Username already taken' });
    }
    
    // Verify license key
    const licenseInfo = db.prepare('SELECT * FROM licenses WHERE license_key = ? AND user_id IS NULL')
                          .get(license_key);
    if (!licenseInfo) {
        console.log('[Register] Error: Invalid/used key -', license_key);
        return res.status(400).json({ error: 'Invalid or already used license key' });
    }
    
    try {
        // Create user
        const hash = bcrypt.hashSync(password, 10);
        const result = db.prepare('INSERT INTO users (username, password_hash) VALUES (?, ?)')
                         .run(username, hash);
        
        // Assign license to user
        db.prepare('UPDATE licenses SET user_id = ? WHERE id = ?')
          .run(result.lastInsertRowid, licenseInfo.id);
        
        console.log('[Register] Success! User:', username, 'ID:', result.lastInsertRowid);
        
        res.status(201).json({
            success: true,
            message: 'Registration successful!',
            user_id: result.lastInsertRowid,
            game: licenseInfo.game_id,
            expires_at: licenseInfo.expires_at
        });
    } catch (err) {
        console.error('[Register] Database error:', err.message);
        return res.status(500).json({ error: 'Registration failed - try again' });
    }
});

/**
 * POST /api/auth/activate
 * Activate additional license key for existing user
 */
router.post('/activate', (req, res) => {
    const { license_key } = req.body;
    const authHeader = req.headers.authorization;
    
    if (!authHeader || !authHeader.startsWith('Bearer ')) {
        return res.status(401).json({ error: 'Login required' });
    }
    
    if (!license_key) {
        return res.status(400).json({ error: 'License key required' });
    }
    
    // Verify token
    const token = authHeader.substring(7);
    let userId;
    try {
        const decoded = jwt.verify(token, JWT_SECRET);
        userId = decoded.userId;
    } catch (err) {
        return res.status(401).json({ error: 'Invalid token' });
    }
    
    // Check license key
    const licenseInfo = db.prepare('SELECT * FROM licenses WHERE license_key = ? AND user_id IS NULL')
                          .get(license_key);
    if (!licenseInfo) {
        return res.status(400).json({ error: 'Invalid or already used license key' });
    }
    
    // Check if user already has active license for this game
    const existingLicense = db.prepare(`
        SELECT * FROM licenses 
        WHERE user_id = ? AND game_id = ? 
        AND (expires_at IS NULL OR expires_at > datetime('now'))
    `).get(userId, licenseInfo.game_id);
    
    if (existingLicense) {
        // Extend existing license
        if (licenseInfo.expires_at && existingLicense.expires_at) {
            // Add days to existing expiration
            db.prepare(`
                UPDATE licenses 
                SET expires_at = datetime(expires_at, '+' || ? || ' days')
                WHERE id = ?
            `).run(
                Math.floor((new Date(licenseInfo.expires_at) - new Date()) / (1000 * 60 * 60 * 24)),
                existingLicense.id
            );
        }
        // Delete the new unused key
        db.prepare('DELETE FROM licenses WHERE id = ?').run(licenseInfo.id);
        
        return res.json({
            success: true,
            message: 'License extended!',
            game: licenseInfo.game_id
        });
    }
    
    // Assign new license to user
    db.prepare('UPDATE licenses SET user_id = ? WHERE id = ?')
      .run(userId, licenseInfo.id);
    
    res.json({
        success: true,
        message: 'License activated!',
        game: licenseInfo.game_id,
        expires_at: licenseInfo.expires_at
    });
});

/**
 * POST /api/auth/verify
 * Verify token is valid
 */
router.post('/verify', (req, res) => {
    const { token } = req.body;
    
    try {
        const decoded = jwt.verify(token, JWT_SECRET);
        const user = db.prepare('SELECT id, username, is_admin FROM users WHERE id = ?')
                       .get(decoded.userId);
        
        if (!user) {
            return res.status(401).json({ valid: false });
        }
        
        res.json({ valid: true, user });
    } catch (err) {
        res.json({ valid: false });
    }
});

/**
 * GET /api/auth/me
 * Get current user info with license status
 */
router.get('/me', (req, res) => {
    const authHeader = req.headers.authorization;
    
    if (!authHeader || !authHeader.startsWith('Bearer ')) {
        return res.status(401).json({ error: 'Login required' });
    }
    
    const token = authHeader.substring(7);
    let userId;
    try {
        const decoded = jwt.verify(token, JWT_SECRET);
        userId = decoded.userId;
    } catch (err) {
        return res.status(401).json({ error: 'Invalid token' });
    }
    
    const user = db.prepare('SELECT id, username, hwid, is_admin, created_at, last_login FROM users WHERE id = ?')
                   .get(userId);
    
    if (!user) {
        return res.status(404).json({ error: 'User not found' });
    }
    
    // Get active licenses
    const licenses = db.prepare(`
        SELECT game_id, expires_at,
               CASE 
                   WHEN expires_at IS NULL THEN -1
                   ELSE CAST((julianday(expires_at) - julianday('now')) AS INTEGER)
               END as days_remaining
        FROM licenses 
        WHERE user_id = ? 
        AND (expires_at IS NULL OR expires_at > datetime('now'))
    `).all(userId);
    
    const hasLicense = licenses.length > 0;
    const cs2License = licenses.find(l => l.game_id === 'cs2');
    
    res.json({
        id: user.id,
        username: user.username,
        hwid: user.hwid,
        hwid_bound: !!user.hwid,
        is_admin: !!user.is_admin,
        has_license: hasLicense,
        licenses: licenses,
        days_remaining: cs2License ? cs2License.days_remaining : 0,
        license_expires: cs2License ? cs2License.expires_at : null
    });
});

module.exports = router;

