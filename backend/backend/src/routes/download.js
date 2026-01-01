/**
 * Download Routes
 */

const express = require('express');
const path = require('path');
const fs = require('fs');
const crypto = require('crypto');
const db = require('../database/db');
const { authenticate, verifyHwid, checkLicense } = require('../middleware/auth');

const router = express.Router();
const STORAGE_PATH = process.env.STORAGE_PATH || '../storage';
const ENCRYPTION_KEY = process.env.ENCRYPTION_KEY || 'default-32-byte-key-change-me!!';

/**
 * GET /api/download/launcher
 * PUBLIC - Download launcher (no auth required)
 * NOTE: No rate limit on this specific endpoint to allow updates
 */
router.get('/launcher', (req, res) => {
    const launcherDir = path.join(__dirname, '../../../storage/games/launcher');
    
    if (!fs.existsSync(launcherDir)) {
        return res.status(404).json({ error: 'Launcher not available' });
    }
    
    // Get latest launcher file
    const files = fs.readdirSync(launcherDir)
        .filter(f => f.endsWith('.enc') || f.endsWith('.exe'))
        .sort((a, b) => {
            const tsA = parseInt(a.split('_')[1]) || 0;
            const tsB = parseInt(b.split('_')[1]) || 0;
            return tsB - tsA;
        });
    
    if (files.length === 0) {
        return res.status(404).json({ error: 'No launcher available' });
    }
    
    const latestFile = files[0];
    const filePath = path.join(launcherDir, latestFile);
    
    console.log(`[Download] Public launcher download: ${latestFile} from ${req.ip}`);
    
    // Decrypt and send
    if (filePath.endsWith('.enc')) {
        try {
            const decrypted = decryptFile(filePath);
            res.setHeader('Content-Disposition', 'attachment; filename="CS-Legit-Launcher.exe"');
            res.setHeader('Content-Type', 'application/octet-stream');
            res.setHeader('Content-Length', decrypted.length);
            res.send(decrypted);
        } catch (err) {
            console.error('Launcher decrypt error:', err);
            return res.status(500).json({ error: 'Download failed' });
        }
    } else {
        res.download(filePath, 'CS-Legit-Launcher.exe');
    }
});

/**
 * Decrypt file for streaming
 */
function decryptFile(filePath) {
    const encrypted = fs.readFileSync(filePath);
    
    // First 16 bytes are IV
    const iv = encrypted.slice(0, 16);
    const data = encrypted.slice(16);
    
    const decipher = crypto.createDecipheriv('aes-256-cbc', 
        Buffer.from(ENCRYPTION_KEY.padEnd(32).slice(0, 32)), 
        iv
    );
    
    return Buffer.concat([decipher.update(data), decipher.final()]);
}

/**
 * GET /api/download/:game/:version
 * Download cheat file
 */
router.get('/:game/:version', authenticate, verifyHwid, (req, res) => {
    const { game, version } = req.params;
    const hwid = req.headers['x-hwid'];
    
    // Check license
    const license = db.prepare(`
        SELECT * FROM licenses 
        WHERE user_id = ? AND game_id = ? 
        AND (expires_at IS NULL OR expires_at > datetime('now'))
    `).get(req.user.id, game);
    
    if (!license) {
        return res.status(403).json({ error: 'No valid license for this game' });
    }
    
    // Get version info
    const versionInfo = db.prepare(`
        SELECT * FROM versions 
        WHERE game_id = ? AND version = ?
    `).get(game, version);
    
    if (!versionInfo) {
        return res.status(404).json({ error: 'Version not found' });
    }
    
    // Construct file path
    const filePath = path.join(STORAGE_PATH, versionInfo.file_path);
    
    if (!fs.existsSync(filePath)) {
        return res.status(404).json({ error: 'File not found' });
    }
    
    // Log download
    db.prepare(`
        INSERT INTO download_logs (user_id, game_id, version, ip_address, hwid)
        VALUES (?, ?, ?, ?, ?)
    `).run(req.user.id, game, version, req.ip, hwid);
    
    // Check if file is encrypted (.enc extension)
    if (filePath.endsWith('.enc')) {
        try {
            const decrypted = decryptFile(filePath);
            
            // Send with original filename (without .enc)
            const originalName = path.basename(filePath, '.enc');
            res.setHeader('Content-Disposition', `attachment; filename="${originalName}"`);
            res.setHeader('Content-Type', 'application/octet-stream');
            res.send(decrypted);
        } catch (err) {
            console.error('Decryption error:', err);
            return res.status(500).json({ error: 'Failed to decrypt file' });
        }
    } else {
        // Send unencrypted file directly
        res.download(filePath);
    }
});

/**
 * GET /api/download/:game/external
 * Download latest external cheat
 */
router.get('/:game/external', authenticate, (req, res) => {
    const { game } = req.params;
    
    // Check license
    const license = db.prepare(`
        SELECT * FROM licenses 
        WHERE user_id = ? AND game_id = ? 
        AND (expires_at IS NULL OR expires_at > datetime('now'))
    `).get(req.user.id, game);
    
    if (!license) {
        return res.status(403).json({ error: 'No valid license for this game' });
    }
    
    // Find latest file in storage
    const gameDir = path.join(__dirname, '../../storage/games', game);
    
    if (!fs.existsSync(gameDir)) {
        return res.status(404).json({ error: 'Game not found' });
    }
    
    // Get all files and find the latest
    const files = fs.readdirSync(gameDir)
        .filter(f => f.endsWith('.enc') || f.endsWith('.exe'))
        .sort((a, b) => {
            // Sort by timestamp in filename (format: version_timestamp_name.ext)
            const tsA = parseInt(a.split('_')[1]) || 0;
            const tsB = parseInt(b.split('_')[1]) || 0;
            return tsB - tsA;
        });
    
    if (files.length === 0) {
        return res.status(404).json({ error: 'No files available' });
    }
    
    const latestFile = files[0];
    const filePath = path.join(gameDir, latestFile);
    
    console.log(`[Download] User ${req.user.id} downloading ${game}/external: ${latestFile}`);
    
    // Log download
    try {
        db.prepare(`
            INSERT INTO download_logs (user_id, game_id, version, ip_address)
            VALUES (?, ?, ?, ?)
        `).run(req.user.id, game, 'external', req.ip);
    } catch (e) {
        // Ignore log errors
    }
    
    // Decrypt and send
    if (filePath.endsWith('.enc')) {
        try {
            const decrypted = decryptFile(filePath);
            const originalName = game + '_external.exe';
            res.setHeader('Content-Disposition', `attachment; filename="${originalName}"`);
            res.setHeader('Content-Type', 'application/octet-stream');
            res.setHeader('Content-Length', decrypted.length);
            res.send(decrypted);
        } catch (err) {
            console.error('Decryption error:', err);
            return res.status(500).json({ error: 'Download failed' });
        }
    } else {
        res.download(filePath);
    }
});

/**
 * GET /api/download/:game/latest
 * Download latest version (alias)
 */
router.get('/:game/latest', authenticate, (req, res) => {
    res.redirect(`/api/download/${req.params.game}/external`);
});

module.exports = router;

