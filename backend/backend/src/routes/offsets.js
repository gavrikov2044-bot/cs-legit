/**
 * Offsets Routes
 * Provides up-to-date game offsets
 */

const express = require('express');
const path = require('path');
const fs = require('fs');
const { authenticate } = require('../middleware/auth');

const router = express.Router();
const STORAGE_PATH = process.env.STORAGE_PATH || '../storage';

/**
 * GET /api/offsets/:game
 * Get current offsets for a game
 */
router.get('/:game', authenticate, (req, res) => {
    const { game } = req.params;
    
    const offsetsPath = path.join(STORAGE_PATH, 'offsets', `${game}.json`);
    
    if (!fs.existsSync(offsetsPath)) {
        return res.status(404).json({ error: 'Offsets not found for this game' });
    }
    
    try {
        const offsets = JSON.parse(fs.readFileSync(offsetsPath, 'utf8'));
        res.json(offsets);
    } catch (err) {
        console.error('Failed to read offsets:', err);
        return res.status(500).json({ error: 'Failed to read offsets' });
    }
});

/**
 * GET /api/offsets/:game/hash
 * Get hash of current offsets (for checking updates)
 */
router.get('/:game/hash', authenticate, (req, res) => {
    const { game } = req.params;
    
    const offsetsPath = path.join(STORAGE_PATH, 'offsets', `${game}.json`);
    
    if (!fs.existsSync(offsetsPath)) {
        return res.status(404).json({ error: 'Offsets not found for this game' });
    }
    
    try {
        const content = fs.readFileSync(offsetsPath, 'utf8');
        const crypto = require('crypto');
        const hash = crypto.createHash('sha256').update(content).digest('hex');
        
        res.json({ hash, game });
    } catch (err) {
        return res.status(500).json({ error: 'Failed to compute hash' });
    }
});

module.exports = router;

