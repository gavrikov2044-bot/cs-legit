/**
 * Database Migration Script
 */

require('dotenv').config();
const db = require('./db');
const bcrypt = require('bcryptjs');

console.log('🔄 Running migrations...');

// Users table
db.exec(`
    CREATE TABLE IF NOT EXISTS users (
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        username TEXT UNIQUE NOT NULL,
        password_hash TEXT NOT NULL,
        hwid TEXT,
        is_admin INTEGER DEFAULT 0,
        created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
        last_login DATETIME
    )
`);

// Licenses table (user_id can be NULL for unassigned keys)
db.exec(`
    CREATE TABLE IF NOT EXISTS licenses (
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        user_id INTEGER,
        game_id TEXT NOT NULL,
        license_key TEXT UNIQUE NOT NULL,
        expires_at DATETIME,
        created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
        FOREIGN KEY (user_id) REFERENCES users(id)
    )
`);

// Games table
db.exec(`
    CREATE TABLE IF NOT EXISTS games (
        id TEXT PRIMARY KEY,
        name TEXT NOT NULL,
        description TEXT,
        icon_url TEXT,
        latest_version TEXT,
        is_active INTEGER DEFAULT 1,
        created_at DATETIME DEFAULT CURRENT_TIMESTAMP
    )
`);

// Versions table
db.exec(`
    CREATE TABLE IF NOT EXISTS versions (
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        game_id TEXT NOT NULL,
        version TEXT NOT NULL,
        changelog TEXT,
        file_path TEXT NOT NULL,
        file_hash TEXT,
        created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
        FOREIGN KEY (game_id) REFERENCES games(id)
    )
`);

// Download logs
db.exec(`
    CREATE TABLE IF NOT EXISTS download_logs (
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        user_id INTEGER NOT NULL,
        game_id TEXT NOT NULL,
        version TEXT NOT NULL,
        ip_address TEXT,
        hwid TEXT,
        downloaded_at DATETIME DEFAULT CURRENT_TIMESTAMP,
        FOREIGN KEY (user_id) REFERENCES users(id)
    )
`);

// Add ban columns if they don't exist
try {
    db.exec('ALTER TABLE users ADD COLUMN is_banned INTEGER DEFAULT 0');
    console.log('✅ Added is_banned column');
} catch (e) {
    // Column already exists
}

try {
    db.exec('ALTER TABLE users ADD COLUMN ban_reason TEXT');
    console.log('✅ Added ban_reason column');
} catch (e) {
    // Column already exists
}

// Create default admin if not exists
const adminExists = db.prepare('SELECT id FROM users WHERE username = ?').get('admin');
if (!adminExists) {
    const password = process.env.ADMIN_PASSWORD || 'admin123';
    const hash = bcrypt.hashSync(password, 10);
    db.prepare('INSERT INTO users (username, password_hash, is_admin) VALUES (?, ?, 1)')
      .run('admin', hash);
    console.log('✅ Created admin user (password: ' + password + ')');
}

// Insert sample games
const gamesExist = db.prepare('SELECT COUNT(*) as count FROM games').get();
if (gamesExist.count === 0) {
    const insertGame = db.prepare(`
        INSERT INTO games (id, name, description, latest_version) 
        VALUES (?, ?, ?, ?)
    `);
    
    insertGame.run('cs2', 'Counter-Strike 2', 'Internal cheat with ESP, Aimbot', '1.0.0');
    insertGame.run('dayz', 'DayZ', 'External overlay with ESP', '1.0.0');
    insertGame.run('rust', 'Rust', 'Kernel-level cheat', '1.0.0');
    
    console.log('✅ Created sample games');
}

console.log('✅ Migrations complete!');

