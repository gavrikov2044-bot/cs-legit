/**
 * CS2 Update Checker
 * Checks if game has been updated and offsets need refresh
 */

const https = require('https');
const fs = require('fs');
const path = require('path');

// CS2 App ID
const CS2_APP_ID = 730;

// Cache for update status
let updateCache = {
    cs2: {
        steamBuild: null,
        offsetsBuild: null,
        needsUpdate: false,
        lastCheck: 0,
        message: 'Unknown'
    }
};

const CACHE_TTL = 5 * 60 * 1000; // 5 minutes

/**
 * Get CS2 build number from Steam API
 */
async function getSteamBuildNumber() {
    return new Promise((resolve, reject) => {
        // Use Steam's public API to get app info
        const url = `https://api.steampowered.com/ISteamApps/UpToDateCheck/v1/?appid=${CS2_APP_ID}&version=0`;
        
        https.get(url, (res) => {
            let data = '';
            res.on('data', chunk => data += chunk);
            res.on('end', () => {
                try {
                    const json = JSON.parse(data);
                    if (json.response && json.response.required_version) {
                        resolve(json.response.required_version);
                    } else {
                        // Fallback: try to get from SteamDB-like endpoint
                        resolve(null);
                    }
                } catch (e) {
                    resolve(null);
                }
            });
        }).on('error', () => resolve(null));
        
        // Timeout after 5 seconds
        setTimeout(() => resolve(null), 5000);
    });
}

/**
 * Get build number from local offsets info.json
 */
function getOffsetsBuildNumber(gameId) {
    const offsetsPath = path.join(__dirname, '../../storage/offsets');
    const infoFile = path.join(offsetsPath, `${gameId}_info.json`);
    const mainFile = path.join(offsetsPath, `${gameId}.json`);
    
    // Try info file first
    if (fs.existsSync(infoFile)) {
        try {
            const info = JSON.parse(fs.readFileSync(infoFile, 'utf8'));
            return {
                build: info.build_number || null,
                timestamp: info.timestamp || null
            };
        } catch (e) {}
    }
    
    // Check main offsets file modification time
    if (fs.existsSync(mainFile)) {
        const stats = fs.statSync(mainFile);
        return {
            build: null,
            timestamp: stats.mtime.toISOString()
        };
    }
    
    return { build: null, timestamp: null };
}

/**
 * Check if game needs update
 */
async function checkGameUpdate(gameId) {
    const now = Date.now();
    
    // Return cached result if fresh
    if (updateCache[gameId] && (now - updateCache[gameId].lastCheck) < CACHE_TTL) {
        return updateCache[gameId];
    }
    
    const result = {
        steamBuild: null,
        offsetsBuild: null,
        needsUpdate: false,
        lastCheck: now,
        message: 'Unknown'
    };
    
    if (gameId === 'cs2') {
        // Get Steam build
        result.steamBuild = await getSteamBuildNumber();
        
        // Get offsets build
        const offsetsInfo = getOffsetsBuildNumber(gameId);
        result.offsetsBuild = offsetsInfo.build;
        result.offsetsTimestamp = offsetsInfo.timestamp;
        
        // Compare builds
        if (result.steamBuild && result.offsetsBuild) {
            result.needsUpdate = result.steamBuild !== result.offsetsBuild;
            result.message = result.needsUpdate ? 
                `Game updated to build ${result.steamBuild}, offsets are for build ${result.offsetsBuild}` :
                'Offsets are up to date';
        } else if (result.offsetsTimestamp) {
            // Check by timestamp - if offsets older than 24h, might need update
            const offsetsAge = (now - new Date(result.offsetsTimestamp).getTime()) / (1000 * 60 * 60);
            result.needsUpdate = offsetsAge > 24;
            result.message = result.needsUpdate ?
                `Offsets are ${Math.floor(offsetsAge)}h old, may need update` :
                'Offsets are recent';
        } else {
            result.needsUpdate = true;
            result.message = 'No offsets available';
        }
    }
    
    updateCache[gameId] = result;
    return result;
}

/**
 * Get status for all games
 */
async function getAllGameStatus() {
    const cs2Status = await checkGameUpdate('cs2');
    
    return {
        cs2: cs2Status
    };
}

module.exports = {
    checkGameUpdate,
    getAllGameStatus,
    getSteamBuildNumber,
    getOffsetsBuildNumber
};

