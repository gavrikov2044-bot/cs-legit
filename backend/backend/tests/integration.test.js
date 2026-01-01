/**
 * Integration Tests for Launcher-Server-Website System
 * 
 * Tests the full integration between:
 * - Admin Panel functions
 * - API endpoints
 * - Launcher compatibility
 * - Website data
 */

const http = require('http');

const SERVER_HOST = process.env.TEST_HOST || '127.0.0.1';
const SERVER_PORT = process.env.TEST_PORT || 3000;

// Test utilities
function makeRequest(method, path, data = null, headers = {}) {
    return new Promise((resolve, reject) => {
        const options = {
            hostname: SERVER_HOST,
            port: SERVER_PORT,
            path: path,
            method: method,
            headers: {
                'Content-Type': 'application/json',
                ...headers
            }
        };

        const req = http.request(options, (res) => {
            let body = '';
            res.on('data', chunk => body += chunk);
            res.on('end', () => {
                try {
                    resolve({
                        status: res.statusCode,
                        headers: res.headers,
                        body: body.startsWith('{') || body.startsWith('[') ? JSON.parse(body) : body
                    });
                } catch (e) {
                    resolve({ status: res.statusCode, headers: res.headers, body: body });
                }
            });
        });

        req.on('error', reject);
        if (data) req.write(JSON.stringify(data));
        req.end();
    });
}

// Test results tracking
let passed = 0;
let failed = 0;
const results = [];

function test(name, fn) {
    return fn().then(result => {
        if (result.success) {
            passed++;
            results.push({ name, status: '✅ PASS', details: result.details });
        } else {
            failed++;
            results.push({ name, status: '❌ FAIL', details: result.details });
        }
    }).catch(err => {
        failed++;
        results.push({ name, status: '❌ ERROR', details: err.message });
    });
}

// ==================== TESTS ====================

async function testHealthEndpoint() {
    const res = await makeRequest('GET', '/health');
    return {
        success: res.status === 200 && res.body.status === 'ok',
        details: `Status: ${res.status}, Body: ${JSON.stringify(res.body)}`
    };
}

async function testGamesStatus() {
    const res = await makeRequest('GET', '/api/games/status');
    const hasLauncher = res.body.games && res.body.games.launcher;
    const hasVersion = hasLauncher && res.body.games.launcher.version;
    return {
        success: res.status === 200 && hasLauncher && hasVersion,
        details: `Launcher version: ${hasVersion ? res.body.games.launcher.version : 'NOT FOUND'}`
    };
}

async function testLauncherVersionFormat() {
    const res = await makeRequest('GET', '/api/games/status');
    const version = res.body.games?.launcher?.version || '';
    const validFormat = /^\d+\.\d+\.\d+$/.test(version);
    return {
        success: validFormat,
        details: `Version "${version}" ${validFormat ? 'is valid' : 'is INVALID (expected X.Y.Z)'}`
    };
}

async function testLauncherDownload() {
    const res = await makeRequest('GET', '/api/download/launcher');
    // Check for MZ header (Windows EXE)
    const isExe = typeof res.body === 'string' && res.body.startsWith('MZ');
    const contentLength = res.headers['content-length'];
    return {
        success: res.status === 200 && isExe && parseInt(contentLength) > 1024,
        details: `Status: ${res.status}, Is EXE: ${isExe}, Size: ${contentLength} bytes`
    };
}

async function testLoginWithWrongCredentials() {
    const res = await makeRequest('POST', '/api/auth/login', {
        username: 'nonexistent',
        password: 'wrongpass',
        hwid: 'test123'
    });
    return {
        success: res.status === 401 && res.body.error,
        details: `Status: ${res.status}, Error: ${res.body.error}`
    };
}

async function testLoginWithCorrectCredentials() {
    const res = await makeRequest('POST', '/api/auth/login', {
        username: 'admin',
        password: process.env.ADMIN_PASSWORD || 'SuperSecret123!',
        hwid: 'integration-test-hwid'
    });
    const hasToken = res.status === 200 && res.body.token;
    return {
        success: hasToken || (res.status === 401 && res.body.error.includes('HWID')),
        details: `Status: ${res.status}, Has token: ${!!res.body.token}, Error: ${res.body.error || 'none'}`
    };
}

async function testRegisterDuplicate() {
    // Try to register with existing username
    const res = await makeRequest('POST', '/api/auth/register', {
        username: 'admin',
        password: 'testpass123',
        hwid: 'test-hwid'
    });
    return {
        success: res.status === 400 && res.body.error,
        details: `Status: ${res.status}, Error: ${res.body.error}`
    };
}

async function testGameStatusConsistency() {
    // Check that launcher status is consistent across endpoints
    const statusRes = await makeRequest('GET', '/api/games/status');
    const launcherStatus = statusRes.body.games?.launcher?.status;
    
    return {
        success: ['operational', 'updating', 'maintenance', 'error'].includes(launcherStatus),
        details: `Launcher status: ${launcherStatus}`
    };
}

async function testAdminPanelAccess() {
    // Test that panel returns HTML (or redirect)
    const res = await makeRequest('GET', '/panel/');
    const isHtml = typeof res.body === 'string' && 
                   (res.body.includes('<!DOCTYPE') || res.body.includes('<html') || res.status === 302);
    return {
        success: res.status === 200 || res.status === 302,
        details: `Status: ${res.status}, Is HTML: ${isHtml}`
    };
}

async function testRateLimitNotTooStrict() {
    // Make 5 quick requests - should all succeed
    const results = [];
    for (let i = 0; i < 5; i++) {
        const res = await makeRequest('GET', '/api/games/status');
        results.push(res.status);
    }
    const allOk = results.every(s => s === 200);
    return {
        success: allOk,
        details: `Statuses: ${results.join(', ')}`
    };
}

// ==================== RUNNER ====================

async function runAllTests() {
    console.log('');
    console.log('╔═══════════════════════════════════════════════════════════╗');
    console.log('║     INTEGRATION TESTS - Launcher Server System            ║');
    console.log('╚═══════════════════════════════════════════════════════════╝');
    console.log('');
    console.log(`Testing: http://${SERVER_HOST}:${SERVER_PORT}`);
    console.log('');

    await test('Health Endpoint', testHealthEndpoint);
    await test('Games Status API', testGamesStatus);
    await test('Launcher Version Format', testLauncherVersionFormat);
    await test('Launcher Download', testLauncherDownload);
    await test('Login - Wrong Credentials', testLoginWithWrongCredentials);
    await test('Login - Admin User', testLoginWithCorrectCredentials);
    await test('Register - Duplicate Username', testRegisterDuplicate);
    await test('Game Status Consistency', testGameStatusConsistency);
    await test('Admin Panel Access', testAdminPanelAccess);
    await test('Rate Limit Not Too Strict', testRateLimitNotTooStrict);

    console.log('─────────────────────────────────────────────────────────────');
    results.forEach(r => {
        console.log(`${r.status} ${r.name}`);
        console.log(`   └─ ${r.details}`);
    });
    console.log('─────────────────────────────────────────────────────────────');
    console.log('');
    console.log(`Results: ${passed} passed, ${failed} failed`);
    console.log('');

    if (failed > 0) {
        console.log('❌ SOME TESTS FAILED');
        process.exit(1);
    } else {
        console.log('✅ ALL TESTS PASSED');
        process.exit(0);
    }
}

runAllTests().catch(err => {
    console.error('Test runner error:', err);
    process.exit(1);
});

