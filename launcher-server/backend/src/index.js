/**
 * Cheat Launcher Server - Main Entry Point
 */

require('dotenv').config();

const express = require('express');
const cors = require('cors');
const helmet = require('helmet');
const compression = require('compression');
const rateLimit = require('express-rate-limit');
const path = require('path');

const authRoutes = require('./routes/auth');
const gamesRoutes = require('./routes/games');
const downloadRoutes = require('./routes/download');
const offsetsRoutes = require('./routes/offsets');
const adminRoutes = require('./routes/admin');

const app = express();

// Serve admin panel static files
app.use('/panel', express.static(path.join(__dirname, '../../admin-panel')));

// Serve public website (main page)
app.use(express.static(path.join(__dirname, '../../public')));

// ============================================
// Middleware
// ============================================

// Compression for all responses (gzip)
app.use(compression({
    level: 6, // Balance between speed and compression
    threshold: 1024, // Only compress responses > 1KB
    filter: (req, res) => {
        if (req.headers['x-no-compression']) return false;
        return compression.filter(req, res);
    }
}));

app.use(helmet());
app.use(cors());
app.use(express.json());

// Rate limiting DISABLED for now - causing registration issues
// TODO: Re-enable with proper configuration after testing
// const limiter = rateLimit({ ... });
// app.use(limiter);

// ============================================
// Routes
// ============================================

app.use('/api/auth', authRoutes);
app.use('/api/games', gamesRoutes);
app.use('/api/download', downloadRoutes);
app.use('/api/offsets', offsetsRoutes);
app.use('/api/admin', adminRoutes);

// Health check endpoint
app.get('/health', (req, res) => {
    const db = require('./database/db');
    
    try {
        // Test database connectivity
        db.prepare('SELECT 1').get();
        
        // Get basic stats
        const userCount = db.prepare('SELECT COUNT(*) as count FROM users').get();
        const gameCount = db.prepare('SELECT COUNT(*) as count FROM games WHERE is_active = 1').get();
        
        res.json({ 
            status: 'ok', 
            version: '1.0.0',
            uptime: process.uptime(),
            timestamp: new Date().toISOString(),
            database: 'connected',
            stats: {
                users: userCount.count,
                games: gameCount.count
            }
        });
    } catch (error) {
        console.error('[Health Check] Error:', error.message);
        res.status(503).json({ 
            status: 'error', 
            message: 'Service unavailable',
            database: 'disconnected'
        });
    }
});

// Detailed metrics endpoint (for monitoring)
app.get('/api/metrics', (req, res) => {
    const db = require('./database/db');
    
    try {
        const stats = {
            users: {
                total: db.prepare('SELECT COUNT(*) as count FROM users').get().count,
                admins: db.prepare('SELECT COUNT(*) as count FROM users WHERE is_admin = 1').get().count
            },
            licenses: {
                total: db.prepare('SELECT COUNT(*) as count FROM licenses').get().count,
                active: db.prepare('SELECT COUNT(*) as count FROM licenses WHERE user_id IS NOT NULL').get().count,
                unused: db.prepare('SELECT COUNT(*) as count FROM licenses WHERE user_id IS NULL').get().count
            },
            games: db.prepare('SELECT COUNT(*) as count FROM games WHERE is_active = 1').get().count,
            uptime: process.uptime(),
            memory: process.memoryUsage(),
            timestamp: new Date().toISOString()
        };
        
        res.json(stats);
    } catch (error) {
        console.error('[Metrics] Error:', error.message);
        res.status(500).json({ error: 'Failed to fetch metrics' });
    }
});

// ============================================
// Error Handler
// ============================================

app.use((err, req, res, next) => {
    console.error(err.stack);
    res.status(500).json({ error: 'Internal server error' });
});

// ============================================
// Start Server
// ============================================

const PORT = process.env.PORT || 3000;
const HOST = process.env.HOST || '0.0.0.0';

// Start Telegram monitoring
const { startMonitoring } = require('./services/telegramMonitor');

const server = app.listen(PORT, HOST, () => {
    console.log(`🚀 Launcher Server running on http://${HOST}:${PORT}`);
    console.log(`📁 Storage: ${path.resolve(process.env.STORAGE_PATH || '../storage')}`);
    console.log(`🌐 Public website: http://single-project.duckdns.org`);
    
    // Initialize WebSocket for real-time updates
    const websocket = require('./services/websocket');
    websocket.initialize(server);
    console.log(`🔌 WebSocket server started on ws://${HOST}:${PORT}/ws`);
    
    // Start monitoring @cstwoupdate for CS2 updates
    startMonitoring();
    console.log(`📱 Telegram monitor started - watching @cstwoupdate`);
});

// Export for WebSocket broadcasting
module.exports = { app, server };


